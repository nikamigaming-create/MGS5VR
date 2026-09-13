#pragma once
#include "controls.hpp"
#include "input_bridge.hpp"

namespace mgs5vr {
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
