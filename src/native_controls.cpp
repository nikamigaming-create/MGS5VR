#include "mgs5vr/native_controls.hpp"
#include "mgs5vr/native_input_delivery.hpp"
#include <cmath>
#include <algorithm>
#include <atomic>
#include <mutex>

namespace mgs5vr {
namespace {
std::mutex termsMutex;
NativeTermsMenu termsMenu;
}
NativeTermsMenu nativeTermsMenu() noexcept {
    std::lock_guard lock(termsMutex);return termsMenu;
}
void publishNativeTermsMenu(NativeTermsMenu value) noexcept {
    std::lock_guard lock(termsMutex);termsMenu=value;
}
namespace {
std::atomic_uint64_t idroidCloseRequestedAt{},handheldControlSample{};
std::atomic_uint64_t handheldAcknowledgement{};
std::atomic_uint64_t handheldWeaponSample{};
std::atomic_bool handheldWeaponRouting{};
std::atomic_bool handheldMenus{},handheldRoutingAvailable{};
std::mutex idroidOpenMutex;
HandheldIdroidOpenQueue idroidOpenRequests;
std::mutex idroidBorrowMutex;
HandheldIdroidBorrow idroidBorrow{};
std::atomic_uintptr_t idroidUiPlayer{};
std::atomic_uint64_t idroidUiActivation{};
}
void setHandheldMenus(bool enabled) noexcept {handheldMenus.store(enabled);}
bool handheldMenusSelected() noexcept {return handheldMenus.load();}
void setHandheldMenuInputReady(bool ready) noexcept {
    acknowledgeHandheldMenuControl(handheldMenuControl(),ready,steadyMilliseconds(),handheldFocusedWeapon());
}
bool handheldMenuInputReady() noexcept {
    const auto ack=handheldAcknowledgement.load(),time=ack>>4,now=steadyMilliseconds();
    return (ack&1)&&time&&now>=time&&now-time<=250
        &&bool(ack&8)==handheldFocusedWeapon(now)
        &&static_cast<HandheldMenuControl>((ack>>1)&3)==handheldMenuControl(now);
}
void acknowledgeHandheldMenuControl(HandheldMenuControl control,bool ready,uint64_t time,bool focusedWeapon) noexcept {
    handheldAcknowledgement.store((time<<4)|(static_cast<uint64_t>(control)<<1)|(ready?1:0)|(focusedWeapon?8:0));
}
void publishHandheldFocusedWeapon(bool active,uint64_t time) noexcept {
    handheldWeaponSample.store((time<<1)|(active?1:0));
}
bool handheldFocusedWeapon(uint64_t now) noexcept {
    const auto sample=handheldWeaponSample.load(),time=sample>>1;
    return (sample&1)&&time&&now>=time&&now-time<=250;
}
void setHandheldFocusedWeaponRoutingAvailable(bool available) noexcept {handheldWeaponRouting.store(available);}
bool handheldFocusedWeaponRoutingAvailable() noexcept {return handheldWeaponRouting.load();}
void setHandheldMenuRoutingAvailable(bool available) noexcept {handheldRoutingAvailable.store(available);}
bool handheldMenuRoutingAvailable() noexcept {return handheldRoutingAvailable.load();}
void publishHandheldMenuControl(HandheldMenuControl control,uint64_t time) noexcept {
    handheldControlSample.store((time<<2)|static_cast<uint64_t>(control));
}
HandheldMenuControl handheldMenuControl(uint64_t now) noexcept {
    const auto sample=handheldControlSample.load(),time=sample>>2;
    return time&&now>=time&&now-time<=250?static_cast<HandheldMenuControl>(sample&3):HandheldMenuControl::native;
}
void requestNativeIdroidClose(bool requested) noexcept {
    idroidCloseRequestedAt.store(requested?steadyMilliseconds():0);
}
bool takeNativeIdroidClose() noexcept {
    const auto requested=idroidCloseRequestedAt.exchange(0),now=steadyMilliseconds();
    return requested&&now>=requested&&now-requested<=1000;
}
void requestHandheldIdroidOpen(uint64_t activation) noexcept {
    std::lock_guard lock(idroidOpenMutex);
    idroidOpenRequests.request(activation,steadyMilliseconds());
}
HandheldIdroidOpenRequest pendingHandheldIdroidOpen() noexcept {
    std::lock_guard lock(idroidOpenMutex);
    return idroidOpenRequests.pending();
}
bool consumeHandheldIdroidOpen(HandheldIdroidOpenRequest expected) noexcept {
    std::lock_guard lock(idroidOpenMutex);
    return idroidOpenRequests.consume(expected);
}
void publishHandheldIdroidOwner(uintptr_t player,uint64_t activation) noexcept {
    idroidUiPlayer.store(0);
    idroidUiActivation.store(activation);
    idroidUiPlayer.store(player);
}
bool handheldIdroidOwned(uintptr_t player,uint64_t activation) noexcept {
    const auto current=idroidUiPlayer.load(),generation=idroidUiActivation.load();
    return current&&generation&&(!player||current==player)&&(!activation||generation==activation)
        &&current==idroidUiPlayer.load();
}
bool requestHandheldIdroidBorrow(uintptr_t player,uint64_t activation) noexcept {
    if(!player||!activation||!handheldIdroidOwned(player,activation))return false;
    std::lock_guard lock(idroidBorrowMutex);
    if(idroidBorrow.player)return idroidBorrow.player==player&&idroidBorrow.activation==activation;
    idroidBorrow={player,activation,steadyMilliseconds(),false,false};return true;
}
HandheldIdroidBorrow handheldIdroidBorrow(uint64_t now) noexcept {
    std::lock_guard lock(idroidBorrowMutex);
    if(idroidBorrow.player&&(now<idroidBorrow.time||now-idroidBorrow.time>10000))idroidBorrow={};
    return idroidBorrow;
}
void acknowledgeHandheldIdroidBorrow(HandheldIdroidBorrow expected) noexcept {
    std::lock_guard lock(idroidBorrowMutex);
    if(expected.player&&expected.player==idroidBorrow.player&&expected.activation==idroidBorrow.activation
            &&expected.time==idroidBorrow.time)idroidBorrow.stopped=true;
}
void returnHandheldIdroidBorrow(HandheldIdroidBorrow expected) noexcept {
    std::lock_guard lock(idroidBorrowMutex);
    if(expected.player&&expected.player==idroidBorrow.player&&expected.activation==idroidBorrow.activation
            &&expected.time==idroidBorrow.time)idroidBorrow.returnRequested=true;
}
void cancelHandheldIdroidBorrow() noexcept {
    std::lock_guard lock(idroidBorrowMutex);idroidBorrow={};
}
bool IdroidBackRecovery::update(bool idroidOpen,bool back,bool available,uint64_t time) noexcept {
    if(!available||!idroidOpen||time<lastTime_){suspend();lastTime_=time;return false;}
    lastTime_=time;
    if(!back){held_=sent_=releaseRequired_=false;return false;}
    if(releaseRequired_)return false;
    if(!held_){held_=true;since_=time;}
    if(!sent_&&time-since_>=750){sent_=true;return true;}
    return false;
}
HandheldPlayerPadPolicy IdroidPadRelease::update(HandheldPlayerPadPolicy requested,bool terminalActive,
        const ControlInputAudit& mapped,const NativeInputDeliverySnapshot& delivered,uint64_t now) noexcept {
    if(now<last_)reset();
    last_=now;
    if(requested!=HandheldPlayerPadPolicy::released){
        policy_=requested;waiting_=false;neutralMappedAt_=0;return policy_;
    }
    // Ordinary focus release keeps its existing locomotion route. Only a
    // terminal stow starts this barrier; a quick reopen cannot cancel it.
    if(policy_==HandheldPlayerPadPolicy::released||(terminalActive&&!waiting_)){
        policy_=HandheldPlayerPadPolicy::released;return policy_;
    }
    const auto& packet=delivered.index0;
    if(!waiting_){
        waiting_=true;since_=now;afterDelivery_=packet.deliveryCounter;neutralMappedAt_=0;
        return policy_;
    }
    const bool weaponOnly=policy_==HandheldPlayerPadPolicy::weaponOnly;
    const bool freshMapped=freshControlInputAudit(mapped,now)&&mapped.time>since_;
    // Axis ownership already quarantines a held menu stick until centering.
    // A fresh gameplay movement after that handoff must not keep the player
    // excluded until the user stops walking again. Back/buttons and trigger
    // release still require a real subsequent retail XInput return.
    const bool locomotion=freshMapped&&mapped.rigInput
        &&(mapped.context==ControlContext::gameplay||mapped.context==ControlContext::horse
            ||mapped.context==ControlContext::vehicle||mapped.context==ControlContext::binoculars);
    const bool mappedNeutral=freshMapped&&!mapped.nativeButtons
        &&(locomotion||mapped.nativeAxes==std::array<int16_t,4>{})
        &&(weaponOnly||mapped.nativeTriggers==std::array<uint8_t,2>{});
    if(!mappedNeutral)neutralMappedAt_=0;
    else if(!neutralMappedAt_)neutralMappedAt_=mapped.time;
    const bool deliveredNeutral=delivered.available&&packet.index==0&&packet.packetValid&&!packet.resultCode
        &&packet.caller.inGame&&packet.deliveryCounter>afterDelivery_&&packet.timeMs>since_
        &&now>=packet.timeMs&&now-packet.timeMs<=250&&!packet.buttons
        &&(packet.axes==std::array<int16_t,4>{}||(locomotion&&packet.source==NativeInputDeliverySource::xrMailbox))
        &&(weaponOnly||packet.triggers==std::array<uint8_t,2>{});
    const bool xrRelease=packet.source==NativeInputDeliverySource::xrMailbox
        &&neutralMappedAt_&&packet.timeMs>=neutralMappedAt_;
    // Session shutdown or a physical-pad handoff can stop XR publication.
    // Their real neutral return still releases the mod's own exclusion.
    const bool inactiveRelease=packet.source==NativeInputDeliverySource::nativeXInput
        ||(packet.source==NativeInputDeliverySource::xrMailbox&&!packet.xrMailboxFresh);
    if(deliveredNeutral&&(xrRelease||inactiveRelease)){
        policy_=HandheldPlayerPadPolicy::released;waiting_=false;neutralMappedAt_=0;
    }
    return policy_;
}
GamepadSample cabinTitleGamepad(GamepadSample sample,bool spatialTitle,bool confirm) noexcept {
    if(!spatialTitle)return sample;
    GamepadSample result{};
    if(confirm)result.buttons=0x1000;
    return result;
}
NativeMenuInput nativeSceneInputMode(bool menuOpen,bool scriptedDemo,bool interactiveLook,
    bool sceneFallback,bool handheldIdroid) noexcept {
    if(menuOpen)return handheldIdroid?NativeMenuInput::liveIdroid:NativeMenuInput::menu;
    if(scriptedDemo)return interactiveLook?NativeMenuInput::cinematicLook:NativeMenuInput::cinematic;
    return sceneFallback?NativeMenuInput::scriptedScene:NativeMenuInput::menu;
}
GamepadSample IdroidTabInput::update(GamepadSample sample,bool focused,uint64_t time) noexcept {
    if(!focused||time<last_){held_.fill(true);until_.fill(0);}
    last_=time;
    if(!focused)return sample;
    for(unsigned side=0;side<2;++side){
        const uint16_t bit=side?0x200:0x100;
        const bool down=(sample.buttons&bit)!=0;
        if(down&&!held_[side])until_[side]=time+60;
        held_[side]=down;
        if(!down)until_[side]=0;
        if(time>=until_[side])sample.buttons&=~bit;
    }
    return sample;
}
void constrainCinematicInput(GamepadSample& sample,NativeMenuInput mode) noexcept {
    if(mode!=NativeMenuInput::cinematic&&mode!=NativeMenuInput::cinematicLook)return;
    sample.leftX=sample.leftY=0;sample.leftTrigger=sample.rightTrigger=0;
    if(mode==NativeMenuInput::cinematic)sample.rightX=sample.rightY=0;
}
std::array<float,2> HospitalHeadLook::update(Quat head,bool available,uint64_t now,uint64_t epoch,uint64_t key) noexcept {
    if(!available||!now||!epoch||(key!=1&&key!=2)||!valid(Pose{head,{}})){reset();return {};}
    if(!last_||epoch_!=epoch||key_!=key||now<last_||now-last_>250){
        head_=head;last_=now;epoch_=epoch;key_=key;gesture_={};until_=0;armed_=true;return {};
    }
    // Retail averages a window of stick history. A brief angular velocity
    // sample is diluted by neutral frames, even after a deliberate head turn.
    // Use a held physical look gesture for at most two seconds. The authored
    // eye anchor stays fixed while retail consumes it, so this cannot double
    // the player's head motion. Returning to neutral releases and rearms it.
    const auto delta=compose(inverse(Pose{head_,{}}),Pose{head,{}}).orientation;
    const auto forward=rotate(delta,Vec3{0,0,-1});
    const float yaw=std::atan2(forward.x,-forward.z);
    const float pitch=std::asin(std::clamp(forward.y,-1.f,1.f));
    const float angle=std::max(std::abs(yaw),std::abs(pitch));
    if(angle<.08f){armed_=true;gesture_={};until_=0;}
    else if(armed_&&angle>=.18f){
        gesture_=std::abs(pitch)>=std::abs(yaw)?std::array<float,2>{0,std::copysign(1.f,pitch)}:
            std::array<float,2>{std::copysign(1.f,yaw),0};
        until_=now+2000;armed_=false;
    }
    last_=now;return now<until_?gesture_:std::array<float,2>{};
}
GamepadSample nativeMenuGamepad(const ControlBindings& bindings,const PhysicalControls& physical,NativeMenuInput mode){
    GamepadSample pad;
    const bool handheld=mode==NativeMenuInput::liveIdroid&&handheldMenuRoutingAvailable();
    const bool previous=handheld&&bindings.active("idroid.previous_tab");
    const bool next=handheld&&bindings.active("idroid.next_tab");
    // A one-shot tab pulse can expire while its physical chord remains down.
    // Keep those inputs consumed until release; do not turn that same hold
    // into Confirm, another tab, map movement or trigger zoom afterwards.
    const auto consumed=handheld?(bindings.activeInputMask("idroid.previous_tab")
        |bindings.retainedInputMask("idroid.previous_tab")
        |bindings.activeInputMask("idroid.next_tab")
        |bindings.retainedInputMask("idroid.next_tab")):uint32_t{};
    const auto available=[&](std::string_view name){return !(bindings.activeInputMask(name)&consumed);};
    struct Button {std::string_view name;uint16_t mask;};
    constexpr Button buttons[]{
        {"menus.confirm",0x1000},{"menus.back",0x2000},{"menus.action_x",0x4000},{"menus.action_y",0x8000},
        {"menus.previous_tab",0x100},{"menus.next_tab",0x200},{"menus.left_click",0x40},{"menus.right_click",0x80},
        {"menus.dpad_up",1},{"menus.dpad_down",2},{"menus.dpad_left",4},{"menus.dpad_right",8}};
    for(const auto& [name,mask]:buttons){
        if(handheld&&name=="menus.previous_tab")continue;
        if(bindings.active(name)&&available(name))pad.buttons|=mask;
    }
    if(previous)pad.buttons|=0x100;
    if(next)pad.buttons|=0x200;
    pad.leftTrigger=available("menus.left_trigger")?static_cast<uint8_t>(bindings.value("menus.left_trigger")*255):0;
    pad.rightTrigger=available("menus.right_trigger")?static_cast<uint8_t>(bindings.value("menus.right_trigger")*255):0;
    const bool menu=mode==NativeMenuInput::menu||mode==NativeMenuInput::liveIdroid;
    const auto moveName=menu?"axes.menu":"axes.move";
    const auto lookName=mode==NativeMenuInput::scriptedScene||mode==NativeMenuInput::cinematicLook?"axes.native_look":"axes.map";
    const auto move=(bindings.axisInputMask(moveName)&consumed)?std::array<float,2>{}:bindings.axis(moveName,physical);
    const auto look=(bindings.axisInputMask(lookName)&consumed)?std::array<float,2>{}:bindings.axis(lookName,physical);
    pad.leftX=static_cast<int16_t>(move[0]*32767);pad.leftY=static_cast<int16_t>(move[1]*32767);
    pad.rightX=static_cast<int16_t>(look[0]*32767);pad.rightY=static_cast<int16_t>(look[1]*32767);
    constrainCinematicInput(pad,mode);
    return pad;
}
uint16_t CinematicMenuNavigation::update(const GamepadSample& sample,bool enabled,uint64_t time) noexcept {
    if(!enabled||time<last_){
        last_=time;direction_=0;available_=false;releaseRequired_=true;
        return 0;
    }
    last_=time;
    if(!available_){available_=true;releaseRequired_=true;}
    const int x=sample.leftX,y=sample.leftY;
    const bool neutral=std::abs(x)<11469&&std::abs(y)<11469;
    if(sample.buttons&0xf){
        // A player's explicitly configured D-pad input owns this packet.
        direction_=0;releaseRequired_=true;return 0xffff;
    }
    if(neutral){direction_=0;releaseRequired_=false;}
    if(releaseRequired_)return 0;
    if(!direction_&&std::max(std::abs(x),std::abs(y))>22937){
        direction_=std::abs(y)>=std::abs(x)?(y>0?1:2):(x<0?4:8);
        return direction_;
    }
    return 0;
}
void CinematicMenuDirectionQueue::publish(uint64_t activation,uint16_t direction,uint64_t time) noexcept {
    if(!activation||activation!=activation_||time<sampleTime_)*this={};
    activation_=activation;sampleTime_=time;
    if(!activation)return;
    if(direction){
        direction_=direction==4||direction==8?direction:0;
        requestTime_=direction_?time:0;delivered_=false;
    }
}
std::optional<bool> CinematicMenuDirectionQueue::take(uint64_t activation,uint32_t index,uint32_t bit,uint64_t time) noexcept {
    if(!activation||activation!=activation_||!sampleTime_||time<sampleTime_||time-sampleTime_>250
       ||!requestTime_||time<requestTime_||time-requestTime_>250||index||(bit!=14&&bit!=15))return {};
    if(!delivered_&&bit==(direction_==4?14u:15u)){delivered_=true;return true;}
    return false;
}
namespace {
std::mutex cinematicDirectionMutex;
CinematicMenuDirectionQueue cinematicDirections;
}
void publishCinematicMenuDirection(uint64_t activation,uint16_t direction,uint64_t time) noexcept {
    std::lock_guard lock(cinematicDirectionMutex);cinematicDirections.publish(activation,direction,time);
}
std::optional<bool> takeCinematicMenuDirection(uint64_t activation,uint32_t index,uint32_t bit,uint64_t time) noexcept {
    std::lock_guard lock(cinematicDirectionMutex);return cinematicDirections.take(activation,index,bit,time);
}
uint32_t vehicleWeaponConsumedInputs(const ControlBindings& bindings,const PhysicalControls& physical){
    uint32_t result{};
    for(const auto name:{"vehicle.ready_weapon","vehicle.fire","vehicle.reload"})
        result|=bindings.activeInputMask(name)|bindings.retainedInputMask(name)|bindings.heldInputMask(name,physical);
    return result;
}
GamepadSample vehicleWeaponGamepad(GamepadSample pad,const ControlBindings& bindings,uint32_t consumed){
    struct Button {std::string_view name;uint16_t mask;};
    constexpr Button buttons[]{
        {"vehicle.native_a",0x1000},{"vehicle.native_b",0x2000},
        {"vehicle.weapon_or_call",0x4000},{"vehicle.interact",0x8000},
        {"vehicle.left_click",0x40},{"vehicle.right_click",0x80}};
    for(const auto& [name,mask]:buttons)
        if(bindings.activeInputMask(name)&consumed)pad.buttons&=~mask;
    pad.buttons&=~uint16_t{0x300};
    if(bindings.axisInputMask("axes.vehicle_steering")&consumed)pad.leftX=pad.leftY=0;
    if(bindings.axisInputMask("axes.equipment")&consumed)pad.rightX=pad.rightY=0;
    return pad;
}
NativeControlSample NativeControls::update(const ControlBindings& bindings,const PhysicalControls& physical,bool screenMode){
    NativeControlSample result;
    if(screenMode_!=screenMode){screenMode_=screenMode;releaseRequired_=true;resetDpad();result.changed=true;}
    const bool toggle=bindings.active("system.native_buttons");
    if(toggle&&!priorToggle_){selected_=!selected_;releaseRequired_=true;resetDpad();result.changed=true;}
    priorToggle_=toggle;result.selected=selected();
    if(releaseRequired_){
        bool neutral=true;
        for(size_t n=0;n<11;++n)neutral=neutral&&std::isfinite(physical.buttons[n])&&physical.buttons[n]<=.09f;
        for(const auto stick:{physical.leftStick,physical.rightStick})
            for(const float axis:stick)neutral=neutral&&std::isfinite(axis)&&std::abs(axis)<.18f;
        if(neutral)releaseRequired_=false;
        result.exclusive=true;return result;
    }
    if(!selected())return result;
    result.exclusive=true;
    auto& pad=result.gamepad;
    for(const auto& button:nativeButtonDefinitions())if(bindings.active(button.name))pad.buttons|=button.mask;
    pad.leftTrigger=static_cast<uint8_t>(bindings.value("native.left_trigger")*255);
    pad.rightTrigger=static_cast<uint8_t>(bindings.value("native.right_trigger")*255);
    const auto move=bindings.axis("axes.native_move",physical),look=bindings.axis("axes.native_look",physical);
    pad.leftX=static_cast<int16_t>(move[0]*32767);pad.leftY=static_cast<int16_t>(move[1]*32767);
    pad.rightX=static_cast<int16_t>(look[0]*32767);pad.rightY=static_cast<int16_t>(look[1]*32767);
    // Menu + one right-stick flick holds the chosen D-pad button. After
    // centering, the same stick is free to browse the game's native equipment
    // cards while Menu keeps that D-pad button held. Releasing Menu releases it.
    // Direct remapped D-pad bindings also work without this optional modifier.
    const bool shift=bindings.active("native.dpad_hold")&&!(pad.buttons&0x0030);
    if(!shift){resetDpad();return result;}
    const bool neutral=std::abs(int(pad.rightX))<6000&&std::abs(int(pad.rightY))<6000;
    if(!shiftHeld_){resetDpad();shiftHeld_=true;waitDirectionNeutral_=!neutral;}
    const auto direction=static_cast<uint16_t>(pad.buttons&0x000f);
    pad.buttons&=~uint16_t{0x000f};
    if(!dpad_){
        if(neutral)waitDirectionNeutral_=false;
        if(!waitDirectionNeutral_&&direction){dpad_=direction;waitBrowseNeutral_=true;}
        pad.rightX=pad.rightY=0;
    }else if(waitBrowseNeutral_){
        if(neutral)waitBrowseNeutral_=false;
        pad.rightX=pad.rightY=0;
    }
    pad.buttons|=dpad_;
    return result;
}
}
