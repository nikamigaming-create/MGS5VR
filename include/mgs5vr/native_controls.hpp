#pragma once
#include "controls.hpp"
#include "input_bridge.hpp"

namespace mgs5vr {
struct NativeTermsMenu {
    uint64_t time{};
    uintptr_t owner{};
    uint32_t phase{},horizontal{},vertical{},action{};
    uint8_t mode{};
    bool verified{},active{};
    bool ready(uint64_t now) const noexcept {
        return verified&&active&&owner&&phase==5&&time&&now>=time&&now-time<=250;
    }
};
NativeTermsMenu nativeTermsMenu() noexcept;
void publishNativeTermsMenu(NativeTermsMenu value) noexcept;
enum class NativeMenuInput { menu, liveIdroid, scriptedScene, cinematic, cinematicLook };
// A visible native menu always owns navigation, including during a cutscene.
NativeMenuInput nativeSceneInputMode(bool menuOpen,bool scriptedDemo,bool interactiveLook,
    bool sceneFallback,bool handheldIdroid) noexcept;
GamepadSample nativeMenuGamepad(const ControlBindings& bindings,const PhysicalControls& physical,NativeMenuInput mode);
// The personal gun consumes its effective physical inputs before native
// driving actions. In particular, a remapped reload must not also exit.
uint32_t vehicleWeaponConsumedInputs(const ControlBindings& bindings,const PhysicalControls& physical);
GamepadSample vehicleWeaponGamepad(GamepadSample pad,const ControlBindings& bindings,uint32_t consumed);
// The native iDroid repeats a held shoulder input. One VR grip squeeze must
// select only one tab, including subpage filters such as Free Roam.
class IdroidTabInput {
public:
    GamepadSample update(GamepadSample sample,bool focused,uint64_t time) noexcept;
private:
    std::array<uint64_t,2> until_{};
    std::array<bool,2> held_{true,true};
    uint64_t last_{};
};
// Retail root update 0x878940: the SECOND shoulder query returns at 0x878a0c
// into dil and advances Mother Base(1) -> Map(3) -> Missions(2). The first
// query returns at 0x8789fa into r14b and travels in the opposite direction.
inline constexpr uintptr_t nativeIdroidNextTabReturnRva=0x878a0c;
// Native pressed bits are readable more than once during a game input frame.
// Sharing this latch across tab-task owners prevents duplicate consumers from
// turning the same event into a native step followed by a boundary wrap.
class NativeIdroidTabEdge {
public:
    bool update(bool nativePressed) noexcept {
        const bool edge=nativePressed&&!pressed_;
        pressed_=nativePressed;
        return edge;
    }
private:
    bool pressed_{};
};
constexpr uint8_t nativeIdroidRootNextWrapTarget(uintptr_t callerRva,uint8_t currentTab,bool nativeNextEdge) noexcept {
    // A still-published XR shoulder pulse is not a second native button event.
    // Map may have advanced to Missions earlier in the same 60 ms pulse.
    return nativeNextEdge&&callerRva==nativeIdroidNextTabReturnRva&&currentTab==2?1:0;
}
// Only a firearm already held before focus may continue firing. Releasing
// it ends that exception; a new grip inside the menu belongs to its tabs.
class IdroidFocusWeapon {
public:
    bool update(bool available,bool focused,bool readyHeld,bool previouslyReady) noexcept {
        if(!available){reset();return false;}
        if(focused&&!focused_)carried_=readyHeld&&previouslyReady;
        if(!focused||!readyHeld)carried_=false;
        focused_=focused;
        return carried_;
    }
    void reset() noexcept {focused_=carried_=false;}
private:
    bool focused_{},carried_{};
};
void constrainCinematicInput(GamepadSample& sample,NativeMenuInput mode) noexcept;
// A spatial rack owns title selection. Sticks, grips and face buttons must not
// also navigate its hidden native menu; only a selected tape confirms it.
GamepadSample cabinTitleGamepad(GamepadSample sample,bool spatialTitle,bool confirm) noexcept;
// Keep native Back taps intact. A deliberate held Back can dismiss an iDroid
// terminal whose tutorial has disabled its ordinary cancel path.
class IdroidBackRecovery {
public:
    bool update(bool idroidOpen,bool back,bool available,uint64_t time) noexcept;
    void suspend() noexcept {held_=sent_=false;releaseRequired_=true;}
private:
    uint64_t since_{},lastTime_{};
    bool held_{},sent_{},releaseRequired_{true};
};
struct NativeInputDeliverySnapshot;
enum class HandheldPlayerPadPolicy : uint8_t { released, blocked, weaponOnly };
// Native Stop can expose the player before the game's previous XInput packet
// has been replaced. Keep the existing field exclusion through a witnessed
// release; a mapped XR zero alone does not prove that retail consumed it.
class IdroidPadRelease {
public:
    HandheldPlayerPadPolicy update(HandheldPlayerPadPolicy requested,bool terminalActive,
        const ControlInputAudit& mapped,const NativeInputDeliverySnapshot& delivered,uint64_t now) noexcept;
    void reset() noexcept {*this=IdroidPadRelease{};}
    bool waiting() const noexcept {return waiting_;}
private:
    HandheldPlayerPadPolicy policy_{};
    uint64_t since_{},afterDelivery_{},neutralMappedAt_{},last_{};
    bool waiting_{};
};
// Transfer a short-lived recovery request from XR input to the native Lua job.
void requestNativeIdroidClose(bool requested) noexcept;
bool takeNativeIdroidClose() noexcept;
// A VR Menu tap opens the native UI independently of the player's stationary
// phone action. The Lua owner must revalidate the field before consuming it.
struct HandheldIdroidOpenRequest { uint64_t activation{},time{}; };
void requestHandheldIdroidOpen(uint64_t activation) noexcept;
HandheldIdroidOpenRequest takeHandheldIdroidOpen() noexcept;
void publishHandheldIdroidOwner(uintptr_t player,uint64_t activation) noexcept;
bool handheldIdroidOwned(uintptr_t player=0,uint64_t activation=0) noexcept;
// A reload borrows the hand through the retail terminal Stop/Start lifecycle.
// Keep the exact handset owner while its UI is deliberately stopped; another
// player, activation, explicit stow or an expired request cancels the return.
struct HandheldIdroidBorrow {
    uintptr_t player{};
    uint64_t activation{},time{};
    bool stopped{},returnRequested{};
};
bool requestHandheldIdroidBorrow(uintptr_t player,uint64_t activation) noexcept;
HandheldIdroidBorrow handheldIdroidBorrow(uint64_t now=steadyMilliseconds()) noexcept;
void acknowledgeHandheldIdroidBorrow(HandheldIdroidBorrow expected) noexcept;
void returnHandheldIdroidBorrow(HandheldIdroidBorrow expected) noexcept;
void cancelHandheldIdroidBorrow() noexcept;
constexpr bool freshHandheldIdroidOpenRequest(HandheldIdroidOpenRequest request,uint64_t activation,uint64_t now) noexcept {
    return request.activation&&request.activation==activation&&request.time&&now>=request.time&&now-request.time<=1000;
}
// XR publishes the selected presentation; native Lua owns its input policy.
void setHandheldMenus(bool enabled) noexcept;
bool handheldMenusSelected() noexcept;
// The native Lua job acknowledges its own player-pad exclusion. Until then,
// handheld menu sticks must not leak into character locomotion.
void setHandheldMenuInputReady(bool ready) noexcept;
bool handheldMenuInputReady() noexcept;
enum class HandheldMenuControl : uint8_t { native, locomotion, menu, blocked };
void acknowledgeHandheldMenuControl(HandheldMenuControl control,bool ready,uint64_t time,bool focusedWeapon=false) noexcept;
void publishHandheldFocusedWeapon(bool active,uint64_t time) noexcept;
bool handheldFocusedWeapon(uint64_t now=steadyMilliseconds()) noexcept;
void setHandheldFocusedWeaponRoutingAvailable(bool available) noexcept;
bool handheldFocusedWeaponRoutingAvailable() noexcept;
void setHandheldMenuRoutingAvailable(bool available) noexcept;
bool handheldMenuRoutingAvailable() noexcept;
void publishHandheldMenuControl(HandheldMenuControl control,uint64_t time) noexcept;
HandheldMenuControl handheldMenuControl(uint64_t now=steadyMilliseconds()) noexcept;
constexpr bool handheldUiInputBlocked(HandheldMenuControl control,bool ready) noexcept {
    return control==HandheldMenuControl::locomotion||control==HandheldMenuControl::blocked
        ||(control==HandheldMenuControl::menu&&!ready);
}
struct NativeControlSample {
    GamepadSample gamepad{};
    bool selected{},exclusive{},changed{};
};
// One exclusive escape hatch to every native input; no gameplay-state, render,
// tracking-pose or native-menu-readiness dependency. Switching requires neutral
// physical controls before either layout can send anything to the game.
class NativeControls {
public:
    NativeControlSample update(const ControlBindings& bindings,const PhysicalControls& physical,bool screenMode=false);
    bool selected() const {return selected_||screenMode_;}
    void suspend(){releaseRequired_=true;priorToggle_=false;resetDpad();}
private:
    void resetDpad(){dpad_=0;shiftHeld_=waitDirectionNeutral_=waitBrowseNeutral_=false;}
    bool selected_{},priorToggle_{},releaseRequired_{},screenMode_{};
    bool shiftHeld_{},waitDirectionNeutral_{},waitBrowseNeutral_{};
    uint16_t dpad_{};
};
}
