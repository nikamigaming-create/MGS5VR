#include "mgs5vr/motion_melee.hpp"
#include "mgs5vr/head_camera.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/log.hpp"
#include <windows.h>
#include <MinHook.h>
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>
#include <sstream>
#include <stdexcept>

namespace {
using namespace mgs5vr;
uintptr_t base{};
std::atomic_bool enabled{};
std::mutex mutex;
std::mutex consumeMutex;
MeleeSweep pending{};
uint64_t consumed{},stroke{},strokeAt{},submittedStroke{},submittedAt{};
uintptr_t attackShape{};
unsigned strikingHand{};
bool hit{};
using Filter=uint32_t(*)(void*,void*,void*,void*,void*);
Filter originalFilter{};
template<class T> T read(uintptr_t address){
    T value{};SIZE_T count{};
    if(address)ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),&value,sizeof(value),&count);
    return count==sizeof(value)?value:T{};
}
template<class T> void write(uintptr_t address,const T& value){std::memcpy(reinterpret_cast<void*>(address),&value,sizeof(value));}
bool function(uintptr_t address){return address>=base+0x1000&&address<base+0x2090000;}
uintptr_t method(uintptr_t object,size_t offset){return read<uintptr_t>(read<uintptr_t>(object)+offset);}
uint32_t filter(void* context,void* shape,void* target,void* contact,void* cqc){
    const auto shapeAddress=reinterpret_cast<uintptr_t>(shape);
    if(enabled.load()){
        std::lock_guard lock(mutex);
        if(shapeAddress==attackShape&&hit&&steadyMilliseconds()-submittedAt<=150
            &&read<uint16_t>(read<uintptr_t>(shapeAddress+0x28)+8)==7)return 2;
    }
    const auto result=originalFilter(context,shape,target,contact,cqc);
    if(!enabled.load())return result;
    const auto targetAddress=reinterpret_cast<uintptr_t>(target);
    const auto tag=read<uint64_t>(targetAddress+0x50);
    std::lock_guard lock(mutex);
    if(shapeAddress!=attackShape||steadyMilliseconds()-submittedAt>150
        ||read<uint16_t>(read<uintptr_t>(shapeAddress+0x28)+8)!=7
        ||result!=1||!(tag&(1ull<<63))||!read<uintptr_t>(targetAddress+0x28))return result;
    if(!hit){
        hit=true;
        std::ostringstream s;s<<"Motion melee native contact stroke="<<stroke<<" hand="<<strikingHand
            <<" target="<<std::hex<<(tag&0xffff)<<std::dec<<" attack=7";log(s.str());
    }
    return result;
}
}
namespace mgs5vr {
void publishMeleeSweep(const MeleeSweep& sweep){
    if(!enabled.load()||!sweep.owner||!sweep.character||sweep.hand>2
        ||!valid(Pose{{},sweep.start})||!valid(Pose{{},sweep.end}))return;
    const auto delta=sweep.end-sweep.start;
    if(dot(delta,delta)>.09f)return;
    std::lock_guard lock(mutex);
    // Keep one physical stroke active through its contact window. A support
    // hand or a second point on the same weapon cannot duplicate that strike.
    if(sweep.started&&(!stroke||sweep.time-strokeAt>120||strikingHand==sweep.hand)){
        ++stroke;strokeAt=sweep.time;hit=false;strikingHand=sweep.hand;
        pending=sweep;
    }else if(!hit&&strikingHand==sweep.hand&&sweep.time>=pending.time)pending=sweep;
}
void consumeMeleeSweep(){
    if(!enabled.load())return;
    std::lock_guard consumeLock(consumeMutex);
    MeleeSweep request;uint64_t serial{};
    {
        std::lock_guard lock(mutex);
        if(hit||!pending.time||pending.time<=consumed)return;
        request=pending;serial=stroke;consumed=request.time;
    }
    const auto now=steadyMilliseconds();
    const auto status=headCamera().status();
    if(now<request.time||now-request.time>100||!status.active||status.activation!=request.activation)return;
    // The character exposes the CQC interface at +0xd8, which is +0x38 into
    // the component. Initialization can precede the derived vtable assignment.
    const auto interface=read<uintptr_t>(request.character+0xd8);
    if(interface<0x38||read<uintptr_t>(interface)!=base+0x23bd370)return;
    const auto c=interface-0x38;
    if(read<uintptr_t>(c)!=base+0x23bd2f8||read<uintptr_t>(c+0x40)!=request.character
        ||read<uintptr_t>(request.owner)!=base+0x23b8218||read<uintptr_t>(request.owner+0x370)!=request.character)return;
    const auto index=read<uint32_t>(request.owner+0x3a0),first=read<uint32_t>(read<uintptr_t>(c+0x28)+0x24);
    if(index<first||index-first>15)return;
    const auto state=read<uintptr_t>(c+0x58)+(index-first)*0x130ull;
    // Native CQC keeps its action callback at +0x10. An active grapple/throw
    // retains the collision object; a physical swing cannot steal it.
    if(!state||read<uintptr_t>(state+0x10))return;
    const auto shape=read<uintptr_t>(state+0xe0),query=read<uintptr_t>(state+0xe8);
    if(!shape||!query||read<uintptr_t>(shape+0x28)!=state+0x110||read<uintptr_t>(shape+0x20)!=c)return;
    const auto registry=read<uintptr_t>(base+0x2c3a8a0),services=read<uintptr_t>(registry+8);
    const auto manager=read<uintptr_t>(read<uintptr_t>(services+0x80)+0x10);
    const auto counter=read<uintptr_t>(read<uintptr_t>(services+0x98)+0x98);
    const auto geometryFn=method(shape,0x58),submitFn=method(manager,0x40),counterFn=method(counter,0);
    if(!function(geometryFn)||!function(submitFn)||!function(counterFn))return;
    using Geometry=void*(*)(void*);using Submit=void(*)(void*,void*);using Counter=uint16_t(*)(void*);
    const auto geometry=reinterpret_cast<uintptr_t>(reinterpret_cast<Geometry>(geometryFn)(reinterpret_cast<void*>(shape)));
    if(!geometry)return;
    const auto delta=request.end-request.start;
    const float radius=.085f+std::sqrt(dot(delta,delta))*.5f;
    const Vec3 center=(request.start+request.end)*.5f;
    write(geometry+0x10,center);write(geometry+0x20,radius);
    const bool freshStroke=serial!=submittedStroke;
    if(freshStroke){
        // Fresh native attack serial, same ATK_Kick parameter entry for fists
        // and held weapons. No input pulse, ammunition use or action animation.
        write(state+0x114,reinterpret_cast<Counter>(counterFn)(reinterpret_cast<void*>(counter)));
        write(state+0x116,static_cast<uint16_t>(index&0x1ff));
        write(state+0x118,uint16_t{7});
        write(state+0x11e,uint8_t{0});
        write(state+0xd1,static_cast<uint8_t>(read<uint8_t>(state+0xd1)&0x7f));
    }
    {
        std::lock_guard lock(mutex);attackShape=shape;submittedAt=now;submittedStroke=serial;
    }
    reinterpret_cast<Submit>(submitFn)(reinterpret_cast<void*>(manager),reinterpret_cast<void*>(query));
    if(freshStroke){
        std::ostringstream s;s<<"Motion melee submitted stroke="<<serial<<" hand="<<request.hand
            <<" center="<<center.x<<','<<center.y<<','<<center.z<<" radius="<<radius<<" attack=7";log(s.str());
    }
}
void installMotionMelee(uintptr_t imageBase){
    base=imageBase;
    if(read<std::array<unsigned char,10>>(base+0x1176560)!=std::array<unsigned char,10>{0x40,0x55,0x41,0x54,0x41,0x56,0x48,0x83,0xec,0x60})
        throw std::runtime_error("Motion melee native collision signatures differ");
    const auto filterAddress=reinterpret_cast<void*>(base+0x1176560);
    if(MH_CreateHook(filterAddress,reinterpret_cast<void*>(&filter),reinterpret_cast<void**>(&originalFilter))!=MH_OK)
        throw std::runtime_error("Cannot create native motion-melee adapters");
    MH_QueueEnableHook(filterAddress);
    if(MH_ApplyQueued()!=MH_OK)throw std::runtime_error("Cannot enable native motion-melee adapters");
    enabled.store(true);log("Motion melee collision adapters installed; native ATK_Kick damage");
}
void stopMotionMelee() noexcept{enabled.store(false);}
}
