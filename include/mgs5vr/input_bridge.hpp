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
struct OnFootActions { bool run{},stance{},dive{},pickupCarry{},switchWeapon{}; };
// Apply after menu/equipment routing, only when on-foot gameplay owns input.
// Native X means weapon-switch while aiming, so Dive must lower native aim.
// Native B must stay held for pickup/carry; a timed reload pulse cannot do it.
void applyOnFootActions(GamepadSample& sample,bool& weaponReady,OnFootActions actions);
// One horizontal flick turns once. Menus/focus changes require a neutral stick.
class SnapTurn {
public:
    float update(float x,float y,bool available);
    void reset(){armed_=false;}
private:
    bool armed_{};
};
struct LocomotionInput { GamepadSample gamepad{};bool stance{}; };
// Gameplay only: up runs, down is native stance (tap/hold), L3 dives.
// A vertical gesture cannot repeat or change meaning until centered. Menus,
// wrist selection and optics keep their own stick and consume held gestures.
class RigLocomotion {
public:
    LocomotionInput update(GamepadSample sample,bool available,uint64_t time);
    void suspend(){*this=RigLocomotion{};clickReleaseRequired_=true;}
private:
    uint64_t lastTime_{},pulseUntil_{};
    int direction_{};
    bool armed_{},clickReleaseRequired_{};
};
// Hold the modifier to open equipment at the wrist. The left stick remains
// locomotion. The first right-stick direction chooses the corresponding native
// D-pad category; after centering, the stick browses without rotating its axes.
// B returns to category selection while the modifier remains held.
// Trigger alone unfolds the category display without sending a native category.
// It waits for a fresh, centered stick gesture. Expanded UI readiness gates browsing;
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
struct BinocularInput {
    bool selected{};
    bool nativePress{};
};
// Hold B once to select binoculars; releasing B keeps them selected.
// A fresh B press stows them. Otherwise a short tap reloads. Menus retain B.
class BinocularHold {
public:
    BinocularInput update(bool available,bool pressed,uint64_t time=steadyMilliseconds());
    void reset(){*this=BinocularHold{};}
    void suspend(){reset();releaseRequired_=true;}
private:
    uint64_t pressedAt_{},nativeUntil_{},lastTime_{};
    bool pressed_{},longSent_{},selected_{},releaseRequired_{},wasAvailable_{true};
};
struct OpticsInput {
    GamepadSample gamepad{};
    float magnification{1};
    bool exclusive{};
    bool nativeActive{};
    bool markRequested{};
    bool clearRequested{};
};
// A selected, held optic owns its controls at every distance from the face.
// Magnification belongs to its lens, never to the HMD eye projection.
class RigOptics {
public:
    OpticsInput update(GamepadSample raw,bool available,bool held,bool atEye=false);
    void reset(){*this=RigOptics{};}
private:
    bool active_{},releaseRequired_{},priorClick_{},priorMark_{};
    bool clearArmed_{};
    unsigned power_{};
};
struct CommandsInput { GamepadSample gamepad{};bool active{},exclusive{},weaponReady{}; };
// The semantic input uses X+LT to open/hold, RT to confirm. Native Call owns
// navigation/R3. An already-published aim or lowered-weapon CQC hold can continue
// through the menu; opening Commands cannot start an attack or aim a new weapon.
class RigCommands {
public:
    CommandsInput update(GamepadSample raw,bool available,uint64_t time,uint64_t drawTime,
                         uint8_t heldAim=0,uint8_t heldCqc=0);
    void observeGameplay(GamepadSample sample,bool weaponReady){
        priorAim_=weaponReady&&sample.leftTrigger>127;
        priorCqc_=!weaponReady&&sample.leftTrigger<=24&&sample.rightTrigger>127;
    }
    void reset(){*this=RigCommands{};}
    void suspend(){reset();releaseRequired_=true;}
    bool active() const {return active_;}
private:
    uint64_t openedAt_{},lastTime_{},confirmUntil_{};
    bool active_{},releaseRequired_{},confirmHeld_{},stickBlocked_{},priorChord_{},confirmed_{};
    bool priorAim_{},priorCqc_{},carryAim_{},carryCqc_{};
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
    void requireAttackRelease(){fireReleaseRequired_=true;}
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
    uint16_t update(bool pressed, bool active, uint64_t milliseconds,bool recenterModifier=false);
    bool recentered() const {return recentered_;}
private:
    uint64_t pressedAt_{}, pulseUntil_{}, lastTime_{};
    uint16_t pulse_{};
    bool held_{}, longSent_{}, releaseRequired_{};
    bool recentered_{};
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
