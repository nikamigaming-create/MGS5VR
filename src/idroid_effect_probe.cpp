#include "mgs5vr/idroid_effect_probe.hpp"
#include "mgs5vr/head_camera.hpp"
#include "mgs5vr/idroid_rig.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/log.hpp"
#include "mgs5vr/arm_ik.hpp"
#include <windows.h>
#include <intrin.h>
#include <MinHook.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <sstream>

namespace {
using namespace mgs5vr;
using CompileGraph=void*(*)(void*,void*,void*);
using InitializeGraph=void(*)(void*,void*,void*);
using CompilePrimitive=void(*)(void*,void*);
using UpdatePrimitive=void(*)(void*,void*);
using RenderPrimitive=void(*)(void*,void*);
CompileGraph originalCompileGraph{};
InitializeGraph originalInitializeGraph{};
CompilePrimitive originalCompilePrimitive{};
UpdatePrimitive originalUpdatePrimitive{};
RenderPrimitive originalRenderPrimitive{};
uintptr_t base{};
uint32_t imageSize{};
std::atomic_bool enabled{};
std::atomic_bool retargetEnabled{};
std::atomic_uint renderReports{},retargetReports{};
NativeIdroidCandidateBudget projectionDiscoveryBudget;
std::array<uintptr_t,8> projectionProperties{};
std::atomic_uint64_t nextGeneration{1};
std::atomic_uint compileCalls{},initializeCalls{},primitiveCalls{},updateCalls{};
std::atomic_uint lightInitializeReports{},lightPrimitiveReports{};
struct Compilation {
    uint64_t generation{};uintptr_t graph{};bool light{};
    uint32_t graphName{};bool graphNameRead{};
};
thread_local Compilation compilation;
struct CompilationRestore {
    Compilation previous=compilation;
    ~CompilationRestore(){compilation=previous;}
};
struct Registration {
    uint64_t generation{};
    uintptr_t graph{},node{},properties{};
    std::array<std::byte,nativeIdroidPrimitivePropertySize> values{};
    bool ready{};
    uint8_t particleIndex{};
};
std::array<Registration,8> registrations;
std::mutex mutex;
std::atomic_uint64_t nextReport{},nextDiscoveryRetry{};
std::atomic_bool registrationsReady{},reportsExhausted{};
uint64_t discoveryWindowEnd{};
unsigned discoveryAttempts{};
unsigned reportCount{},registrationCount{};
NativeIdroidCandidateBudget candidateBudget;
std::array<NativeIdroidCandidateReportState,16> candidatesSeen;
std::atomic_uint64_t nextCandidateRetry{};
std::atomic_bool candidatesExhausted{};
struct CandidatePostObservation {
    NativeIdroidPrimitiveCandidate candidate;
    Pose emitter{},screen{};float width{};
    uint64_t sampleTime{},activation{},rigSequence{};
};
thread_local std::optional<CandidatePostObservation> postCandidate;

void writeCaller(std::ostringstream& s,uintptr_t caller){
    if(imageSize&&caller>=base&&caller-base<imageSize)
        s<<" caller_rva=0x"<<std::hex<<caller-base<<std::dec;
    else s<<" caller_rva=unknown";
}
template<class T> bool read(uintptr_t address,T& out) noexcept {
    SIZE_T copied{};
    return address&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),
        &out,sizeof(out),&copied)&&copied==sizeof(out);
}
bool readBytes(uintptr_t address,void* out,size_t bytes) noexcept {
    SIZE_T copied{};
    return address&&out&&bytes&&address<=UINTPTR_MAX-(bytes-1)
        &&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),out,bytes,&copied)
        &&copied==bytes;
}
bool writableBytes(uintptr_t address,size_t bytes) noexcept {
    MEMORY_BASIC_INFORMATION region{};
    if(!address||!bytes||address>UINTPTR_MAX-(bytes-1)
        ||VirtualQuery(reinterpret_cast<const void*>(address),&region,sizeof(region))!=sizeof(region)
        ||region.State!=MEM_COMMIT||(region.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
    const auto start=reinterpret_cast<uintptr_t>(region.BaseAddress);
    const auto protection=region.Protect&0xffu;
    return (protection==PAGE_READWRITE||protection==PAGE_WRITECOPY
        ||protection==PAGE_EXECUTE_READWRITE||protection==PAGE_EXECUTE_WRITECOPY)
        &&address>=start&&address-start<=region.RegionSize&&bytes<=region.RegionSize-(address-start);
}
bool writeBytes(uintptr_t address,const void* data,size_t bytes) noexcept {
    SIZE_T copied{};
    return writableBytes(address,bytes)&&WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),
        data,bytes,&copied)&&copied==bytes;
}
template<class T,size_t N> T field(const std::array<std::byte,N>& bytes,size_t offset) noexcept {
    T out{};
    if(offset<=N&&sizeof(T)<=N-offset)std::memcpy(&out,bytes.data()+offset,sizeof(T));
    return out;
}
bool retainedGraphMatches(uintptr_t graph) noexcept {
    uintptr_t vtable{};uint32_t name{};
    return read(graph,vtable)&&read(graph+0xe0,name)
        &&matchesNativeIdroidLightRetainedGraph(base,vtable,name);
}
bool retainedParticleNode(uintptr_t graph,uint8_t index,uintptr_t& node) noexcept {
    // Native 1B31980 relocates +118, using count +130. 1B30660 copies
    // 100-byte particle records, and 1B47990 relocates their primitive at B8.
    uintptr_t particles{};uint8_t count{};
    return retainedGraphMatches(graph)&&read(graph+0x118,particles)
        &&read(graph+0x130,count)&&particles&&index<count
        &&read(particles+static_cast<uintptr_t>(index)*0x100+0xb8,node)&&node;
}
bool readNode(uintptr_t node,uintptr_t& properties) noexcept {
    std::array<std::byte,0x50> bytes{};
    if(!read(node,bytes))return false;
    properties=field<uintptr_t>(bytes,0x38);
    return matchesNativeIdroidPrimitive(base,field<uintptr_t>(bytes,0),
        field<uintptr_t>(bytes,0x18),field<uintptr_t>(bytes,0x20),
        field<uintptr_t>(bytes,0x30),properties,field<uint16_t>(bytes,0x40));
}
void finishCompilation(Compilation scope,uintptr_t graph) {
    // 1B83668/1B8366D destroy/free the temporary graph before CompileGraph
    // returns. The output holder owns the separate retained 24E1450 graph.
    // Never dereference the prototype or its original primitive buffers here.
    if(!retainedGraphMatches(graph))return;
    uintptr_t particles{};uint8_t count{};
    if(!read(graph+0x118,particles)||!read(graph+0x130,count)||!particles||!count)return;
    std::lock_guard lock(mutex);
    for(unsigned index=0;index<count;++index){
        uintptr_t node{};
        uintptr_t properties{};
        std::array<std::byte,nativeIdroidPrimitivePropertySize> values{};
        if(!retainedParticleNode(graph,static_cast<uint8_t>(index),node)
            ||!readNode(node,properties)||!read(properties,values))continue;
        auto slot=std::find_if(registrations.begin(),registrations.end(),[&](const auto& record){
            return record.node==node||record.properties==properties;
        });
        if(slot==registrations.end())slot=std::find_if(registrations.begin(),registrations.end(),
            [](const auto& record){return !record.node;});
        if(slot==registrations.end())slot=std::min_element(registrations.begin(),registrations.end(),
            [](const auto& a,const auto& b){return a.generation<b.generation;});
        *slot={scope.generation,graph,node,properties,values,true,static_cast<uint8_t>(index)};
        const auto& record=*slot;
        registrationsReady.store(true);
        if(registrationCount<16){
            ++registrationCount;
            std::ostringstream s;s<<"iDroid Light probe registration graph_name=0xfdbc61cd graph=0x"
                <<std::hex<<record.graph<<" primitive=0x"<<record.node<<" properties=0x"
                <<record.properties<<" model_resource=0x"<<field<uintptr_t>(record.values,0x40)
                <<std::dec<<" generation="<<record.generation
                <<" particle_index="<<index<<" retained_graph=1"
                <<" native_graph_verified=1 player_owner_verified=0 read_only=1";
            log(s.str());
        }
    }
}
void* compileGraph(void* source,void* output,void* parameters){
    if(!enabled.load())return originalCompileGraph(source,output,parameters);
    const auto ordinal=compileCalls.fetch_add(1)+1;
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    Compilation completed;
    void* result{};
    {
        const CompilationRestore restore;
        compilation={nextGeneration.fetch_add(1),0,false};
        result=originalCompileGraph(source,output,parameters);
        completed=compilation;
    }
    if(ordinal<=8)try{
        std::ostringstream s;s<<"iDroid Light probe hook=compile_graph call="<<ordinal
            <<" generation="<<completed.generation<<" light_name_matched="<<completed.light
            <<" graph=0x"<<std::hex<<completed.graph<<" graph_name=0x"<<completed.graphName<<std::dec
            <<" graph_name_read="<<completed.graphNameRead;
        writeCaller(s,caller);
        s<<" player_owner_verified=0 read_only=1";log(s.str());
    }catch(...){}
    try{
        uintptr_t retained{};
        if(read(reinterpret_cast<uintptr_t>(output),retained))finishCompilation(completed,retained);
    }catch(...){}
    return result;
}
void initializeGraph(void* graph,void* properties,void* parameters){
    originalInitializeGraph(graph,properties,parameters);
    if(!enabled.load())return;
    const auto ordinal=initializeCalls.fetch_add(1)+1;
    const auto address=reinterpret_cast<uintptr_t>(graph);
    uintptr_t vtable{};uint32_t name{};
    const bool typeRead=read(address,vtable),nameRead=read(address+0xe0,name);
    const bool light=typeRead&&nameRead&&matchesNativeIdroidLightGraph(base,vtable,name);
    // Copy while this initialized prototype is still alive. Primitive factory
    // diagnostics and the post-compile report use only the copied scalar.
    if(compilation.generation){compilation.graphName=name;compilation.graphNameRead=nameRead;}
    // Report the first generic calls and reserve a separate small budget for
    // exact Light initializations, including loader paths without compile TLS.
    const bool report=ordinal<=4||(light&&lightInitializeReports.fetch_add(1)<4);
    if(report)try{
        std::ostringstream s;s<<"iDroid Light probe hook=initialize_graph call="<<ordinal
            <<" generation="<<compilation.generation<<" light_name_matched="<<light
            <<" graph=0x"<<std::hex<<address<<" graph_name=0x"<<name<<std::dec
            <<" type_read="<<typeRead<<" name_read="<<nameRead;
        writeCaller(s,reinterpret_cast<uintptr_t>(_ReturnAddress()));
        if(vtable>=base&&imageSize&&vtable-base<imageSize)
            s<<" graph_vtable_rva=0x"<<std::hex<<vtable-base<<std::dec;
        else s<<" graph_vtable_rva=unknown";
        s<<" player_owner_verified=0 read_only=1";log(s.str());
    }catch(...){}
    if(light&&compilation.generation){compilation.graph=address;compilation.light=true;}
}
void compilePrimitive(void* node,void* properties){
    originalCompilePrimitive(node,properties);
    if(!enabled.load())return;
    const auto ordinal=primitiveCalls.fetch_add(1)+1;
    const bool report=ordinal<=4||(compilation.light&&lightPrimitiveReports.fetch_add(1)<4);
    if(report)try{
        const auto address=reinterpret_cast<uintptr_t>(node);
        uintptr_t values{};
        const bool nodeMatches=readNode(address,values);
        std::array<std::byte,0x50> bytes{};
        const bool nodeRead=read(address,bytes);
        std::ostringstream s;s<<"iDroid Light probe hook=compile_primitive call="<<ordinal
            <<" generation="<<compilation.generation<<" light_scope="<<compilation.light
            <<" graph_name=0x"<<std::hex<<compilation.graphName<<std::dec
            <<" graph_name_read="<<compilation.graphNameRead
            <<" node_matches="<<nodeMatches<<" node_read="<<nodeRead
            <<" node=0x"<<std::hex<<address<<" properties=0x"<<values<<std::dec
            <<" property_size="<<field<uint16_t>(bytes,0x40);
        writeCaller(s,reinterpret_cast<uintptr_t>(_ReturnAddress()));
        s<<" player_owner_verified=0 read_only=1";log(s.str());
    }catch(...){}
}
void observeCurrentCandidate(uintptr_t instance,uintptr_t caller){
    // Ordinary callbacks continue after compile/cache loading. The exact
    // current callback owner at 1B8835E supplies an already-finalized primitive
    // instance. Discovery therefore does not depend on a compile registration.
    // Generic packed-interpreter labels are never called an EffectName here.
    const auto now=steadyMilliseconds();
    if(caller!=base+0x1b8835eu||candidatesExhausted.load()||now<nextCandidateRetry.load())return;
    const auto frame=headCamera().publishedRigFrame(now);
    if(!frame||!frame->applied||!frame->menuIdroid||!frame->idroidDeviceTracked
        ||!frame->activation||!frame->playerOwner||now<frame->sampleTime||now-frame->sampleTime>150){
        nextCandidateRetry.store(now+500);return;
    }
    {
        std::lock_guard lock(mutex);
        if(!candidateBudget.admit(now)){
            nextCandidateRetry.store(candidateBudget.windowEnd);
            if(candidateBudget.total>=NativeIdroidCandidateBudget::totalLimit)candidatesExhausted.store(true);
            return;
        }
    }
    const auto candidate=readNativeIdroidPrimitiveCandidate(base,instance,&readBytes);
    if(!candidate)return;
    const auto expected=compose(frame->idroidDevice,Pose{{},{0,.0434f,-.0019f}});
    const std::array<float,7> emitter{expected.orientation.x,expected.orientation.y,
        expected.orientation.z,expected.orientation.w,expected.position.x,expected.position.y,expected.position.z};
    {
        std::lock_guard lock(mutex);
        if(candidateBudget.total>=NativeIdroidCandidateBudget::totalLimit
            ||candidateBudget.reports>=NativeIdroidCandidateBudget::reportsPerWindow)return;
        auto slot=std::find_if(candidatesSeen.begin(),candidatesSeen.end(),[&](const auto& entry){
            return entry.properties==candidate->properties&&entry.model==candidate->model;
        });
        if(slot!=candidatesSeen.end()&&!slot->changed(*candidate,emitter,frame->activation,now))return;
        if(slot==candidatesSeen.end())slot=std::find_if(candidatesSeen.begin(),candidatesSeen.end(),
            [](const auto& entry){return !entry.properties;});
        if(slot==candidatesSeen.end())slot=std::min_element(candidatesSeen.begin(),candidatesSeen.end(),
            [](const auto& a,const auto& b){return a.nextReport<b.nextReport;});
        slot->remember(*candidate,emitter,frame->activation,now);
        candidateBudget.reported();
        if(candidateBudget.total>=NativeIdroidCandidateBudget::totalLimit)candidatesExhausted.store(true);
    }
    const auto& c=*candidate;
    std::ostringstream s;s<<"iDroid Light probe discovery sample_ms="<<now
        <<" candidate_only=1 native_graph_verified=0 graph_name=unknown player_owner_verified=0 coherent_frame=0"
        <<" registration_ready="<<registrationsReady.load()
        <<" instance=0x"<<std::hex<<c.instance<<" properties=0x"<<c.properties<<" model_instance=0x"<<c.model
        <<" model_type_rva=0x20f4d90 parent=0x"<<c.parent<<std::dec
        <<" property_stable=1 model_type_verified=1 vertices="<<c.vertices<<" indices="<<c.indices
        <<" triangles="<<c.triangles<<" authored_cone_geometry_candidate="
        <<(c.vertices==32u&&c.indices==96u&&c.triangles==32u)
        <<" native_flags="<<static_cast<unsigned>(c.flags)<<" particle_batch_count="<<c.batchCount
        <<" first_particle="<<c.first<<" last_particle="<<c.last
        <<" parent_matrix_read="<<c.parentMatrixRead<<" parent_affine="
        <<(c.parentMatrixRead&&nativeAffinePose(c.parentMatrix).has_value())
        <<" bones="<<c.bones<<" bone_names_read="<<c.boneNamesRead
        <<" rig_activation="<<frame->activation<<" rig_sequence="<<frame->rigSequence
        <<" rig_age_ms="<<now-frame->sampleTime;
    writeCaller(s,caller);
    s<<" properties_hex="<<std::hex;
    for(const auto value:c.values){const auto b=std::to_integer<unsigned>(value);s<<"0123456789abcdef"[b>>4]<<"0123456789abcdef"[b&15];}
    if(c.boneNamesRead){s<<" bone_names=";for(unsigned i=0;i<c.bones;++i){if(i)s<<',';s<<c.boneNames[i];}}
    s<<" model_resource=0x"<<c.modelResource<<" model_resource_code64=0x"<<c.modelResourceCode
        <<" authored_cone_resource_match="<<(c.modelResourceCode==0x84a3182ae4e26449ull)
        <<" interpreter_argument3=0x"<<c.interpreterArgument3<<" interpreter_argument4=0x"<<c.interpreterArgument4;
    s<<std::dec;
    if(c.parentMatrixRead){s<<" parent_matrix=";for(size_t i=0;i<c.parentMatrix.size();++i){if(i)s<<',';s<<c.parentMatrix[i];}}
    s<<" model_matrix=";for(size_t i=0;i<c.modelMatrix.size();++i){if(i)s<<',';s<<c.modelMatrix[i];}
    s<<" expected_camera="<<expected.position.x<<','<<expected.position.y<<','<<expected.position.z;
    s<<" expected_camera_orientation="<<expected.orientation.x<<','<<expected.orientation.y<<','
        <<expected.orientation.z<<','<<expected.orientation.w<<" rig_player_owner=0x"<<std::hex<<frame->playerOwner<<std::dec;
    if(const auto pose=trackedIdroidPose(*frame)){
        s<<" screen_center="<<pose->screen.position.x<<','<<pose->screen.position.y<<','<<pose->screen.position.z;
        if(c.modelResourceCode==nativeIdroidConeResourceCode)
            postCandidate=CandidatePostObservation{c,expected,pose->screen,frame->controllers.idroidScreenWidth,
                now,frame->activation,frame->rigSequence};
    }
    s<<" read_only=1";log(s.str());
}
void observePostUpdateClones(uintptr_t particles,const CandidatePostObservation& sample){
    const auto clones=readNativeIdroidConeDraws(base,particles,sample.candidate,&readBytes);
    std::array<std::byte,16> pool{};
    const bool poolRead=particles==sample.candidate.particlePool&&read(particles,pool);
    std::ostringstream s;s<<"iDroid Light draw sample_ms="<<steadyMilliseconds()
        <<" source_sample_ms="<<sample.sampleTime<<" rig_activation="<<sample.activation
        <<" rig_sequence="<<sample.rigSequence<<" pool=0x"<<std::hex<<particles<<std::dec
        <<" typed_draw_batch_read="<<clones.has_value()<<" player_owner_verified=0 read_only=1"
        <<" native_pool_header_read="<<poolRead;
    if(poolRead)s<<" native_pool_count="<<field<uint32_t>(pool,0)<<" native_pool_capacity="<<field<uint32_t>(pool,4)
        <<" native_pool_entries=0x"<<std::hex<<field<uintptr_t>(pool,8)<<std::dec;
    s<<" full_emitter_frame_match="<<matchesNativeIdroidConeEmitter(sample.candidate,sample.emitter);
    if(clones){
        s<<" active_draws="<<clones->activeCount;
        const auto point=[](const std::array<float,16>& w,Vec3 p){return Vec3{
            p.x*w[0]+p.y*w[4]+p.z*w[8]+w[12],p.x*w[1]+p.y*w[5]+p.z*w[9]+w[13],
            p.x*w[2]+p.y*w[6]+p.z*w[10]+w[14]};};
        for(uint32_t i=0;i<clones->activeCount;++i){
            const auto& c=clones->active[i];const auto coneNear=point(c.world,{0,0,.01f}),coneFar=point(c.world,{0,0,2});
            s<<" draw"<<i<<"=0x"<<std::hex<<c.record<<std::dec<<" world"<<i<<'=';
            for(size_t j=0;j<c.world.size();++j){if(j)s<<',';s<<c.world[j];}
            s<<" raw_near_center"<<i<<'='<<coneNear.x<<','<<coneNear.y<<','<<coneNear.z
                <<" raw_far_center"<<i<<'='<<coneFar.x<<','<<coneFar.y<<','<<coneFar.z;
        }
    }
    log(s.str());
}
void observePrimitive(uintptr_t instance,uintptr_t caller){
    // Native 1B6D640 reads instance+30 as its finalized property block. Native
    // 1B6C240 reads the parent matrix at *instance and particle arrays at A0.
    // The parent identity is deliberately unknown until its player attachment
    // chain is verified. The registration proves the exact Light graph only.
    const auto now=steadyMilliseconds();
    if(!registrationsReady.load()||reportsExhausted.load()
        ||now<nextReport.load()||now<nextDiscoveryRetry.load())return;
    // Most native updates belong to unrelated effects. Once a report is
    // emitted, no native memory is read for 500 ms. While the Light is absent
    // or not yet joined, discovery is bounded to 256 attempts per 500 ms.
    {
        std::lock_guard lock(mutex);
        if(now<nextReport.load()||reportCount>=128)return;
        if(now>=discoveryWindowEnd){discoveryWindowEnd=now+500;discoveryAttempts=0;}
        if(discoveryAttempts>=256){nextDiscoveryRetry.store(discoveryWindowEnd);return;}
        ++discoveryAttempts;
    }
    uintptr_t properties{};
    if(!read(instance+0x30,properties))return;
    Registration record;
    {
        std::lock_guard lock(mutex);
        if(now<nextReport.load()||reportCount>=128)return;
        const auto found=std::find_if(registrations.begin(),registrations.end(),[&](const auto& value){
            return value.ready&&value.properties==properties;
        });
        if(found==registrations.end())return;
        record=*found;
        nextReport.store(now+500);++reportCount;
        if(reportCount>=128)reportsExhausted.store(true);
    }
    uintptr_t currentNode{},currentProperties{};
    std::array<std::byte,nativeIdroidPrimitivePropertySize> values{};
    if(!retainedParticleNode(record.graph,record.particleIndex,currentNode)||currentNode!=record.node
        ||!readNode(currentNode,currentProperties)
        ||currentProperties!=record.properties||!read(properties,values)||values!=record.values)return;
    std::array<std::byte,0xa8> header{};
    if(!read(instance,header)||field<uintptr_t>(header,0x30)!=properties)return;
    const auto parent=field<uintptr_t>(header,0);
    std::array<float,16> matrix{};
    const bool matrixRead=read(parent,matrix);
    const auto emitter=matrixRead?nativeAffinePose(matrix):std::nullopt;
    const auto frame=headCamera().publishedRigFrame(now);
    const bool fit=frame&&frame->applied&&frame->menuIdroid&&frame->idroidDeviceTracked
        &&frame->activation&&frame->playerOwner&&now>=frame->sampleTime&&now-frame->sampleTime<=150;
    std::ostringstream s;s<<"iDroid Light probe update sample_ms="<<now
        <<" generation="<<record.generation<<" native_graph_verified=1 player_owner_verified=0 coherent_frame=0"
        <<" instance=0x"<<std::hex<<instance<<" parent=0x"<<parent<<" properties=0x"<<properties
        <<" model_resource=0x"<<field<uintptr_t>(values,0x40)<<std::dec
        <<" native_flags="<<static_cast<unsigned>(field<uint8_t>(header,0x28))
        <<" particle_batch_count="<<field<uint32_t>(header,0x60)
        <<" first_particle="<<field<uint32_t>(header,0x64)
        <<" last_particle="<<field<uint32_t>(header,0x68)
        <<" emitter_affine="<<static_cast<bool>(emitter)<<" current_idroid_fit="<<fit;
    writeCaller(s,caller);
    if(matrixRead){s<<" emitter_matrix=";for(size_t i=0;i<matrix.size();++i){if(i)s<<',';s<<matrix[i];}}
    if(fit){
        // Owned FMDL camera bone, not CNP_HOLOGRAM and not the palm. Its 92
        // weighted device vertices prohibit using this as a writable projector.
        const auto expected=compose(frame->idroidDevice,Pose{{},{0,.0434f,-.0019f}});
        s<<" rig_activation="<<frame->activation<<" rig_sequence="<<frame->rigSequence
            <<" rig_age_ms="<<now-frame->sampleTime<<" player=0x"<<std::hex<<frame->playerOwner<<std::dec
            <<" expected_camera="<<expected.position.x<<','<<expected.position.y<<','<<expected.position.z;
        if(emitter){
            const auto d=emitter->position-expected.position;
            s<<" emitter_camera_distance="<<std::sqrt(dot(d,d));
        }
        if(const auto pose=trackedIdroidPose(*frame))s<<" screen_center="
            <<pose->screen.position.x<<','<<pose->screen.position.y<<','<<pose->screen.position.z;
    }
    s<<" read_only=1";log(s.str());
}
void updatePrimitive(void* particles,void* instance){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    postCandidate.reset();
    if(enabled.load())try{
        const auto ordinal=updateCalls.fetch_add(1)+1;
        if(ordinal<=8){
            std::ostringstream s;s<<"iDroid Light probe hook=update_primitive call="<<ordinal
                <<" compile_calls="<<compileCalls.load()<<" initialize_calls="<<initializeCalls.load()
                <<" primitive_calls="<<primitiveCalls.load()
                <<" registrations_ready="<<registrationsReady.load()
                <<" reports_exhausted="<<reportsExhausted.load();
            writeCaller(s,caller);
            s<<" player_owner_verified=0 read_only=1";log(s.str());
        }
        observeCurrentCandidate(reinterpret_cast<uintptr_t>(instance),caller);
        observePrimitive(reinterpret_cast<uintptr_t>(instance),caller);
    }catch(...){}
    originalUpdatePrimitive(particles,instance);
    if(enabled.load()&&postCandidate)try{
        observePostUpdateClones(reinterpret_cast<uintptr_t>(particles),*postCandidate);
    }catch(...){}
    postCandidate.reset();
}
void renderPrimitive(void* particles,void* instance){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    // Native rendering metadata is completed first; its later sort/culling
    // consumer sees the coherent fitted matrix AND matching bounds together.
    originalRenderPrimitive(particles,instance);
    if(!enabled.load()&&!retargetEnabled.load())return;
    try{
        const auto now=steadyMilliseconds();
        const auto frame=headCamera().publishedRigFrame(now);
        if(!frame||!frame->menuIdroid||!frame->idroidDeviceTracked||!frame->playerOwner
            ||!frame->controllers.handheldMenus||frame->menuWorldQuad)return;
        uintptr_t properties{};
        const auto address=reinterpret_cast<uintptr_t>(instance);
        if(!read(address+0x30,properties))return;
        {
            std::lock_guard lock(mutex);
            if(std::find(projectionProperties.begin(),projectionProperties.end(),properties)==projectionProperties.end()
                &&!projectionDiscoveryBudget.admit(now))return;
        }
        const auto candidate=readNativeIdroidPrimitiveCandidate(base,address,&readBytes);
        if(!candidate||candidate->modelResourceCode!=nativeIdroidConeResourceCode)return;
        {
            std::lock_guard lock(mutex);
            if(std::find(projectionProperties.begin(),projectionProperties.end(),properties)==projectionProperties.end()){
                const auto empty=std::find(projectionProperties.begin(),projectionProperties.end(),uintptr_t{});
                if(empty!=projectionProperties.end())*empty=properties;
            }
        }
        const auto pose=trackedIdroidPose(*frame);
        if(!pose)return;
        const auto emitter=compose(frame->idroidDevice,Pose{{},{0,.0434f,-.0019f}});
        const auto pool=reinterpret_cast<uintptr_t>(particles);
        if(enabled.load()&&renderReports.fetch_add(1)<16){
            std::ostringstream s;s<<"iDroid Light render callback sample_ms="<<now
                <<" properties=0x"<<std::hex<<properties<<std::dec<<" batch_count="<<candidate->batchCount
                <<" first="<<candidate->first<<" last="<<candidate->last;
            writeCaller(s,caller);s<<" read_only=1";log(s.str());
            observePostUpdateClones(pool,CandidatePostObservation{*candidate,emitter,pose->screen,
                frame->controllers.idroidScreenWidth,now,frame->activation,frame->rigSequence});
        }
        if(!retargetEnabled.load())return;
        const auto current=headCamera().publishedRigFrame(steadyMilliseconds());
        if(!current||current->activation!=frame->activation||current->rigSequence!=frame->rigSequence
            ||current->playerOwner!=frame->playerOwner)return;
        const bool fitted=retargetNativeIdroidConeDraw(base,pool,*candidate,emitter,pose->screen,
            frame->controllers.idroidScreenWidth,&readBytes,&writableBytes,&writeBytes);
        if(fitted&&retargetReports.fetch_add(1)<16){
            std::ostringstream s;s<<"iDroid Light retarget sample_ms="<<now
                <<" properties=0x"<<std::hex<<properties<<std::dec<<" rig_activation="<<frame->activation
                <<" rig_sequence="<<frame->rigSequence<<" player_owner_verified=1 canonical_resource_verified=1"
                <<" current_draw_and_bounds_fitted=1 fitted_draw_count="<<candidate->batchCount
                <<" device_bones_changed=0";log(s.str());
        }
    }catch(...){}
}
template<size_t N> bool matches(uintptr_t rva,const std::array<unsigned char,N>& expected) noexcept {
    std::array<unsigned char,N> actual{};
    return read(base+rva,actual)&&actual==expected;
}
}
namespace mgs5vr {
void installIdroidEffectProbe(uintptr_t imageBase,bool configured,bool retarget) noexcept {
    wchar_t setting[8]{};
    const bool environment=GetEnvironmentVariableW(L"MGS5VR_IDROID_EFFECT_PROBE",setting,8)==1&&setting[0]==L'1';
    if(!configured&&!environment&&!retarget)return;
    if(enabled.load()||retargetEnabled.load())return;
    base=imageBase;
    // Archived owned 1.0.15.4 native view: registration, full entry ABIs,
    // ModuleGraph's final type/name write, and property-buffer pointer write.
    // All comparisons happen before creating any hook. Failure preserves flow.
    const bool verified=base
        &&matches(0x1b81ea0,std::array<unsigned char,19>{0x40,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0xac,0x24,0x60,0xfd,0xff})
        &&matches(0x1b853a0,std::array<unsigned char,14>{0x48,0x89,0x5c,0x24,0x18,0x57,0x48,0x83,0xec,0x70,0x48,0x8b,0x05,0x7f})
        &&matches(0x1b6bb00,std::array<unsigned char,18>{0x40,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0x6c,0x24,0xb0,0x48})
        &&matches(0x1b6d640,std::array<unsigned char,21>{0x48,0x83,0xec,0x38,0x4c,0x8b,0x4a,0x30,0x4c,0x8d,0x51,0x20,0x41,0x8b,0x41,0x5c,0x4c,0x03,0xd0,0x41,0x8b})
        &&matches(0x1b81fc9,std::array<unsigned char,11>{0x48,0x8d,0x05,0x90,0xf4,0x95,0x00,0x49,0x89,0x45,0x00})
        &&matches(0x1b854a3,std::array<unsigned char,6>{0x89,0x87,0xe0,0x00,0x00,0x00})
        &&matches(0x1b1d5fb,std::array<unsigned char,13>{0x48,0x89,0x5d,0x08,0x48,0x8b,0x5c,0x24,0x30,0x66,0x89,0x75,0x10})
        &&matches(0x1b82f93,std::array<unsigned char,10>{0x48,0x8d,0x05,0xb6,0xe4,0x95,0x00,0x48,0x89,0x03})
        &&matches(0x1b80074,std::array<unsigned char,12>{0x8b,0x87,0xe0,0x00,0x00,0x00,0x89,0x83,0xe0,0x00,0x00,0x00})
        &&matches(0x1b31a96,std::array<unsigned char,8>{0x44,0x0f,0xb6,0x87,0x30,0x01,0x00,0x00})
        &&matches(0x1b3078c,std::array<unsigned char,14>{0x49,0x8b,0x82,0xa0,0x00,0x00,0x00,0x48,0x89,0x83,0xb8,0x00,0x00,0x00})
        &&matches(0x1b47a3e,std::array<unsigned char,7>{0x48,0x89,0xbd,0xb8,0x00,0x00,0x00})
        &&matches(0x1b1d577,std::array<unsigned char,4>{0x48,0x89,0x7b,0x08})
        &&matches(0x1b6bed7,std::array<unsigned char,4>{0x48,0x89,0x45,0x10})
        &&matches(0x1b6bf84,std::array<unsigned char,19>{0x41,0xb8,0x70,0x00,0x00,0x00,0x48,0x8d,0x55,0xd0,0x49,0x8d,0x4e,0x30,0xe8,0x09,0x16,0xfb,0xff})
        &&matches(0x1c7851,std::array<unsigned char,11>{0x48,0x8d,0x55,0x10,0x48,0x8d,0x8b,0x18,0x01,0x00,0x00})
        &&matches(0x76d16b,std::array<unsigned char,13>{0x48,0x8d,0x53,0x30,0x48,0x8d,0x4f,0x08,0xe8,0x28,0x86,0x91,0xff})
        &&matches(0x1b86c6c,std::array<unsigned char,12>{0x0f,0xb7,0x42,0x40,0x48,0x03,0x42,0x28,0x48,0x89,0x41,0x38})
        &&matches(0x1b6c94f,std::array<unsigned char,35>{0x4a,0x8b,0x04,0xdb,0x4c,0x8b,0x44,0x24,0x30,0x44,0x0f,0x29,0x40,0x40,0x44,0x0f,0x29,0x50,0x50,0x44,0x0f,0x28,0x54,0x24,0x50,0x44,0x0f,0x29,0x60,0x60,0x44,0x0f,0x29,0x68,0x70})
        &&matches(0x1b6d670,std::array<unsigned char,23>{0x48,0x89,0x5c,0x24,0x10,0x57,0x48,0x8b,0x02,0x4c,0x8b,0x4a,0x30,0x4c,0x8b,0xd2,0x8b,0x52,0x68,0x0f,0xb6,0x78,0x4c});
    if(!verified){log("iDroid Light read-only probe disabled: native ABI differs");return;}
    // Caller diagnostics are an RVA only when the return address belongs to
    // this PE image. A different module or unreadable header remains unknown.
    IMAGE_DOS_HEADER dos{};IMAGE_NT_HEADERS64 nt{};
    imageSize=read(base,dos)&&dos.e_magic==IMAGE_DOS_SIGNATURE
        &&dos.e_lfanew>=static_cast<LONG>(sizeof(dos))&&dos.e_lfanew<=0x1000
        &&read(base+static_cast<uintptr_t>(dos.e_lfanew),nt)&&nt.Signature==IMAGE_NT_SIGNATURE
        &&nt.OptionalHeader.Magic==IMAGE_NT_OPTIONAL_HDR64_MAGIC
        &&nt.OptionalHeader.SizeOfImage&&nt.OptionalHeader.SizeOfImage<=0x40000000u
        ?nt.OptionalHeader.SizeOfImage:0;
    std::array<void*,5> addresses{reinterpret_cast<void*>(base+0x1b81ea0),reinterpret_cast<void*>(base+0x1b853a0),
        reinterpret_cast<void*>(base+0x1b6bb00),reinterpret_cast<void*>(base+0x1b6d640),reinterpret_cast<void*>(base+0x1b6d670)};
    const std::array<void*,5> detours{reinterpret_cast<void*>(&compileGraph),reinterpret_cast<void*>(&initializeGraph),
        reinterpret_cast<void*>(&compilePrimitive),reinterpret_cast<void*>(&updatePrimitive),reinterpret_cast<void*>(&renderPrimitive)};
    const std::array<void**,5> originals{reinterpret_cast<void**>(&originalCompileGraph),reinterpret_cast<void**>(&originalInitializeGraph),
        reinterpret_cast<void**>(&originalCompilePrimitive),reinterpret_cast<void**>(&originalUpdatePrimitive),reinterpret_cast<void**>(&originalRenderPrimitive)};
    size_t created{};
    MH_STATUS status=MH_OK;
    for(;created<addresses.size();++created){
        status=MH_CreateHook(addresses[created],detours[created],originals[created]);
        if(status!=MH_OK)break;
    }
    if(status==MH_OK)for(const auto address:addresses){status=MH_QueueEnableHook(address);if(status!=MH_OK)break;}
    if(status==MH_OK)status=MH_ApplyQueued();
    if(status!=MH_OK){
        for(size_t i=0;i<created;++i){MH_DisableHook(addresses[i]);MH_RemoveHook(addresses[i]);}
        log("iDroid Light read-only probe disabled: hook installation failed");return;
    }
    candidateBudget={};candidatesSeen={};nextCandidateRetry.store(0);candidatesExhausted.store(false);
    projectionDiscoveryBudget={};projectionProperties={};renderReports.store(0);retargetReports.store(0);
    enabled.store(configured||environment);retargetEnabled.store(retarget);
    std::ostringstream s;s<<"iDroid Light probe installed opt_in="<<(configured?"game_ini":environment?"environment":"retarget")
        <<" diagnostics="<<(configured||environment)<<" retarget_requested="<<retarget
        <<" canonical_source_and_current_emitter_required=1";log(s.str());
}
void stopIdroidEffectProbe() noexcept {
    enabled.store(false);
    retargetEnabled.store(false);
    std::lock_guard lock(mutex);registrations={};registrationsReady.store(false);
    candidateBudget={};candidatesSeen={};candidatesExhausted.store(true);
    projectionDiscoveryBudget={};projectionProperties={};
}
}
