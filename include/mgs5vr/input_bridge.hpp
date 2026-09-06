#pragma once
#include <cstdint>
#include <mutex>
#include <optional>
namespace mgs5vr {
struct GamepadSample {
    uint16_t buttons{};
    uint8_t leftTrigger{},rightTrigger{};
    int16_t leftX{},leftY{},rightX{},rightY{};
    bool operator==(const GamepadSample&) const = default;
};
// In tracked-rig mode, left trigger is a modifier: left stick selects the
// native equipment category, X calls/radios, and Y uses binoculars/scopes.
// Release the stick/buttons before returning them to movement and actions.
class RigEquipment {
public:
    GamepadSample update(GamepadSample sample,bool modifier,bool allowOptics=true);
    void reset(){blockedButtons_=0;blockedStick_=false;}
private:
    uint16_t blockedButtons_{};
    bool blockedStick_{};
};
// MGSV assigns Start to iDroid and Back to Pause. A short menu press emits
// Start on release; holding for 550 ms emits Back once, without opening iDroid.
class MenuButton {
public:
    uint16_t update(bool pressed, bool active, uint64_t milliseconds);
private:
    uint64_t pressedAt_{}, pulseUntil_{}, lastTime_{};
    uint16_t pulse_{};
    bool held_{}, longSent_{}, releaseRequired_{};
};
// A virtual gamepad exists after the first active XR sample. Lost focus/tracking
// and stale samples return neutral success so the game sees every button release.
class GamepadMailbox {
public:
    void publish(GamepadSample sample,bool active,uint64_t steadyMilliseconds);
    std::optional<GamepadSample> read(uint64_t steadyMilliseconds, bool* freshActive = nullptr) const;
private:
    mutable std::mutex mutex_;
    GamepadSample sample_{};
    uint64_t timestamp_{};
    bool connected_{},active_{};
};
GamepadMailbox& gamepadMailbox();
uint64_t steadyMilliseconds();
void installGamepadHook();
}
