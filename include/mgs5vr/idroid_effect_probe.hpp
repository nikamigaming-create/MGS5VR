#pragma once
#include "core.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <limits>
#include <optional>

namespace mgs5vr {
// Diagnostics are read-only. Separately enabling retarget admits only the
// canonical player Light's current draw transform/bounds, never device bones,
// cached models, controllers, menus or saves. Both options default off.
void installIdroidEffectProbe(uintptr_t imageBase,bool configured=false,bool retarget=false) noexcept;
void stopIdroidEffectProbe() noexcept;

constexpr uint32_t nativeIdroidLightGraphName=0xfdbc61cdu;
constexpr uint16_t nativeIdroidPrimitivePropertySize=0x70u;
constexpr uint64_t nativeIdroidConeResourceCode=0x84a3182ae4e26449ull;
constexpr bool matchesNativeIdroidLightGraph(uintptr_t imageBase,
    uintptr_t vtable,uint32_t name) noexcept {
    return imageBase&&vtable==imageBase+0x24e1460u&&name==nativeIdroidLightGraphName;
}
constexpr bool matchesNativeIdroidLightRetainedGraph(uintptr_t imageBase,
    uintptr_t vtable,uint32_t name) noexcept {
    return imageBase&&vtable==imageBase+0x24e1450u&&name==nativeIdroidLightGraphName;
}
constexpr bool matchesNativeIdroidPrimitive(uintptr_t imageBase,
    uintptr_t destroy,uintptr_t update,uintptr_t render,uintptr_t initialize,
    uintptr_t properties,uint16_t propertySize) noexcept {
    return imageBase&&destroy==imageBase+0x1b6bab0u
        &&update==imageBase+0x1b6d640u&&render==imageBase+0x1b6c0d0u
        &&initialize==imageBase+0x1b6c230u&&properties
        &&propertySize==nativeIdroidPrimitivePropertySize;
}

// A current model-primitive callback can outlive the compilation hook's
// installation. These reads identify only that callback's finalized model;
// they do not assign an effect graph, device owner or pose generation.
struct NativeIdroidPrimitiveCandidate {
    uintptr_t instance{},properties{},model{},parent{};
    uintptr_t particlePool{};
    uintptr_t interpreterArgument3{},interpreterArgument4{},modelResource{};
    uint64_t modelResourceCode{};
    std::array<std::byte,nativeIdroidPrimitivePropertySize> values{};
    std::array<float,16> parentMatrix{},modelMatrix{};
    std::array<uint32_t,16> boneNames{};
    uint32_t vertices{},indices{},triangles{},batchCount{},first{},last{};
    uint16_t bones{};
    uint8_t flags{};
    bool parentMatrixRead{},boneNamesRead{};
};
// Keep the finite output budget for changed poses. A stationary Help page can
// stay open indefinitely without consuming another candidate report. Identity
// and motion here schedule diagnostics only; neither certifies player ownership.
struct NativeIdroidCandidateReportState {
    uintptr_t properties{},model{},parent{};
    uint64_t activation{},nextReport{};
    uint32_t batchCount{},first{},last{};
    std::array<float,16> parentMatrix{};
    // Quaternion xyzw followed by translation xyz, from one published frame.
    std::array<float,7> emitterPose{};
    bool parentMatrixRead{};
    bool changed(const NativeIdroidPrimitiveCandidate& c,
        const std::array<float,7>& emitter,uint64_t currentActivation,uint64_t now) const noexcept {
        if(now<nextReport)return false;
        if(!properties||properties!=c.properties||model!=c.model||parent!=c.parent
            ||activation!=currentActivation||parentMatrixRead!=c.parentMatrixRead
            ||batchCount!=c.batchCount||first!=c.first||last!=c.last)return true;
        const auto displaced=[](float a,float b,float tolerance){
            return std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)>tolerance;
        };
        if(c.parentMatrixRead){
            for(size_t i=0;i<12;++i)if(displaced(parentMatrix[i],c.parentMatrix[i],.015f))return true;
            for(size_t i=12;i<15;++i)if(displaced(parentMatrix[i],c.parentMatrix[i],.005f))return true;
        }
        for(size_t i=4;i<7;++i)if(displaced(emitterPose[i],emitter[i],.005f))return true;
        float dot{},normBefore{},normAfter{};
        for(size_t i=0;i<4;++i){
            dot+=emitterPose[i]*emitter[i];normBefore+=emitterPose[i]*emitterPose[i];normAfter+=emitter[i]*emitter[i];
        }
        // Equivalent quaternion signs are the same pose; approximately two
        // degrees of rotation is enough to retain an independently aimed pose.
        return std::isfinite(dot)&&normBefore>0&&normAfter>0
            &&std::abs(dot)<.9998477f*std::sqrt(normBefore*normAfter);
    }
    void remember(const NativeIdroidPrimitiveCandidate& c,
        const std::array<float,7>& emitter,uint64_t currentActivation,uint64_t now) noexcept {
        properties=c.properties;model=c.model;parent=c.parent;activation=currentActivation;
        batchCount=c.batchCount;first=c.first;last=c.last;
        parentMatrix=c.parentMatrix;parentMatrixRead=c.parentMatrixRead;emitterPose=emitter;
        nextReport=now>std::numeric_limits<uint64_t>::max()-500u
            ?std::numeric_limits<uint64_t>::max():now+500u;
    }
};
struct NativeIdroidCandidateBudget {
    static constexpr unsigned attemptsPerWindow=128,reportsPerWindow=8,totalLimit=64;
    uint64_t windowEnd{};
    unsigned attempts{},reports{},total{};
    bool admit(uint64_t now) noexcept {
        if(total>=totalLimit)return false;
        if(!windowEnd||(windowEnd!=std::numeric_limits<uint64_t>::max()&&now>=windowEnd)){
            windowEnd=now>std::numeric_limits<uint64_t>::max()-500u
                ?std::numeric_limits<uint64_t>::max():now+500u;
            attempts=reports=0;
        }
        if(attempts>=attemptsPerWindow||reports>=reportsPerWindow)return false;
        ++attempts;return true;
    }
    void reported() noexcept {++reports;++total;}
};
template<class Reader> std::optional<NativeIdroidPrimitiveCandidate>
readNativeIdroidPrimitiveCandidate(uintptr_t imageBase,uintptr_t instance,Reader&& read) {
    const auto bounded=[](uintptr_t p,size_t bytes){
        return p&&bytes&&p<=std::numeric_limits<uintptr_t>::max()-(bytes-1);
    };
    const auto field=[]<class T,size_t N>(const std::array<std::byte,N>& bytes,size_t offset){
        T out{};if(offset<=N&&sizeof(T)<=N-offset)std::memcpy(&out,bytes.data()+offset,sizeof(T));return out;
    };
    std::array<std::byte,0xa8> header{},after{};
    if(!imageBase||imageBase>std::numeric_limits<uintptr_t>::max()-0x20f4d90u
        ||!bounded(instance,header.size())||!read(instance,header.data(),header.size()))return std::nullopt;
    NativeIdroidPrimitiveCandidate out;
    out.instance=instance;out.properties=field.template operator()<uintptr_t>(header,0x30);
    if(!bounded(out.properties,out.values.size())||!read(out.properties,out.values.data(),out.values.size()))return std::nullopt;
    out.model=field.template operator()<uintptr_t>(out.values,0x40);
    std::array<std::byte,0x128> model{};
    if(!bounded(out.model,model.size())||!read(out.model,model.data(),model.size())
        ||field.template operator()<uintptr_t>(model,0)!=imageBase+0x20f4d90u)return std::nullopt;
    // 1B6BED7 stores the newly created GrModel into the stack property block;
    // 1B6BF84..92 copies its complete 70-byte finalized block. The slot is a
    // model instance, not a filename hash or an effect owner.
    out.vertices=field.template operator()<uint32_t>(out.values,0x4c);
    out.indices=field.template operator()<uint32_t>(out.values,0x50);
    out.triangles=field.template operator()<uint32_t>(out.values,0x54);
    out.parent=field.template operator()<uintptr_t>(header,0);
    out.particlePool=field.template operator()<uintptr_t>(header,0x38);
    // The packed interpreter's incoming arguments are copied into its current
    // callback header at +8/+10. Their semantic owners remain unverified.
    out.interpreterArgument3=field.template operator()<uintptr_t>(header,8);
    out.interpreterArgument4=field.template operator()<uintptr_t>(header,0x10);
    // GrModel constructor 1C7851..5F initializes this resource holder; 76D16B..
    // 73 copies source resource+30 into holder+8. This is the finalized native
    // resource code, distinct from the shared model instance at property+40.
    out.modelResource=field.template operator()<uintptr_t>(model,0x118);
    out.modelResourceCode=field.template operator()<uint64_t>(model,0x120);
    out.flags=field.template operator()<uint8_t>(header,0x28);
    // 1B8B396 stores the incoming task count; 1B8B3AA stores first+count.
    // This is a chunk range, not the capacity of the shared particle pool.
    out.batchCount=field.template operator()<uint32_t>(header,0x60);
    out.first=field.template operator()<uint32_t>(header,0x64);
    out.last=field.template operator()<uint32_t>(header,0x68);
    if(out.batchCount>65536u||out.first>out.last||out.last>65536u||out.last-out.first!=out.batchCount
        ||out.vertices>1048576u||out.indices>3145728u||out.triangles>1048576u)return std::nullopt;
    out.modelMatrix=field.template operator()<std::array<float,16>>(model,0x40);
    out.bones=field.template operator()<uint16_t>(model,0xf8);
    const auto names=field.template operator()<uintptr_t>(model,0xe8);
    if(out.bones&&out.bones<=out.boneNames.size()&&bounded(names,out.bones*sizeof(uint32_t)))
        out.boneNamesRead=read(names,out.boneNames.data(),out.bones*sizeof(uint32_t));
    if(bounded(out.parent,sizeof(out.parentMatrix)))out.parentMatrixRead=read(out.parent,out.parentMatrix.data(),sizeof(out.parentMatrix));
    std::array<std::byte,nativeIdroidPrimitivePropertySize> current{};
    uintptr_t currentType{},currentResource{};uint64_t currentResourceCode{};
    if(!read(instance,after.data(),after.size())||field.template operator()<uintptr_t>(after,0x30)!=out.properties
        ||field.template operator()<uintptr_t>(after,0)!=out.parent
        ||field.template operator()<uintptr_t>(after,0x38)!=out.particlePool
        ||field.template operator()<uintptr_t>(after,8)!=out.interpreterArgument3
        ||field.template operator()<uintptr_t>(after,0x10)!=out.interpreterArgument4
        ||field.template operator()<uint32_t>(after,0x60)!=out.batchCount
        ||field.template operator()<uint32_t>(after,0x64)!=out.first
        ||field.template operator()<uint32_t>(after,0x68)!=out.last
        ||!read(out.properties,current.data(),current.size())||current!=out.values
        ||!read(out.model,&currentType,sizeof(currentType))||currentType!=imageBase+0x20f4d90u
        ||!read(out.model+0x118,&currentResource,sizeof(currentResource))||currentResource!=out.modelResource
        ||!read(out.model+0x120,&currentResourceCode,sizeof(currentResourceCode))||currentResourceCode!=out.modelResourceCode)return std::nullopt;
    if(out.parentMatrixRead){
        std::array<float,16> currentParent{};
        out.parentMatrixRead=read(out.parent,currentParent.data(),sizeof(currentParent))&&currentParent==out.parentMatrix;
    }
    return out;
}

struct NativeIdroidConeDraw {uintptr_t record{};std::array<float,16> world{};};
// The observed player attachment follows this complete camera-bone frame,
// including rotation, across independent held poses. This tests numerical
// equality with that published frame; a close position alone never owns it.
inline bool matchesNativeIdroidConeEmitter(const NativeIdroidPrimitiveCandidate& c,Pose emitter) noexcept {
    if(c.modelResourceCode!=nativeIdroidConeResourceCode||c.vertices!=32||c.indices!=96||c.triangles!=32
        ||c.bones!=1||!c.boneNamesRead||c.boneNames[0]!=0x78d68a95u||!c.parentMatrixRead
        ||!(c.flags&1u)||!valid(emitter)||!c.parent||c.parent>UINTPTR_MAX-0x50u
        ||c.interpreterArgument3!=c.parent+0x50u)return false;
    const auto x=rotate(emitter.orientation,{1,0,0}),y=rotate(emitter.orientation,{0,1,0}),z=rotate(emitter.orientation,{0,0,1});
    const std::array<float,16> expected{x.x,x.y,x.z,0,y.x,y.y,y.z,0,z.x,z.y,z.z,0,
        emitter.position.x,emitter.position.y,emitter.position.z,1};
    for(size_t i=0;i<expected.size();++i){
        // Two float ULPs at the observed ACC altitude require 0.25 mm;
        // basis equality remains substantially tighter than any hand angle.
        const float tolerance=i>=12&&i<=14?.00025f:.000005f;
        if(!std::isfinite(c.parentMatrix[i])||std::abs(c.parentMatrix[i]-expected[i])>tolerance)return false;
    }
    return true;
}
struct NativeIdroidConeDraws {
    uintptr_t pool{},models{};uint32_t count{},capacity{},first{},last{};
    std::array<NativeIdroidConeDraw,8> active{};uint32_t activeCount{};
};
template<class Reader> std::optional<NativeIdroidConeDraws>
readNativeIdroidConeDraws(uintptr_t imageBase,uintptr_t callbackPool,
    const NativeIdroidPrimitiveCandidate& c,Reader&& read) {
    const auto bounded=[](uintptr_t p,size_t bytes){
        return p&&bytes&&p<=std::numeric_limits<uintptr_t>::max()-(bytes-1);
    };
    const auto field=[]<class T,size_t N>(const std::array<std::byte,N>& bytes,size_t offset){
        T out{};if(offset<=N&&sizeof(T)<=N-offset)std::memcpy(&out,bytes.data()+offset,sizeof(T));return out;
    };
    if(!imageBase||imageBase>std::numeric_limits<uintptr_t>::max()-0x20f4d90u
        ||!callbackPool||callbackPool!=c.particlePool||c.modelResourceCode!=nativeIdroidConeResourceCode
        ||!c.modelResource||c.vertices!=32||c.indices!=96||c.triangles!=32
        ||c.first>=c.last||c.last-c.first!=c.batchCount||c.batchCount>8)return std::nullopt;
    std::array<std::byte,nativeIdroidPrimitivePropertySize> properties{};
    std::array<std::byte,0x128> sourceModel{};
    if(!bounded(c.properties,properties.size())||!read(c.properties,properties.data(),properties.size())||properties!=c.values
        ||field.template operator()<uintptr_t>(properties,0x40)!=c.model
        ||!bounded(c.model,sourceModel.size())||!read(c.model,sourceModel.data(),sourceModel.size())
        ||field.template operator()<uintptr_t>(sourceModel,0)!=imageBase+0x20f4d90u
        ||field.template operator()<uintptr_t>(sourceModel,0x118)!=c.modelResource
        ||field.template operator()<uint64_t>(sourceModel,0x120)!=nativeIdroidConeResourceCode)return std::nullopt;
    const auto sourceBones=field.template operator()<uint16_t>(sourceModel,0xf8);
    const auto sourceNames=field.template operator()<uintptr_t>(sourceModel,0xe8);
    std::array<uint32_t,16> names{};
    if(!c.boneNamesRead||sourceBones!=c.bones||!sourceBones||sourceBones>names.size()
        ||!bounded(sourceNames,sourceBones*sizeof(uint32_t))
        ||!read(sourceNames,names.data(),sourceBones*sizeof(uint32_t))||names!=c.boneNames)return std::nullopt;
    NativeIdroidConeDraws out;out.pool=callbackPool;out.first=c.first;out.last=c.last;
    std::array<std::byte,16> pool{},poolAfter{};
    if(!bounded(callbackPool,pool.size())||!read(callbackPool,pool.data(),pool.size()))return std::nullopt;
    out.count=field.template operator()<uint32_t>(pool,0);
    out.capacity=field.template operator()<uint32_t>(pool,4);
    out.models=field.template operator()<uintptr_t>(pool,8);
    if(!out.count||out.count>out.capacity||out.capacity>1024||out.last>out.count
        ||!bounded(out.models,static_cast<size_t>(out.count)*sizeof(uintptr_t)))return std::nullopt;
    // Entries are plain rendering-particle records, not GrModel objects. Their
    // +120 is a sorting FLOAT, not a resource code. Their layout is owned by
    // this exact ModelPrimitiveShape callback and its header+38 pool; 1B6C94F
    // dereferences its entries and writes +40..70, and 1B6D6C0 reads the same
    // entries. Resource identity belongs to the typed source model above.
    for(uint32_t i=out.first;i<out.last;++i){
        auto& clone=out.active[out.activeCount];
        if(!read(out.models+i*sizeof(uintptr_t),&clone.record,sizeof(clone.record))
            ||clone.record==c.model||(clone.record&15u)||!bounded(clone.record,0x128))return std::nullopt;
        for(uint32_t prior=0;prior<out.activeCount;++prior)if(out.active[prior].record==clone.record)return std::nullopt;
        if(!read(clone.record+0x40,clone.world.data(),sizeof(clone.world)))return std::nullopt;
        for(const auto value:clone.world)if(!std::isfinite(value))return std::nullopt;
        if(std::abs(clone.world[3])>.00001f||std::abs(clone.world[7])>.00001f
            ||std::abs(clone.world[11])>.00001f||std::abs(clone.world[15]-1)>.00001f)return std::nullopt;
        std::array<float,16> currentWorld{};
        if(!read(clone.record+0x40,currentWorld.data(),sizeof(currentWorld))||currentWorld!=clone.world)return std::nullopt;
        ++out.activeCount;
    }
    uintptr_t currentPool{},currentProperties{},currentParent{};uint32_t currentCapacity{},currentFirst{},currentLast{};
    if(!read(callbackPool,poolAfter.data(),poolAfter.size())||poolAfter!=pool
        ||!read(c.instance+0x38,&currentPool,sizeof(currentPool))||currentPool!=callbackPool
        ||!read(c.instance+0x30,&currentProperties,sizeof(currentProperties))||currentProperties!=c.properties
        ||!read(c.instance,&currentParent,sizeof(currentParent))||currentParent!=c.parent
        ||!read(c.instance+0x60,&currentCapacity,sizeof(currentCapacity))||currentCapacity!=c.batchCount
        ||!read(c.instance+0x64,&currentFirst,sizeof(currentFirst))||currentFirst!=c.first
        ||!read(c.instance+0x68,&currentLast,sizeof(currentLast))||currentLast!=c.last)return std::nullopt;
    for(uint32_t i=0;i<out.activeCount;++i){
        uintptr_t current{};
        if(!read(out.models+(out.first+i)*sizeof(uintptr_t),&current,sizeof(current))||current!=out.active[i].record)return std::nullopt;
    }
    std::array<float,16> parentAfter{};
    if(!c.parentMatrixRead||!read(c.parent,parentAfter.data(),sizeof(parentAfter))||parentAfter!=c.parentMatrix)return std::nullopt;
    uintptr_t currentModelType{},currentResource{};uint64_t currentResourceCode{};
    std::array<std::byte,nativeIdroidPrimitivePropertySize> propertiesAfter{};
    if(!read(c.properties,propertiesAfter.data(),propertiesAfter.size())||propertiesAfter!=c.values
        ||!read(c.model,&currentModelType,sizeof(currentModelType))||currentModelType!=imageBase+0x20f4d90u
        ||!read(c.model+0x118,&currentResource,sizeof(currentResource))||currentResource!=c.modelResource
        ||!read(c.model+0x120,&currentResourceCode,sizeof(currentResourceCode))||currentResourceCode!=c.modelResourceCode)return std::nullopt;
    return out;
}

// An affine fit retains the authored cone topology. Its near-ring centre is
// the emitter; every far-ring vertex lies in the configured display plane.
// This is a configured endpoint fit, not a measured cutscene screen distance.
inline std::optional<std::array<float,16>> nativeIdroidConeScreenFit(Vec3 emitter,
    Pose screen,float width) noexcept {
    if(!valid(Pose{{},emitter})||!valid(screen)||!std::isfinite(width)||width<=0||width>3)return std::nullopt;
    const auto centre=screen.position-emitter;
    const auto normal=rotate(screen.orientation,{0,0,1});
    const auto distance2=dot(centre,centre);
    if(distance2<.000025f||distance2>4.f||std::abs(dot(centre,normal))<.005f)return std::nullopt;
    const auto x=rotate(screen.orientation,{1,0,0})*(width/(2*.8f));
    const auto y=rotate(screen.orientation,{0,1,0})*(width*9.f/16.f/(2*.8f));
    const auto z=centre*(1.f/(2.f-.01f));
    const auto origin=emitter-z*.01f;
    return std::array<float,16>{x.x,x.y,x.z,0,y.x,y.y,y.z,0,z.x,z.y,z.z,0,origin.x,origin.y,origin.z,1};
}
inline std::optional<std::array<float,8>> nativeIdroidConeBounds(const std::array<float,16>& world) noexcept {
    for(const auto value:world)if(!std::isfinite(value))return {};
    std::array<float,8> bounds{};bounds[3]=bounds[7]=1;
    for(size_t axis=0;axis<3;++axis){
        const float coneNear=world[12+axis]+world[8+axis]*.01f;
        const float coneFar=world[12+axis]+world[8+axis]*2.f;
        const float radial=std::sqrt(world[axis]*world[axis]+world[4+axis]*world[4+axis]);
        bounds[axis]=std::min(coneNear-.004f*radial,coneFar-.8f*radial)-.00025f;
        bounds[4+axis]=std::max(coneNear+.004f*radial,coneFar+.8f*radial)+.00025f;
        if(!std::isfinite(bounds[axis])||!std::isfinite(bounds[4+axis]))return {};
    }
    return bounds;
}
// Retained field run 20261002T140550163961Z publishes three coincident Light
// particles, not one. Admit only a complete 1..3-particle range from that exact
// canonical source after its native render callback has updated metadata.
// Preflight and snapshot every transform/bounds pair before the first write.
template<class Reader,class Writable,class Writer> bool retargetNativeIdroidConeDraw(
    uintptr_t imageBase,uintptr_t callbackPool,const NativeIdroidPrimitiveCandidate& c,
    Pose emitter,Pose screen,float width,Reader&& read,Writable&& writable,Writer&& write) {
    constexpr uint32_t maximumDraws=3;
    if(!matchesNativeIdroidConeEmitter(c,emitter)||c.first!=0||!c.batchCount
        ||c.batchCount>maximumDraws||c.last!=c.batchCount)return false;
    const auto world=nativeIdroidConeScreenFit(emitter.position,screen,width);
    const auto bounds=world?nativeIdroidConeBounds(*world):std::nullopt;
    const auto draws=world&&bounds?readNativeIdroidConeDraws(imageBase,callbackPool,c,read):std::nullopt;
    if(!draws||draws->count!=c.batchCount||draws->activeCount!=c.batchCount)return false;
    std::array<std::array<float,8>,maximumDraws> oldBounds{};
    for(uint32_t i=0;i<draws->activeCount;++i){
        const auto& draw=draws->active[i];
        // The observed canonical Light repeats one authored shape. A batch
        // with differing transforms is outside that evidence and stays native.
        if(draw.world!=draws->active[0].world
            ||!writable(draw.record+0x40,sizeof(*world))
            ||!writable(draw.record+0xa0,sizeof(*bounds)))return false;
        std::array<float,16> currentWorld{};
        if(!read(draw.record+0xa0,oldBounds[i].data(),sizeof(oldBounds[i]))
            ||!read(draw.record+0x40,currentWorld.data(),sizeof(currentWorld))
            ||currentWorld!=draw.world)return false;
    }
    // Bounds acquisition must not allow a replaced property/source generation,
    // pool entry or sibling transform to pass an earlier snapshot.
    const auto current=readNativeIdroidConeDraws(imageBase,callbackPool,c,read);
    if(!current||current->models!=draws->models||current->count!=draws->count
        ||current->capacity!=draws->capacity||current->activeCount!=draws->activeCount)return false;
    for(uint32_t i=0;i<draws->activeCount;++i)
        if(current->active[i].record!=draws->active[i].record
            ||current->active[i].world!=draws->active[i].world)return false;
    // A writer reporting failure may already have copied part of its span.
    // Preserve both spans before mutation and restore both independently: a
    // failed transform restore must not short-circuit the bounds restore.
    const auto restore=[&]{
        bool restored=true;
        for(uint32_t i=0;i<draws->activeCount;++i){
            const auto& draw=draws->active[i];
            const bool worldRestored=write(draw.record+0x40,draw.world.data(),sizeof(draw.world));
            const bool boundsRestored=write(draw.record+0xa0,oldBounds[i].data(),sizeof(oldBounds[i]));
            restored=worldRestored&&boundsRestored&&restored;
        }
        return restored;
    };
    for(uint32_t i=0;i<draws->activeCount;++i){
        const auto record=draws->active[i].record;
        if(!write(record+0x40,world->data(),sizeof(*world))){restore();return false;}
        if(!write(record+0xa0,bounds->data(),sizeof(*bounds))){restore();return false;}
    }
    return true;
}
}
