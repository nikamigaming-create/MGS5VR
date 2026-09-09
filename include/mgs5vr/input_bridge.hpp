#pragma once
#include <cstdint>
#include <mutex>
#include <optional>
#include "core.hpp"
namespace mgs5vr {
uint64_t steadyMilliseconds();
struct GamepadSample {
    uint16_t buttons{};
    uint8_t leftTrigger{},rightTrigger{};
    int16_t leftX{},leftY{},rightX{},rightY{};
    bool operator==(const GamepadSample&) const = default;
};
// Hold the modifier to open equipment at the wrist. The left stick remains
// locomotion. The first right-stick direction chooses the corresponding native
// D-pad category; after centering, the stick browses without rotating its axes.
// B returns to category selection while the modifier remains held.
// Trigger alone sends no native category. Expanded UI readiness gates browsing;
// one settled eight-way flick selects one card until the stick is centered again.
// Release closes selection. A held navigation stick cannot leak into turning.
class RigEquipment {
public:
    GamepadSample update(GamepadSample sample,bool modifier,bool allowOptics,uint64_t time,uint64_t pickerDrawTime);
    void reset(){*this=RigEquipment{};}
    unsigned phase() const {return !active_?0:!categoryChosen_?1:waitBrowseNeutral_?2:3;}
    unsigned category() const {return category_;}
private:
    uint16_t blockedButtons_{};
    bool blockedStick_{},active_{},categoryChosen_{},waitBrowseNeutral_{},wasNeutral_{},browseLatched_{};
    uint64_t neutralSince_{},openedAt_{},directionSince_{},useUntil_{},lastTime_{};
    int candidateDirection_{-1};
    int16_t browseX_{},browseY_{};
    unsigned category_{};
};
enum class TravelMode { unknown,onFoot,horse,vehicle };
struct WheelSample { float axis{};bool gripped{},engaged{};uint64_t time{}; };
// Authored driver hand contact and tracked grip are in the same LOCAL frame.
// A fresh squeeze near that contact takes the wheel; release always lets go.
class WheelSteering {
public:
    WheelSample update(Pose tracked,Pose contact,bool available,bool squeeze,uint64_t time,uint64_t epoch,Vec3 forward={0,0,-1});
    void reset(){*this=WheelSteering{};}
private:
    Pose start_{};
    Vec3 axis_{0,0,-1};
    uint64_t time_{},epoch_{};
    bool gripped_{},held_{};
};
class WheelMailbox {
public:
    void publish(WheelSample sample);
    WheelSample read(uint64_t time) const;
private:
    mutable std::mutex mutex_;
    WheelSample sample_{};
};
WheelMailbox& wheelMailbox();
struct RumbleSample { float low{},high{};uint64_t time{}; };
class RumbleMailbox {
public:
    void publish(RumbleSample sample);
    RumbleSample read(uint64_t time) const;
private:
    mutable std::mutex mutex_;
    RumbleSample sample_{};
};
RumbleMailbox& rumbleMailbox();
struct OpticsInput { GamepadSample gamepad{};float magnification{1};bool exclusive{}; };
// The reserved wrist+Y chord opens stereo magnification. R-click changes power;
// B closes it. Controls are released before returning to ordinary gameplay.
class RigOptics {
public:
    OpticsInput update(GamepadSample raw,bool available);
    void reset(){*this=RigOptics{};}
private:
    bool active_{},releaseRequired_{},priorChord_{},priorClick_{},priorBack_{};
    unsigned power_{};
};
struct MotionStrike { bool strike{},started{};float curl{};Vec3 start{},end{}; };
// No buttons arm a punch. Detect a deliberate, outward hand stroke relative to
// the tracked head, rejecting walking, tracking jumps and a returning hand.
class MotionMelee {
public:
    MotionStrike update(Pose head,Pose contact,bool available,uint64_t time,uint64_t epoch,bool weapon=false);
    void reset(){*this=MotionMelee{};}
private:
    Vec3 previous_{},previousContact_{},start_{};
    uint64_t previousTime_{},startedAt_{},cooldownUntil_{},epoch_{};
    bool primed_{},moving_{},striking_{},latched_{};
};
struct RigInputSample { GamepadSample gamepad; bool weaponReady{}; };
// Mounted vehicle triggers retain the game's accelerator/brake meanings.
// A travel-mode transition consumes held controls until they are released.
class RigInput {
public:
    RigInputSample update(GamepadSample raw,bool leftGrip,bool rightGrip,TravelMode mode,
                         uint64_t time=steadyMilliseconds(),uint64_t pickerDrawTime=0,bool throwing=false);
    unsigned equipmentPhase() const {return equipment_.phase();}
    unsigned equipmentCategory() const {return equipment_.category();}
    void suspend(){releaseRequired_=true;equipment_.reset();}
    void reset(){mode_=TravelMode::unknown;releaseRequired_=false;fireReleaseRequired_=false;wristMode_=false;equipment_.reset();}
private:
    TravelMode mode_{TravelMode::unknown};
    bool releaseRequired_{};
    bool fireReleaseRequired_{};
    bool wristMode_{};
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
