#include "mgs5vr/vehicle_weapon.hpp"
#include "mgs5vr/head_camera.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/log.hpp"
#include <windows.h>
#include <intrin.h>
#include <MinHook.h>
#include <array>
#include <atomic>
#include <cstring>
#include <mutex>
#include <sstream>

namespace {
using namespace mgs5vr;
// Retail PlayerPad semantics, independently of XInput's pedal assignments.
constexpr uint32_t holdBit=0x8,fireBit=0x10,reloadBit=0x40;
constexpr uint32_t ownedBits=holdBit|fireBit|reloadBit;
constexpr uint64_t maximumAge=150;
uintptr_t base{};
std::atomic_bool enabled{};
std::atomic_uintptr_t activePad{},activeWeapon{};
struct Request { bool requested{},fire{},reload{};uint64_t time{},activation{}; };
struct NativeInput {
    uintptr_t pad{},player{};
    uint64_t time{},activation{};
    uint32_t held{},pressed{},released{};
    bool active{},fireReleaseRequired{},reloadReleaseRequired{};
};
struct Owner {
    uintptr_t player{},character{},context{},pad{},weapon{},state{},renderState{};
    uint32_t index{};
    uint64_t activation{};
};
std::mutex mutex;
Request request;
NativeInput input;
VehicleWeaponState state;
using Getter=uint32_t(*)(void*);
using CurrentSample=bool(*)(void*);
using PadUpdate=void(*)(void*);
using WeaponUpdate=bool(*)(void*,uint32_t);
using AnyBits=bool(*)(void*,uint32_t,...);
std::array<Getter,8> originalGetters{};
PadUpdate originalPadUpdate{};
WeaponUpdate originalWeaponUpdate{};
WeaponUpdate originalEligibilityUpdate{};
CurrentSample originalCurrentSample{};
AnyBits originalAnyBits{};
void* anyBitsDispatcher{};
thread_local Owner eligibilityOwner;
thread_local uint64_t eligibilityAdaptations{};
constexpr std::array<uintptr_t,8> getterRvas{
    0x1205f00,0x1205f20,0x1205db0,0x1205dd0,
    0x1205e80,0x1205ea0,0x1205ee0,0x1205ec0};
constexpr std::array<uintptr_t,8> getterSlots{0x20,0x28,0x30,0x38,0x40,0x48,0x58,0x50};
template<class T> bool read(uintptr_t address,T& value){
    SIZE_T count{};
    return address&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),
        &value,sizeof(value),&count)&&count==sizeof(value);
}
template<class T> T get(uintptr_t address){T value{};read(address,value);return value;}
bool fresh(uint64_t time,uint64_t now){return time&&now>=time&&now-time<=maximumAge;}
bool nativePad(uintptr_t pad){
    const auto table=get<uintptr_t>(pad);
    return table==base+0x23c8ba0||table==base+0x23c8d40;
}
// Join the semantic owner to the current local player and accepted XR camera.
// Merely finding a PlayerPad-shaped object never authorizes input injection.
Owner currentOwner(uint64_t now){
    const auto status=headCamera().status();
    if(!status.active||status.suspended||status.nativeMenuOpen||status.nativeIdroidOpen)return {};
    const auto frame=headCamera().publishedView();
    if(!frame||!frame->applied||frame->activation!=status.activation||!frame->playerOwner
       ||!fresh(frame->sampleTime,now)||!frame->trackingSequence||frame->menuOpen)return {};
    const auto& c=frame->controllers;
    if(!c.presentationFocused||!c.vehicleControls||c.nativeGamepad||c.nativeActionControls
       ||c.equipmentOpen||c.commandControls||c.frontEnd||c.loading||c.scriptedDemo||c.authoredCamera
       ||c.optic.active||!c.hands[1].gripTracked||!c.hands[1].aimTracked)return {};
    Owner result;
    result.player=frame->playerOwner;result.activation=frame->activation;
    if(get<uintptr_t>(result.player)!=base+0x23b8218)return {};
    result.character=get<uintptr_t>(result.player+0x370);
    result.index=get<uint32_t>(result.player+0x3a0);
    result.context=get<uintptr_t>(result.character+0x138);
    if(!result.character||!result.context||get<uint32_t>(result.context+0x218)!=result.index)return {};
    result.pad=get<uintptr_t>(result.context+0xb0);
    if(!nativePad(result.pad)||get<uintptr_t>(result.pad+0x280)!=result.context)return {};
    result.weapon=get<uintptr_t>(result.character+0x80);
    if(get<uintptr_t>(result.weapon)!=base+0x23b3e80
       ||get<uintptr_t>(result.weapon+8)!=result.character)return {};
    const auto instances=get<uintptr_t>(result.weapon+0x38);
    const auto first=get<uint32_t>(instances+0x24);
    const auto states=get<uintptr_t>(result.weapon+0x58);
    const auto render=get<uintptr_t>(result.character+0x78);
    const auto renderFirst=get<uint32_t>(render+8);
    const auto renderStates=get<uintptr_t>(render+0x280);
    if(!instances||!states||!renderStates||result.index<first||result.index-first>15
       ||result.index<renderFirst||result.index-renderFirst>15)return {};
    result.state=states+(result.index-first)*0x610ull;
    result.renderState=renderStates+(result.index-renderFirst)*32ull;
    if(get<uintptr_t>(result.player+0x370)!=result.character
       ||get<uintptr_t>(result.context+0xb0)!=result.pad)return {};
    return result;
}
bool driverRestrictionOwned(const Owner& owner,void* renderState){
    if(!enabled.load()||!owner.player||owner.renderState!=reinterpret_cast<uintptr_t>(renderState))return false;
    const auto now=steadyMilliseconds();
    Request pending;NativeInput sampled;
    {std::lock_guard lock(mutex);pending=request;sampled=input;}
    const auto current=currentOwner(now);
    const auto actions=get<uintptr_t>(get<uintptr_t>(owner.context+0x60)+0x18);
    const auto nativeAction=actions?get<uint16_t>(actions+owner.index*2ull):uint16_t{};
    const auto weaponClass=get<uint32_t>(owner.state+0x27c);
    const auto restrictions=get<uint32_t>(owner.renderState+0x18);
    return current.player==owner.player&&current.activation==owner.activation
        &&current.pad==owner.pad&&current.weapon==owner.weapon&&current.state==owner.state
        &&current.renderState==owner.renderState&&nativeAction==0x6f
        &&pending.requested&&pending.activation==owner.activation&&fresh(pending.time,now)
        &&sampled.active&&sampled.pad==owner.pad&&sampled.player==owner.player
        &&sampled.activation==owner.activation&&fresh(sampled.time,now)&&(sampled.held&holdBit)
        &&!(get<uint32_t>(owner.pad+0x28)&holdBit)
        &&(weaponClass&0x1c0)==0x40&&(weaponClass&1)&&(restrictions&0xc)==8;
}
bool driverWeaponRestriction(void* renderState,uint32_t first,uint32_t second,uint32_t terminal){
    // This typed entry is reached only from the verified two-bit query in
    // 103e010. The dispatcher tail-forwards every other variadic call intact.
    const auto& owner=eligibilityOwner;
    if(first==0xc3&&second==0xc2&&terminal==0&&driverRestrictionOwned(owner,renderState)){
        // The observed retail driver action publishes C3 to forbid its
        // stock firearm pose. VR supplies the tracked pose. Keep C2 and
        // every preceding/following native eligibility check, and let
        // the original weapon update publish readiness and run the gun.
        ++eligibilityAdaptations;
        return originalAnyBits(renderState,second,0);
    }
    return originalAnyBits(renderState,first,second,terminal);
}
bool driverFireRestriction(void* renderState,uint32_t first,uint32_t second,
        uint32_t third,uint32_t fourth,uint32_t terminal){
    // Only the firearm-family branch in 1095df0 reaches this typed entry.
    // Its native manipulation, new-state, ready, ammo and timing gates run
    // unchanged; the exact driver restriction is the sole omitted query bit.
    const auto owner=currentOwner(steadyMilliseconds());
    if(first==0xc3&&second==0xc2&&third==5&&fourth==0x3a&&terminal==0
       &&driverRestrictionOwned(owner,renderState))
        return originalAnyBits(renderState,second,third,fourth,0);
    return originalAnyBits(renderState,first,second,third,fourth,terminal);
}
bool driverCurrentSample(void* object){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const bool nativeResult=originalCurrentSample(object);
    if(nativeResult||caller!=base+0x10961a2)return nativeResult;
    const auto owner=currentOwner(steadyMilliseconds());
    // The retail firearm cadence loop can consume raw gamepad history.
    // Our driver controls own semantic input instead, so use its existing
    // current-aggregate path. Otherwise pedal history could become FIRE.
    // Only this exact call changes, with all native timing/shot rules kept.
    return owner.pad==reinterpret_cast<uintptr_t>(object)
        &&driverRestrictionOwned(owner,reinterpret_cast<void*>(owner.renderState));
}
bool prepareAnyBitsDispatcher(){
    // Win64 variadic callers may have further stack arguments. An ordinary
    // C++ detour cannot forward those transparently. This leaf tail-dispatch
    // preserves RCX/RDX/R8/R9, RSP and all stack arguments for every caller
    // except the verified four- and six-argument weapon queries above.
    std::array<unsigned char,68> code{
        0x48,0xb8,0,0,0,0,0,0,0,0,              // mov rax, verified return
        0x48,0x39,0x04,0x24,0x75,0x0c,          // cmp [rsp],rax; jne next
        0x48,0xb8,0,0,0,0,0,0,0,0,0xff,0xe0,   // mov rax, adapter; jmp rax
        0x48,0xb8,0,0,0,0,0,0,0,0,              // mov rax, verified fire return
        0x48,0x39,0x04,0x24,0x75,0x0c,          // cmp [rsp],rax; jne original
        0x48,0xb8,0,0,0,0,0,0,0,0,0xff,0xe0,   // mov rax, fire adapter; jmp rax
        0x48,0xb8,0,0,0,0,0,0,0,0,0xff,0x20};  // mov rax, &original; jmp [rax]
    const uintptr_t returnAddress=base+0x103e1a6;
    const uintptr_t fireReturnAddress=base+0x1095f21;
    const auto adapter=reinterpret_cast<uintptr_t>(&driverWeaponRestriction);
    const auto fireAdapter=reinterpret_cast<uintptr_t>(&driverFireRestriction);
    const auto original=reinterpret_cast<uintptr_t>(&originalAnyBits);
    std::memcpy(code.data()+2,&returnAddress,sizeof(returnAddress));
    std::memcpy(code.data()+18,&adapter,sizeof(adapter));
    std::memcpy(code.data()+30,&fireReturnAddress,sizeof(fireReturnAddress));
    std::memcpy(code.data()+46,&fireAdapter,sizeof(fireAdapter));
    std::memcpy(code.data()+58,&original,sizeof(original));
    anyBitsDispatcher=VirtualAlloc(nullptr,code.size(),MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if(!anyBitsDispatcher)return false;
    std::memcpy(anyBitsDispatcher,code.data(),code.size());
    DWORD previous{};
    if(!VirtualProtect(anyBitsDispatcher,code.size(),PAGE_EXECUTE_READ,&previous)
       ||!FlushInstructionCache(GetCurrentProcess(),anyBitsDispatcher,code.size())){
        VirtualFree(anyBitsDispatcher,0,MEM_RELEASE);anyBitsDispatcher=nullptr;return false;
    }
    return true;
}
uint32_t semantic(void* object,uint32_t native,unsigned channel){
    if(!enabled.load()||activePad.load()!=reinterpret_cast<uintptr_t>(object))return native;
    NativeInput sample;Request pending;
    {std::lock_guard lock(mutex);sample=input;pending=request;}
    const auto now=steadyMilliseconds();
    if(!sample.active||sample.pad!=reinterpret_cast<uintptr_t>(object)||!fresh(sample.time,now)
       ||pending.activation!=sample.activation)return native;
    uint32_t bits=channel==0?sample.held:channel==1?sample.pressed:sample.released;
    // Lost focus/ownership cancels immediately, even between native pad ticks.
    if(!pending.requested||!fresh(pending.time,now))bits=channel==2?(sample.released|sample.held):0;
    uint32_t nativeMask{};
    if(!read(sample.pad+0x28,nativeMask))return native&~ownedBits;
    // Even native's unmasked getters receive only permitted injected actions.
    // We never remove a named gameplay/tutorial/menu input mask.
    return (native&~ownedBits)|(bits&~nativeMask);
}
uint32_t aggregate(void* p){return semantic(p,originalGetters[0](p),0);}
uint32_t aggregateUnmasked(void* p){return semantic(p,originalGetters[1](p),0);}
uint32_t held(void* p){return semantic(p,originalGetters[2](p),0);}
uint32_t heldUnmasked(void* p){return semantic(p,originalGetters[3](p),0);}
uint32_t pressed(void* p){return semantic(p,originalGetters[4](p),1);}
uint32_t pressedUnmasked(void* p){return semantic(p,originalGetters[5](p),1);}
uint32_t released(void* p){return semantic(p,originalGetters[6](p),2);}
uint32_t releasedUnmasked(void* p){return semantic(p,originalGetters[7](p),2);}
void padUpdate(void* object){
    if(enabled.load()){
        const auto now=steadyMilliseconds();
        const auto owner=currentOwner(now);
        std::lock_guard lock(mutex);
        const auto pad=reinterpret_cast<uintptr_t>(object);
        if(owner.pad==pad&&owner.pad){
            const bool same=input.pad==pad&&input.player==owner.player&&input.activation==owner.activation;
            const bool wanted=request.requested&&request.activation==owner.activation&&fresh(request.time,now);
            const auto previous=same?input.held:0u;
            const bool acquiring=wanted&&(!same||!(previous&holdBit));
            if(!same)input={};
            if(acquiring){input.fireReleaseRequired=true;input.reloadReleaseRequired=true;}
            if(!request.fire)input.fireReleaseRequired=false;
            if(!request.reload)input.reloadReleaseRequired=false;
            const bool ready=state.ready&&state.player==owner.player&&state.activation==owner.activation
                &&fresh(state.time,now);
            uint32_t desired=wanted?holdBit:0;
            if(wanted&&ready&&request.fire&&!input.fireReleaseRequired)desired|=fireBit;
            if(wanted&&ready&&request.reload&&!input.reloadReleaseRequired)desired|=reloadBit;
            input.pad=pad;input.player=owner.player;input.activation=owner.activation;input.time=now;
            input.held=desired;input.pressed=desired&~previous;input.released=previous&~desired;
            // Keep one native release tick, then relinquish these semantics.
            input.active=wanted||previous!=0;
            activePad.store(input.active?pad:0);activeWeapon.store(owner.weapon);
        }else if(input.pad==pad){input={};state={};activePad.store(0);activeWeapon.store(0);}
    }
    // This routine copies the held getter's HOLD bit to PlayerPad+0x27c bit0.
    // Retail weapon update subsequently consumes it; no native state is written here.
    originalPadUpdate(object);
}
void sampleWeaponState(void* component,uint32_t index,bool initialUpdate,bool nativeResult,bool driverAdapted);
bool eligibilityUpdate(void* component,uint32_t index){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const auto previousAdaptations=eligibilityAdaptations;
    const auto previousOwner=eligibilityOwner;
    eligibilityOwner={};
    if(enabled.load()&&activeWeapon.load()==reinterpret_cast<uintptr_t>(component)){
        const auto owner=currentOwner(steadyMilliseconds());
        if(owner.weapon==reinterpret_cast<uintptr_t>(component)&&owner.index==index)eligibilityOwner=owner;
    }
    // Both initial readiness (10542f0) and the continuing weapon phase
    // (1040330) query 103e010. Scoping only the initial call made the next
    // phase clear eligibility and tear down the native draw every frame.
    const bool nativeResult=originalEligibilityUpdate(component,index);
    eligibilityOwner=previousOwner;
    // 1054050 supplies the initial ready acknowledgement, but retail stops
    // calling it once the gun enters its active draw/fire state. The continuing
    // 1040100 phase still publishes real eligibility through this exact call.
    // Refresh only an acknowledgement acquired from the initial native update;
    // a successful eligibility query alone must never manufacture readiness.
    if(caller==base+0x1040335)
        sampleWeaponState(component,index,false,false,eligibilityAdaptations!=previousAdaptations);
    return nativeResult;
}
bool weaponUpdate(void* component,uint32_t index){
    const auto previousAdaptations=eligibilityAdaptations;
    const bool nativeResult=originalWeaponUpdate(component,index);
    const bool driverAdapted=eligibilityAdaptations!=previousAdaptations;
    sampleWeaponState(component,index,true,nativeResult,driverAdapted);
    return nativeResult;
}
void sampleWeaponState(void* component,uint32_t index,bool initialUpdate,bool nativeResult,bool driverAdapted){
    if(!enabled.load()||activeWeapon.load()!=reinterpret_cast<uintptr_t>(component))return;
    const auto now=steadyMilliseconds();
    const auto owner=currentOwner(now);
    if(!owner.pad||owner.weapon!=reinterpret_cast<uintptr_t>(component)||owner.index!=index)return;
    VehicleWeaponState next;
    next.time=now;next.activation=owner.activation;next.player=owner.player;
    next.weaponFlags=get<uint32_t>(owner.state+0x3c0);
    next.renderFlags=get<uint32_t>(owner.renderState);
    next.renderPoseFlags=get<uint32_t>(owner.renderState+0xc);
    next.renderActionFlags=get<uint32_t>(owner.renderState+0x18);
    const auto actionTable=get<uintptr_t>(get<uintptr_t>(owner.context+0x60)+0x18);
    if(actionTable)next.nativeActionId=get<uint16_t>(actionTable+static_cast<uintptr_t>(index)*2);
    next.nativePosture=get<uint8_t>(owner.state+0x3d4);
    next.nativeAction=get<uint8_t>(owner.state+0x2c1);
    next.equippedSlot=get<uint8_t>(owner.state+0x3d9);
    next.requestedSlot=get<uint8_t>(owner.state+0x3da);
    const auto weaponClass=get<uint32_t>(owner.state+0x27c);
    const auto mask=get<uint32_t>(owner.pad+0x28);
    next.inputMask=mask;
    next.inputMasked=(mask&ownedBits)!=0;
    next.nativeEligible=(weaponClass&0x1c0)==0x40&&(weaponClass&1)!=0
        &&(next.renderFlags&0x01000000)!=0&&(next.renderActionFlags&0x8000)==0
        &&(next.weaponFlags&0x10)==0;
    bool report{};
    {
        std::lock_guard lock(mutex);
        next.requested=request.requested&&request.activation==owner.activation&&fresh(request.time,now);
        next.semanticHeld=input.pad==owner.pad?(input.held&~mask):0;
        const bool acknowledged=initialUpdate?nativeResult:
            state.ready&&state.requested&&state.player==owner.player&&state.activation==owner.activation;
        next.ready=next.requested&&next.nativeEligible&&acknowledged
            &&input.active&&input.player==owner.player&&input.activation==owner.activation
            &&fresh(input.time,now)&&(next.semanticHeld&holdBit)
            &&(next.weaponFlags&0x000c0000)!=0&&(mask&holdBit)==0;
        report=next.requested!=state.requested||(next.requested&&(next.ready!=state.ready
            ||next.nativeEligible!=state.nativeEligible||next.inputMasked!=state.inputMasked
            ||next.nativePosture!=state.nativePosture||next.nativeActionId!=state.nativeActionId
            ||next.equippedSlot!=state.equippedSlot||next.requestedSlot!=state.requestedSlot));
        state=next;
    }
    if(report){
        std::ostringstream s;s<<"Vehicle personal weapon requested="<<next.requested<<" ready="<<next.ready
            <<" eligible="<<next.nativeEligible<<" masked="<<next.inputMasked
            <<" posture="<<unsigned(next.nativePosture)<<" action="<<unsigned(next.nativeAction)
            <<" slots="<<unsigned(next.equippedSlot)<<','<<unsigned(next.requestedSlot)
            <<" native_action=0x"<<std::hex<<next.nativeActionId
            <<" semantic=0x"<<std::hex<<next.semanticHeld<<" weapon=0x"<<next.weaponFlags
            <<" render=0x"<<next.renderFlags<<" render_pose=0x"<<next.renderPoseFlags
            <<" render_action=0x"<<next.renderActionFlags<<" mask=0x"<<next.inputMask
            <<" driver_query="<<driverAdapted;log(s.str());
    }
}
bool verifyGetters(){
    for(size_t i=0;i<getterRvas.size();++i){
        const uint8_t source=static_cast<uint8_t>(i<2?0x48:i<4?0x4c:i<6?0x50:0x54);
        const uint8_t fallback=static_cast<uint8_t>(i<2?0x60:i<4?0x64:i<6?0x68:0x6c);
        const bool masked=(i%2)==0;
        if(masked){
            const std::array<unsigned char,25> signature{0x48,0x8b,0x41,0x08,0x8b,0x50,source,0x85,0xd2,
                0x75,0x06,0x8b,0x91,fallback,0x04,0x00,0x00,0x8b,0x41,0x28,0xf7,0xd0,0x23,0xc2,0xc3};
            if(get<std::array<unsigned char,25>>(base+getterRvas[i])!=signature)return false;
        }else{
            const std::array<unsigned char,19> signature{0x48,0x8b,0x41,0x08,0x8b,0x40,source,0x85,0xc0,
                0x75,0x06,0x8b,0x81,fallback,0x04,0x00,0x00,0xf3,0xc3};
            if(get<std::array<unsigned char,19>>(base+getterRvas[i])!=signature)return false;
        }
        for(const uintptr_t table:{0x23c8ba0ull,0x23c8d40ull})
            if(get<uintptr_t>(base+table+getterSlots[i])!=base+getterRvas[i])return false;
    }
    for(const uintptr_t table:{0x23c8ba0ull,0x23c8d40ull})
        if(get<uintptr_t>(base+table+0x98)!=base+0x12063f0)return false;
    return get<std::array<unsigned char,14>>(base+0xff3e90)
        ==std::array<unsigned char,14>{0x89,0x54,0x24,0x10,0x4c,0x89,0x44,0x24,0x18,0x4c,0x89,0x4c,0x24,0x20}
        &&get<std::array<unsigned char,22>>(base+0x103e192)
        ==std::array<unsigned char,22>{0xba,0xc3,0,0,0,0x45,0x33,0xc9,0x49,0x8b,0xce,
            0x44,0x8d,0x42,0xff,0xe8,0xea,0x5c,0xfb,0xff,0x84,0xc0}
        &&get<std::array<unsigned char,15>>(base+0x103e010)
        ==std::array<unsigned char,15>{0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x48,0x89,0x7c,0x24,0x20}
        &&get<std::array<unsigned char,12>>(base+0x104032b)
        ==std::array<unsigned char,12>{0x8b,0xd6,0x48,0x8b,0xcd,0xe8,0xdb,0xdc,0xff,0xff,0x4c,0x8b}
        &&get<std::array<unsigned char,36>>(base+0x1095eff)
        ==std::array<unsigned char,36>{0xba,0xc3,0,0,0,0x44,0x8d,0x4d,0x05,0x49,0x8b,0xce,
            0x44,0x8d,0x42,0xff,0x48,0x89,0x6c,0x24,0x28,0xc7,0x44,0x24,0x20,0x3a,0,0,0,
            0xe8,0x6f,0xdf,0xf5,0xff,0x84,0xc0}
        &&get<std::array<unsigned char,14>>(base+0x12063f0)
        ==std::array<unsigned char,14>{0x0f,0xb6,0x81,0x88,0x04,0,0,0xc1,0xe8,0x06,0x83,0xe0,0x01,0xc3}
        &&get<std::array<unsigned char,6>>(base+0x109619c)
        ==std::array<unsigned char,6>{0xff,0x90,0x98,0,0,0}
        &&get<std::array<unsigned char,13>>(base+0x1206a00)
        ==std::array<unsigned char,13>{0x48,0x8b,0xc4,0x56,0x41,0x54,0x48,0x81,0xec,0xb8,0,0,0}
        &&get<std::array<unsigned char,15>>(base+0x1054050)
        ==std::array<unsigned char,15>{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18};
}
}
namespace mgs5vr {
void installVehicleWeaponInput(uintptr_t imageBase){
    if(enabled.load())return;
    if(anyBitsDispatcher){
        log("Vehicle personal weapon adapter unavailable: earlier hook cleanup remains pending");return;
    }
    base=imageBase;
    if(!verifyGetters()){
        log("Vehicle personal weapon adapter unavailable: native semantic input signatures differ");return;
    }
    if(!prepareAnyBitsDispatcher()){
        log("Vehicle personal weapon adapter unavailable: native eligibility dispatcher allocation failed");return;
    }
    const std::array<void*,13> detours{reinterpret_cast<void*>(&aggregate),reinterpret_cast<void*>(&aggregateUnmasked),
        reinterpret_cast<void*>(&held),reinterpret_cast<void*>(&heldUnmasked),reinterpret_cast<void*>(&pressed),
        reinterpret_cast<void*>(&pressedUnmasked),reinterpret_cast<void*>(&released),reinterpret_cast<void*>(&releasedUnmasked),
        reinterpret_cast<void*>(&padUpdate),reinterpret_cast<void*>(&weaponUpdate),anyBitsDispatcher,
        reinterpret_cast<void*>(&eligibilityUpdate),reinterpret_cast<void*>(&driverCurrentSample)};
    std::array<void*,13> targets{};
    std::array<void**,13> originals{};
    for(size_t i=0;i<8;++i){targets[i]=reinterpret_cast<void*>(base+getterRvas[i]);originals[i]=reinterpret_cast<void**>(&originalGetters[i]);}
    targets[8]=reinterpret_cast<void*>(base+0x1206a00);originals[8]=reinterpret_cast<void**>(&originalPadUpdate);
    targets[9]=reinterpret_cast<void*>(base+0x1054050);originals[9]=reinterpret_cast<void**>(&originalWeaponUpdate);
    targets[10]=reinterpret_cast<void*>(base+0xff3e90);originals[10]=reinterpret_cast<void**>(&originalAnyBits);
    targets[11]=reinterpret_cast<void*>(base+0x103e010);originals[11]=reinterpret_cast<void**>(&originalEligibilityUpdate);
    targets[12]=reinterpret_cast<void*>(base+0x12063f0);originals[12]=reinterpret_cast<void**>(&originalCurrentSample);
    size_t created{};
    MH_STATUS result=MH_OK;
    for(;created<targets.size();++created){
        result=MH_CreateHook(targets[created],detours[created],originals[created]);
        if(result!=MH_OK)break;
    }
    if(result==MH_OK){
        for(void* target:targets){result=MH_QueueEnableHook(target);if(result!=MH_OK)break;}
        if(result==MH_OK)result=MH_ApplyQueued();
    }
    if(result!=MH_OK){
        bool dispatcherRemoved=true;
        for(size_t i=0;i<created;++i){
            MH_DisableHook(targets[i]);
            const auto removed=MH_RemoveHook(targets[i]);
            if(i==10&&removed!=MH_OK&&removed!=MH_ERROR_NOT_CREATED)dispatcherRemoved=false;
        }
        // A failed patch removal can leave a live jump to this allocation.
        // Keep its forwarding stub valid until process exit in that case.
        if(dispatcherRemoved){VirtualFree(anyBitsDispatcher,0,MEM_RELEASE);anyBitsDispatcher=nullptr;}
        log(std::string("Vehicle personal weapon adapter unavailable: ")+MH_StatusToString(result));return;
    }
    enabled.store(true);
    log("Vehicle personal weapon semantic adapter installed; native eligibility and ammo/cadence retained");
}
void stopVehicleWeaponInput() noexcept{
    enabled.store(false);activePad.store(0);activeWeapon.store(0);
    std::lock_guard lock(mutex);request={};input={};state={};
}
bool vehicleWeaponRoutingAvailable() noexcept{return enabled.load();}
void publishVehicleWeaponInput(bool requested,bool fire,bool reload,uint64_t time,uint64_t activation){
    std::lock_guard lock(mutex);
    request={requested&&enabled.load(),fire,reload,time,activation};
    if(!request.requested){state.requested=false;state.ready=false;}
}
VehicleWeaponState vehicleWeaponState(){
    VehicleWeaponState result;
    {std::lock_guard lock(mutex);result=state;}
    const auto status=headCamera().status();
    if(!enabled.load()||!status.active||status.suspended||status.nativeMenuOpen||status.nativeIdroidOpen
       ||status.activation!=result.activation||!fresh(result.time,steadyMilliseconds()))return {};
    return result;
}
}
