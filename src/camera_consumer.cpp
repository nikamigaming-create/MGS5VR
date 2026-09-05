#include "mgs5vr/camera_consumer.hpp"
#include "mgs5vr/log.hpp"
#include <windows.h>
#include <intrin.h>
#include <MinHook.h>
#include <array>
#include <atomic>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <stdexcept>

namespace {
using Getter=const float*(*)(void*);
std::array<Getter,3> originals{};
constexpr std::array<uintptr_t,3> getterRvas{0x44ddd0,0x44dc70,0x44dca0};
struct Read {
    uintptr_t object{},caller{},returned{};
    uint64_t calls{},tick{};
    DWORD thread{};
    unsigned method{};
};
std::array<Read,128> reads{};
std::mutex readsMutex;
std::atomic_bool enabled{};
std::atomic_uint64_t missed{};
uintptr_t base{};
std::ofstream output;
unsigned reportCount{};

void observed(unsigned method,void* object,const float* returned,uintptr_t caller) noexcept {
    if(!enabled.load(std::memory_order_relaxed))return;
    try {
        // Reading a camera must never wait for a diagnostic file write.
        std::unique_lock lock(readsMutex,std::try_to_lock);
        if(!lock.owns_lock()){++missed;return;}
        Read* slot=nullptr;
        for(auto& r:reads)if(r.object==reinterpret_cast<uintptr_t>(object)&&r.caller==caller&&r.method==method){slot=&r;break;}
        if(!slot)for(auto& r:reads)if(!r.object){slot=&r;break;}
        if(!slot){++missed;return;}
        slot->object=reinterpret_cast<uintptr_t>(object);slot->returned=reinterpret_cast<uintptr_t>(returned);
        slot->caller=caller;slot->method=method;slot->thread=GetCurrentThreadId();
        slot->tick=GetTickCount64();++slot->calls;
    }catch(...){}
}
__declspec(noinline) const float* get0(void* object){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const auto result=originals[0](object);observed(0,object,result,caller);return result;
}
__declspec(noinline) const float* get1(void* object){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const auto result=originals[1](object);observed(1,object,result,caller);return result;
}
__declspec(noinline) const float* get2(void* object){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const auto result=originals[2](object);observed(2,object,result,caller);return result;
}
template<size_t N> bool matches(uintptr_t address,const std::array<unsigned char,N>& signature){
    std::array<unsigned char,N> bytes{};SIZE_T copied{};
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),bytes.data(),bytes.size(),&copied)
        &&copied==bytes.size()&&bytes==signature;
}
}
namespace mgs5vr {
void installCameraConsumerObserver(uintptr_t moduleBase,const std::filesystem::path& evidenceDirectory){
    if(evidenceDirectory.empty())return;
    if(!evidenceDirectory.is_absolute())throw std::runtime_error("Camera consumer evidence directory must be absolute");
    // Independently verified leaf getters in the exact 1.0.15.4 binary. The two
    // conditional getters select alternate pose storage; their roles are unknown.
    constexpr std::array<unsigned char,8> raw{0x48,0x8d,0x81,0xf0,0,0,0,0xc3};
    constexpr std::array<unsigned char,25> alternate1{
        0x80,0xb9,0xb1,0,0,0,0,0x48,0x8d,0x81,0x10,0x01,0,0,0x75,0x07,
        0x48,0x8d,0x81,0xf0,0,0,0,0xf3,0xc3};
    constexpr std::array<unsigned char,25> alternate2{
        0x80,0xb9,0xb2,0,0,0,0,0x48,0x8d,0x81,0x30,0x01,0,0,0x75,0x07,
        0x48,0x8d,0x81,0xf0,0,0,0,0xf3,0xc3};
    if(!matches(moduleBase+getterRvas[0],raw)||!matches(moduleBase+getterRvas[1],alternate1)
        ||!matches(moduleBase+getterRvas[2],alternate2))throw std::runtime_error("Camera getter signature mismatch; observer refused");
    base=moduleBase;
    std::filesystem::create_directories(evidenceDirectory);
    output.open(evidenceDirectory/("consumers-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64())+".jsonl"));
    if(!output)throw std::runtime_error("Cannot open camera consumer evidence");
    output<<"{\"schema\":1,\"image_base\":"<<base<<",\"clock\":\"GetTickCount64 milliseconds\","
        "\"snapshot_kind\":\"last observation per object and callsite\",\"coherent_frame_snapshot\":false}\n";
    output.flush();
    constexpr std::array<Getter,3> detours{&get0,&get1,&get2};
    unsigned created=0;
    try {
        for(size_t n=0;n<getterRvas.size();++n){
            auto* address=reinterpret_cast<void*>(base+getterRvas[n]);
            const auto result=MH_CreateHook(address,reinterpret_cast<void*>(detours[n]),reinterpret_cast<void**>(&originals[n]));
            if(result!=MH_OK)throw std::runtime_error(std::string("Camera getter hook: ")+MH_StatusToString(result));
            ++created;
        }
        for(const auto rva:getterRvas){
            const auto result=MH_EnableHook(reinterpret_cast<void*>(base+rva));
            if(result!=MH_OK)throw std::runtime_error(std::string("Camera getter enable: ")+MH_StatusToString(result));
        }
        enabled.store(true);
        log("Read-only camera consumer observers enabled; all native getter results preserved");
    }catch(...){
        enabled.store(false);
        for(unsigned n=0;n<created;++n)MH_DisableHook(reinterpret_cast<void*>(base+getterRvas[n]));
        // A thread may already be inside a successfully enabled wrapper. Keep
        // its original target valid even if enabling a later getter failed.
        output.close();throw;
    }
}
void reportCameraConsumers(){
    if(!output||reportCount>=512)return;
    std::array<Read,128> snapshot;
    {std::lock_guard lock(readsMutex);snapshot=reads;}
    output<<"{\"tick_ms\":"<<GetTickCount64()<<",\"missed\":"<<missed.load()<<",\"reads\":[";
    bool first=true;
    for(const auto& r:snapshot)if(r.object){
        if(!first)output<<',';first=false;
        output<<"{\"object\":\"0x"<<std::hex<<r.object<<"\",\"caller\":\"0x"<<r.caller
            <<"\",\"method_rva\":\"0x"<<getterRvas[r.method]<<"\",\"returned\":\"0x"<<r.returned
            <<std::dec<<"\",\"calls\":"<<r.calls<<",\"thread\":"<<r.thread<<",\"last_tick_ms\":"<<r.tick<<'}';
    }
    output<<"]}\n";output.flush();++reportCount;
}
void stopCameraConsumerObserver() noexcept {
    enabled.store(false);
    try{reportCameraConsumers();output.close();}catch(...){}
    // The containing DLL is pinned. Retain trampolines until process teardown.
}
}
