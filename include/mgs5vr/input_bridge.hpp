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
// native equipment category, X calls/radios, and Y is reserved for optics.
// Release the stick/buttons before returning them to movement and actions.
class RigEquipment {
public:
    GamepadSample update(GamepadSample sample,bool modifier,bool allowOptics=true);
    void reset(){blockedButtons_=0;blockedStick_=false;}
private:
    uint16_t blockedButtons_{};
    bool blockedStick_{};
};
enum class TravelMode { unknown,onFoot,horse,vehicle };
struct RigInputSample { GamepadSample gamepad; bool weaponReady{},supportRequested{}; };
// Mounted vehicle triggers retain the game's accelerator/brake meanings.
// A travel-mode transition consumes held controls until they are released.
class RigInput {
public:
    RigInputSample update(GamepadSample raw,bool leftGrip,bool rightGrip,TravelMode mode);
    void suspend(){releaseRequired_=true;equipment_.reset();}
    void reset(){mode_=TravelMode::unknown;releaseRequired_=false;equipment_.reset();}
private:
    TravelMode mode_{TravelMode::unknown};
    bool releaseRequired_{};
    RigEquipment equipment_;
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
