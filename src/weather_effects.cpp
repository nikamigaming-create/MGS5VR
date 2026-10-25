#include "mgs5vr/weather_effects.hpp"
#include "mgs5vr/head_camera.hpp"
#include "mgs5vr/log.hpp"
#include <windows.h>
#include <MinHook.h>
#include <array>
#include <atomic>

namespace {
uintptr_t base{};
using CreateEffect=void(*)(uintptr_t,uintptr_t,uintptr_t);
CreateEffect originalCreate{};
using UpdateEffect=void(*)(uintptr_t,uintptr_t,float);
UpdateEffect originalUpdate{};
template<class T> T get(uintptr_t address){
    T value{};SIZE_T copied{};
    if(!address||!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),&value,sizeof(value),&copied)
       ||copied!=sizeof(value))return {};
    return value;
}
bool suppressCameraEffect(uintptr_t owner,uintptr_t slot){
    // Owned TPP WeatherSandStorm: data +C0 is vfxFileSandStormCamera;
    // update 704400 binds it to owner +50. +38/+40/+48 are start/far/near.
    // Remove only the monoscopic camera spray. World weather remains native.
    const auto status=mgs5vr::headCamera().status();
    if(slot==owner+0x50&&owner==get<uintptr_t>(base+0x2be54a0)
       &&status.active&&!status.suspended){
        if(const auto effect=get<uintptr_t>(slot)){
            // Same stop-and-release sequence used by native seed reset
            // 705590. Execute here on the owning weather update thread.
            uint32_t result{};
            using Stop=void(*)(uintptr_t,uint32_t*,bool);
            using Release=void(*)(uintptr_t,uintptr_t);
            reinterpret_cast<Stop>(base+0x418100)(effect,&result,true);
            reinterpret_cast<Release>(base+0x16090)(slot,0);
        }
        static std::atomic_bool reported{};
        if(!reported.exchange(true))mgs5vr::log("VR sandstorm: camera spray suppressed; world near/far effects retained");
        return true;
    }
    return false;
}
void createEffect(uintptr_t owner,uintptr_t slot,uintptr_t resource){
    if(!suppressCameraEffect(owner,slot))originalCreate(owner,slot,resource);
}
void updateEffect(uintptr_t owner,uintptr_t slot,float intensity){
    // Also retire spray created during a native cinematic before VR resumed.
    if(!suppressCameraEffect(owner,slot))originalUpdate(owner,slot,intensity);
}
}
namespace mgs5vr {
void initializeWeatherEffects(uintptr_t moduleBase) noexcept {
    base=moduleBase;
    constexpr std::array<unsigned char,15> createBytes{0x48,0x8b,0xc4,0x55,0x48,0x8d,0x68,0xb8,0x48,0x81,0xec,0x40,1,0,0};
    constexpr std::array<unsigned char,16> stopBytes{0xc7,2,0,0,0,0,0x48,0x8b,0xc2,0x45,0x84,0xc0,0x74,5,0x83,0x49};
    constexpr std::array<unsigned char,16> releaseBytes{0x48,0x89,0x5c,0x24,8,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0xda,0x48,0x8b,0xf9};
    constexpr std::array<unsigned char,15> updateBytes{0x4c,0x8b,0xdc,0x49,0x89,0x5b,0x18,0x57,0x48,0x81,0xec,0xa0,0,0,0};
    if(get<std::array<unsigned char,15>>(base+0x705ad0)!=updateBytes
       ||get<std::array<unsigned char,15>>(base+0x704270)!=createBytes
       ||get<std::array<unsigned char,16>>(base+0x418100)!=stopBytes
       ||get<std::array<unsigned char,16>>(base+0x16090)!=releaseBytes){
        log("VR sandstorm camera-effect adapter unavailable: native signature mismatch");return;
    }
    auto target=reinterpret_cast<void*>(base+0x704270);
    if(MH_CreateHook(target,reinterpret_cast<void*>(&createEffect),reinterpret_cast<void**>(&originalCreate))!=MH_OK)return;
    auto update=reinterpret_cast<void*>(base+0x705ad0);
    if(MH_CreateHook(update,reinterpret_cast<void*>(&updateEffect),reinterpret_cast<void**>(&originalUpdate))!=MH_OK){MH_RemoveHook(target);return;}
    if(MH_EnableHook(target)!=MH_OK||MH_EnableHook(update)!=MH_OK){
        MH_DisableHook(target);MH_DisableHook(update);MH_RemoveHook(target);MH_RemoveHook(update);return;
    }
    log("VR sandstorm camera-effect adapter installed");
}
}
