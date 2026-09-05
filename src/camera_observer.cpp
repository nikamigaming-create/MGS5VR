#include "mgs5vr/camera_observer.hpp"
#include "mgs5vr/camera_consumer.hpp"
#include "mgs5vr/render_camera.hpp"
#include "mgs5vr/head_camera.hpp"
#include "mgs5vr/player_visibility.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/log.hpp"
#include <windows.h>
#include <MinHook.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <thread>

extern "C" {
void* MgsCameraTrampoline{};
void MgsCameraIntercept();
}
namespace {
struct Observation {
    uintptr_t object{},caller{},owner{},source{},context{};
    // Raw retail values: coordinate basis and distance units are not yet proven.
    std::array<float,8> values{};
    uint64_t calls{};
    DWORD thread{};
};
std::mutex observationMutex;
std::array<Observation,16> observations{};
std::atomic_uint64_t totalCalls{},unrecordedCalls{};
uintptr_t imageBase{};
bool ownerContextVerified{};
HANDLE reportStop{},reportThread{};
std::ofstream evidence;
unsigned evidenceSamples{};

template<size_t N> bool readMemory(uintptr_t address,std::array<unsigned char,N>& bytes){
    SIZE_T copied{};
    return address&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),bytes.data(),N,&copied)&&copied==N;
}
template<size_t N> std::string hexBytes(const std::array<unsigned char,N>& bytes){
    std::ostringstream s;s<<std::hex<<std::setfill('0');
    for(const auto b:bytes)s<<std::setw(2)<<static_cast<unsigned>(b);
    return s.str();
}
void recordOwner(const Observation& o){
    if(!evidence||!o.owner||evidenceSamples>=512)return;
    std::array<unsigned char,0x500> owner{};
    std::array<unsigned char,0x200> camera{};
    std::array<unsigned char,0x80> table{};
    if(!readMemory(o.owner,owner)||!readMemory(o.object,camera))return;
    uintptr_t linkedCamera{},vtable{};
    std::memcpy(&linkedCamera,owner.data()+0x380,sizeof(linkedCamera));
    if(linkedCamera!=o.object)return;
    std::memcpy(&vtable,camera.data(),sizeof(vtable));
    const bool tableValid=readMemory(vtable,table);
    evidence<<"{\"tick_ms\":"<<GetTickCount64()<<",\"calls\":"<<o.calls
        <<",\"owner\":\"0x"<<std::hex<<o.owner<<"\",\"camera\":\"0x"<<o.object
        <<"\",\"context\":\"0x"<<o.context<<"\",\"source\":\"0x"<<o.source
        <<"\",\"vtable\":\"0x"<<vtable<<std::dec<<"\",\"owner_bytes\":\""<<hexBytes(owner)
        <<"\",\"camera_bytes\":\""<<hexBytes(camera)<<"\",\"vtable_bytes\":\""
        <<(tableValid?hexBytes(table):std::string{})<<"\"}\n";
    evidence.flush();++evidenceSamples;
}

DWORD WINAPI report(void*){
    uint64_t lastTotal=~uint64_t{};
    while(WaitForSingleObject(reportStop,2000)==WAIT_TIMEOUT){
      try {
        mgs5vr::reportCameraConsumers();
        mgs5vr::reportRenderCamera();
        const auto total=totalCalls.load();if(total==lastTotal)continue;lastTotal=total;
        std::array<Observation,16> snapshot;
        {std::lock_guard guard(observationMutex);snapshot=observations;}
        mgs5vr::log("Camera observer calls="+std::to_string(total)+" overflow="+std::to_string(unrecordedCalls.load()));
        for(const auto& o:snapshot)if(o.object){
            const auto& v=o.values;
            const float norm=v[0]*v[0]+v[1]*v[1]+v[2]*v[2]+v[3]*v[3];
            bool finite=true;for(const auto value:v)finite=finite&&std::isfinite(value);
            std::ostringstream s;
            s<<"Camera candidate object=0x"<<std::hex<<o.object<<" caller_rva=0x"<<(o.caller-imageBase)
             <<std::dec<<" calls="<<o.calls<<" thread="<<o.thread<<" q="
             <<v[0]<<','<<v[1]<<','<<v[2]<<','<<v[3]
             <<" p="<<v[4]<<','<<v[5]<<','<<v[6]
             <<" p4="<<v[7]<<" finite_normalized="<<(finite&&std::abs(norm-1.0f)<0.01f);
            if(o.owner)s<<" owner=0x"<<std::hex<<o.owner<<" context=0x"<<o.context;
            mgs5vr::log(s.str());
            recordOwner(o);
        }
      }catch(...){}
    }
    return 0;
}
}
extern "C" void MgsCameraObserved(void* object,const float* source,uintptr_t caller,const uintptr_t* context) noexcept {
    ++totalCalls;
    try {
        std::array<float,8> v;std::memcpy(v.data(),source,sizeof(v));
        if(ownerContextVerified&&caller==imageBase+0x1118b1a&&context&&mgs5vr::headCamera().available()){
            // The native camera owner publishes the player root at +0x30 and
            // bone 4's affine local transform at +0x70. The latter is populated
            // by the native animation reader, independently of ADS/boom state.
            std::array<unsigned char,0x388> ownerBytes{};
            if(readMemory(context[0],ownerBytes)){
                uintptr_t type{},linked{};std::memcpy(&type,ownerBytes.data(),8);std::memcpy(&linked,ownerBytes.data()+0x380,8);
                if(type==imageBase+0x23b8218&&linked==reinterpret_cast<uintptr_t>(object)){
                    std::array<float,16> root{},head{};
                    std::memcpy(root.data(),ownerBytes.data()+0x30,sizeof(root));
                    std::memcpy(head.data(),ownerBytes.data()+0x70,sizeof(head));
                    mgs5vr::headCamera().publishPlayerHead(linked,context[0],{{v[0],v[1],v[2],v[3]},{v[4],v[5],v[6]}},root,head,mgs5vr::steadyMilliseconds());
                    const auto status=mgs5vr::headCamera().status();
                    mgs5vr::updatePlayerVisibility(context[0],status.active||status.pending);
                }
            }
        }
        std::lock_guard guard(observationMutex);
        Observation* slot=nullptr;
        for(auto& o:observations)if(o.object==reinterpret_cast<uintptr_t>(object)){slot=&o;break;}
        if(!slot)for(auto& o:observations)if(!o.object){slot=&o;break;}
        if(!slot){++unrecordedCalls;return;}
        slot->object=reinterpret_cast<uintptr_t>(object);slot->caller=caller;
        slot->source=reinterpret_cast<uintptr_t>(source);
        // Only this verified callsite gives RSI/RBP the observed owner/context meaning.
        if(ownerContextVerified&&caller==imageBase+0x1118b1a&&context){slot->owner=context[0];slot->context=context[1];}
        slot->values=v;++slot->calls;slot->thread=GetCurrentThreadId();
    }catch(...){} // Diagnostics must not alter control flow of the retail setter.
}
namespace mgs5vr {
void installCameraObserver(const std::filesystem::path& evidenceDirectory){
    // Candidate identified from the BSD-2-Clause IGCS MGS5 camera signature;
    // independently matched in 1.0.15.4 at RVA 0x44e960. See third-party notice.
    constexpr std::array<unsigned char,22> expected{
        0x0f,0x28,0x02,0x0f,0x29,0x81,0xf0,0,0,0,0x0f,0x28,0x4a,0x10,
        0x0f,0x29,0x89,0,1,0,0,0xc3};
    imageBase=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    initializePlayerVisibility(imageBase);
    constexpr std::array<unsigned char,20> callsite{
        0x48,0x8b,0x8e,0x80,0x03,0,0,0x48,0x8d,0x95,0x10,0x03,0,0,
        0x48,0x8b,0x01,0xff,0x50,0x08};
    std::array<unsigned char,callsite.size()> liveCallsite{};
    ownerContextVerified=readMemory(imageBase+0x1118b06,liveCallsite)&&liveCallsite==callsite;
    if(!ownerContextVerified)log("Camera owner callsite differs; owner observations disabled");
    if(!evidenceDirectory.empty()){
        if(!evidenceDirectory.is_absolute())throw std::runtime_error("Camera evidence directory must be absolute");
        std::filesystem::create_directories(evidenceDirectory);
        evidence.open(evidenceDirectory/("owner-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64())+".jsonl"));
        if(!evidence)throw std::runtime_error("Cannot open camera evidence file");
        evidence<<"{\"schema\":1,\"version\":\"1.0.15.4\",\"sha256\":\"085c2f82d1c963c40b3d2d55786661dfee2b18cbbf388a710c00fa76c5e9bb45\",\"image_base\":"<<imageBase
            <<",\"clock\":\"GetTickCount64 milliseconds\",\"coherent_frame_snapshot\":false}\n";
    }
    auto* address=reinterpret_cast<void*>(imageBase+0x44e960);
    MEMORY_BASIC_INFORMATION region{};
    if(!VirtualQuery(address,&region,sizeof(region))||region.State!=MEM_COMMIT
        ||(region.Protect&PAGE_GUARD)||!(region.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))
        ||reinterpret_cast<uintptr_t>(region.BaseAddress)+region.RegionSize<reinterpret_cast<uintptr_t>(address)+expected.size()
        ||std::memcmp(address,expected.data(),expected.size())!=0)
        throw std::runtime_error("Camera observer signature differs in live game; hook refused");
    const auto created=MH_CreateHook(address,reinterpret_cast<void*>(&MgsCameraIntercept),&MgsCameraTrampoline);
    if(created!=MH_OK)throw std::runtime_error(std::string("Camera observer create: ")+MH_StatusToString(created));
    const auto enabled=MH_EnableHook(address);
    if(enabled!=MH_OK){MH_RemoveHook(address);throw std::runtime_error(std::string("Camera observer enable: ")+MH_StatusToString(enabled));}
    log("Camera observer enabled at verified RVA 0x44e960. No camera pose overrides.");
    try{installCameraConsumerObserver(imageBase,evidenceDirectory);}
    catch(const std::exception& e){log(std::string("Camera consumer observer unavailable: ")+e.what());}
    reportStop=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    if(reportStop)reportThread=CreateThread(nullptr,0,&report,nullptr,0,nullptr);
    if(!reportThread)log("Camera observer report thread unavailable");
}
void stopCameraObserver() noexcept {
    if(reportStop)SetEvent(reportStop);
    if(reportThread&&WaitForSingleObject(reportThread,2500)==WAIT_OBJECT_0){
        CloseHandle(reportThread);reportThread=nullptr;
        CloseHandle(reportStop);reportStop=nullptr;
        evidence.close();
        stopCameraConsumerObserver();
        stopRenderCamera();
    }
}
}
