#include "mgs5vr/native_actions.hpp"
#include "mgs5vr/native_controls.hpp"
#include "mgs5vr/native_text_input.hpp"
#include "mgs5vr/vehicle_weapon.hpp"
#include "mgs5vr/log.hpp"
#include "mgs5vr/animal_interaction.hpp"
#include "mgs5vr/small_animal.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/native_input_delivery.hpp"
#include "mgs5vr/controller_rig.hpp"
#include "mgs5vr/player_visibility.hpp"
#include "mgs5vr/idroid_attachment.hpp"
#include "mgs5vr/controls.hpp"
#include "mgs5vr/control_prompts.hpp"
#include "mgs5vr/controls_editor.hpp"
#include "mgs5vr/weapon_obstruction.hpp"
#include "mgs5vr/ui_renderer.hpp"
#include "mgs5vr/render_camera.hpp"
#include "mgs5vr/native_menu_restrictions.hpp"
#include "mgs5vr/native_game_status.hpp"
#include "mgs5vr/tracking_fault.hpp"
#include "mgs5vr/opening_selector.hpp"
#include "mgs5vr/optic_events.hpp"
#include "mgs5vr/optic_markers.hpp"
#include "native_cabin_script.hpp"
#include "native_presentation_script.hpp"
#include "native_god_mode_script.hpp"
#include "native_idroid_interruption_script.hpp"
#include "native_idroid_open_script.hpp"
#include <windows.h>
#include <MinHook.h>
#include <intrin.h>
#include <array>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sstream>
#include <thread>
#include <charconv>
#include <unordered_set>

namespace mgs5vr {
namespace {
using Call=int(*)(void*,int,int,int);
using Load=int(*)(void*,const char*,size_t,const char*);
using Top=int(*)(void*);
using SetTop=void(*)(void*,int);
using String=const char*(*)(void*,int,size_t*);
Call original{};Load load{};Top getTop{};SetTop setTop{};String getString{};
using Send=void*(*)(void*,int32_t*,uint32_t,void*);
Send originalSend{};
uintptr_t imageBase{};
std::atomic_uint64_t traceUntil{};
std::mutex traceMutex;
std::unordered_set<uint64_t> traced;
std::atomic_bool enabled{};
std::atomic_bool nativeLuaReady{};
uint64_t cameraFreezeActivation{},cameraFreezeReference{};
std::mutex requestMutex;
std::filesystem::path requestPath,resultPath,tracePath;
bool externalCommands{};
uint64_t checkedAt{};
std::mutex stowHelpMutex;
NativeIdroidStowHelpLease stowHelpLease;
thread_local unsigned depth{};
// Bounded observations of the retail UI input and stow handshake, plus
// focused-firearm ownership filtering at the native UI trigger consumers.
using UiButton=bool(*)(void*,uint32_t,uint32_t,bool);
using UiClose=bool(*)(void*);
UiButton originalUiButton{},originalUiEdge{},originalUiRelease{};UiClose originalUiClose{};
using UiRepeat=bool(*)(void*,uint32_t,uint32_t,double,double,bool);
using UiRepeatState=uint32_t(*)(void*,uint32_t,uint32_t,double,bool);
UiRepeat originalUiRepeat{};
UiRepeatState originalUiRepeatState{};
using UiTabUpdate=void(*)(void*);
UiTabUpdate originalUiTabUpdate{};
using UiTabAction=bool(*)(uint32_t);
UiTabAction originalUiTabAction{};
thread_local uintptr_t idroidTabOwner{};
thread_local bool idroidTabWrapped{};
thread_local NativeIdroidTabEdge idroidTabNativeEdge;

void uiTabUpdate(void* object){
    const auto previous=idroidTabOwner;
    const auto previousWrapped=idroidTabWrapped;
    idroidTabOwner=reinterpret_cast<uintptr_t>(object);
    idroidTabWrapped=false;
    originalUiTabUpdate(object);
    idroidTabOwner=previous;
    idroidTabWrapped=previousWrapped;
}
using UiInputMasked=bool(*)(void*);
UiInputMasked originalUiInputMasked{};
std::atomic_uint64_t handheldUiBlockedQueries{};
using UiDisable=void(*)(void*,uint8_t,bool);
UiDisable originalUiDisable{};
struct MenuDisableRead {uintptr_t owner{},caller{};unsigned mode{},index{},before{},after{};bool disabled{};};
std::deque<MenuDisableRead> menuDisableReads;
NativeMenuRestrictionLease tutorialMenuLease;
bool menuRestrictionAbi{};
std::atomic_uint64_t menuTraceUntil{},menuButtonCalls{},menuEdgeCalls{},menuCloseCalls{};
std::atomic_uint32_t menuTraceImageSize{};
std::mutex menuTraceMutex;
struct MenuMaskRead {uintptr_t type{};uint32_t count{};bool valid{};};
struct MenuInputRead {
    uint64_t time{};uint32_t index{},bit{},held{},pressed{};uint16_t xr{};bool result{},bypass{};
    uintptr_t object{},caller{};uint16_t xrBefore{};MenuMaskRead maskBefore{},maskAfter{};bool edge{};
};
std::deque<MenuInputRead> menuInputReads;
using EulaUpdate=void(*)(void*);
EulaUpdate originalEulaUpdate{};
struct EulaRead {
    uint64_t time{};uintptr_t owner{},type{},control{};uint32_t phase{},selection{},line{},lines{};
    uint8_t active{},readToEnd{},selectorVisible{},mode{},flags{};bool valid{},route{};
    uint64_t inputTime{};uint16_t buttons{};ControlContext context{};
};
EulaRead eulaRead;
std::deque<EulaRead> eulaReads;
std::atomic_uint64_t eulaBridgeEdges{},eulaScopedQueries{};
thread_local bool eulaInputScope{};
constexpr wchar_t actionPipeName[]=L"\\\\.\\pipe\\MGS5VR.NativeActions";
constexpr uint32_t maxPipeScript=1024*1024;
constexpr size_t maxQueuedActions=256;
constexpr size_t maxActionsPerTransaction=8;
uint64_t candidateKey(const char* value,size_t length){
    uint64_t hash=14695981039346656037ull;
    for(size_t i=0;i<length;++i){hash^=static_cast<unsigned char>(value[i]);hash*=1099511628211ull;}
    return hash?hash:1;
}
#pragma pack(push,1)
struct PipeRequestHeader { uint64_t id{}; uint32_t length{}; };
struct PipeResponseHeader {
    uint64_t id{};
    int32_t status{};
    uint32_t length{};
    uint64_t queuedMilliseconds{};
    uint64_t executionMicroseconds{};
};
#pragma pack(pop)
static_assert(sizeof(PipeRequestHeader)==12);
static_assert(sizeof(PipeResponseHeader)==32);
struct ActionConnection {
    HANDLE pipe{INVALID_HANDLE_VALUE};
    std::mutex writeMutex;
    std::condition_variable responseReady;
    PipeResponseHeader response{};
    std::string responseText;
    bool hasResponse{};
    std::atomic_bool open{true};
};
struct ActionRequest {
    uint64_t id{};
    std::string script;
    std::chrono::steady_clock::time_point queuedAt{};
    std::shared_ptr<ActionConnection> connection;
    std::shared_ptr<std::atomic_bool> cancelled{std::make_shared<std::atomic_bool>(false)};
};
std::mutex fastQueueMutex,connectionMutex;
std::deque<ActionRequest> fastQueue;
std::shared_ptr<ActionConnection> activeConnection;
std::thread actionPipeThread;
std::atomic_bool actionPipeStopping{true};
template<class T> T read(uintptr_t address){
    T value{};SIZE_T size{};
    if(address)ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),&value,sizeof(value),&size);
    return size==sizeof(value)?value:T{};
}
template<class T> std::optional<T> readKnown(uintptr_t address){
    T value{};SIZE_T size{};
    if(!address||!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),&value,sizeof(value),&size)
       ||size!=sizeof(value))return {};
    return value;
}
std::optional<bool> idroidPadOwner(){
    // Exact supported inline manager, S_DISABLE_PLAYER_PAD record, and the
    // low48 hash of MGS5VR_iDroid. Never reset another native owner's entry.
    constexpr uintptr_t managerRva=0x2bd6160,managerTypeRva=0x2191a90;
    constexpr uint64_t ownerHash=0xc1dbf5bd3008ull;
    if(!imageBase||readKnown<uintptr_t>(imageBase+managerRva)
            !=std::optional<uintptr_t>{imageBase+managerTypeRva})return {};
    const auto record=imageBase+managerRva+0x10+2*0x78;
    const auto before=readKnown<NativeGameStatusRecord>(record);
    const auto after=readKnown<NativeGameStatusRecord>(record);
    if(!before||!after||*before!=*after||readKnown<uintptr_t>(imageBase+managerRva)
            !=std::optional<uintptr_t>{imageBase+managerTypeRva})return {};
    return nativeGameStatusHasOwner(*before,ownerHash);
}
struct CabinStowPlayer {
    uintptr_t player{},character{},context{};
    uint64_t activation{};
    uint32_t index{};
    bool operator==(const CabinStowPlayer&) const=default;
};
std::optional<CabinStowPlayer> cabinStowPlayer(){
    const auto now=steadyMilliseconds();
    const auto frame=headCamera().publishedRigFrame(now);
    const auto status=headCamera().status();
    if(!frame||!frame->applied||!frame->playerOwner||!frame->activation||!frame->rigSequence
       ||!frame->sampleTime||now<frame->sampleTime||now-frame->sampleTime>150
       ||!status.active||status.suspended||status.activation!=frame->activation
       ||!nativeCabinPlay()||nativeTitleModeActive()||nativeLoadingTipsOpen()||nativeScriptedDemoActive())return {};
    CabinStowPlayer result{frame->playerOwner,0,0,frame->activation,0};
    if(readKnown<uintptr_t>(result.player)!=std::optional<uintptr_t>{imageBase+0x23b8218})return {};
    const auto character=readKnown<uintptr_t>(result.player+0x370);
    const auto index=readKnown<uint32_t>(result.player+0x3a0);
    if(!character||!*character||!index||*index>15
       ||readKnown<uintptr_t>(*character)!=std::optional<uintptr_t>{imageBase+0x2295210})return {};
    const auto context=readKnown<uintptr_t>(*character+0x138);
    if(!context||!*context||readKnown<uint32_t>(*context+0x218)!=index)return {};
    result.character=*character;result.context=*context;result.index=*index;
    if(readKnown<uintptr_t>(result.player+0x370)!=character
       ||readKnown<uint32_t>(result.player+0x3a0)!=index
       ||readKnown<uintptr_t>(result.character+0x138)!=context)return {};
    return result;
}
std::optional<bool> cabinStowSubject(const CabinStowPlayer& owner){
    // This is the same current phase selected by PlayerInfo.AndCheckStatus
    // (53c990 / 53ca40). Its SUBJECT enum is bit 56. Unlike the Lua boolean,
    // an unavailable player/status record must not look like an exited owner.
    for(unsigned attempt=0;attempt<2;++attempt){
        if(readKnown<uint32_t>(imageBase+0x2bd3fd4)!=std::optional<uint32_t>{owner.index})return {};
        const auto phase=readKnown<uint32_t>(imageBase+0x2bd3fd0);
        if(!phase||*phase>1)return {};
        const auto status=imageBase+0x2bd3ff0+(*phase*16ull+owner.index)*0xe0;
        const auto flags=readKnown<std::array<uint32_t,5>>(status+0xc0);
        const auto stateRoot=readKnown<Vec3>(status+0x40);
        const auto playerRoot=readKnown<Vec3>(owner.player+0x60);
        if(readKnown<uintptr_t>(status)!=std::optional<uintptr_t>{imageBase+0x2190b28}
           ||!flags||!stateRoot||!playerRoot)return {};
        const auto difference=*stateRoot-*playerRoot;
        if(!valid(Pose{{},difference})||dot(difference,difference)>4)return {};
        if(readKnown<uint32_t>(imageBase+0x2bd3fd0)!=phase)continue;
        if(readKnown<uint32_t>(imageBase+0x2bd3fd4)!=std::optional<uint32_t>{owner.index}
           ||cabinStowPlayer()!=std::optional<CabinStowPlayer>{owner})return {};
        return ((*flags)[1]&(1u<<24))!=0;
    }
    return {};
}
struct CabinStowTerminal {
    uintptr_t system{},terminal{};
    bool open{},closing{};
    bool operator==(const CabinStowTerminal&) const=default;
};
std::optional<CabinStowTerminal> cabinStowTerminal(){
    const auto system=readKnown<uintptr_t>(imageBase+0x2bf1518);
    if(!system||!*system||readKnown<uintptr_t>(*system)!=std::optional<uintptr_t>{imageBase+0x2242d78})return {};
    const auto terminal=readKnown<uintptr_t>(*system+0x7c0);
    if(!terminal||!*terminal||readKnown<uintptr_t>(*terminal)!=std::optional<uintptr_t>{imageBase+0x22705a8})return {};
    const auto open=readKnown<uint8_t>(*terminal+0x20);
    const auto closing=readKnown<uint8_t>(*terminal+0x25);
    if(!open||*open>1||!closing||*closing>1
       ||readKnown<uintptr_t>(imageBase+0x2bf1518)!=system
       ||readKnown<uintptr_t>(*system+0x7c0)!=terminal)return {};
    return CabinStowTerminal{*system,*terminal,*open==1,*closing==1};
}
std::string cabinStowPlayerGuard(const CabinStowPlayer& owner){
    // AndCheckStatus alone returns false when the local player is unavailable.
    // Match its native local index to the current typed render/player owner.
    return "if type(TppMission)~='table' or type(TppMission.IsHelicopterSpace)~='function' "
        "or type(vars)~='table' or type(vars.missionCode)~='number' or not TppMission.IsHelicopterSpace(vars.missionCode) "
        "or type(TppSequence)~='table' or type(TppSequence.GetCurrentSequenceName)~='function' "
        "or TppSequence.GetCurrentSequenceName()~='Seq_Game_MainGame' "
        "or type(PlayerInfo)~='table' or type(PlayerInfo.GetLocalPlayerIndex)~='function' "
        "or type(PlayerInfo.AndCheckStatus)~='function' or type(PlayerStatus)~='table' "
        "or type(PlayerStatus.SUBJECT)~='number' then return 'unknown owner' end "
        "local playerIndex=PlayerInfo.GetLocalPlayerIndex(); if type(playerIndex)~='number' or playerIndex~="
        +std::to_string(owner.index)+" then return 'different owner' end "
        "local subject=PlayerInfo.AndCheckStatus{PlayerStatus.SUBJECT}; "
        "if type(subject)~='boolean' then return 'unknown owner' end ";
}
MenuMaskRead menuInputMask(uintptr_t object){
    MenuMaskRead value;SIZE_T bytes{};
    if(!object||!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(object),&value.type,sizeof(value.type),&bytes)
        ||bytes!=sizeof(value.type)||(value.type!=imageBase+0x227d870&&value.type!=imageBase+0x2548660))return value;
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(object+0x18),&value.count,sizeof(value.count),&bytes)
        ||bytes!=sizeof(value.count))return value;
    uintptr_t after{};
    value.valid=ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(object),&after,sizeof(after),&bytes)
        &&bytes==sizeof(after)&&after==value.type;
    return value;
}
bool focusedWeaponOwnsUiTrigger(void* object,uint32_t index,uint32_t bit){
    // The supported XInput converter sets FOX bits 4/5 for LT/RT at
    // 0x350ec/0x350fa. Only those UI queries are withheld. The player's
    // separate native trigger path still sees the firearm's aim/fire packet.
    return index==0&&(bit==4||bit==5)&&handheldFocusedWeapon()
        &&handheldMenuControl()==HandheldMenuControl::menu&&controllerRigEnabled()
        &&nativeIdroidOpen()&&!nativePauseMenuOpen()&&!nativeGamepadActive()
        &&reinterpret_cast<uintptr_t>(object)==imageBase+0x2bff868
        &&menuInputMask(reinterpret_cast<uintptr_t>(object)).valid;
}
bool observeUiButton(void* object,uint32_t index,uint32_t bit,bool bypass,uintptr_t caller,UiButton getter,bool edge){
    const auto owner=reinterpret_cast<uintptr_t>(object);
    // Explicit, expiring diagnostics also cover fresh-start menus such as
    // native Terms of Use. Those use the named-action edge getter rather
    // than the held getter used by the iDroid.
    const bool tracing=steadyMilliseconds()<menuTraceUntil.load();
    const auto xrBefore=tracing?controlInputSnapshot().nativeButtons:uint16_t{};
    // Observe only an armed physical edge before the native query. Idle
    // queries do not pay for an owner walk. Missing pre-state stays unknown.
    const auto before=xrBefore?menuInputMask(owner):MenuMaskRead{};
    const auto result=getter(object,index,bit,bypass);
    if(tracing&&steadyMilliseconds()<menuTraceUntil.load()){
        if(edge)++menuEdgeCalls;else ++menuButtonCalls;
        const auto audit=controlInputSnapshot();
        if(result||audit.nativeButtons||xrBefore){
            using Pad=void*(*)(uint32_t);
            const auto pad=index<4?reinterpret_cast<Pad>(imageBase+0x1b8a0)(index):nullptr;
            MenuInputRead v{steadyMilliseconds(),index,bit,read<uint32_t>(reinterpret_cast<uintptr_t>(pad)+0x20),
                read<uint32_t>(reinterpret_cast<uintptr_t>(pad)+(edge?0x2c:0x28)),audit.nativeButtons,result,bypass};
            v.object=owner;v.caller=caller;v.xrBefore=xrBefore;v.maskBefore=before;v.maskAfter=menuInputMask(owner);v.edge=edge;
            std::lock_guard lock(menuTraceMutex);
            if(menuInputReads.size()>=128)menuInputReads.pop_front();
            menuInputReads.push_back(v);
        }
    }
    return result;
}
bool uiButton(void* object,uint32_t index,uint32_t bit,bool bypass){
    if(focusedWeaponOwnsUiTrigger(object,index,bit))return false;
    return observeUiButton(object,index,bit,bypass,reinterpret_cast<uintptr_t>(_ReturnAddress()),originalUiButton,false);
}
bool uiEdge(void* object,uint32_t index,uint32_t bit,bool bypass){
    if(focusedWeaponOwnsUiTrigger(object,index,bit))return false;
    const auto native=observeUiButton(object,index,bit,bypass,reinterpret_cast<uintptr_t>(_ReturnAddress()),originalUiEdge,true);
    if(eulaInputScope&&!index&&reinterpret_cast<uintptr_t>(object)==imageBase+0x2bff868)++eulaScopedQueries;
    if(!index&&bit==1&&reinterpret_cast<uintptr_t>(object)==imageBase+0x2bff868
       &&controllerRigEnabled()&&!nativeGamepadActive()){
        NativeIdroidStowHelpRequest request;
        {std::lock_guard stowGuard(stowHelpMutex);request=stowHelpLease.pending(steadyMilliseconds());}
        if(request.owner&&!request.acknowledged
           &&nativeIdroidStowHelpState(request.owner)==NativeIdroidStowHelpState::ready){
            const auto mask=menuInputMask(reinterpret_cast<uintptr_t>(object));
            const auto camera=headCamera().status();
            const auto controls=controlInputSnapshot();
            const auto now=steadyMilliseconds();
            // The exact Help pause can retire the player camera before its
            // UI reads Confirm. It still owns menu input; rendering acceptance
            // waits for the real player/rig return after native acknowledgment.
            const bool helpCamera=camera.active||(camera.awaitingPlayer
                &&camera.reason==HeadCameraStop::playerHeadUnavailable);
            if(mask.valid&&!mask.count&&helpCamera&&!camera.suspended&&camera.nativeMenuOpen
               &&controls.context==ControlContext::menus&&!controls.rigInput&&freshControlInputAudit(controls,now)){
                bool claimed{};
                {std::lock_guard stowGuard(stowHelpMutex);claimed=stowHelpLease.acknowledge(request,now);}
                if(claimed){
                    log(std::string("Requested terminal stow: owned Help native Confirm; original=")+(native?"1":"0"));
                    return true;
                }
            }
        }
    }
    if(!index&&(bit==14||bit==15)&&reinterpret_cast<uintptr_t>(object)==imageBase+0x2bff868
       &&controllerRigEnabled()&&nativePauseMenuOpen()&&nativeScriptedDemoActive()
       &&!nativeIdroidOpen()&&!nativeGamepadActive()){
        const auto mask=menuInputMask(reinterpret_cast<uintptr_t>(object));
        const auto popup=nativePopupSnapshot();
        const auto camera=headCamera().status();
        const auto controls=controlInputSnapshot();
        const auto now=steadyMilliseconds();
        if(mask.valid&&!mask.count&&popup.active&&!*popup.active
           &&camera.active&&!camera.suspended&&camera.nativeMenuOpen
           &&controls.context==ControlContext::menus&&!controls.rigInput&&freshControlInputAudit(controls,now)){
            if(const auto direction=takeCinematicMenuDirection(camera.activation,index,bit,now)){
                if(*direction){
                    std::ostringstream message;
                    message<<"VR cinematic menu direction bit="<<bit<<" activation="<<camera.activation<<" native_result="<<native;
                    log(message.str());
                }
                return *direction;
            }
        }
    }
    return native;
}
bool uiRelease(void* object,uint32_t index,uint32_t bit,bool bypass){
    return !focusedWeaponOwnsUiTrigger(object,index,bit)&&originalUiRelease(object,index,bit,bypass);
}
bool uiRepeat(void* object,uint32_t index,uint32_t bit,double interval,double initialDelay,bool bypass){
    return !focusedWeaponOwnsUiTrigger(object,index,bit)&&originalUiRepeat(object,index,bit,interval,initialDelay,bypass);
}
uint32_t uiRepeatState(void* object,uint32_t index,uint32_t bit,double interval,bool bypass){
    return focusedWeaponOwnsUiTrigger(object,index,bit)?0:originalUiRepeatState(object,index,bit,interval,bypass);
}
bool uiTabAction(uint32_t action){
    const auto native=originalUiTabAction(action);
    const auto callerRva=reinterpret_cast<uintptr_t>(_ReturnAddress())-imageBase;
    const bool rootNext=idroidTabOwner&&callerRva==nativeIdroidNextTabReturnRva;
    const bool nextEdge=rootNext&&idroidTabNativeEdge.update(native);
    if(idroidTabOwner&&steadyMilliseconds()<menuTraceUntil.load()){
        const auto audit=controlInputSnapshot();
        if(audit.nativeButtons==0x200&&callerRva==nativeIdroidNextTabReturnRva){
            static uint64_t last{};
            const auto now=steadyMilliseconds();
            if(now-last>300){
                last=now;
                std::ostringstream trace;
                trace<<"iDroid tab owner="<<std::hex<<idroidTabOwner<<" caller="
                    <<callerRva<<" action="<<action
                    <<" native="<<native<<" common="<<read<uintptr_t>(idroidTabOwner+0x100)
                    <<" phase="<<read<uint32_t>(idroidTabOwner+0x130)<<","<<read<uint32_t>(idroidTabOwner+0x140)
                    <<" tab="<<unsigned(read<uint8_t>(idroidTabOwner+0x17da))
                    <<" control="<<unsigned(handheldMenuControl())<<" ready="<<handheldMenuInputReady();
                log(trace.str());
            }
        }
    }
    // The root tab update has already saved its previous tab at +17d8 before
    // querying this real native RB edge. Select the first tab at the last
    // boundary and consume that edge. The remainder of the original update
    // performs its normal page transition, animation and sound exactly once.
    // Native IDs/order: Mother Base=1, Map=3, Missions=2. The SECOND native
    // query supplies next-tab (dil); the first query supplies previous (r14b).
    // IdroidTabInput already makes one short pulse per physical squeeze. The
    // native action query owns its edge: never manufacture another event from
    // that pulse's XR audit after the game has already advanced to Missions.
    if(native&&!idroidTabWrapped&&rootNext
       &&controllerRigEnabled()&&handheldMenusSelected()&&nativeIdroidOpen()
       &&!nativePauseMenuOpen()&&!nativeGamepadActive()
       &&handheldMenuControl()==HandheldMenuControl::menu&&handheldMenuInputReady()){
        if(!nextEdge)return false;
        const auto tutorial=nativeIdroidTutorialMode();
        const auto popup=nativePopupSnapshot();
        const auto ui=read<uintptr_t>(imageBase+0x2bf1518);
        // Read common state from the tab task currently executing, then
        // validate that task's native root-page and input-ready fields.
        const auto common=read<uintptr_t>(idroidTabOwner+0x100);
        const auto wrapTarget=nativeIdroidRootNextWrapTarget(callerRva,read<uint8_t>(idroidTabOwner+0x17da),nextEdge);
        using Busy=bool(*)();
        if(tutorial&&*tutorial==0&&popup.active&&!*popup.active
           &&read<uintptr_t>(ui)==imageBase+0x2242d78&&common
           &&read<uint8_t>(common+0x820)!=0
           &&wrapTarget
           &&read<uint32_t>(idroidTabOwner+0x130)==0x24
           &&read<uint32_t>(idroidTabOwner+0x140)==7
           &&read<uint8_t>(common+0xc96)==3&&read<uint8_t>(common+0xc78)==0
           &&!reinterpret_cast<Busy>(imageBase+0x91ed80)()){
            *reinterpret_cast<uint8_t*>(idroidTabOwner+0x17da)=wrapTarget;
            // This native reset normally follows the root-tab mutation.
            // Our boundary change precedes the update's local old-tab copy,
            // so invoke its exact reset here before the normal transition.
            using Reset=void(*)(void*);
            reinterpret_cast<Reset>(imageBase+0x91ed20)(reinterpret_cast<void*>(common));
            idroidTabWrapped=true;
            log("Held-focus iDroid next tab wrapped Missions to Mother Base");
            return false;
        }
    }
    return native;
}
void eulaUpdate(void* object){
    const auto owner=reinterpret_cast<uintptr_t>(object);
    EulaRead value;value.time=steadyMilliseconds();value.owner=owner;value.type=read<uintptr_t>(owner);
    value.control=read<uintptr_t>(owner+0x100);
    // The live update owner supplies membership; the exact recovered update
    // slot and retail control type must both match before reading its fields.
    const bool updateSlot=value.type==imageBase+0x224c680
        &&read<uintptr_t>(value.type+0x80)==imageBase+0x89ccf0;
    if(updateSlot&&read<uintptr_t>(value.control)==imageBase+0x227a050){
        value.active=read<uint8_t>(owner+0xc0);value.readToEnd=read<uint8_t>(owner+0xc2);
        value.selectorVisible=read<uint8_t>(owner+0xc3);value.phase=read<uint32_t>(owner+0xdc);
        value.selection=read<uint32_t>(owner+0xe0);value.line=read<uint32_t>(owner+0xe4);
        value.lines=read<uint32_t>(owner+0xd8);
        value.flags=read<uint8_t>(value.control+0x38e0);value.mode=read<uint8_t>(value.control+0x38e1);
        value.valid=read<uintptr_t>(owner)==value.type&&read<uintptr_t>(owner+0x100)==value.control
            &&read<uintptr_t>(value.control)==imageBase+0x227a050;
    }
    const auto controls=controlInputSnapshot();
    // Snapshot the producer before reading the observation clock. A newer
    // XR publication must not look like future input halfway through a read.
    value.time=steadyMilliseconds();value.inputTime=controls.time;
    value.buttons=controls.nativeButtons;value.context=controls.context;
    const bool route=value.valid&&value.active==1&&value.phase==5&&controllerRigEnabled()
        &&!nativeGamepadActive()&&controls.context==ControlContext::menus
        &&freshControlInputAudit(controls,value.time);
    NativeTermsMenu observed;
    observed.time=value.time;observed.owner=owner;observed.verified=value.valid;
    observed.active=value.active==1;observed.phase=value.phase;observed.mode=value.mode;
    if(value.valid&&value.active==1&&value.phase==5){
        observed.horizontal=read<uint32_t>(owner+0x9e8c);
        observed.vertical=read<uint32_t>(owner+0xa04c);
        // The retail selector resolves the same row/column to its authored
        // action; observing it does not select or accept an agreement.
        if(observed.horizontal<=1&&observed.vertical<=3){
            observed.action=read<uint32_t>(owner+0x188+observed.vertical*4);
            if(!observed.action)observed.action=read<uint32_t>(owner+0x188+(observed.vertical+observed.horizontal)*4);
        }
    }
    publishNativeTermsMenu(observed);
    value.route=route;
    const auto previousScope=eulaInputScope;eulaInputScope=route;
    originalEulaUpdate(object);
    eulaInputScope=previousScope;
    if(steadyMilliseconds()<menuTraceUntil.load()){
        std::lock_guard lock(menuTraceMutex);
        if(eulaReads.empty()||eulaReads.back().owner!=value.owner||eulaReads.back().buttons!=value.buttons
           ||eulaReads.back().route!=value.route||eulaReads.back().phase!=value.phase){
            if(eulaReads.size()>=64)eulaReads.pop_front();eulaReads.push_back(value);
        }
        eulaRead=value;
    }
}
bool uiClose(void* object){
    const auto result=originalUiClose(object);
    if(steadyMilliseconds()<menuTraceUntil.load())++menuCloseCalls;
    return result;
}
bool uiInputMasked(void* object){
    const bool native=originalUiInputMasked(object);
    if(native||!controllerRigEnabled()||!handheldMenusSelected()||!nativeIdroidOpen()
       ||nativePauseMenuOpen()||nativeGamepadActive())return native;
    const auto owner=reinterpret_cast<uintptr_t>(object);
    const auto type=read<uintptr_t>(owner);
    // The retail singleton uses the concrete UiPad type in ordinary gameplay
    // and its base type during other UI lifecycles. Both implement this same
    // verified mask slot. Requiring only the base type left map input live.
    if(owner!=imageBase+0x2bff868
       ||(type!=imageBase+0x227d870&&type!=imageBase+0x2548660))return native;
    const auto control=handheldMenuControl();
    if(!handheldUiInputBlocked(control,handheldMenuInputReady()))return native;
    ++handheldUiBlockedQueries;
    return true;
}
void uiDisable(void* object,uint8_t index,bool disabled){
    const auto owner=reinterpret_cast<uintptr_t>(object);
    const auto mode=nativeIdroidTutorialMode();
    const auto before=read<uint8_t>(owner+index*40+8);
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    if(index<78&&read<uintptr_t>(owner)==imageBase+0x227ee50){
        std::lock_guard lock(menuTraceMutex);
        const auto ui=read<uintptr_t>(imageBase+0x2bf1518);
        const auto tutorial=read<uintptr_t>(ui+0x90);
        const bool typed=read<uintptr_t>(ui)==imageBase+0x2242d78
            &&read<uintptr_t>(tutorial)==imageBase+0x2272810;
        // Verified GuiMbTutorial begin disables all entries before assigning
        // the mode. Its FOB handler then permits entries 33/72. Observe the
        // begin's prior values, not the already-disabled mode-10 snapshot.
        if(typed&&disabled&&caller==imageBase+0xf4285b)
            tutorialMenuLease.beginEntry(owner,tutorial,index,(before&0x40)!=0);
        else if(disabled)tutorialMenuLease.independentDisable(owner,index);
        originalUiDisable(object,index,disabled);
        if(steadyMilliseconds()<menuTraceUntil.load()){
            if(menuDisableReads.size()>=128)menuDisableReads.pop_front();
            menuDisableReads.push_back({owner,caller,mode.value_or(255),index,before,read<uint8_t>(owner+index*40+8),disabled});
        }
    }else originalUiDisable(object,index,disabled);
}
bool restoreCompletedGuideRestrictions(){
    const auto mode=nativeIdroidTutorialMode();
    if(!menuRestrictionAbi||!originalUiDisable||!mode||*mode!=0||nativePauseMenuOpen())return false;
    const auto ui=read<uintptr_t>(imageBase+0x2bf1518);
    const auto tutorial=read<uintptr_t>(ui+0x90);
    const auto common=read<uintptr_t>(ui+0x70);
    const auto menu=read<uintptr_t>(common+0xca0);
    if(read<uintptr_t>(ui)!=imageBase+0x2242d78||read<uintptr_t>(tutorial)!=imageBase+0x2272810
       ||read<uintptr_t>(menu)!=imageBase+0x227ee50)return false;
    std::lock_guard lock(menuTraceMutex);
    if(!tutorialMenuLease.ready(menu,tutorial))return false;
    for(unsigned index=0;index<NativeMenuRestrictionLease::entries;++index){
        const auto prior=tutorialMenuLease.prior(index);
        if(prior)originalUiDisable(reinterpret_cast<void*>(menu),static_cast<uint8_t>(index),*prior);
    }
    tutorialMenuLease.reset();return true;
}
bool haveGuideRestrictionLease(){
    if(!menuRestrictionAbi||!originalUiDisable)return false;
    const auto ui=read<uintptr_t>(imageBase+0x2bf1518);
    const auto tutorial=read<uintptr_t>(ui+0x90);
    const auto common=read<uintptr_t>(ui+0x70);
    const auto menu=read<uintptr_t>(common+0xca0);
    if(read<uintptr_t>(ui)!=imageBase+0x2242d78||read<uintptr_t>(tutorial)!=imageBase+0x2272810
       ||read<uintptr_t>(menu)!=imageBase+0x227ee50)return false;
    std::lock_guard lock(menuTraceMutex);return tutorialMenuLease.ready(menu,tutorial);
}
void* send(void* context,int32_t* result,uint32_t object,void* command){
    if((object>>9)==19&&GetTickCount64()<traceUntil.load()){
        const auto bytes=read<std::array<unsigned char,64>>(reinterpret_cast<uintptr_t>(command));
        uint64_t hash{};std::memcpy(&hash,bytes.data(),8);
        std::lock_guard lock(traceMutex);
        if(traced.size()<128&&traced.insert(hash).second){
            std::ostringstream s;s<<"Native D-Dog command="<<std::hex<<hash<<" caller="
                <<reinterpret_cast<uintptr_t>(_ReturnAddress())<<" bytes=";
            constexpr char digits[]="0123456789abcdef";
            for(auto b:bytes)s<<digits[b>>4]<<digits[b&15];
            log(s.str());
        }
    }
    return originalSend(context,result,object,command);
}
void traceCommands(){
    std::error_code error;
    if(!std::filesystem::exists(tracePath,error))return;
    if(!originalSend){
        const auto services=read<uintptr_t>(read<uintptr_t>(imageBase+0x2c3a8a0)+8);
        const auto objects=read<uintptr_t>(services+0x60);
        const auto function=read<uintptr_t>(read<uintptr_t>(objects)+0x38);
        if(function<imageBase+0x1000||function>=imageBase+0x2090000)return;
        auto* address=reinterpret_cast<void*>(function);
        if(MH_CreateHook(address,reinterpret_cast<void*>(&send),reinterpret_cast<void**>(&originalSend))!=MH_OK
           ||MH_EnableHook(address)!=MH_OK)return;
    }
    {std::lock_guard lock(traceMutex);traced.clear();}
    traceUntil.store(GetTickCount64()+15000);
    std::filesystem::remove(tracePath,error);
    log("Native D-Dog command observation enabled for 15 seconds");
}
bool readPipe(HANDLE pipe,void* output,uint32_t length){
    auto* bytes=static_cast<unsigned char*>(output);uint32_t offset{};
    while(offset<length){DWORD read{};if(!ReadFile(pipe,bytes+offset,length-offset,&read,nullptr)||!read)return false;offset+=read;}
    return true;
}
bool writePipe(HANDLE pipe,const void* input,uint32_t length){
    const auto* bytes=static_cast<const unsigned char*>(input);uint32_t offset{};
    while(offset<length){DWORD written{};if(!WriteFile(pipe,bytes+offset,length-offset,&written,nullptr)||!written)return false;offset+=written;}
    return true;
}
void closeConnection(const std::shared_ptr<ActionConnection>& connection){
    if(!connection)return;
    std::lock_guard lock(connection->writeMutex);
    if(!connection->open.exchange(false))return;
    if(connection->pipe!=INVALID_HANDLE_VALUE){
        DisconnectNamedPipe(connection->pipe);CloseHandle(connection->pipe);connection->pipe=INVALID_HANDLE_VALUE;
    }
}
void wakeActionPipe(){
    const auto pipe=CreateFileW(actionPipeName,GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr);
    if(pipe!=INVALID_HANDLE_VALUE)CloseHandle(pipe);
}
bool publishFastInput(const std::string& request,std::string& result);
void sendFastResponse(const ActionRequest& request,int32_t status,
    uint64_t queuedMilliseconds,uint64_t executionMicroseconds,const std::string& result);
bool flushFastResponse(const ActionRequest& request){
    const auto& connection=request.connection;
    std::unique_lock lock(connection->writeMutex);
    const auto ready=connection->responseReady.wait_for(lock,std::chrono::seconds(15),[&]{
        return connection->hasResponse||actionPipeStopping.load()||!connection->open.load();
    });
    if(actionPipeStopping.load()||!connection->open.load())return false;
    if(!ready){
        request.cancelled->store(true);
        connection->responseText="native action timed out waiting for Lua transaction";
        connection->response={request.id,4,static_cast<uint32_t>(connection->responseText.size()),15000,0};
    }
    const auto header=connection->response;
    auto body=std::move(connection->responseText);
    connection->hasResponse=false;
    lock.unlock();
    // This pipe was opened synchronously. Only its I/O thread may read/write
    // it: writing from the game's Lua thread while ReadFile waits for another
    // request deadlocks the native update until the client disconnects.
    return writePipe(connection->pipe,&header,sizeof(header))
        &&writePipe(connection->pipe,body.data(),header.length);
}
void actionPipeLoop(){
    while(!actionPipeStopping.load()){
        const auto pipe=CreateNamedPipeW(actionPipeName,PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT,PIPE_UNLIMITED_INSTANCES,1<<20,1<<20,0,nullptr);
        if(pipe==INVALID_HANDLE_VALUE){log("Native action pipe creation failed error="+std::to_string(GetLastError()));std::this_thread::yield();continue;}
        auto connection=std::make_shared<ActionConnection>();connection->pipe=pipe;
        const bool connected=ConnectNamedPipe(pipe,nullptr)||GetLastError()==ERROR_PIPE_CONNECTED;
        if(!connected){CloseHandle(pipe);continue;}
        {std::lock_guard lock(connectionMutex);activeConnection=connection;}
        log("Native action pipe connected");
        while(!actionPipeStopping.load()&&connection->open.load()){
            PipeRequestHeader header{};
            if(!readPipe(pipe,&header,sizeof(header)))break;
            if(!header.length||header.length>maxPipeScript)break;
            ActionRequest request;request.id=header.id;request.script.resize(header.length);
            if(!readPipe(pipe,request.script.data(),header.length))break;
            request.queuedAt=std::chrono::steady_clock::now();request.connection=connection;
            std::string immediateResult;
            if(publishFastInput(request.script,immediateResult)){
                const auto finished=std::chrono::steady_clock::now();
                const auto execution=std::chrono::duration_cast<std::chrono::microseconds>(finished-request.queuedAt).count();
                const auto status=immediateResult.rfind("unknown input:",0)==0?1:0;
                sendFastResponse(request,status,0,static_cast<uint64_t>(std::max<int64_t>(0,execution)),immediateResult);
                log("Native fast input completed id="+std::to_string(request.id)+" exec_us="+std::to_string(execution));
                if(!flushFastResponse(request))break;
                continue;
            }
            bool queued=false;
            {std::lock_guard lock(fastQueueMutex);
                if(fastQueue.size()<maxQueuedActions){fastQueue.push_back(request);queued=true;}}
            if(queued)log("Native action pipe queued id="+std::to_string(header.id)+" bytes="+std::to_string(header.length));
            if(!queued){
                const std::string result="action queue full";
                sendFastResponse(request,3,0,0,result);
            }
            if(!flushFastResponse(request))break;
        }
        {std::lock_guard lock(connectionMutex);if(activeConnection==connection)activeConnection.reset();}
        {
            std::lock_guard lock(fastQueueMutex);
            fastQueue.erase(std::remove_if(fastQueue.begin(),fastQueue.end(),
                [&](const ActionRequest& request){return request.connection==connection;}),fastQueue.end());
        }
        closeConnection(connection);log("Native action pipe disconnected");
    }
}
void sendFastResponse(const ActionRequest& request,int32_t status,
    uint64_t queuedMilliseconds,uint64_t executionMicroseconds,const std::string& result){
    const auto connection=request.connection;if(!connection||!connection->open.load())return;
    const PipeResponseHeader response{request.id,status,static_cast<uint32_t>(std::min<size_t>(result.size(),maxPipeScript)),
        queuedMilliseconds,executionMicroseconds};
    std::lock_guard lock(connection->writeMutex);
    if(!connection->open.load()||connection->pipe==INVALID_HANDLE_VALUE)return;
    if(request.cancelled->load())return;
    connection->response=response;
    connection->responseText=result.substr(0,response.length);
    connection->hasResponse=true;
    connection->responseReady.notify_one();
}
bool luaReady(void* state){
    const int top=getTop(state);constexpr char check[]="return type(TppMain)=='table' and type(GameObject)=='table' and type(gvars)=='table' and type(vars)=='table' and type(TppMission)=='table' and type(TppBuddyService)=='table' and 'ready' or 'wait'";
    int status=load(state,check,sizeof(check)-1,"@mgs5vr-state-check");if(!status)status=original(state,0,1,0);
    size_t length{};const auto* value=getString(state,-1,&length);const bool ready=!status&&value&&length==5&&std::memcmp(value,"ready",5)==0;
    setTop(state,top);nativeLuaReady.store(ready);return ready;
}
bool publishFastInput(const std::string& request,std::string& result){
    if(request=="inspect-ui-draws"||request=="diagnostics:ui-draws:reset"){
        result=inspectUiDrawInventory(request.ends_with(":reset"));return true;
    }
    constexpr std::string_view coverPrefix="diagnostics:cover-probe:";
    constexpr std::string_view coverInspectPrefix="inspect-cover-probe:";
    if(request.starts_with(coverPrefix)||request.starts_with(coverInspectPrefix)){
        const auto now=steadyMilliseconds();bool accepted{},parsed{};uint64_t nonce{};
        if(request=="diagnostics:cover-probe:off"){clearCoverProbe();accepted=parsed=true;}
        else if(request.starts_with(coverPrefix)){
            if(const auto arguments=parseCoverProbeRequest(std::string_view(request).substr(coverPrefix.size()))){
                parsed=true;nonce=arguments->nonce;accepted=leaseCoverProbe(*arguments,now);
            }
        }else {
            const auto argument=std::string_view(request).substr(coverInspectPrefix.size());
            if(!argument.empty()&&argument.size()<=20){
                const auto value=std::from_chars(argument.data(),argument.data()+argument.size(),nonce);
                parsed=value.ec==std::errc{}&&value.ptr==argument.data()+argument.size()&&nonce;
            }
        }
        auto cover=coverProbeSnapshot(now,nonce);
        if(!parsed){cover={};cover.reason=CoverProbeReason::request;}
        else if(request.starts_with(coverPrefix)&&!accepted){
            cover={};cover.request.nonce=nonce;cover.reason=CoverProbeReason::lease;
        }
        const auto vector=[](std::ostream& out,Vec3 value){out<<'['<<value.x<<','<<value.y<<','<<value.z<<']';};
        std::ostringstream out;out<<std::boolalpha<<std::setprecision(std::numeric_limits<float>::max_digits10);
        out<<"{\"schema\":1,\"source\":\"native_marker_job_terrain_obstruction\",\"scope\":\"person_marker_points_to_candidate_eye;not_enemy_ai_concealment\""
            <<",\"parsed\":"<<parsed<<",\"accepted\":"<<accepted<<",\"available\":"<<cover.available
            <<",\"reason\":\""<<coverProbeReasonName(cover.reason)<<"\",\"reason_code\":"<<static_cast<unsigned>(cover.reason)
            <<",\"nonce\":"<<cover.request.nonce<<",\"now_ms\":"<<now<<",\"issued_ms\":"<<cover.request.issuedMs
            <<",\"expires_ms\":"<<cover.request.expiresMs<<",\"observed_player_ms\":"<<cover.request.observedMs
            <<",\"sample_ms\":"<<cover.sampleMs<<",\"frame_sample_ms\":"<<cover.frame.sampleMs
            <<",\"tracking_sequence\":"<<cover.frame.trackingSequence
            <<",\"activation\":"<<cover.frame.owner.activation<<",\"player_owner\":"<<cover.frame.owner.playerOwner
            <<",\"presentation_epoch\":"<<cover.frame.owner.presentationEpoch
            <<",\"requested_owner\":{\"activation\":"<<cover.request.owner.activation
            <<",\"player_owner\":"<<cover.request.owner.playerOwner<<",\"presentation_epoch\":"<<cover.request.owner.presentationEpoch
            <<"},\"observed_player\":";vector(out,cover.request.observedPlayer);
        out<<",\"native_eye\":";vector(out,cover.frame.nativeEye);
        out<<",\"eye_offset\":";vector(out,cover.eyeOffset);
        out<<",\"requested_candidates\":"<<cover.request.count<<",\"persons\":[";
        for(size_t i=0;i<cover.personCount;++i){const auto& person=cover.persons[i];if(i)out<<',';
            out<<"{\"id\":"<<person.id<<",\"marker_record_index\":"<<person.recordIndex
                <<",\"native_flags\":"<<static_cast<unsigned>(person.flags)<<",\"distance_from_player\":"<<person.distance<<",\"marker\":";
            vector(out,person.marker);out<<'}';
        }
        out<<"],\"candidates\":[";
        for(size_t i=0;i<cover.count;++i){const auto& candidate=cover.candidates[i];if(i)out<<',';
            out<<"{\"node\":"<<candidate.node<<",\"point\":";vector(out,candidate.point);
            out<<",\"eye\":";vector(out,candidate.eye);
            out<<",\"all_marker_rays_obstructed\":"<<candidate.allMarkerRaysObstructed<<",\"rays\":[";
            for(size_t j=0;j<candidate.count;++j){const auto& ray=candidate.rays[j];if(j)out<<',';
                out<<"{\"actor_id\":"<<ray.actor<<",\"marker\":";vector(out,ray.marker);
                out<<",\"endpoint\":";vector(out,ray.endpoint);
                out<<",\"distance\":"<<ray.distance<<",\"surface_distance\":"<<ray.surfaceDistance
                    <<",\"query_valid\":"<<ray.queryValid<<",\"query_reason\":"<<static_cast<unsigned>(ray.queryReason)
                    <<",\"terrain_obstructed\":"<<ray.terrainObstructed<<",\"ray_result\":"<<ray.rayResult
                    <<",\"hit_count\":"<<ray.hitCount<<",\"hit_index\":"<<ray.hitIndex<<",\"raw_surface\":";
                vector(out,ray.rawSurface);out<<",\"raw_along\":"<<ray.rawAlong<<",\"raw_lateral_squared\":"<<ray.rawLateralSquared<<'}';
            }
            out<<"]}";
        }
        out<<"]}";result=out.str();return true;
    }
    constexpr std::string_view combatPrefix="diagnostics:combat-sight:";
    if(request=="inspect-optic-markers"){
        const auto markers=opticWaypoints();const auto now=steadyMilliseconds();
        const auto status=headCamera().status();
        const bool available=status.active&&markers.activation==status.activation
            &&now>=markers.sampleTime&&now-markers.sampleTime<=150;
        std::ostringstream out;out<<std::boolalpha<<"{\"source\":\"native_acquired_marker_records\",\"available\":"<<available
            <<",\"now_ms\":"<<now<<",\"sample_ms\":"<<markers.sampleTime<<",\"activation\":"<<markers.activation<<",\"points\":[";
        if(available)for(size_t i=0;i<markers.count;++i){const auto& point=markers.points[i];if(i)out<<',';
            out<<"{\"actor\":"<<point.actor<<",\"letter\":"<<unsigned(point.letter)
                <<",\"position\":["<<point.position.x<<','<<point.position.y<<','<<point.position.z<<"]}";}
        out<<"]}";result=out.str();return true;
    }
    if(request.starts_with(combatPrefix)||request=="inspect-combat-sight"){
        const auto now=steadyMilliseconds();bool armed=false;
        if(request.starts_with(combatPrefix)){
            const auto argument=std::string_view(request).substr(combatPrefix.size());uint64_t duration{};
            const auto separator=argument.find(':');const auto lease=argument.substr(0,separator);
            const auto parsed=std::from_chars(lease.data(),lease.data()+lease.size(),duration);
            uint16_t marker=0xffff;bool targetValid=true;
            if(separator!=std::string_view::npos){
                const auto target=argument.substr(separator+1);unsigned value{};
                const auto identity=std::from_chars(target.data(),target.data()+target.size(),value);
                targetValid=identity.ec==std::errc{}&&identity.ptr==target.data()+target.size()&&value>0&&value<0xffff;
                if(targetValid)marker=static_cast<uint16_t>(value);
            }
            if(parsed.ec==std::errc{}&&parsed.ptr==lease.data()+lease.size()&&targetValid)armed=leaseCombatSight(now,duration,marker);
        }
        const auto sight=combatSightSnapshot();const auto expiry=combatSightLeaseExpiry();
        std::ostringstream out;out<<std::boolalpha<<"{\"schema\":1,\"source\":\"native_marker_job_world_visibility\",\"armed\":"<<armed
            <<",\"enabled\":"<<(expiry>now)<<",\"now_ms\":"<<now<<",\"expires_ms\":"<<expiry
            <<",\"sample_ms\":"<<sight.sampleTime<<",\"activation\":"<<sight.activation
            <<",\"presentation_epoch\":"<<sight.presentationEpoch<<",\"player_owner\":"<<sight.playerOwner
            <<",\"tracking_sequence\":"<<sight.trackingSequence<<",\"muzzle_tracked\":"<<sight.muzzleTracked
            <<",\"muzzle\":{\"position\":["<<sight.muzzle.position.x<<','<<sight.muzzle.position.y<<','<<sight.muzzle.position.z
            <<"],\"orientation\":["<<sight.muzzle.orientation.x<<','<<sight.muzzle.orientation.y<<','<<sight.muzzle.orientation.z<<','<<sight.muzzle.orientation.w
            <<"]},\"requested_marker\":{\"id\":"<<sight.requestedMarker.id<<",\"found\":"<<sight.requestedMarker.found
            <<",\"kind\":"<<unsigned(sight.requestedMarker.kind)<<",\"flags\":"<<unsigned(sight.requestedMarker.flags)
            <<",\"position\":["<<sight.requestedMarker.position.x<<','<<sight.requestedMarker.position.y<<','<<sight.requestedMarker.position.z
            <<"]},\"candidates\":"<<sight.candidates<<",\"targets\":[";
        for(size_t i=0;i<sight.count;++i){const auto& target=sight.targets[i];if(i)out<<',';
            out<<"{\"id\":"<<target.id<<",\"marker\":["<<target.marker.x<<','<<target.marker.y<<','<<target.marker.z
                <<"],\"aim\":["<<target.aim.x<<','<<target.aim.y<<','<<target.aim.z<<"],\"distance\":"<<target.distance
                <<",\"surface_distance\":"<<target.surfaceDistance<<",\"query_valid\":"<<target.queryValid
                <<",\"unoccluded\":"<<target.unoccluded<<",\"aim_kind\":"<<static_cast<unsigned>(target.aimKind)
                <<",\"query_reason\":"<<static_cast<unsigned>(target.queryReason)
                <<",\"ray_result\":"<<target.rayResult<<",\"hit_count\":"<<target.hitCount<<",\"hit_index\":"<<target.hitIndex
                <<",\"raw_surface\":["<<target.rawSurface.x<<','<<target.rawSurface.y<<','<<target.rawSurface.z
                <<"],\"raw_along\":"<<target.rawAlong<<",\"raw_lateral_squared\":"<<target.rawLateralSquared
                <<",\"hit_transform_read\":"<<target.hitTransformRead<<",\"hit_transform\":[";
            for(size_t j=0;j<target.hitTransform.size();++j){if(j)out<<',';out<<target.hitTransform[j];}
            out<<"],\"hit_record_words\":[";
            for(size_t j=0;j<target.hitRecordWords.size();++j){if(j)out<<',';out<<target.hitRecordWords[j];}
            out<<"]}";}
        out<<"]}";result=out.str();return true;
    }
    constexpr std::string_view faultPrefix="diagnostics:controller-tracking-loss:";
    constexpr std::string_view headFaultPrefix="diagnostics:head-tracking-loss:";
    const bool headFault=request.starts_with(headFaultPrefix);
    if(headFault||request.starts_with(faultPrefix)){
        auto& probe=headFault?headTrackingFaultProbe():controllerTrackingFaultProbe();bool armed=false;
        const auto arguments=std::string_view(request).substr(headFault?headFaultPrefix.size():faultPrefix.size());
        if(arguments=="off"){probe.clear();armed=true;}
        else if(const auto separator=arguments.find(':');separator!=std::string_view::npos){
            unsigned mask{};uint64_t duration{};
            const auto first=std::from_chars(arguments.data(),arguments.data()+separator,mask);
            const auto last=std::from_chars(arguments.data()+separator+1,arguments.data()+arguments.size(),duration);
            const auto status=headCamera().status();
            if(first.ec==std::errc{}&&first.ptr==arguments.data()+separator&&last.ec==std::errc{}
               &&last.ptr==arguments.data()+arguments.size()&&status.active&&!status.suspended)
                armed=probe.arm(mask,duration,steadyMilliseconds());
        }
        result=std::string("{\"schema\":1,\"synthetic_tracking_fault\":true,\"enabled\":")
            +(probe.enabled()?"true":"false")+",\"armed\":"+(armed?"true":"false")
            +",\"mask\":"+std::to_string(probe.mask(steadyMilliseconds()))+"}";
        return true;
    }
    if(request=="inspect-native-exposure"){result=renderCameraExposureDiagnostics();return true;}
    if(request=="diagnostics:scope-exposure-isolation:on"||request=="diagnostics:scope-exposure-isolation:off"){
        if(!setRenderCameraExposureIsolation(request.ends_with(":on")))
            result="unsupported scope exposure diagnostic build";
        else result=renderCameraExposureDiagnostics();
        return true;
    }
    if(request=="diagnostics:optic-lens-render:on"||request=="diagnostics:optic-lens-render:off"){
        if(!setRenderCameraLensRendering(request.ends_with(":on")))
            result="unsupported optic lens diagnostic build";
        else result=renderCameraExposureDiagnostics();
        return true;
    }
    if(request=="trace-native-menu-input"||request=="inspect-native-menu-input"){
        if(request=="trace-native-menu-input"){
            std::lock_guard lock(menuTraceMutex);menuInputReads.clear();menuDisableReads.clear();eulaReads.clear();eulaRead={};
            menuTraceImageSize=0;
            const auto dos=read<IMAGE_DOS_HEADER>(imageBase);
            if(dos.e_magic==IMAGE_DOS_SIGNATURE&&dos.e_lfanew>0&&dos.e_lfanew<0x100000){
                const auto nt=read<IMAGE_NT_HEADERS64>(imageBase+dos.e_lfanew);
                if(nt.Signature==IMAGE_NT_SIGNATURE&&nt.OptionalHeader.Magic==IMAGE_NT_OPTIONAL_HDR64_MAGIC)
                    menuTraceImageSize=nt.OptionalHeader.SizeOfImage;
            }
            menuButtonCalls=0;menuEdgeCalls=0;menuCloseCalls=0;menuTraceUntil=steadyMilliseconds()+15000;
        }
        const auto ui=read<uintptr_t>(imageBase+0x2bf1518);
        const auto terminal=read<uintptr_t>(ui+0x7c0);
        const auto tutorial=read<uintptr_t>(ui+0x90);
        const bool valid=read<uintptr_t>(ui)==imageBase+0x2242d78
            &&read<uintptr_t>(terminal)==imageBase+0x22705a8
            &&read<uintptr_t>(tutorial)==imageBase+0x2272810;
        std::ostringstream out;out<<std::boolalpha;
        out<<"{\"schema\":1,\"verified\":"<<valid<<",\"installed\":"<<(originalUiButton&&originalUiClose)
           <<",\"armed\":"<<(steadyMilliseconds()<menuTraceUntil.load())
           <<",\"button_calls\":"<<menuButtonCalls.load()<<",\"edge_calls\":"<<menuEdgeCalls.load()
           <<",\"edge_installed\":"<<(originalUiEdge!=nullptr)<<",\"close_getter_calls\":"<<menuCloseCalls.load();
        if(valid)out<<",\"terminal_open\":"<<unsigned(read<uint8_t>(terminal+0x20))
            <<",\"deferred_close\":"<<unsigned(read<uint8_t>(terminal+0x25))
            <<",\"active_tutorial_mode\":"<<unsigned(read<uint8_t>(tutorial+8));
        out<<",\"reads\":[";
        std::lock_guard lock(menuTraceMutex);bool first=true;
        for(const auto& v:menuInputReads){if(!first)out<<',';first=false;
            out<<"{\"ms\":"<<v.time<<",\"index\":"<<v.index<<",\"bit\":"<<v.bit
               <<",\"held\":"<<v.held<<",\"pressed\":"<<v.pressed<<",\"xr\":"<<v.xr
               <<",\"result\":"<<v.result<<",\"bypass\":"<<v.bypass<<",\"edge\":"<<v.edge
               <<",\"object\":"<<v.object<<",\"caller\":"<<v.caller<<",\"caller_rva\":";
            if(v.caller>=imageBase&&v.caller-imageBase<menuTraceImageSize.load())out<<v.caller-imageBase;else out<<"null";
            out<<",\"xr_before\":"<<v.xrBefore;
            const auto mask=[&](const char* name,const MenuMaskRead& value){
                out<<",\""<<name<<"\":{\"verified\":"<<value.valid<<",\"vtable\":"<<value.type<<",\"count\":";
                if(value.valid)out<<value.count;else out<<"null";
                out<<",\"masked\":";if(value.valid)out<<(value.count>0);else out<<"null";out<<'}';
            };
            mask("mask_before",v.maskBefore);mask("mask_after",v.maskAfter);
            out<<",\"mask_owner_type_stable\":"<<(v.maskBefore.valid&&v.maskAfter.valid&&v.maskBefore.type==v.maskAfter.type)
               <<",\"snapshot_atomic\":false}";
        }
        out<<"],\"disabled_entries\":[";first=true;
        for(const auto& v:menuDisableReads){if(!first)out<<',';first=false;
            out<<"{\"owner\":"<<v.owner<<",\"caller_rva\":"<<(v.caller-imageBase)<<",\"mode\":"<<v.mode
               <<",\"index\":"<<v.index<<",\"before\":"<<v.before<<",\"after\":"<<v.after<<",\"disabled\":"<<v.disabled<<'}';}
        out<<"],\"eula\":{\"installed\":"<<(originalEulaUpdate!=nullptr)<<",\"verified\":"<<eulaRead.valid
           <<",\"bridge_edges\":"<<eulaBridgeEdges.load()<<",\"scoped_queries\":"<<eulaScopedQueries.load()
           <<",\"route\":"<<eulaRead.route<<",\"input_ms\":"<<eulaRead.inputTime
           <<",\"buttons\":"<<eulaRead.buttons<<",\"context\":\""<<controlContextName(eulaRead.context)<<'"'
           <<",\"sample_ms\":"<<eulaRead.time<<",\"owner\":"<<eulaRead.owner<<",\"vtable\":"<<eulaRead.type
           <<",\"control\":"<<eulaRead.control<<",\"active\":"<<unsigned(eulaRead.active)
           <<",\"phase\":"<<eulaRead.phase<<",\"mode\":"<<unsigned(eulaRead.mode)<<",\"flags\":"<<unsigned(eulaRead.flags)
           <<",\"read_to_end\":"<<unsigned(eulaRead.readToEnd)<<",\"selector_visible\":"<<unsigned(eulaRead.selectorVisible)
           <<",\"selection\":"<<eulaRead.selection<<",\"line\":"<<eulaRead.line<<",\"lines\":"<<eulaRead.lines
           <<",\"snapshot_atomic\":false},\"eula_updates\":[";first=true;
        for(const auto& v:eulaReads){if(!first)out<<',';first=false;
            out<<"{\"ms\":"<<v.time<<",\"owner\":"<<v.owner<<",\"verified\":"<<v.valid<<",\"active\":"<<unsigned(v.active)
               <<",\"phase\":"<<v.phase<<",\"mode\":"<<unsigned(v.mode)<<",\"route\":"<<v.route
               <<",\"input_ms\":"<<v.inputTime<<",\"buttons\":"<<v.buttons<<",\"context\":\""<<controlContextName(v.context)<<"\"}";}
        out<<"]}";result=out.str();return true;
    }
    if(request=="inspect-weapon-obstruction"){
        const auto witness=weaponObstructionWitness();
        std::ostringstream out;out<<"{\"sample_ms\":"<<witness.sampleTime<<",\"now_ms\":"<<steadyMilliseconds()
            <<",\"frame_ms\":"<<witness.frameTime<<",\"activation\":"<<witness.activation
            <<",\"player_owner\":"<<witness.player<<",\"native_hits\":"<<witness.nativeHits
            <<",\"applied_hits\":"<<witness.appliedHits<<",\"mode\":"<<static_cast<unsigned>(witness.mode)<<"}";
        result=out.str();return true;
    }
    if(request=="inspect-bot-state"){
        // Read-only transport/scene diagnostics must also work while Lua is
        // suspended at title/loading/Pause. These are individually sampled
        // publications, not a claimed coherent native simulation transaction.
        const auto rendered=headCamera().publishedView();
        const auto controlSample=controlInputSnapshot();
        const auto camera=headCamera().status();
        const auto menu=nativeMenuOpen();
        const auto tutorialMode=nativeIdroidTutorialMode();
        const auto popupSampleMs=steadyMilliseconds();
        const auto popup=nativePopupSnapshot();
        const auto delivery=nativeInputDeliveryAudit().snapshot();
        const auto now=steadyMilliseconds();
        const auto terms=nativeTermsMenu();
        const auto ownedIdroidPad=idroidPadOwner();
        std::ostringstream out;out<<std::boolalpha;
        out<<"{\"schema\":1,\"now_ms\":"<<now
           <<",\"lua_ready\":"<<nativeLuaReady.load()
           <<",\"title\":"<<nativeTitleModeActive()<<",\"title_menu\":"<<nativeTitleMenuOpen()
           <<",\"title_cabin\":"<<nativeTitleCabinMode()<<",\"cabin\":"<<nativeCabinPlay()
           <<",\"opening_cinematic\":"<<nativeOpeningCinematic()
           <<",\"cabin_transit\":"<<nativeCabinTransit()
           <<",\"loading\":"<<nativeLoadingTipsOpen()<<",\"demo\":"<<nativeScriptedDemoActive()
           <<",\"demo_candidate\":"<<(nativeDemoMode()==NativeDemoMode::staleCandidate)
           <<",\"demo_recovery_ready\":"<<headCamera().staleDemoRecoveryReady()
           <<",\"demo_interactive_look\":"<<(nativeDemoMode()==NativeDemoMode::interactiveLook)
           <<",\"menu\":"<<(menu?(*menu?"true":"false"):"null")
           <<",\"pause\":"<<(menu?(nativePauseMenuOpen()?"true":"false"):"null")
           <<",\"idroid\":"<<nativeIdroidOpen()<<",\"gamepad\":"<<nativeGamepadActive()
           <<",\"idroid_ui_only_owner\":"<<handheldIdroidOwned()
           <<",\"idroid_player_pad_owner\":"<<(ownedIdroidPad?(*ownedIdroidPad?"true":"false"):"null")
           <<",\"idroid_closing\":"<<nativeIdroidClosing()<<",\"idroid_tutorial_mode\":";
        writeNativeIdroidTutorialModeJson(out,tutorialMode);
        out<<",\"tracking_fault_mask\":"<<controllerTrackingFaultProbe().mask(steadyMilliseconds())
           <<",\"head_tracking_fault_mask\":"<<headTrackingFaultProbe().mask(steadyMilliseconds())
           <<",\"idroid_handheld\":"<<handheldMenusSelected()
           <<",\"idroid_menu_input_ready\":"<<handheldMenuInputReady()
           <<",\"idroid_tutorial_paused\":"<<nativeIdroidTutorialPaused()
           <<",\"idroid_menu_routing_available\":"<<handheldMenuRoutingAvailable()
           <<",\"idroid_menu_control\":"<<static_cast<unsigned>(handheldMenuControl())
           <<",\"idroid_focused_weapon\":"<<handheldFocusedWeapon()
           <<",\"idroid_focused_weapon_available\":"<<handheldFocusedWeaponRoutingAvailable()
           <<",\"idroid_ui_blocked_queries\":"<<handheldUiBlockedQueries.load()
           <<",\"camera_active\":"<<camera.active<<",\"camera_available\":"<<headCamera().available()
           <<",\"camera_pending\":"<<camera.pending<<",\"camera_suspended\":"<<camera.suspended
           <<",\"camera_awaiting_player\":"<<camera.awaitingPlayer
           <<",\"camera_native_menu\":"<<camera.nativeMenuOpen
           <<",\"camera_native_idroid\":"<<camera.nativeIdroidOpen
           <<",\"camera_reason\":"<<static_cast<unsigned>(camera.reason)
           <<",\"camera_activation_cause\":"<<unsigned(camera.activationCause)
           <<",\"camera_source_owner\":"<<camera.sourceCamera<<",\"camera_player_owner\":"<<camera.sourcePlayerOwner
           <<",\"camera_owner_head_ms\":"<<camera.ownerHeadTime
           <<",\"camera_await_cause\":"<<unsigned(camera.awaitCause)<<",\"camera_await_join_flags\":"<<unsigned(camera.awaitJoinFlags)
           <<",\"camera_await_ms\":"<<camera.awaitTime<<",\"camera_await_owner_age_ms\":"<<camera.awaitOwnerAge
           <<",\"activation\":"<<camera.activation<<",\"opening_assets\":"<<openingPropsAvailable();
        out<<",\"popup_observer\":";writeNativePopupSnapshotJson(out,popup,popupSampleMs);
        out<<",\"terms_menu\":{\"verified\":"<<terms.verified<<",\"active\":"<<terms.active
           <<",\"ready\":"<<terms.ready(now)<<",\"sample_ms\":"<<terms.time<<",\"owner\":"<<terms.owner
           <<",\"phase\":"<<terms.phase<<",\"mode\":"<<unsigned(terms.mode)
           <<",\"horizontal\":"<<terms.horizontal<<",\"vertical\":"<<terms.vertical<<",\"action\":"<<terms.action<<'}';
        out<<",\"xinput_delivery\":";writeNativeInputDeliveryJson(out,delivery,now);
        if(freshControlInputAudit(controlSample,now)){
            const auto* audit=&controlSample;
            out<<",\"controls\":{\"context\":\""<<controlContextName(audit->context)
               <<"\",\"age_ms\":"<<now-audit->time<<",\"sample_ms\":"<<audit->time
               <<",\"rig_input\":"<<audit->rigInput<<",\"travel_mode\":"<<audit->travelMode
               <<",\"native_packet_source\":\"xr_runtime_final_mapped_packet\",\"native_buttons\":"<<audit->nativeButtons
               <<",\"native_axes\":["<<audit->nativeAxes[0]<<','<<audit->nativeAxes[1]<<','<<audit->nativeAxes[2]<<','<<audit->nativeAxes[3]
               <<"],\"native_triggers\":["<<static_cast<unsigned>(audit->nativeTriggers[0])<<','<<static_cast<unsigned>(audit->nativeTriggers[1])
               <<"],\"physical\":[";
            for(size_t i=0;i<11;++i){if(i)out<<',';out<<audit->physical.buttons[i];}
            out<<"],\"sticks\":["<<audit->physical.leftStick[0]<<','<<audit->physical.leftStick[1]
               <<','<<audit->physical.rightStick[0]<<','<<audit->physical.rightStick[1]<<"],\"touches\":{";
            constexpr std::array<const char*,10> touchNames{"left_thumbrest","right_thumbrest","a_touch","b_touch","x_touch","y_touch",
                "left_stick_touch","right_stick_touch","left_trigger_touch","right_trigger_touch"};
            for(size_t i=0;i<touchNames.size();++i){if(i)out<<',';out<<'"'<<touchNames[i]<<"\":"<<audit->physical.buttons[19+i];}
            out<<"}}";
        }else out<<",\"controls\":null";
        if(rendered){
            const auto& frame=*rendered;
            out<<",\"rendered\":{\"source\":\"last_camera_publication\",\"tracking_sequence\":"<<frame.trackingSequence
               <<",\"rig_sequence\":"<<frame.rigSequence<<",\"activation\":"<<frame.activation
               <<",\"sample_ms\":"<<frame.sampleTime<<",\"player_sequence\":"<<frame.playerSequence
               <<",\"player_owner\":"<<frame.playerOwner<<",\"player_head\":["<<frame.playerHead.x<<','<<frame.playerHead.y<<','<<frame.playerHead.z
               <<"],\"left_palm_tracked\":"<<frame.renderedPalmTracked[0]
               <<",\"right_palm_tracked\":"<<frame.renderedPalmTracked[1];
            out<<",\"raw_left_grip_tracked\":"<<frame.controllers.hands[0].gripTracked
               <<",\"presentation_focused\":"<<frame.controllers.presentationFocused
               <<",\"presentation_epoch\":"<<frame.controllers.presentationEpoch
               <<",\"raw_left_aim_tracked\":"<<frame.controllers.hands[0].aimTracked
               <<",\"raw_right_grip_tracked\":"<<frame.controllers.hands[1].gripTracked
               <<",\"raw_right_aim_tracked\":"<<frame.controllers.hands[1].aimTracked
               <<",\"idroid_stowed\":"<<frame.controllers.handheldIdroidStowed
               <<",\"handheld_menu_free\":"<<frame.controllers.handheldMenuFree;
            reportControlsEditor(out);
            out<<",\"cabin_play\":"<<frame.controllers.cabinPlay
               <<",\"handheld_menus\":"<<frame.controllers.handheldMenus
               <<",\"menu_open\":"<<frame.menuOpen
               <<",\"camera_world_frozen\":"<<frame.controllers.cameraWorldFrozen
               <<",\"menu_idroid\":"<<frame.menuIdroid;
            out<<",\"rocket_arm_guidance\":"<<frame.rocketArmGuidance;
            const auto pose=[&](const char* name,Pose value){
                const auto p=value.position;const auto q=value.orientation;
                out<<",\""<<name<<"\":{\"position\":["<<p.x<<','<<p.y<<','<<p.z
                   <<"],\"orientation\":["<<q.x<<','<<q.y<<','<<q.z<<','<<q.w<<"]}";
            };
            const auto authoredHead=authoredHeadVisibility();
            out<<",\"authored_head\":{\"owner\":"<<authoredHead.owner<<",\"model\":"<<authoredHead.model
               <<",\"sample_ms\":"<<authoredHead.time<<",\"activation\":"<<authoredHead.activation
               <<",\"concealed\":"<<authoredHead.concealed<<'}';
            pose("authored_head_pose",authoredHead.head);pose("authored_head_viewer",authoredHead.viewer);
            pose("native_pose",frame.nativePose);
            out<<",\"native_body_source_tracked\":"<<frame.nativeBodySourceTracked;
            pose("native_body_source_root",frame.nativeBodySourceRoot);
            pose("native_body_source_chest",frame.nativeBodySourceChest);
            pose("native_body_source_head",frame.nativeBodySourceHead);
            pose("native_body_source_eye",frame.nativeBodySourceEye);
            out<<",\"player_posture\":"<<static_cast<unsigned>(frame.controllers.playerPosture)
               <<",\"full_body_requested\":"<<frame.controllers.fullBody
               <<",\"full_body\":{\"valid\":"<<frame.body.valid<<",\"spine_clamped\":"<<frame.body.spineClamped
               <<",\"mounted\":"<<frame.body.mounted
               <<",\"rejection\":"<<frame.body.rejection
               <<",\"floor_queries\":"<<frame.body.floorQueries<<",\"floor_contacts\":"<<frame.body.floorContacts
               <<",\"floor_clearance\":["<<frame.body.floorClearance[0]<<','<<frame.body.floorClearance[1]<<']'
               <<",\"native_floor_clearance\":["<<frame.body.nativeFloorClearance[0]<<','<<frame.body.nativeFloorClearance[1]<<']'
               <<",\"eye_error\":["<<frame.body.eyeError.x<<','<<frame.body.eyeError.y<<','<<frame.body.eyeError.z<<']'
               <<",\"head_contacts\":"<<frame.body.headContacts<<'}'
               <<",\"head_look_lesson\":"<<frame.controllers.headLookLessonKey
               <<",\"cinematic_free_move\":"<<frame.controllers.cinematicFreeMove
               <<",\"cinematic_move\":["<<frame.controllers.cinematicMove[0]<<','<<frame.controllers.cinematicMove[1]<<']'
               <<",\"cinematic_follow_valid\":"<<frame.cinematicFollowValid
               <<",\"cinematic_free_offset\":["<<frame.cinematicFreeOffset.x<<','<<frame.cinematicFreeOffset.y<<','<<frame.cinematicFreeOffset.z<<']';
            pose("cinematic_base",frame.cinematicBasePose);
            pose("body_pelvis",frame.body.pelvis);pose("body_chest",frame.body.chest);
            pose("body_head",frame.body.head);pose("body_eye",frame.body.eye);
            pose("left_foot",frame.body.feet[0]);pose("right_foot",frame.body.feet[1]);
            pose("head_pose",frame.headPose);
            out<<",\"physical_floor_tracked\":"<<frame.controllers.physicalFloorTracked
               <<",\"physical_floor_y\":"<<frame.controllers.physicalFloorY;
            pose("locomotion_heading",Pose{frame.locomotionHeading,{}});
            out<<",\"locomotion_heading_tracked\":"<<frame.locomotionHeadingTracked;
            out<<",\"eyes\":[";
            for(size_t eye=0;eye<2;++eye){if(eye)out<<',';const auto& v=frame.views[eye];const auto p=v.pose.position;const auto q=v.pose.orientation;
                out<<"{\"position\":["<<p.x<<','<<p.y<<','<<p.z<<"],\"orientation\":["<<q.x<<','<<q.y<<','<<q.z<<','<<q.w
                   <<"],\"fov\":["<<v.fov.left<<','<<v.fov.right<<','<<v.fov.up<<','<<v.fov.down<<"]}";}
            out<<']';
            if(const auto studio=frame.controllers.studio){
                out<<",\"studio\":{\"available\":"<<studio->available<<",\"cabin\":"<<studio->cabin
                   <<",\"camera_hand\":"<<studio->cameraHand<<",\"marker_hand\":"<<studio->markerHand
                   <<",\"camera_candidate_mask\":"<<unsigned(studio->cameraCandidateMask)
                   <<",\"editor\":"<<studio->editor<<",\"shutter\":"<<studio->shutter<<",\"photo\":"<<studio->lastPhoto
                   <<",\"input_mask\":"<<unsigned(studio->inputMask)<<",\"keyboard_only\":"<<studio->keyboardOnly;
                out<<",\"selected_photo\":"<<studio->selectedPhoto<<",\"review\":"<<studio->review
                   <<",\"confirm_photo\":"<<studio->confirmPhoto<<",\"confirm_paper\":"<<studio->confirmPaper
                   <<",\"hover_control\":"<<studio->hoverControl<<",\"hover_key\":"<<studio->hoverKey
                   <<",\"freeze_requested\":"<<studio->freezeRequested<<",\"freeze_supported\":"<<studio->freezeSupported
                   <<",\"freeze_state\":"<<unsigned(studio->freezeState)
                   <<",\"library\":[";
                for(size_t i=0;i<studio->photos.size();++i){if(i)out<<',';out<<studio->photos[i];}out<<']';
                pose("room",studio->roomTracking);pose("camera",studio->camera);pose("marker",studio->marker);pose("keyboard",studio->keyboard);
                pose("board",cabinNativeBoard);pose("notes_home",Pose{{-.70710678f,0,0,.70710678f},studioNotesHome});
                pose("camera_home",Pose{{},studioCameraHome});pose("marker_home",Pose{{},studioMarkerHome});
                out<<",\"papers\":[";bool first=true;
                for(const auto& paper:studio->papers){if(!first)out<<',';first=false;out<<"{\"id\":"<<paper.id<<",\"photo\":"<<paper.photo
                    <<",\"hand\":"<<paper.hand<<",\"pinned\":"<<paper.pinned<<",\"text\":"<<std::quoted(paper.ink->text)
                    <<",\"strokes\":"<<paper.ink->strokes.size()<<",\"field\":"<<paper.field;
                    pose("pose",paper.pose);pose("field_pose",paper.fieldPose);out<<'}';}
                out<<"]}";
            }
            pose("left_palm",frame.renderedPalms[0]);
            pose("left_grip",frame.controllers.hands[0].grip);
            pose("right_palm",frame.renderedPalms[1]);
            out<<",\"left_index_tip_tracked\":"<<frame.controllers.hands[0].indexTipTracked
               <<",\"right_index_tip_tracked\":"<<frame.controllers.hands[1].indexTipTracked;
            pose("left_index_tip_local",Pose{{},frame.controllers.hands[0].indexTip});
            pose("right_index_tip_local",Pose{{},frame.controllers.hands[1].indexTip});
            pose("right_grip",frame.controllers.hands[1].grip);
            pose("idroid_device",frame.idroidDevice);
            if(frame.rocketArmGuidance)pose("rocket_arm_camera",frame.rocketArmCamera);
            pose("scope_ocular",frame.weaponScope.ocular);
            pose("scope_objective",frame.weaponScope.objective);
            pose("weapon_support_grip",frame.weaponSupportGrip);
            pose("driver_left_wheel_grip",frame.driverWheelGrips[0]);
            pose("driver_right_wheel_grip",frame.driverWheelGrips[1]);
            out<<",\"driver_wheel_tracked\":"<<frame.driverWheelTracked
               <<",\"driver_wheel_held\":"<<frame.driverWheelHeld
               <<",\"driver_wheel_hands\":"<<unsigned(frame.driverWheelHands)
               <<",\"driver_wheel_axis\":"<<frame.driverWheelAxis;
            const auto vehicleGun=vehicleWeaponState();
            out<<",\"vehicle_weapon\":{\"available\":"<<vehicleWeaponRoutingAvailable()
               <<",\"requested\":"<<vehicleGun.requested<<",\"ready\":"<<vehicleGun.ready
               <<",\"eligible\":"<<vehicleGun.nativeEligible<<",\"player\":"<<vehicleGun.player
               <<",\"activation\":"<<vehicleGun.activation<<",\"time\":"<<vehicleGun.time
               <<",\"held\":"<<vehicleGun.semanticHeld<<",\"input_mask\":"<<vehicleGun.inputMask
               <<",\"weapon_flags\":"<<vehicleGun.weaponFlags<<",\"render_flags\":"<<vehicleGun.renderFlags
               <<",\"action_flags\":"<<vehicleGun.renderActionFlags<<",\"pose_flags\":"<<vehicleGun.renderPoseFlags
               <<",\"native_action\":"<<vehicleGun.nativeActionId<<",\"posture\":"<<unsigned(vehicleGun.nativePosture)
               <<",\"equipped_slot\":"<<unsigned(vehicleGun.equippedSlot)<<",\"requested_slot\":"<<unsigned(vehicleGun.requestedSlot)<<'}';
            pose("binocular_ocular",frame.controllers.optic.pose.rightEyepiece);
            out<<",\"scope_tracked\":"<<frame.weaponScope.tracked
               <<",\"scope_dot_reticle\":"<<frame.weaponScope.dotReticle
               <<",\"scope_opaque_at_one_power\":"<<frame.weaponScope.opaqueAtOnePower
               <<",\"idroid_device_tracked\":"<<frame.idroidDeviceTracked
               <<",\"scope_magnification\":"<<frame.weaponScope.magnification
               <<",\"scope_radius\":"<<frame.weaponScope.radius
               <<",\"native_firearm\":"<<frame.nativeFirearmActive
               <<",\"weapon_support_tracked\":"<<frame.weaponSupportGripTracked
               <<",\"weapon_support_attached\":"<<frame.weaponSupportAttached
               <<",\"binocular_held\":"<<frame.controllers.optic.held
               <<",\"binocular_magnification\":"<<frame.controllers.magnification;
            const auto& weapon=frame.weaponRig;
            out<<",\"weapon_native_observer\":{\"source\":\"retained_native_skin_sampling\",\"sample_ms\":"<<weapon.sampleTime
               <<",\"sampled\":"<<weapon.sampled<<",\"character\":"<<weapon.character<<",\"component\":"<<weapon.component
               <<",\"component_accepted\":"<<weapon.componentAccepted<<",\"instances\":"<<weapon.instances
               <<",\"instance_first\":"<<weapon.instanceFirst<<",\"player_index\":"<<weapon.playerIndex
               <<",\"instance_index_matched\":"<<weapon.instanceIndexMatched<<",\"state\":"<<weapon.state
               <<",\"body_model\":"<<weapon.bodyModel<<",\"weapon_model\":null,\"weapon_definition\":null"
               <<",\"resource_handle\":"<<weapon.resourceHandle<<",\"resource_accepted\":"<<weapon.resourceAccepted
               <<",\"support_identity\":"<<weapon.supportIdentity<<",\"scope_identity\":"<<frame.weaponScope.weaponIdentity
               <<",\"magnifier_interface\":"<<weapon.magnifierInterface<<",\"magnifier_model\":"<<weapon.magnifierModel
               <<",\"magnifier_index\":"<<weapon.magnifierIndex<<",\"magnifier_model_code\":"<<weapon.magnifierModelCode
               <<",\"flags\":"<<weapon.flags<<",\"mode\":"<<weapon.mode<<",\"muzzle_attempted\":"<<weapon.muzzleAttempted
               <<",\"native_aim_target_read\":"<<weapon.nativeAimTargetRead;
            out<<",\"native_aim_target\":["<<weapon.nativeAimTarget.x<<','<<weapon.nativeAimTarget.y<<','<<weapon.nativeAimTarget.z<<']';
            out
               <<",\"muzzle_matrix_read\":"<<weapon.muzzleMatrixRead<<",\"attachment_valid\":"<<weapon.attachmentValid
               <<",\"muzzle_socket_valid\":"<<weapon.muzzleSocketValid<<",\"muzzle_solved\":"<<weapon.muzzleSolved
               <<",\"optical_descriptor_read\":"<<weapon.opticalRead<<",\"optical_descriptor\":[";
            for(size_t i=0;i<weapon.optical.size();++i){if(i)out<<',';out<<static_cast<unsigned>(weapon.optical[i]);}
            out<<']';pose("muzzle_in_grip",weapon.muzzleInGrip);
            out<<",\"sight_query_attempted\":"<<weapon.sightQueryAttempted
               <<",\"sight_rear_read\":"<<weapon.sightRearRead<<",\"sight_front_read\":"<<weapon.sightFrontRead
               <<",\"sight_rear_valid\":"<<weapon.sightRearValid<<",\"sight_front_valid\":"<<weapon.sightFrontValid;
            pose("sight_rear_native_model",weapon.sightRear);pose("sight_front_native_model",weapon.sightFront);
            pose("sight_rear_in_wrist",weapon.sightRearInWrist);pose("sight_front_in_wrist",weapon.sightFrontInWrist);
            pose("sight_rear_local",weapon.sightRearLocal);pose("sight_front_local",weapon.sightFrontLocal);out<<'}';
            out<<'}';
        }else out<<",\"rendered\":null";
        // Presentation settings stay operable with immersive rendering off.
        // Their diagnostics must not depend on an active native eye camera.
        reportControlsEditor(out);
        const auto textInput=nativeTextInputStatus();
        out<<",\"text_input\":{\"ready\":"<<textInput.ready<<",\"active\":"<<textInput.active
           <<",\"visible\":"<<textInput.visible
           <<",\"down\":"<<textInput.down<<",\"up\":"<<textInput.up<<",\"converted\":"<<textInput.converted
           <<",\"discarded\":"<<textInput.discarded<<",\"pending\":"<<textInput.pending<<'}';
        if(const auto anchor=headCamera().openingTrackingOrigin(now)){
            const auto p=anchor->position;const auto q=anchor->orientation;
            out<<",\"opening_position\":["<<p.x<<','<<p.y<<','<<p.z
               <<"],\"opening_orientation\":["<<q.x<<','<<q.y<<','<<q.z<<','<<q.w<<']';
            const auto focus=nativeTitleFocus();const auto bench=rendered?rendered->controllers.openingBench:CabinBenchFrame{};
            out<<",\"opening_native_menu\":"<<(rendered&&rendered->controllers.openingNativeMenu);
            out<<",\"opening_focus\":{\"ready\":"<<focus.ready<<",\"row\":"<<unsigned(focus.row)<<",\"actions\":[";
            for(size_t i=0;i<focus.count;++i){if(i)out<<',';out<<unsigned(focus.actions[i]);}
            out<<"]},\"opening_bench\":{\"inserted\":"<<bench.inserted<<",\"lid\":"<<bench.lidOpen<<",\"tapes\":[";
            for(size_t i=0;i<bench.tapes.size();++i){if(i)out<<',';const auto& tape=bench.tapes[i];
                out<<"{\"hand\":"<<tape.hand<<",\"state\":"<<unsigned(tape.state)
                   <<",\"position\":["<<tape.pose.position.x<<','<<tape.pose.position.y<<','<<tape.pose.position.z
                   <<"],\"orientation\":["<<tape.pose.orientation.x<<','<<tape.pose.orientation.y<<','<<tape.pose.orientation.z<<','<<tape.pose.orientation.w<<"]}";}
            out<<"],\"layout\":{\"verified\":"<<bench.layout.verified<<",\"generation\":"<<bench.layout.generation;
            const auto layoutPose=[&](const char* name,Pose value){const auto p=value.position;const auto q=value.orientation;
                out<<",\""<<name<<"\":{\"position\":["<<p.x<<','<<p.y<<','<<p.z<<"],\"orientation\":["<<q.x<<','<<q.y<<','<<q.z<<','<<q.w<<"]}";};
            layoutPose("seat",bench.layout.seat);layoutPose("player",bench.layout.player);layoutPose("slot",bench.layout.slot);
            layoutPose("eject",Pose{bench.layout.seat.orientation,bench.layout.eject});
            out<<"}}";
        }
        out<<'}';result=out.str();return true;
    }
    constexpr std::string_view prefix="input:";
    if(request.rfind(prefix,0)!=0)return false;
    const auto key=request.substr(prefix.size());GamepadSample sample{};
    if(key=="a")sample.buttons=0x1000;
    else if(key=="b")sample.buttons=0x2000;
    else if(key=="x")sample.buttons=0x4000;
    else if(key=="y")sample.buttons=0x8000;
    else if(key=="start")sample.buttons=0x0010;
    else if(key=="back")sample.buttons=0x0020;
    else if(key!="release"){result="unknown input: "+key;return true;}
    if(key=="a")requestLoadingPromptConfirm();
    gamepadMailbox().publishExternal(sample,key!="release",steadyMilliseconds());
    result="input published: "+key;return true;
}
bool executeFast(void* state,const std::string& request,int& status,std::string& result){
    if(publishFastInput(request,result)){status=result.rfind("unknown input:",0)==0?1:0;return true;}
    if(!luaReady(state))return false;
    if(request=="inspect-animal-touch"){result=inspectAnimalTouch();status=0;return true;}
    if(request=="native-dog-response"){result=requestNativeDogResponse();status=0;return true;}
    if(request=="inspect-small-animals"){result=inspectSmallAnimals();status=0;return true;}
    const int top=getTop(state);const std::string script="if type(TppMain)~='table' or type(GameObject)~='table' or type(gvars)~='table' or type(vars)~='table' or type(TppMission)~='table' or type(TppBuddyService)~='table' then return '__MGS5VR_WAIT__' end\n"+request;
    status=load(state,script.data(),script.size(),"@mgs5vr-native-actions");if(!status)status=original(state,0,1,0);
    size_t length{};const auto* text=getString(state,-1,&length);result=text?std::string(text,std::min(length,size_t{32768})):"(no return value)";setTop(state,top);
    return !(status==0&&result=="__MGS5VR_WAIT__");
}
void drainFastActions(void* state){
    for(size_t count=0;count<maxActionsPerTransaction;++count){
        ActionRequest request;
        {std::lock_guard lock(fastQueueMutex);if(fastQueue.empty())return;request=std::move(fastQueue.front());fastQueue.pop_front();}
        if(request.cancelled->load()||!request.connection->open.load())continue;
        const auto started=std::chrono::steady_clock::now();int status{};std::string result;
        if(!executeFast(state,request.script,status,result)){
            static uint64_t lastWaitingLog{};
            const auto now=GetTickCount64();
            if(now-lastWaitingLog>=1000){lastWaitingLog=now;log("Native action pipe waiting for Lua readiness");}
            std::lock_guard lock(fastQueueMutex);fastQueue.push_front(std::move(request));return;
        }
        const auto finished=std::chrono::steady_clock::now();
        const auto queued=std::chrono::duration_cast<std::chrono::milliseconds>(started-request.queuedAt).count();
        const auto execution=std::chrono::duration_cast<std::chrono::microseconds>(finished-started).count();
        sendFastResponse(request,status,static_cast<uint64_t>(std::max<int64_t>(0,queued)),static_cast<uint64_t>(std::max<int64_t>(0,execution)),result);
        log("Native fast action completed id="+std::to_string(request.id)+" queue_ms="+std::to_string(queued)+" exec_us="+std::to_string(execution));
    }
}
void pump(void* state){
    pumpNativeTextInput();
    drainFastActions(state);
    std::unique_lock lock(requestMutex,std::try_to_lock);
    const auto now=GetTickCount64();
    if(!lock.owns_lock()||now-checkedAt<100)return;
    checkedAt=now;
    if(luaReady(state)){
        const auto request=cameraFreezeRequest();
        const auto leaseNow=steadyMilliseconds();
        const auto camera=headCamera().status();
        const bool wanted=request.requested&&leaseNow>=request.at&&leaseNow-request.at<500
            &&request.activation&&request.referenceEpoch&&camera.active&&camera.activation==request.activation
            &&!nativeLoadingTipsOpen()&&!nativeTitleModeActive()&&!nativeScriptedDemoActive()
            &&!camera.nativeMenuOpen&&!nativeGamepadActive();
        const bool changedOwner=cameraWorldFrozen()&&(cameraFreezeActivation!=request.activation
            ||cameraFreezeReference!=request.referenceEpoch);
        // End an old lease before a new owner can request a freeze. Never
        // unregister retail pause owners or open the stock Pause screen.
        const bool pause=wanted&&!changedOwner;
        if(pause!=cameraWorldFrozen()){
            const int top=getTop(state);
            const std::string_view script=pause
                ?"if type(TppPause)=='table' and type(TppPause.RegisterPause)=='function' "
                 "and type(TppPause.PAUSE_LEVEL_MB_DVC_TUTORIAL)=='number' then "
                 "TppPause.RegisterPause('MGS5VR_Camera',TppPause.PAUSE_LEVEL_MB_DVC_TUTORIAL); return 'ok' end return 'unavailable'"
                :"if type(TppPause)=='table' and type(TppPause.UnregisterPause)=='function' then "
                 "TppPause.UnregisterPause('MGS5VR_Camera'); return 'ok' end return 'unavailable'";
            int status=load(state,script.data(),script.size(),"@mgs5vr-camera-freeze");
            if(!status)status=original(state,0,1,0);
            size_t length{};const auto* value=getString(state,-1,&length);
            if(!status&&value&&std::string_view(value,length)=="ok"){
                cameraFreezeActivation=pause?request.activation:0;
                cameraFreezeReference=pause?request.referenceEpoch:0;
                acknowledgeCameraFreeze(pause?StudioFreeze::frozen:StudioFreeze::live,pause);
                log(pause?"Camera Freeze: named native world pause acquired; tracked rendering retained"
                    :"Camera Freeze: named native world pause released");
            }else acknowledgeCameraFreeze(StudioFreeze::unavailable,cameraWorldFrozen());
            setTop(state,top);
        }else if(!wanted&&!cameraWorldFrozen())acknowledgeCameraFreeze(StudioFreeze::live,false);
    }
    // Menu entry/Help can briefly lack a fresh rig and can re-admit the same
    // player under a new camera activation. Neither is a terminal-owner change.
    // Keep the UI-only resource until a verified different player or UI exit.
    if(handheldIdroidOwned()){
        if(luaReady(state)){
            const int top=getTop(state);
            int status=load(state,nativeIdroidInterruptionScript.data(),nativeIdroidInterruptionScript.size(),"@mgs5vr-idroid-interruption");
            if(!status)status=original(state,0,1,0);
            size_t length{};const auto* value=getString(state,-1,&length);
            if(!status&&value&&std::string_view(value,length)=="closed"){
                cancelHandheldIdroidBorrow();publishHandheldIdroidOwner(0,0);nativeMenuOpen();
                log("Handheld iDroid: owned terminal closed for native mission failure");
            }
            setTop(state,top);
        }
        const auto rig=headCamera().publishedRigFrame(steadyMilliseconds());
        const auto borrow=handheldIdroidBorrow();
        if((!nativeIdroidOpen()&&!borrow.player)||nativeLoadingTipsOpen()||nativeTitleModeActive()||nativeScriptedDemoActive()
                ||(rig&&!handheldIdroidOwned(rig->playerOwner))){
            cancelHandheldIdroidBorrow();publishHandheldIdroidOwner(0,0);
        }
        else if(rig&&rig->activation&&headCamera().status().activation==rig->activation
                &&!handheldIdroidOwned(rig->playerOwner,rig->activation))
            publishHandheldIdroidOwner(rig->playerOwner,rig->activation);
    }
    pumpHandheldIdroidEquipment();
    // Help can pause the gameplay Lua owner. Its native UI cleanup must run
    // on the pump independently of gameplay globals or coroutine addresses.
    NativeIdroidStowHelpRequest stowHelp;
    {std::lock_guard stowGuard(stowHelpMutex);stowHelp=stowHelpLease.pending(now);}
    if(controllerRigEnabled()&&stowHelp.owner&&stowHelp.acknowledged
       &&nativeIdroidStowHelpState(stowHelp.owner)==NativeIdroidStowHelpState::closed){
        bool completed{};
        {std::lock_guard stowGuard(stowHelpMutex);completed=stowHelpLease.finish(stowHelp);}
        if(completed){
            nativeMenuOpen();
            restoreCompletedGuideRestrictions();
            releaseNativeIdroidTutorialPause();
            log("Requested terminal stow: native UI acknowledged and closed owned Help");
        }
    }
    if(controllerRigEnabled()&&luaReady(state)){
        const int top=getTop(state);
        if(const auto bindings=controlPromptBindings()){
            const std::string godScript=std::string("local requested=")
                +(bindings->setting("settings.god_mode")!=0?"true\n":"false\n")+std::string(nativeGodModeScript);
            int godStatus=load(state,godScript.data(),godScript.size(),"@mgs5vr-god-mode");
            if(!godStatus)godStatus=original(state,0,1,0);
            size_t length{};const auto* value=getString(state,-1,&length);
            const std::string report=std::to_string(godStatus)+":"+(value?std::string(value,std::min(length,size_t{80})):"no result");
            static std::string previousGodReport;
            if(report!=previousGodReport){log("Native god mode "+report);previousGodReport=report;}
            setTop(state,top);
        }
        const auto cameraStatus=headCamera().status();
        const auto currentRig=headCamera().publishedRigFrame(steadyMilliseconds());
        const auto borrow=handheldIdroidBorrow();
        if(borrow.player&&!borrow.stopped&&!borrow.returnRequested&&currentRig
                &&currentRig->playerOwner==borrow.player&&cameraStatus.activation==borrow.activation
                &&cameraStatus.active&&!cameraStatus.suspended&&handheldMenusSelected()
                &&handheldIdroidOwned(borrow.player,borrow.activation)&&!nativeGamepadActive()
                &&!nativePauseMenuOpen()&&!nativeCabinPlay()
                &&nativeIdroidTutorialMode()==std::optional<unsigned>{0}){
            constexpr std::string_view stopScript=
                "if not TppUiCommand.IsShowPopup() and not TppSave.IsSaving() "
                "and not gvars.ini_isTitleMode and not DemoDaemon.IsDemoPlaying() "
                "and TppSequence.IsMissionPrepareFinished() and not TppMission.IsHelicopterSpace(vars.missionCode) "
                "and vars.missionCode~=10010 and TppSequence.GetCurrentSequenceName():match('^Seq_Game_') "
                "and PlayerInfo.AndCheckStatus{PlayerStatus.NORMAL_ACTION} then "
                "if TppUiCommand.IsMbDvcTerminalOpened() then TppUiCommand.StopMbDvcTerminal() end "
                "return TppUiCommand.IsMbDvcTerminalOpened() and 'pending' or 'stopped' end return 'owner changed'";
            int status=load(state,stopScript.data(),stopScript.size(),"@mgs5vr-handheld-idroid-borrow");
            if(!status)status=original(state,0,1,0);
            size_t length{};const auto* value=getString(state,-1,&length);
            if(!status&&value&&std::string_view(value,length)=="stopped"){
                acknowledgeHandheldIdroidBorrow(borrow);nativeMenuOpen();
                log("VR handheld iDroid stopped for native reload; same handset retained");
            }else if(status||(value&&std::string_view(value,length)=="owner changed"))cancelHandheldIdroidBorrow();
            setTop(state,top);
        }
        const auto openRequest=pendingHandheldIdroidOpen();
        const bool openRequestAllowed=freshHandheldIdroidOpenRequest(openRequest,cameraStatus.activation,steadyMilliseconds())
                &&cameraStatus.active&&!cameraStatus.suspended
                &&!cameraStatus.nativeMenuOpen&&handheldMenusSelected()&&!nativeGamepadActive()
                &&!nativeIdroidOpen()&&!nativePauseMenuOpen()&&!nativeCabinPlay()
                &&nativeIdroidTutorialMode()==std::optional<unsigned>{0}
                &&(!borrow.player||(borrow.returnRequested&&currentRig&&borrow.player==currentRig->playerOwner
                    &&borrow.activation==cameraStatus.activation));
        if(openRequestAllowed&&currentRig&&currentRig->playerOwner&&consumeHandheldIdroidOpen(openRequest)){
            int status=load(state,nativeIdroidOpenScript.data(),nativeIdroidOpenScript.size(),"@mgs5vr-handheld-idroid-open");
            if(!status)status=original(state,0,1,0);
            size_t length{};const auto* result=getString(state,-1,&length);
            if(!status&&result&&std::string_view(result,length)=="opened"){
                nativeMenuOpen();
                if(nativeIdroidOpen()){
                    publishHandheldIdroidOwner(currentRig->playerOwner,cameraStatus.activation);
                    if(borrow.player)cancelHandheldIdroidBorrow();
                }
            }
            log("VR handheld iDroid open status="+std::to_string(status)+": "+(result?std::string(result,std::min(length,size_t{200})):"no result"));
            setTop(state,top);
        }else if(openRequest.time){
            // Keep the original one-second deadline and exact activation.
            // Nothing has executed yet: a menu-generation/skin publication
            // gap must not silently consume this freshly requested opening.
            const bool waitForRig=openRequestAllowed&&!currentRig;
            const bool consumed=!waitForRig&&consumeHandheldIdroidOpen(openRequest);
            static uint64_t reportedDeferredRevision{};
            if(!waitForRig||reportedDeferredRevision!=openRequest.revision){
                if(waitForRig)reportedDeferredRevision=openRequest.revision;
                const auto sampled=steadyMilliseconds();
                std::ostringstream diagnostic;
                diagnostic<<"VR handheld iDroid request "<<(waitForRig?"deferred":"rejected")
                <<" activation="<<openRequest.activation<<" revision="<<openRequest.revision<<" consumed="<<consumed
                <<" current_activation="<<cameraStatus.activation<<" request_time="<<openRequest.time
                <<" sample_time="<<sampled<<" fresh="
                <<freshHandheldIdroidOpenRequest(openRequest,cameraStatus.activation,sampled)
                <<" rig="<<bool(currentRig)<<" player="<<(currentRig?currentRig->playerOwner:0)
                <<" active="<<cameraStatus.active<<" suspended="<<cameraStatus.suspended
                <<" native_menu="<<cameraStatus.nativeMenuOpen<<" selected="<<handheldMenusSelected()
                <<" gamepad="<<nativeGamepadActive()<<" open="<<nativeIdroidOpen()
                <<" pause="<<nativePauseMenuOpen()<<" cabin="<<nativeCabinPlay()<<" tutorial=";
                writeNativeIdroidTutorialModeJson(diagnostic,nativeIdroidTutorialMode());
                diagnostic<<" borrowed_player="<<borrow.player<<" borrowed_return="<<borrow.returnRequested
                    <<" snapshot_atomic=0";
                log(diagnostic.str());
            }
        }
        if(cameraStatus.active&&!cameraStatus.suspended&&!cameraStatus.nativeMenuOpen){
            while(const auto event=takeOpticEvent(steadyMilliseconds(),cameraStatus.activation)){
                std::ostringstream script;
                script.precision(9);
                // The retail queue owns delivery and resend generations.
                // Calling TppMain.OnMessage directly loses a RESEND result.
                script<<"if type(Mission.SendMessageToSubscribers)=='function' and TppSequence.IsMissionPrepareFinished() then "
                    <<"Mission.SendMessageToSubscribers('Player','"
                    <<(event->kind==OpticEventKind::raised?"OnBinocularsMode":"PutMarkerWithBinocle")<<"'";
                if(event->kind==OpticEventKind::waypoint)
                    script<<','<<event->position.x<<','<<event->position.y<<','<<event->position.z;
                script<<");return 'queued' end return 'unavailable'";
                const auto text=script.str();
                int status=load(state,text.data(),text.size(),"@mgs5vr-physical-optic-event");
                if(!status)status=original(state,0,1,0);
                size_t length{};const auto* value=getString(state,-1,&length);
                if(!status&&value&&std::string_view(value,length)=="queued")acknowledgeOpticUse(*event);
                log("Physical optic player notification kind="+std::to_string(static_cast<int>(event->kind))
                    +" status="+std::to_string(status)+" result="+(value?std::string(value,std::min(length,size_t{180})):"no result"));
                setTop(state,top);
            }
        }
        // The retail game's named player-pad exclusion blocks character
        // actions without pausing skin, animation or terminal updates. Own a
        // separate registration so another tutorial's exclusion is preserved.
        static bool idroidPadRegistered{},accFobGuideHelpSeen{};
        static IdroidPadRelease idroidPadRelease;
        static void* idroidPadState{};
        if(idroidPadState!=state){
            idroidPadRegistered=accFobGuideHelpSeen=false;idroidPadState=state;idroidPadRelease.reset();
        }
        const bool handheldIdroid=nativeIdroidOpen()&&!nativeIdroidClosing()&&handheldMenusSelected();
        // ACC already fixes the character in its seat and releases the native
        // player-pad exclusion for terminal controls. Retain that ownership;
        // Grip focus excludes field movement; the free handheld route releases
        // it. Resolve the mission in
        // this Lua transaction rather than an expiring render-side heartbeat.
        bool idroidCabin{},idroidPadPolicyKnown=true;
        if(handheldIdroid){
            constexpr std::string_view ownerScript=
                "if type(TppMission)=='table' and type(TppMission.IsHelicopterSpace)=='function' "
                "and type(vars.missionCode)=='number' then return "
                "TppMission.IsHelicopterSpace(vars.missionCode) and 'cabin' or 'field' end return 'unavailable'";
            int ownerStatus=load(state,ownerScript.data(),ownerScript.size(),"@mgs5vr-idroid-pad-owner");
            if(!ownerStatus)ownerStatus=original(state,0,1,0);
            size_t length{};const auto* owner=getString(state,-1,&length);
            idroidPadPolicyKnown=!ownerStatus&&owner&&length==5
                &&(std::memcmp(owner,"cabin",5)==0||std::memcmp(owner,"field",5)==0);
            idroidCabin=idroidPadPolicyKnown&&std::memcmp(owner,"cabin",5)==0;
            setTop(state,top);
        }
        const auto idroidControl=handheldMenuControl();
        const bool guardIdroid=handheldIdroid&&!idroidCabin&&idroidControl!=HandheldMenuControl::locomotion;
        const bool requestedWeapon=guardIdroid&&idroidControl==HandheldMenuControl::menu
            &&handheldFocusedWeapon()&&handheldFocusedWeaponRoutingAvailable();
        const auto requestedPolicy=requestedWeapon?HandheldPlayerPadPolicy::weaponOnly:
            guardIdroid?HandheldPlayerPadPolicy::blocked:HandheldPlayerPadPolicy::released;
        auto appliedPolicy=requestedPolicy;
        if(idroidPadPolicyKnown){
            const auto mapped=controlInputSnapshot();
            const auto delivered=nativeInputDeliveryAudit().snapshot();
            const bool wasWaiting=idroidPadRelease.waiting();
            // A deliberate reload already owns a separate, validated
            // terminal Stop/Start lease. Its post-Stop B pulse is intentional;
            // it must not be mistaken for a menu Back handoff.
            const bool borrowedHand=borrow.player&&borrow.activation==cameraStatus.activation
                &&handheldIdroidOwned(borrow.player,borrow.activation);
            appliedPolicy=idroidPadRelease.update(requestedPolicy,handheldIdroid||borrowedHand,
                mapped,delivered,steadyMilliseconds());
            if(!wasWaiting&&idroidPadRelease.waiting())
                log("Handheld iDroid stow: retaining player-pad policy until native input release");
            else if(wasWaiting&&!idroidPadRelease.waiting()&&appliedPolicy==HandheldPlayerPadPolicy::released)
                log("Handheld iDroid stow: native input release witnessed");
        }
        const bool focusedWeapon=appliedPolicy==HandheldPlayerPadPolicy::weaponOnly;
        const bool excludePlayer=appliedPolicy==HandheldPlayerPadPolicy::blocked;
        bool idroidPadAcknowledged=false;
        if(idroidPadPolicyKnown){
            // Set/Reset are idempotent for this owner. IsSet is an aggregate
            // and cannot confirm our registration or release: another native
            // owner would otherwise keep all handheld menu input blocked.
            const std::string script=
                std::string("local weapon=")+(focusedWeapon?"true ":"false ")+
                "if type(Player)~='table' or type(Player.ResetPadMask)~='function' then return 'unavailable' end "
                "if weapon then "
                "if type(Player.SetPadMask)~='function' or type(PlayerPad)~='table' "
                "or type(PlayerPad.TRIGGER_L)~='number' or type(PlayerPad.TRIGGER_R)~='number' "
                "or type(PlayerPad.HOLD)~='number' or type(PlayerPad.FIRE)~='number' then return 'unavailable' end "
                "Player.SetPadMask{settingName='MGS5VR_iDroidFocus',except=true,buttons=PlayerPad.HOLD+PlayerPad.FIRE,"
                "triggers=PlayerPad.TRIGGER_L+PlayerPad.TRIGGER_R} "
                "else Player.ResetPadMask{settingName='MGS5VR_iDroidFocus'} end "
                "if type(TppGameStatus)=='table' and type(TppGameStatus.Set)=='function' "
                "and type(TppGameStatus.Reset)=='function' then TppGameStatus."
                +(excludePlayer?"Set":"Reset")+"('MGS5VR_iDroid','S_DISABLE_PLAYER_PAD'); return 'ok' end return 'unavailable'";
            int padStatus=load(state,script.data(),script.size(),"@mgs5vr-idroid-player-pad");
            if(!padStatus)padStatus=original(state,0,1,0);
            size_t length{};const auto* result=getString(state,-1,&length);
            const auto ownedPad=idroidPadOwner();
            if(!padStatus&&result&&length==2&&std::memcmp(result,"ok",2)==0
                    &&ownedPad.has_value()&&*ownedPad==excludePlayer){
                idroidPadAcknowledged=true;
                if(excludePlayer!=idroidPadRegistered){
                    idroidPadRegistered=excludePlayer;
                    log(excludePlayer?"Handheld iDroid: player pad excluded; hand and UI updates retained"
                        :"Handheld iDroid: owned player-pad exclusion released");
                }
            }
            setTop(state,top);
        }
        acknowledgeHandheldMenuControl(idroidControl,
            handheldIdroid&&idroidPadAcknowledged&&appliedPolicy==requestedPolicy,steadyMilliseconds(),focusedWeapon);
        // Spatial presentation keeps the retail iDroid's native world-update
        // policy. A mod-owned tutorial pause also freezes the Development
        // child's model/exit work: Back arrives but the page cannot return.
        // Native lessons and the explicit Pause menu retain their own owners.
        // Mode 10 is the native FOB_MISSION restriction (name hash f835f633).
        // Cold ACC can retain it after the campaign reports FINISH. Let the
        // player dismiss its informational Help first; then release only that
        // completed guide's input restriction and separately owned world pause.
        const auto guideMode=nativeIdroidTutorialMode();
        if(!nativeCabinPlay()||!nativeIdroidOpen()||!guideMode||*guideMode!=10)accFobGuideHelpSeen=false;
        else if(nativePauseMenuOpen())accFobGuideHelpSeen=true;
        if(nativeCabinPlay()&&nativeIdroidOpen()
           &&!nativePauseMenuOpen()&&accFobGuideHelpSeen&&guideMode&&*guideMode==10&&haveGuideRestrictionLease()){
            constexpr std::string_view guideScript=
                "if TppMission.IsHelicopterSpace(vars.missionCode) and TppSequence.GetCurrentSequenceName()=='Seq_Game_MainGame' "
                "and gvars.trm_fobTutorialState==127 and not mvars.heliSpace_nowMissionListGuidance "
                "and type(TppUiCommand.SetTutorialMode)=='function' and not TppUiCommand.IsShowPopup() then "
                "TppUiCommand.SetTutorialMode(false); return 'released' end return 'active guide'";
            int guideStatus=load(state,guideScript.data(),guideScript.size(),"@mgs5vr-acc-completed-guide");
            if(!guideStatus)guideStatus=original(state,0,1,0);
            size_t length{};const auto* result=getString(state,-1,&length);
            if(!guideStatus&&result&&length==8&&std::memcmp(result,"released",8)==0){
                const auto restored=restoreCompletedGuideRestrictions();
                log(std::string("Completed ACC FOB guide released; native tutorial pause released=")
                    +(releaseNativeIdroidTutorialPause()?"1":"0")+"; prior menu restrictions restored="+(restored?"1":"0"));
            }
            setTop(state,top);
        }
        const bool heldBackClose=takeNativeIdroidClose();
        const auto tutorialMode=nativeIdroidTutorialMode();
        const auto recovery=nativeIdroidRecoveryForTutorialMode(tutorialMode);
        // Native root Back and held Back share the ordinary ACC close
        // lifecycle. Its player must leave SUBJECT before terminal Stop;
        // hiding the terminal first leaves the next Menu tap exiting that
        // player action instead of opening the device.
        const bool cabinClose=nativeCabinPlay()&&nativeIdroidOpen()&&nativeIdroidClosing()
            &&recovery==NativeIdroidRecovery::ordinary&&!nativePauseMenuOpen();
        const bool uiOnlyClose=handheldIdroidOwned()&&nativeIdroidOpen()&&nativeIdroidClosing()
            &&recovery!=NativeIdroidRecovery::refuse&&!nativePauseMenuOpen();
        if((heldBackClose||cabinClose||uiOnlyClose)&&recovery!=NativeIdroidRecovery::refuse&&!nativePauseMenuOpen()){
            const bool cabinLifecycle=nativeCabinPlay()&&!handheldIdroidOwned()
                &&recovery==NativeIdroidRecovery::ordinary;
            const auto player=cabinLifecycle?cabinStowPlayer():std::optional<CabinStowPlayer>{};
            const auto terminal=cabinLifecycle?cabinStowTerminal():std::optional<CabinStowTerminal>{};
            const auto subject=player?cabinStowSubject(*player):std::optional<bool>{};
            const auto cabinAction=cabinLifecycle?nativeCabinIdroidStow(true,false,tutorialMode,
                terminal&&terminal->open,terminal?std::optional<bool>{terminal->closing}:std::nullopt,
                heldBackClose,player.has_value(),subject):NativeCabinIdroidStow::unchanged;
            static std::string previousCabinStow;
            if(cabinLifecycle){
                std::ostringstream diagnostic;
                diagnostic<<"Native ACC iDroid stow action="<<static_cast<unsigned>(cabinAction)
                    <<" player="<<(player?player->player:0)<<" index="<<(player?std::to_string(player->index):"unknown")
                    <<" activation="<<(player?player->activation:0)<<" terminal="<<(terminal?terminal->terminal:0)
                    <<" open="<<(terminal?std::to_string(terminal->open):"unknown")
                    <<" closing="<<(terminal?std::to_string(terminal->closing):"unknown")
                    <<" subject="<<(subject?std::to_string(*subject):"unknown");
                const auto message=diagnostic.str();
                if(message!=previousCabinStow){log(message);previousCabinStow=message;}
            }else previousCabinStow.clear();
            if(cabinAction!=NativeCabinIdroidStow::wait){
                // Field players consume the deferred close in their native stow
                // task. Calling Stop directly there leaves the equipped-device
                // task alive and can reopen Help. Ordinary ACC must also let its
                // native player release the device owner before terminal Stop.
                // Do not mark any tutorial complete or write progression variables.
                const bool immediateStop=cabinLifecycle?cabinAction==NativeCabinIdroidStow::finishStop
                    :nativeCabinPlay()||handheldIdroidOwned();
                // SECURITY also opens its post-close Help when a UI-only terminal
                // stops immediately. Capture that exact tutorial owner before Stop
                // resets its mode; the pump can release only its resulting Help.
                const auto requestedHelpOwner=(heldBackClose||uiOnlyClose)
                    ?captureNativeIdroidStowHelpOwner():0;
                const std::string closeFunction=immediateStop?"StopMbDvcTerminal":"CloseMbDvcTerminal";
                const std::string closeScript=
                    "if type(TppUiCommand)=='table' and type(TppUiCommand.IsMbDvcTerminalOpened)=='function' "
                    "and type(TppUiCommand."+closeFunction+")== 'function' and TppUiCommand.IsMbDvcTerminalOpened() "
                    "and type(TppUiCommand.IsShowPopup)=='function' and not TppUiCommand.IsShowPopup() then local completedGuide=false; "
                    +(cabinLifecycle?cabinStowPlayerGuard(*player)
                        +std::string(immediateStop?"if subject then return 'waiting player owner' end ":""):std::string{})
                    +std::string(cabinClose&&!cabinLifecycle
                        ?"if not (TppMission.IsHelicopterSpace(vars.missionCode) and TppSequence.GetCurrentSequenceName()=='Seq_Game_MainGame') then return 'different owner' end "
                        :"")
                    // Only the reproduced completed ACC guide permits clearing its
                    // tutorial mode here. Other lessons retain progression; held
                    // Back requests only the native terminal's stow operation.
                    +std::string(heldBackClose&&nativeCabinPlay()&&tutorialMode&&*tutorialMode==10&&haveGuideRestrictionLease()
                        ?"if TppMission.IsHelicopterSpace(vars.missionCode) and TppSequence.GetCurrentSequenceName()=='Seq_Game_MainGame' "
                         "and gvars.trm_fobTutorialState==127 and not mvars.heliSpace_nowMissionListGuidance "
                         "and type(TppUiCommand.SetTutorialMode)=='function' then TppUiCommand.SetTutorialMode(false); completedGuide=true end "
                        :"")
                    +"TppUiCommand."+closeFunction+"(); return completedGuide and 'completed guide closed' or 'requested' end return 'already closed or overlay'";
                int closeStatus=load(state,closeScript.data(),closeScript.size(),"@mgs5vr-idroid-back-recovery");
                // Loading the chunk must not turn an earlier owner/status sample
                // into authority over a replacement player or terminal. The Lua
                // transaction also rechecks the exact local index and SUBJECT.
                if(!closeStatus&&cabinLifecycle){
                    const auto currentPlayer=cabinStowPlayer();
                    const auto currentTerminal=cabinStowTerminal();
                    const auto currentSubject=currentPlayer?cabinStowSubject(*currentPlayer):std::optional<bool>{};
                    if(nativePauseMenuOpen()||currentPlayer!=player||currentTerminal!=terminal
                       ||nativeCabinIdroidStow(nativeCabinPlay(),handheldIdroidOwned(),nativeIdroidTutorialMode(),
                           currentTerminal&&currentTerminal->open,
                           currentTerminal?std::optional<bool>{currentTerminal->closing}:std::nullopt,
                           heldBackClose,currentPlayer.has_value(),currentSubject)!=cabinAction)
                        closeStatus=-1;
                }
                if(!closeStatus)closeStatus=original(state,0,1,0);
                size_t length{};const auto* result=getString(state,-1,&length);
                constexpr std::string_view completedGuide="completed guide closed";
                if(!closeStatus&&result&&length==completedGuide.size()&&std::memcmp(result,completedGuide.data(),length)==0){
                    const auto restored=restoreCompletedGuideRestrictions();
                    log(std::string("Requested ACC recovery: native tutorial pause released=")+(releaseNativeIdroidTutorialPause()?"1":"0")
                        +"; prior menu restrictions restored="+(restored?"1":"0"));
                }
                if(!closeStatus&&heldBackClose&&!immediateStop&&!cabinLifecycle&&nativeIdroidClosing()){
                    // Deliberate stow must run the native player task even while
                    // a lesson has paused the field. Release only the verified
                    // tutorial object's own pause handle, retaining its mode,
                    // restrictions, allocated handle and progression.
                    log(std::string("Requested field iDroid stow: tutorial pause released=")
                        +(releaseNativeIdroidTutorialPause(true)?"1":"0")
                        +"; post-close Help owner captured="+(requestedHelpOwner?"1":"0"));
                }
                if(!closeStatus&&requestedHelpOwner){
                    std::lock_guard stowGuard(stowHelpMutex);stowHelpLease.begin(requestedHelpOwner,now);
                }
                if(!closeStatus&&heldBackClose&&nativeIdroidRecoveryForTutorialMode(nativeIdroidTutorialMode())==NativeIdroidRecovery::ordinary){
                    // Restore only entry disables captured before this lesson
                    // borrowed them. An independent later disable remains owned
                    // by the game, and no completion/save variable is changed.
                    restoreCompletedGuideRestrictions();
                    releaseNativeIdroidTutorialPause();
                }
                log(std::string(cabinClose?"Native ACC iDroid root-Back cleanup status=":"Native iDroid held-Back recovery status=")+std::to_string(closeStatus)+": "
                    +(result?std::string(result,std::min(length,size_t{300})):"no result"));
                setTop(state,top);
            }
        }
        static const std::string presentationObservation="local function presentation()\n"+std::string(nativePresentationScript)
            +"\nend\nlocal visible=false; if type(FadeFunction)=='table' and type(FadeFunction.IsFadeOut)=='function' "
             "and type(FadeFunction.IsFadeProcessing)=='function' then "
             "local a,out=pcall(FadeFunction.IsFadeOut); local b,processing=pcall(FadeFunction.IsFadeProcessing); "
             "visible=a and b and out==false and processing==false end; return presentation(),visible and 'clear' or 'covered'";
        int presentationStatus=load(state,presentationObservation.data(),presentationObservation.size(),"@mgs5vr-presentation-state");
        if(!presentationStatus)presentationStatus=original(state,0,2,0);
        size_t fadeLength{};const auto* fadeValue=!presentationStatus?getString(state,-1,&fadeLength):nullptr;
        publishNativeSceneClear(fadeValue&&fadeLength==5&&std::memcmp(fadeValue,"clear",5)==0);
        size_t presentationLength{};const auto* presentationValue=!presentationStatus?getString(state,-2,&presentationLength):nullptr;
        constexpr std::string_view popupPrefix="native-popup:";
        const bool nativePopupActive=!presentationStatus&&presentationValue&&presentationLength>popupPrefix.size()
            &&std::memcmp(presentationValue,popupPrefix.data(),popupPrefix.size())==0;
        if(nativePopupActive){presentationValue+=popupPrefix.size();presentationLength-=popupPrefix.size();}
        const bool nativeCanvasActive=nativePopupActive&&
            ((presentationLength==11&&std::memcmp(presentationValue,"mission-end",11)==0)
             ||(presentationLength==6&&std::memcmp(presentationValue,"sortie",6)==0));
        const bool avatarEditorActive=!presentationStatus&&presentationValue&&presentationLength==11
            &&std::memcmp(presentationValue,"avatar-edit",11)==0;
        const bool titleCabinActive=!presentationStatus&&presentationValue&&presentationLength==11
            &&std::memcmp(presentationValue,"title-cabin",11)==0;
        const bool hospitalTitleMenuActive=!presentationStatus&&presentationValue&&presentationLength==19
            &&std::memcmp(presentationValue,"title-hospital-menu",19)==0;
        const bool scriptedDemoTitleActive=!presentationStatus&&presentationValue&&presentationLength==19
            &&std::memcmp(presentationValue,"scripted-demo-title",19)==0;
        const bool openingDemoActive=!presentationStatus&&presentationValue&&presentationLength==21
            &&std::memcmp(presentationValue,"scripted-opening-demo",21)==0;
        const bool openingLookActive=!presentationStatus&&presentationValue&&presentationLength==23
            &&std::memcmp(presentationValue,"scripted-opening-look-",22)==0
            &&(presentationValue[22]=='0'||presentationValue[22]=='1');
        constexpr std::string_view staleCandidatePrefix="stale-demo-candidate:";
        const bool staleDemoCandidate=!presentationStatus&&presentationValue
            &&presentationLength>staleCandidatePrefix.size()
            &&std::memcmp(presentationValue,staleCandidatePrefix.data(),staleCandidatePrefix.size())==0;
        const auto staleCandidateIdentity=staleDemoCandidate
            ?candidateKey(presentationValue+staleCandidatePrefix.size(),presentationLength-staleCandidatePrefix.size()):0;
        const bool scriptedLookTitleActive=!presentationStatus&&presentationValue&&presentationLength==19
            &&std::memcmp(presentationValue,"scripted-look-title",19)==0;
        const bool hospitalLookActive=!presentationStatus&&presentationValue&&presentationLength==15
            &&std::memcmp(presentationValue,"scripted-look-",14)==0
            &&(presentationValue[14]=='0'||presentationValue[14]=='1');
        const bool scriptedLookActive=openingLookActive||scriptedLookTitleActive||hospitalLookActive;
        const uint64_t lookLessonKey=(openingLookActive||hospitalLookActive)
            ?uint64_t(presentationValue[presentationLength-1]-'0')+1:0;
        const bool titleActive=titleCabinActive||hospitalTitleMenuActive||openingDemoActive||openingLookActive||scriptedDemoTitleActive||scriptedLookTitleActive||(!presentationStatus&&presentationValue&&presentationLength==5
            &&std::memcmp(presentationValue,"title",5)==0);
        const bool cabinActive=!presentationStatus&&presentationValue&&presentationLength==5
            &&std::memcmp(presentationValue,"cabin",5)==0;
        const bool cabinTransit=!presentationStatus&&presentationValue&&presentationLength==12
            &&std::memcmp(presentationValue,"cabin-flight",12)==0;
        const bool profileMenuActive=!presentationStatus&&presentationValue&&presentationLength==12
            &&std::memcmp(presentationValue,"profile-menu",12)==0;
        const bool sceneMenuActive=!presentationStatus&&presentationValue
            &&(nativePopupActive||profileMenuActive||(presentationLength==10&&std::memcmp(presentationValue,"cabin-menu",10)==0)
                ||(presentationLength==9&&std::memcmp(presentationValue,"game-over",9)==0));
        const bool scriptedDemoActive=profileMenuActive||openingDemoActive||scriptedDemoTitleActive||(!presentationStatus&&presentationValue&&presentationLength==13
            &&std::memcmp(presentationValue,"scripted-demo",13)==0);
        publishNativeAvatarEdit(avatarEditorActive);
        const bool knownDemoPresentation=hospitalTitleMenuActive||scriptedLookActive||scriptedDemoActive||staleDemoCandidate
            ||(presentationValue&&((presentationLength==5&&std::memcmp(presentationValue,"title",5)==0)
                ||(presentationLength==5&&std::memcmp(presentationValue,"cabin",5)==0)
                ||(presentationLength==6&&std::memcmp(presentationValue,"closed",6)==0)
                ||avatarEditorActive||titleCabinActive||sceneMenuActive||cabinTransit));
        // A failed or malformed Lua query is not a negative demo signal. Keep
        // the previous conservative mode; its candidate heartbeat expires.
        if(!presentationStatus&&knownDemoPresentation)
            publishNativeDemoMode(scriptedLookActive?NativeDemoMode::interactiveLook:
                scriptedDemoActive?NativeDemoMode::cinematic:
                staleDemoCandidate?NativeDemoMode::staleCandidate:NativeDemoMode::none,
                staleCandidateIdentity,lookLessonKey);
        publishNativeTitleMode(titleActive);
        publishNativeHospitalTitleMenu(hospitalTitleMenuActive);
        publishNativeOpeningCinematic(openingDemoActive||openingLookActive);
        publishNativeTitleCabinMode(titleCabinActive);
        publishNativeCabinPlay(cabinActive);
        publishNativeCabinTransit(cabinTransit);
        publishNativeSceneMenu(sceneMenuActive);
        publishNativeSceneCanvas(nativeCanvasActive);
        publishNativeProfileMenu(profileMenuActive&&!nativePopupActive);
        setTop(state,top);
        const bool requested=openingCabinEnabled()&&nativeTitleMenuOpen()&&headCamera().status().active;
        const std::string script=std::string("local requested=")+(requested?"true\n":"false\n")+std::string(nativeCabinScript);
        int status=load(state,script.data(),script.size(),"@mgs5vr-native-cabin");
        if(!status)status=original(state,0,1,0);
        size_t length{};const auto* value=getString(state,-1,&length);
        const std::string result=value?std::string(value,std::min(length,size_t{500})):"no result";
        setTop(state,top);
        static std::string previous;
        const auto report=std::to_string(status)+":"+result;
        if(report!=previous){log("Native cabin lifecycle "+report);previous=report;}
    }
    if(!externalCommands)return;
    traceCommands();
    std::error_code error;
    if(!std::filesystem::exists(requestPath,error))return;
    const auto size=std::filesystem::file_size(requestPath,error);
    if(error||!size||size>65536)return;
    std::ifstream input(requestPath,std::ios::binary);
    std::string request(static_cast<size_t>(size),'\0');
    if(!input.read(request.data(),static_cast<std::streamsize>(size)))return;
    input.close();
    if(request=="inspect-animal-touch"||request=="native-dog-response"||request=="inspect-small-animals"){
        const int top=getTop(state);
        constexpr char check[]="return type(TppMain)=='table' and type(GameObject)=='table' and type(gvars)=='table' and type(vars)=='table' and type(TppMission)=='table' and type(TppBuddyService)=='table' and 'ready' or 'wait'";
        int status=load(state,check,sizeof(check)-1,"@mgs5vr-state-check");
        if(!status)status=original(state,0,1,0);
        size_t length{};const auto* value=getString(state,-1,&length);
        const bool ready=!status&&value&&length==5&&std::memcmp(value,"ready",5)==0;
        setTop(state,top);
        if(!ready)return;
        const auto result=request=="inspect-animal-touch"?inspectAnimalTouch():request=="inspect-small-animals"?inspectSmallAnimals():requestNativeDogResponse();
        std::ofstream output(resultPath,std::ios::binary|std::ios::trunc);output<<"OK\n"<<result;output.close();
        std::filesystem::remove(requestPath,error);return;
    }
    const int top=getTop(state);
    if(top<0||top>4096)return;
    const std::string script="if type(TppMain)~='table' or type(GameObject)~='table' or type(gvars)~='table' or type(vars)~='table' or type(TppMission)~='table' or type(TppBuddyService)~='table' then return '__MGS5VR_WAIT__' end\n"+request;
    int status=load(state,script.data(),script.size(),"@mgs5vr-native-actions");
    if(!status)status=original(state,0,1,0);
    size_t length{};const auto* text=getString(state,-1,&length);
    const std::string result=text?std::string(text,(std::min)(length,size_t{32768})):"(no return value)";
    setTop(state,top);
    if(!status&&result=="__MGS5VR_WAIT__")return;
    std::ofstream output(resultPath,std::ios::binary|std::ios::trunc);
    output<<(status?"ERROR\n":"OK\n")<<result;output.close();
    std::filesystem::remove(requestPath,error);
    log("Native action request completed status="+std::to_string(status)+" result="+result.substr(0,500));
}
int call(void* state,int arguments,int results,int handler){
    ++depth;
    const auto status=original(state,arguments,results,handler);
    if(enabled.load()&&depth==1)try{pump(state);}catch(const std::exception& e){log(std::string("Native action request failed: ")+e.what());}
    --depth;return status;
}
template<size_t N> bool matches(uintptr_t address,const std::array<unsigned char,N>& expected){
    std::array<unsigned char,N> bytes{};SIZE_T count{};
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),bytes.data(),N,&count)
        &&count==N&&bytes==expected;
}
}
void installNativeActions(uintptr_t base,const std::filesystem::path& folder,bool allowExternalCommands){
    if(!matches(base+0x1a116c0,std::array<unsigned char,13>{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x40,0x41,0x8b,0xf8})
       ||!matches(base+0x1a178e0,std::array<unsigned char,9>{0x48,0x83,0xec,0x38,0x48,0x89,0x54,0x24,0x20})
       ||!matches(base+0x1a112e0,std::array<unsigned char,13>{0x48,0x8b,0x41,0x10,0x48,0x2b,0x41,0x18,0x48,0xc1,0xf8,0x04,0xc3})
       ||!matches(base+0x1a11f70,std::array<unsigned char,7>{0x85,0xd2,0x78,0x42,0x4c,0x63,0xc2})
       ||!matches(base+0x1a12150,std::array<unsigned char,10>{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10}))
        throw std::runtime_error("Native Lua action ABI differs");
    load=reinterpret_cast<Load>(base+0x1a178e0);getTop=reinterpret_cast<Top>(base+0x1a112e0);
    setTop=reinterpret_cast<SetTop>(base+0x1a11f70);getString=reinterpret_cast<String>(base+0x1a12150);
    requestPath=folder/L"mgs5vr-native-actions.lua";resultPath=folder/L"mgs5vr-native-actions-result.txt";
    imageBase=base;tracePath=folder/L"mgs5vr-animal-trace.txt";
    installNativeTextInput(base);
    // UiPad vtable slot 0x120 is the native input-mask predicate. All ordinary
    // button/axis getters consult it; retain its existing tutorial count.
    const bool inputMaskAbi=matches(base+0x1dd7eb0,std::array<unsigned char,9>{0x33,0xc0,0x39,0x41,0x18,0x0f,0x97,0xc0,0xc3})
        &&read<uintptr_t>(base+0x2548780)==base+0x1dd7eb0;
    auto* inputMask=reinterpret_cast<void*>(base+0x1dd7eb0);
    const bool inputMaskInstalled=inputMaskAbi
        &&MH_CreateHook(inputMask,reinterpret_cast<void*>(&uiInputMasked),reinterpret_cast<void**>(&originalUiInputMasked))==MH_OK
        &&MH_EnableHook(inputMask)==MH_OK;
    setHandheldMenuRoutingAvailable(inputMaskInstalled);
    log(inputMaskInstalled?"Handheld iDroid UI input gate verified; held focus can transfer movement ownership"
        :"Handheld iDroid UI input gate unavailable; existing menu input ownership retained");
    auto* address=reinterpret_cast<void*>(base+0x1a116c0);
    if(MH_CreateHook(address,reinterpret_cast<void*>(&call),reinterpret_cast<void**>(&original))!=MH_OK||MH_EnableHook(address)!=MH_OK)
        throw std::runtime_error("Cannot install native action queue");
    externalCommands=allowExternalCommands;
    const auto installInputHook=[&](uintptr_t rva,void* detour,void** trampoline){
        auto* target=reinterpret_cast<void*>(base+rva);
        return MH_CreateHook(target,detour,trampoline)==MH_OK&&MH_EnableHook(target)==MH_OK;
    };
    constexpr std::array<unsigned char,10> buttonSignature{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20};
    const bool buttonInstalled=matches(base+0x1dd7ba0,buttonSignature)
        &&installInputHook(0x1dd7ba0,reinterpret_cast<void*>(&uiButton),reinterpret_cast<void**>(&originalUiButton));
    if(externalCommands&&matches(base+0x92a160,std::array<unsigned char,12>{0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x48,0x8b,0x49}))
        installInputHook(0x92a160,reinterpret_cast<void*>(&uiClose),reinterpret_cast<void**>(&originalUiClose));
    // UiPad slot 0x40 is the logical edge queried by native Terms. The
    // expiring observer retains the original result without a fallback edge.
    const bool edgeAbi=matches(base+0x1dd78c0,std::array<unsigned char,10>{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20})
        &&read<uintptr_t>(base+0x227d8b0)==base+0x1dd78c0;
    const bool edgeInstalled=edgeAbi&&installInputHook(0x1dd78c0,reinterpret_cast<void*>(&uiEdge),reinterpret_cast<void**>(&originalUiEdge));
    if(edgeInstalled)log("Native Terms button-edge observer verified");
    const bool triggerGate=buttonInstalled&&edgeInstalled
        &&matches(base+0x1dd7980,buttonSignature)
        &&matches(base+0x1dd7a40,std::array<unsigned char,8>{0x48,0x8b,0xc4,0x57,0x48,0x83,0xec,0x60})
        &&matches(base+0x1dd7760,std::array<unsigned char,6>{0x40,0x57,0x48,0x83,0xec,0x40})
        &&installInputHook(0x1dd7980,reinterpret_cast<void*>(&uiRelease),reinterpret_cast<void**>(&originalUiRelease))
        &&installInputHook(0x1dd7a40,reinterpret_cast<void*>(&uiRepeat),reinterpret_cast<void**>(&originalUiRepeat))
        &&installInputHook(0x1dd7760,reinterpret_cast<void*>(&uiRepeatState),reinterpret_cast<void**>(&originalUiRepeatState));
    setHandheldFocusedWeaponRoutingAvailable(triggerGate);
    log(triggerGate?"Handheld iDroid held-firearm trigger/UI ownership adapter installed"
        :"Handheld iDroid firearm exception unavailable: native trigger-query ABI differs");
    auto* tabs=reinterpret_cast<void*>(base+0x878940);
    auto* tabAction=reinterpret_cast<void*>(base+0x915890);
    if(matches(base+0x878940,std::array<unsigned char,13>{0x40,0x53,0x48,0x83,0xec,0x30,0x0f,0xb6,0x81,0xda,0x17,0,0})
       &&matches(base+0x878a05,std::array<unsigned char,10>{0x8b,0xc8,0xe8,0x84,0xce,0x09,0,0x0f,0xb6,0xf8})
       &&matches(base+0x878bb2,std::array<unsigned char,20>{0x0f,0xb6,0x8b,0xda,0x17,0,0,0xff,0xc9,0x74,0x0e,
            0x83,0xe9,0x02,0x0f,0x84,0x6c,0xff,0xff,0xff})
       &&matches(base+0x915890,std::array<unsigned char,9>{0x40,0x53,0x48,0x83,0xec,0x20,0x8b,0xd9,0xe8})
       &&matches(base+0x91ed80,std::array<unsigned char,7>{0xb1,0x15,0xe9,0x49,0xfd,0xc1,0xff})
       &&matches(base+0x91ed20,std::array<unsigned char,20>{0x48,0xb8,0x98,0x9f,0x16,0xbf,0xa0,0xb8,0,0,
            0xc7,0x81,0x30,0x08,0,0,0,0,0,0})
       &&MH_CreateHook(tabAction,reinterpret_cast<void*>(&uiTabAction),reinterpret_cast<void**>(&originalUiTabAction))==MH_OK
       &&MH_EnableHook(tabAction)==MH_OK
       &&MH_CreateHook(tabs,reinterpret_cast<void*>(&uiTabUpdate),reinterpret_cast<void**>(&originalUiTabUpdate))==MH_OK
       &&MH_EnableHook(tabs)==MH_OK)log("Native iDroid root tab update verified; grip next-tab wrap available");
    auto* eula=reinterpret_cast<void*>(base+0x89ccf0);
    if(matches(base+0x89ccf0,std::array<unsigned char,7>{0x80,0xb9,0xc0,0,0,0,0})
       &&MH_CreateHook(eula,reinterpret_cast<void*>(&eulaUpdate),reinterpret_cast<void**>(&originalEulaUpdate))==MH_OK
       &&MH_EnableHook(eula)==MH_OK)log("Native Terms modal owner verified; menu sticks retained");
    const bool restrictionAbi=matches(base+0x9683d0,std::array<unsigned char,9>{0x4c,0x8b,0xc9,0x0f,0xb6,0xc2,0x45,0x84,0xc0})
        &&matches(base+0xf42840,std::array<unsigned char,11>{0x48,0x8b,0x46,0x28,0x41,0xb0,0x01,0x40,0x0f,0xb6,0xd7})
        &&matches(base+0xf42855,std::array<unsigned char,9>{0xff,0x90,0xa0,0,0,0,0x40,0xfe,0xc7});
    if(restrictionAbi){
        auto* disable=reinterpret_cast<void*>(base+0x9683d0);
        menuRestrictionAbi=MH_CreateHook(disable,reinterpret_cast<void*>(&uiDisable),reinterpret_cast<void**>(&originalUiDisable))==MH_OK
            &&MH_EnableHook(disable)==MH_OK;
        if(menuRestrictionAbi)log("Native tutorial menu-restriction ownership captured; completed ACC guide recovery available");
    }
    enabled.store(true);
    if(externalCommands){actionPipeStopping.store(false);actionPipeThread=std::thread(actionPipeLoop);}
    log(externalCommands?"Native title reader and local action queue installed":"Native title reader installed; external actions disabled");
}
void stopNativeActions() noexcept{
    stopNativeTextInput();
    requestCameraFreeze(false);
    enabled.store(false);setHandheldMenuRoutingAvailable(false);
    publishHandheldMenuControl(HandheldMenuControl::native,0);
    publishHandheldFocusedWeapon(false,0);setHandheldFocusedWeaponRoutingAvailable(false);
    requestNativeIdroidClose(false);requestHandheldIdroidOpen(0);cancelHandheldIdroidBorrow();publishHandheldIdroidOwner(0,0);actionPipeStopping.store(true);
    std::shared_ptr<ActionConnection> connection;{std::lock_guard lock(connectionMutex);connection=activeConnection;}
    if(connection)connection->responseReady.notify_all();
    if(connection&&connection->pipe!=INVALID_HANDLE_VALUE)CancelIoEx(connection->pipe,nullptr);
    wakeActionPipe();
    if(actionPipeThread.joinable())actionPipeThread.join();
    std::lock_guard lock(fastQueueMutex);fastQueue.clear();
}
}
