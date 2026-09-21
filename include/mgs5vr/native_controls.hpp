#pragma once
#include "controls.hpp"
#include "input_bridge.hpp"

namespace mgs5vr {
enum class NativeMenuInput { menu, liveIdroid, scriptedScene, cinematic };
GamepadSample nativeMenuGamepad(const ControlBindings& bindings,const PhysicalControls& physical,NativeMenuInput mode);
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
struct NativeControlSample {
    GamepadSample gamepad{};
    bool selected{},exclusive{},changed{};
};
// One exclusive escape hatch to every native input; no gameplay-state, render,
// tracking-pose or native-menu-readiness dependency. Switching requires neutral
// physical controls before either layout can send anything to the game.
class NativeControls {
public:
    NativeControlSample update(const ControlBindings& bindings,const PhysicalControls& physical);
    bool selected() const {return selected_;}
    void suspend(){releaseRequired_=true;priorToggle_=false;resetDpad();}
private:
    void resetDpad(){dpad_=0;shiftHeld_=waitDirectionNeutral_=waitBrowseNeutral_=false;}
    bool selected_{},priorToggle_{},releaseRequired_{};
    bool shiftHeld_{},waitDirectionNeutral_{},waitBrowseNeutral_{};
    uint16_t dpad_{};
};
}
