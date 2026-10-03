#pragma once
#include "controls.hpp"
#include "input_bridge.hpp"

namespace mgs5vr {
enum class NativeMenuInput { menu, liveIdroid, scriptedScene, cinematic, cinematicLook };
// A visible native menu always owns navigation, including during a cutscene.
NativeMenuInput nativeSceneInputMode(bool menuOpen,bool scriptedDemo,bool interactiveLook,
    bool sceneFallback,bool handheldIdroid) noexcept;
GamepadSample nativeMenuGamepad(const ControlBindings& bindings,const PhysicalControls& physical,NativeMenuInput mode);
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
// Transfer a short-lived recovery request from XR input to the native Lua job.
void requestNativeIdroidClose(bool requested) noexcept;
bool takeNativeIdroidClose() noexcept;
// Only the mod's spatial field terminal owns this world pause. Verified ACC
// cabin play already owns the seated player and must keep its native tasks live.
constexpr bool nativeIdroidWorldPauseRequired(bool open,bool closing,bool handheld,bool cabinPlay) noexcept {
    return open&&!closing&&!handheld&&!cabinPlay;
}
// The native Lua job owns the pause registration; XR only publishes the mode.
void setHandheldMenus(bool enabled) noexcept;
bool handheldMenusSelected() noexcept;
// The native Lua job acknowledges its own player-pad exclusion. Until then,
// handheld menu sticks must not leak into character locomotion.
void setHandheldMenuInputReady(bool ready) noexcept;
bool handheldMenuInputReady() noexcept;
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
