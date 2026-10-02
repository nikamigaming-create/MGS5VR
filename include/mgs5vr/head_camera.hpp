#pragma once
#include "core.hpp"
#include "optic_rig.hpp"
#include "room_bounds.hpp"
#include "stereo.hpp"
#include "hud.hpp"
#include "stale_demo_recovery.hpp"
#include "native_demo_state.hpp"
#include <mutex>

namespace mgs5vr {
enum class HeadCameraStop { none, manual, trackingLost, staleTracking, clockMismatch, cameraChanged, matrixMismatch, playerHeadUnavailable, rigFrameMismatch, sceneUnavailable, staleRig };
struct HeadCameraStatus { bool enabled{},active{},pending{}; HeadCameraStop reason{}; uint64_t cancellations{},activation{}; bool suspended{},awaitingPlayer{},nativeMenuOpen{},nativeIdroidOpen{}; uint32_t rigRejectFlags{}; uint64_t rigAgeMs{}; uint64_t sceneTransitionActivation{}; };
// New pixels require a currently accepted camera publication. Suspended
// sources can retain only an image that was admitted before the suspension.
constexpr bool mayAcceptNewStereoPair(HeadCameraStatus status) noexcept {
    return status.active&&!status.suspended;
}
// This only permits reprojecting an image already admitted with its exact rig
// and eye poses. A rejected or delayed source rig is never published as new pixels.
constexpr bool mayReprojectAcceptedStereo(HeadCameraStatus status) noexcept {
    return status.active&&(!status.suspended||status.reason==HeadCameraStop::staleTracking
        ||status.reason==HeadCameraStop::playerHeadUnavailable||status.reason==HeadCameraStop::staleRig
        ||(status.reason==HeadCameraStop::rigFrameMismatch&&status.rigRejectFlags==10&&status.rigAgeMs>150));
}
// A planned native menu/loading transition may retain explicitly frozen
// surroundings. A failed camera/pose transaction must not become that scene.
constexpr bool mayRetainStereoSurround(HeadCameraStatus status) noexcept {
    return status.reason!=HeadCameraStop::cameraChanged&&status.reason!=HeadCameraStop::matrixMismatch
        &&status.reason!=HeadCameraStop::clockMismatch&&status.reason!=HeadCameraStop::rigFrameMismatch
        &&status.reason!=HeadCameraStop::sceneUnavailable;
}
// All poses use the same OpenXR LOCAL space and predicted display time as
// the eyes. Grip is the attachment frame; aim is a separate pointing frame.
struct TrackedHand {
    Pose grip{},aim{};
    bool gripTracked{},aimTracked{};
    float trigger{},squeeze{};
    bool triggerTouched{},thumbTouched{};
};
struct ControllerFrame {
    std::array<TrackedHand,2> hands{};
    int64_t predictedXrTime{};
    uint64_t referenceEpoch{};
    bool weaponReady{};
    bool supportGrip{};
    bool vehicleControls{};
    bool nativeGamepad{}; // Physical XInput owns actions and native animated hands.
    bool wheelGrip{};
    bool allowMotionMelee{true},allowAnimalTouch{true};
    bool equipmentOpen{}; // Includes the unfolded chooser before any category is selected.
    unsigned equipmentCategory{}; // 0 closed/choosing; 1..4 native category.
    std::array<std::array<char,96>,4> equipmentLabels{};
    float wristSurfaceLift{.02f},wristSelectorHeight{.15f},wristPickerWidth{.42f};
    float weaponHudSetback{.06f}; // Compact readout only, toward the elbow.
    float wristTextScale{1.5f}; // Native arm text size; equipment card geometry is independent.
    // Native iDroid display width; height remains 16:9 so the map never
    // stretches when the user chooses a more comfortable panel size.
    float idroidScreenWidth{.45f};
    float idroidScreenDepth{.08f},playerHeightOffset{};
    // Display fit relative to the native hologram socket, not the hand rig.
    Vec3 idroidScreenOffset{};
    Quat idroidScreenRotation{};
    // iDroid-only controller contact calibration. The native wrist/socket and
    // cupped fingers move together; ordinary weapon and left-arm fits are separate.
    Vec3 idroidGripOffset{};
    Quat idroidGripRotation{};
    bool handheldMenus{}; // Opt-in: native handheld iDroid. Pause always uses the world panel.
    float menuQuadWidth{1.2f},menuQuadDistance{1.3f},menuQuadTilt{-10.f};
    float supportGripRadius{.10f},supportDetachRadius{.30f};
    float handRestCurl{.08f},handTouchCurl{.20f};
    HudMode hudMode{HudMode::binocularsOnly};
    float magnification{1};
    uint64_t weaponZoomSequence{};
    float weaponAimPitch{-30.f}; // Firearms only, in the runtime pointing frame.
    float scopeEyeRelief{.1f};
    bool scopePoseStabilization{true};
    std::array<float,2> strikeCurl{};
    bool commandControls{};
    OpticSample optic{};
    uint64_t opticMarkSequence{};
    uint64_t opticClearSequence{};
    uint64_t opticIntelSequence{};
    bool binocularAutoMark{true};
    bool binocularActorGlow{true};
    uint64_t binocularMarkDwellMs{650};
    float snapYaw{}; // Absolute world-Y turn carried by this tracking publication.
    bool frontEnd{}; // Native Title/loading backdrop and floating menu.
    bool loading{}; // Native loading/help terminal owns a live, actionable screen.
    bool avatarEditor{}; // Native name/appearance layout belongs on a quad over the retained hospital view.
    bool scriptedDemo{}; // Native demo state; accept its authored camera without a player-head publication.
    bool scriptedLook{}; // Hospital look lessons retain their interactive native pitch/roll.
    bool authoredCamera{}; // Follow the current native shot, including its position, pitch and roll.
    bool openingSelector{}; // Physical title tape rack owns a native pulse.
    bool openingBackend{}; // Actual title only; never suppress loading/prologue UI.
    bool cabinPlay{}; // Post-loading helicopter cabin sandbox owns tracked interaction.
    std::array<float,2> cabinMove{}; // Title-cabin thumbstick locomotion; does not enter the native gamepad.
    NativeRoomBounds cabinBounds{}; // Sampled native actor envelope, not a walking boundary.
    int openingSelection{-1}; // Hovered/active tape, in openingTapeLabels order.
    Pose openingOrigin{};
    Pose openingWorldOrigin{}; // Native anchor captured once, before collision correction.
    bool openingWorldAnchored{};
};
// A platform overlay takes input focus, not ownership of the game scene.
// Keep presentation settings and native scene identity; discard every action.
inline ControllerFrame passiveControllerFrame(ControllerFrame frame,int64_t time,uint64_t epoch){
    frame.hands={};frame.optic={};frame.strikeCurl={};frame.cabinMove={};
    frame.weaponReady=frame.supportGrip=frame.vehicleControls=frame.wheelGrip=false;
    frame.allowMotionMelee=frame.allowAnimalTouch=frame.commandControls=frame.equipmentOpen=false;
    frame.equipmentCategory=0;frame.openingSelection=-1;
    frame.predictedXrTime=time;frame.referenceEpoch=epoch;
    return frame;
}
struct HeadCameraSample {
    Pose nativePose{}, headPose{};
    uint64_t trackingSequence{}, activation{};
    bool applied{};
    EyeFrame eye{};
    std::array<EyeView,2> views{};
    uint64_t sampleTime{};
    bool stereoTracked{};
    uint64_t playerSequence{};
    uintptr_t playerOwner{};
    Vec3 playerHead{};
    ControllerFrame controllers{};
    uint64_t rigSequence{};
    Pose wristPanel{}; // World-space forearm surface from this skin publication.
    bool wristPanelTracked{};
    bool menuOpen{};
    bool menuIdroid{};
    bool menuWorldQuad{}; // Independent of wrist tracking and the handheld iDroid preference.
    Pose menuPanel{};
    WeaponScopeSample weaponScope{}; // Same solved weapon/skin publication as this eye pair.
    bool nativeFirearmActive{}; // Native weapon class, excludes throwables/placement and mounted weapons.
    Pose weaponSupportGrip{}; // Authored native support contact in this exact LOCAL rig frame.
    bool weaponSupportGripTracked{},weaponSupportAttached{};
    // Final native anatomical palm frames from the published first-person
    // skin. Handheld devices use these instead of guessing a palm side from
    // the controller grip alone.
    std::array<Pose,2> renderedPalms{};
    std::array<bool,2> renderedPalmTracked{};
    Pose idroidDevice{}; // Native device root from this solved wrist/socket transaction, WORLD space.
    bool idroidDeviceTracked{};
};
// Native listener adapters consume the center-head pose from the camera's
// existing publication, never a newer tracking sample or an individual eye.
std::optional<Pose> trackedListenerPose(const HeadCameraSample& frame,
    HeadCameraStatus status,uint64_t milliseconds);
// Row-vector affine transforms, in native FOX units. The local transform is
// the character's published head bone; the root places that character in world.
std::optional<Vec3> playerHeadPosition(const std::array<float,16>& worldFromPlayer,
                                     const std::array<float,16>& playerFromHead);
// Experimental render-camera control. Does not change native weapon ballistics.
class HeadCamera {
public:
    void configure(bool enabled,float nativeUnitsPerMeter=1,bool requirePlayerHead=false);
    bool publishPlayerHead(uintptr_t camera,uintptr_t owner,Pose sourceCamera,
                           const std::array<float,16>& worldFromPlayer,
                           const std::array<float,16>& playerFromHead,uint64_t milliseconds);
    void track(Pose head,bool validTracking,uint64_t milliseconds);
    void trackStereo(Pose head,const std::array<EyeView,2>& views,bool validTracking,uint64_t milliseconds,
                     ControllerFrame controllers={});
    void toggle();
    void recenter();
    void setNativeMenuOpen(bool open,bool idroid=false);
    // Present can continue while a native loading screen publishes no camera.
    // Keep the user's VR choice but expose mono menu pixels until that exact
    // camera resumes; this is not permission to adopt a different owner.
    // A publisher timeout alone is not a loading/menu transition.
    void awaitScene(bool nativeLoading);
    void cancel(HeadCameraStop reason=HeadCameraStop::manual);
    HeadCameraSample resolve(uintptr_t camera,Pose nativePose,uint64_t milliseconds);
    // Samples the steady clock while holding the same lock as the tracked pose.
    // A pose published between an earlier timestamp and lock acquisition must
    // not be mistaken for a clock reversal.
    HeadCameraSample resolveCurrent(uintptr_t camera,Pose nativePose);
    HeadCameraSample resolveCurrentForRig(uintptr_t camera,Pose nativePose);
    bool publishRigFrame(uintptr_t camera,uintptr_t owner,Pose sourceCamera,HeadCameraSample frame);
    std::optional<Pose> openingTrackingOrigin(uint64_t milliseconds) const;
    bool available() const;
    bool active() const;
    HeadCameraStatus status() const;
    bool staleDemoRecoveryReady() const;
    // Read-only diagnostics of the last accepted render-camera publication.
    // This does not resolve a newer pose or claim a submitted final-eye frame.
    std::optional<HeadCameraSample> publishedView() const;
private:
    HeadCameraSample resolveLocked(uintptr_t camera,Pose nativePose,uint64_t milliseconds,bool useRig=true);
    Pose followCinematicLocked(Pose nativePose,uint64_t milliseconds,bool advance);
    void cancelLocked(HeadCameraStop reason);
    void suspendLocked(HeadCameraStop reason);
    void awaitPlayerLocked();
    mutable std::mutex mutex_;
    Pose head_{}, origin_{},frontEndOrigin_{},frontEndPanel_{};
    Pose lastTitleSource_{},avatarEditorBackdrop_{};
    uintptr_t camera_{},playerOwner_{};
    uint64_t time_{},sequence_{},activation_{};
    uint64_t sceneTransitionActivation_{},sceneTransitionAt_{}; // Intentional native shot/control handoffs only.
    uint64_t trackingEpoch_{};
    float units_{1};
    float snapYaw_{};
    Vec3 snapTranslation_{}; // Native-yaw local, not a persistent world-space offset.
    bool enabled_{},tracking_{},pending_{},active_{};
    bool stereoTracking_{};
    bool suspended_{};
    bool awaitingPlayer_{};
    bool nativeMenuOpen_{};
    bool nativeIdroidOpen_{};
    bool awaitingScene_{};
    HeadCameraSample lastView_{};
    uint64_t cameraRenderTime_{}; // Source liveness survives invalidating a previous scene's pixels.
    Pose menuNative_{},menuHead_{},menuPanel_{};
    bool menuAnchored_{};
    bool recenterPending_{};
    bool lastTitleSourceValid_{},avatarEditorBackdropValid_{};
    bool scriptedDemoActive_{};
    struct CinematicFollowState {
        Pose pose{};
        Vec3 rawPosition{};
        float rawYaw{};
        uintptr_t camera{};
        uint64_t time{},activation{};
        bool valid{};
    } cinematicFollow_;
    std::array<EyeView,2> views_{};
    ControllerFrame controllers_{};
    struct RigFrame { uintptr_t camera{},owner{}; Pose sourceCamera{}; HeadCameraSample sample{}; } rig_;
    uint64_t rigSequence_{};
    uint32_t rigRejectFlags_{};
    uint64_t rigAgeMs_{};
    HeadCameraStop reason_{};
    uint64_t cancellations_{};
    struct PlayerHead {
        uintptr_t camera{},owner{};
        Pose sourceCamera{};
        Vec3 position{};
        uint64_t time{},sequence{};
    };
    std::array<PlayerHead,8> playerHeads_{};
    uint64_t playerSequence_{},ownerHeadTime_{};
    StaleDemoRecoveryDwell staleDemoRecoveryDwell_;
    uint64_t recoveryPlayerSequence_{};
    bool requirePlayerHead_{};
};
HeadCamera& headCamera();
}
