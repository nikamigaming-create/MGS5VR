#include "mgs5vr/native_cover.hpp"
#include "mgs5vr/head_camera.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/log.hpp"
#include <windows.h>
#include <intrin.h>
#include <MinHook.h>
#include <array>
#include <atomic>

namespace {
uintptr_t base{};
std::atomic_bool enabled{};
using WeaponReady=bool(*)(void*,uint32_t);
WeaponReady originalReady{};

template<class T> T read(uintptr_t address){
    T value{};
    SIZE_T count{};
    if(!address||!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),
        &value,sizeof(value),&count)||count!=sizeof(value))return {};
    return value;
}

bool localImmersiveCover(uintptr_t weapon,uint32_t index){
    using namespace mgs5vr;
    const auto status=headCamera().status();
    if(!status.active||status.suspended||status.nativeMenuOpen||status.nativeIdroidOpen)return false;
    const auto frame=headCamera().publishedView();
    const auto now=steadyMilliseconds();
    if(!frame||!frame->applied||frame->activation!=status.activation||!frame->playerOwner
       ||!frame->sampleTime||now<frame->sampleTime||now-frame->sampleTime>150||frame->menuOpen)return false;
    const auto& c=frame->controllers;
    if(!c.presentationFocused||c.nativeGamepad||c.vehicleMounted||c.vehicleControls
       ||c.equipmentOpen||c.commandControls||c.frontEnd||c.loading||c.scriptedDemo||c.authoredCamera)return false;
    const auto player=frame->playerOwner;
    if(read<uintptr_t>(player)!=base+0x23b8218||read<uint32_t>(player+0x3a0)!=index)return false;
    const auto character=read<uintptr_t>(player+0x370);
    if(!character||read<uintptr_t>(character+0x80)!=weapon
       ||read<uintptr_t>(weapon)!=base+0x23b3e80||read<uintptr_t>(weapon+8)!=character)return false;
    const auto cover=read<uintptr_t>(character+0xa0);
    if(read<uintptr_t>(cover)!=base+0x23b59d0||read<uintptr_t>(cover+8)!=character)return false;
    const auto instances=read<uintptr_t>(cover+0x38);
    const auto records=read<uintptr_t>(cover+0x68);
    if(!instances||!records)return false;
    const auto first=read<uint32_t>(instances+0x24);
    if(index<first||index-first>15)return false;
    const auto record=records+static_cast<uintptr_t>(index-first)*0x3b0;
    // Mode 2 owns ordinary wall cover. Its ready-weapon branch requests
    // modes 3/4, whose authored peek moves the actor out of cover.
    return read<uint8_t>(record+0x10)==2;
}

bool ready(void* weapon,uint32_t index){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const bool result=originalReady(weapon,index);
    // This is ONLY the cover movement consumer. Weapon, firing, animation,
    // AI and every other readiness consumer receive their native result.
    // Physical head/hand motion stays with the existing VR rig; no root or
    // camera position is rewritten and native wall/posture ownership remains.
    if(result&&enabled.load(std::memory_order_relaxed)&&caller==base+0x10c84b1
       &&localImmersiveCover(reinterpret_cast<uintptr_t>(weapon),index)){
        static std::atomic_bool reported{};
        if(!reported.exchange(true))mgs5vr::log("Native cover suppressed local automatic ready-to-peek request");
        return false;
    }
    return result;
}

bool verified(){
    return read<uintptr_t>(base+0x23b3e80+0x28)==base+0x1051ac0
        &&read<std::array<unsigned char,22>>(base+0x1051ac0)==std::array<unsigned char,22>{
            0x48,0x8b,0x41,0x38,0x44,0x8b,0xca,0x44,0x2b,0x48,0x24,0x4d,0x69,0xc9,0x10,0x06,0,0,0x4c,0x03,0x49,0x58}
        &&read<std::array<unsigned char,20>>(base+0x10c84a1)==std::array<unsigned char,20>{
            0x48,0x8b,0x8a,0x80,0,0,0,0x8b,0xd7,0x4c,0x8b,0x01,0x41,0xff,0x50,0x28,0x84,0xc0,0x74,0x4e}
        &&read<std::array<unsigned char,21>>(base+0x10c84e9)==std::array<unsigned char,21>{
            0xc6,0x46,0x11,0x04,0xc6,0x46,0x16,0,0xe9,0xd7,0x34,0,0,0xc6,0x46,0x11,0x03,0xc6,0x46,0x16,0};
}
}

namespace mgs5vr {
bool nativeWallCover(uintptr_t player) noexcept{
    if(!enabled.load(std::memory_order_relaxed)||read<uintptr_t>(player)!=base+0x23b8218)return false;
    const auto index=read<uint32_t>(player+0x3a0);
    const auto character=read<uintptr_t>(player+0x370);
    const auto cover=read<uintptr_t>(character+0xa0);
    if(!character||read<uintptr_t>(cover)!=base+0x23b59d0||read<uintptr_t>(cover+8)!=character)return false;
    const auto instances=read<uintptr_t>(cover+0x38);
    const auto records=read<uintptr_t>(cover+0x68);
    if(!instances||!records)return false;
    const auto first=read<uint32_t>(instances+0x24);
    if(index<first||index-first>15)return false;
    return read<uint8_t>(records+static_cast<uintptr_t>(index-first)*0x3b0+0x10)==2;
}
void installNativeCover(uintptr_t imageBase){
    if(enabled.load())return;
    base=imageBase;
    if(!verified()){log("Native cover adapter unavailable: ready-to-peek signatures differ");return;}
    auto* target=reinterpret_cast<void*>(base+0x1051ac0);
    auto result=MH_CreateHook(target,reinterpret_cast<void*>(&ready),reinterpret_cast<void**>(&originalReady));
    if(result!=MH_OK){log("Native cover adapter unavailable: readiness hook creation failed");return;}
    enabled.store(true);
    result=MH_EnableHook(target);
    if(result!=MH_OK){
        enabled.store(false);MH_RemoveHook(target);
        log("Native cover adapter unavailable: readiness hook enable failed");return;
    }
    log("Native cover ready-to-peek adapter installed for local immersive wall cover");
}
void stopNativeCover() noexcept{enabled.store(false);}
}
