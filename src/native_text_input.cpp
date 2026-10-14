#include "mgs5vr/native_text_input.hpp"
#include "mgs5vr/log.hpp"
#include <windows.h>
#include <MinHook.h>
#include <atomic>
#include <deque>
#include <mutex>
#include <cstring>

namespace mgs5vr {
namespace {
uintptr_t base{};
using PublishKey=void(*)(const RAWKEYBOARD*,HWND);
using ConvertKey=bool(*)(void*,const RAWKEYBOARD*);
PublishKey publishKey{};
ConvertKey originalConvert{};
std::atomic_uint64_t profileUntil{},downs{},ups{},conversions{},discarded{};
std::atomic_uint64_t panelUntil{},panelGeneration{},panelReference{},panelActivation{};
std::atomic_bool installed{};
std::atomic_bool cancelRequested{};
struct Pending {uint16_t character{};uint64_t at{},generation{},referenceEpoch{},activation{};};
std::mutex mutex;
std::deque<Pending> pending;
RAWKEYBOARD active{};
HWND activeWindow{};
uint64_t releaseAt{};
thread_local const RAWKEYBOARD* synthetic{};
thread_local uint16_t syntheticCharacter{};
template<class T> T read(uintptr_t address){T value{};SIZE_T n{};
    if(!address||!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),&value,sizeof value,&n)||n!=sizeof value)return {};
    return value;}
HWND window(){
    const auto owner=read<uintptr_t>(base+0x2b642e0);
    // The registered native window class owns +0x90; this is the same HWND
    // passed into the retail RAWKEYBOARD publisher, never an OS input target.
    return owner&&read<uintptr_t>(owner)==base+0x20b45a0?read<HWND>(owner+0x90):nullptr;
}
bool owner(){return nativeProfileMenuActive()
    &&read<uintptr_t>(base+0x2a60110)==base+0x227c790
    &&read<uint8_t>(base+0x2a6014a)==1;}
bool panelVisible(){const auto until=panelUntil.load();return until&&GetTickCount64()<=until;}
bool convert(void* output,const RAWKEYBOARD* raw){
    const bool result=originalConvert(output,raw);
    if(raw!=synthetic||!output)return result;
    // Retail converts RAWKEYBOARD to a 16-byte Fox key event: +8 ANSI, +10
    // UTF-16, +12 release. Override only our own call's character, preserving
    // the native scan/key mapping and every ordinary keyboard event.
    const uint16_t character=(raw->Flags&RI_KEY_BREAK)?0:syntheticCharacter;
    std::memcpy(static_cast<unsigned char*>(output)+8,&character,sizeof character);
    std::memcpy(static_cast<unsigned char*>(output)+10,&character,sizeof character);
    ++conversions;return true;
}
void emit(const RAWKEYBOARD& key,HWND target,uint16_t character){
    synthetic=&key;syntheticCharacter=character;
    publishKey(&key,target);
    synthetic=nullptr;syntheticCharacter=0;
}
RAWKEYBOARD packet(uint16_t character){
    RAWKEYBOARD value{};
    if(character=='\b')value.VKey=VK_BACK;
    else if(character=='\r')value.VKey=VK_RETURN;
    else if(character==' ')value.VKey=VK_SPACE;
    else if(character=='-')value.VKey=VK_OEM_MINUS;
    else if(character=='.')value.VKey=VK_OEM_PERIOD;
    else if(character==',')value.VKey=VK_OEM_COMMA;
    else value.VKey=character;
    value.MakeCode=static_cast<USHORT>(MapVirtualKeyW(value.VKey,MAPVK_VK_TO_VSC));
    value.Message=WM_KEYDOWN;return value;
}
void release(){
    if(!releaseAt)return;
    auto key=active;key.Flags|=RI_KEY_BREAK;key.Message=WM_KEYUP;
    if(activeWindow&&window()==activeWindow){emit(key,activeWindow,0);++ups;}
    active={};activeWindow=nullptr;releaseAt=0;
}
}
void publishNativeProfileMenu(bool isActive) noexcept {profileUntil.store(isActive?GetTickCount64()+250:0);}
bool nativeProfileMenuActive() noexcept {const auto until=profileUntil.load();return until&&GetTickCount64()<=until;}
bool nativeTextInputReady() noexcept {return installed.load();}
void publishNativeTextPanel(bool visible,uint64_t generation,uint64_t referenceEpoch,uint64_t activation) noexcept {
    if(!visible){panelUntil.store(0);cancelRequested.store(true);return;}
    panelGeneration.store(generation);panelReference.store(referenceEpoch);panelActivation.store(activation);
    panelUntil.store(GetTickCount64()+250);
}
void installNativeTextInput(uintptr_t imageBase) noexcept {
    base=imageBase;
    constexpr unsigned char signature[]{0x48,0x89,0x5c,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x57,0x48,0x81,0xec,0x90,0x01,0x00,0x00};
    std::array<unsigned char,sizeof signature> bytes{};SIZE_T n{};
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(base+0x1a45650),bytes.data(),bytes.size(),&n)
       ||n!=bytes.size()||std::memcmp(bytes.data(),signature,sizeof signature)
       ||read<uint8_t>(base+0x1a45790)!=0xe9){log("Native VR text input ABI unavailable");return;}
    const auto target=base+0x1a45795+read<int32_t>(base+0x1a45791);
    if(target<base||target>=base+167890944u)return;
    const auto hook=reinterpret_cast<void*>(base+0x1a45650);
    if(MH_CreateHook(hook,reinterpret_cast<void*>(&convert),reinterpret_cast<void**>(&originalConvert))!=MH_OK)return;
    if(MH_EnableHook(hook)!=MH_OK){MH_RemoveHook(hook);originalConvert=nullptr;return;}
    publishKey=reinterpret_cast<PublishKey>(base+0x1a45790);installed.store(true);
    log("Native VR keyboard uses retail Fox RAWKEYBOARD event publication");
}
bool queueNativeTextKey(uint16_t character,uint64_t generation,uint64_t referenceEpoch,uint64_t activation) noexcept {
    const bool valid=character=='\b'||character=='\r'||character==' '||character=='-'||character=='.'||character==','
        ||(character>='A'&&character<='Z')||(character>='0'&&character<='9');
    if(!valid||!installed.load()||!owner()||!panelVisible()||!generation||!referenceEpoch||!activation
        ||panelGeneration.load()!=generation||panelReference.load()!=referenceEpoch||panelActivation.load()!=activation)return false;
    std::lock_guard lock(mutex);if(pending.size()>=16)return false;
    pending.push_back({character,GetTickCount64(),generation,referenceEpoch,activation});return true;
}
void pumpNativeTextInput() noexcept {
    if(!installed.load())return;
    std::lock_guard lock(mutex);const auto now=GetTickCount64();
    if(cancelRequested.exchange(false)){release();discarded+=pending.size();pending.clear();return;}
    if(releaseAt&&(now>=releaseAt||!owner()||!panelVisible()))release();
    while(!pending.empty()&&(!owner()||!panelVisible()||now<pending.front().at||now-pending.front().at>1000
        ||pending.front().generation!=panelGeneration.load()||pending.front().referenceEpoch!=panelReference.load()
        ||pending.front().activation!=panelActivation.load())){
        pending.pop_front();++discarded;
    }
    if(releaseAt||pending.empty())return;
    const auto target=window();if(!target)return;
    const auto value=pending.front();pending.pop_front();active=packet(value.character);activeWindow=target;
    emit(active,target,value.character);++downs;releaseAt=now+80;
}
NativeTextInputStatus nativeTextInputStatus() noexcept {
    std::lock_guard lock(mutex);return {downs.load(),ups.load(),conversions.load(),discarded.load(),
        static_cast<uint32_t>(pending.size()),installed.load(),owner(),panelVisible()};
}
void cancelNativeTextKeys() noexcept {cancelRequested.store(true);}
void stopNativeTextInput() noexcept {
    std::lock_guard lock(mutex);if(installed.load())release();pending.clear();profileUntil.store(0);panelUntil.store(0);
    installed.store(false);
    if(base)MH_DisableHook(reinterpret_cast<void*>(base+0x1a45650));
}
}
