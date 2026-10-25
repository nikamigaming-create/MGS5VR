#pragma once
#include "cabin_props.hpp"
#include "cabin_studio.hpp"
#include "core.hpp"
#include "optic_rig.hpp"
#include "room_bounds.hpp"
#include "stereo.hpp"
#include "hud.hpp"
#include "stale_demo_recovery.hpp"
#include "native_demo_state.hpp"
#include "vehicle_view.hpp"
#include <mutex>

namespace mgs5vr {
std::optional<float> physicalFloorHeightOffset(float headY,float floorY,float units,float standingEyeHeight);
enum class HeadCameraStop { none, manual, trackingLost, staleTracking, clockMismatch, cameraChanged, matrixMismatch, playerHeadUnavailable, rigFrameMismatch, sceneUnavailable, staleRig, awaitingRig };
enum class PlayerPosture { unknown,standing,crouched,prone };
struct HeadCameraStatus { bool enabled{},active{},pending{}; HeadCameraStop reason{}; uint64_t cancellations{},activation{}; bool suspended{},awaitingPlayer{},nativeMenuOpen{},nativeIdroidOpen{}; uint32_t rigRejectFlags{}; uint64_t rigAgeMs{}; uint64_t sceneTransitionActivation{};
    // Read-only provenance: 1 authored mode, 2 reference rebase, 3 avatar
    // source, 4 authored source, 5 unowned shot, 6 player-owner join,
    // 7 player resume, 8 pending camera, 9 scene resume, 10 explicit recenter.
    uint8_t activationCause{};uintptr_t sourceCamera{},sourcePlayerOwner{};uint64_t ownerHeadTime{};
    // Last retirement entry: 1 loading, 2 menu, 3 missing exact skin join.
    uint8_t awaitCause{},awaitJoinFlags{};uint64_t awaitTime{},awaitOwnerAge{};
};
// New pixels require a currently accepted camera publication. Suspended
// sources can retain only an image that was admitted before the suspension.
constexpr bool mayAcceptNewStereoPair(HeadCameraStatus status) noexcept {
    return status.active&&!status.suspended;
}
// This only permits reprojecting an image already admitted with its exact rig
// and eye poses. A rejected or delayed source rig is never published as new pixels.
constexpr bool mayReprojectAcceptedStereo(HeadCameraStatus status) noexcept {
    return status.active&&(!status.suspended||status.reason==HeadCameraStop::trackingLost||status.reason==HeadCameraStop::staleTracking
        ||status.reason==HeadCameraStop::playerHeadUnavailable||status.reason==HeadCameraStop::staleRig||status.reason==HeadCameraStop::awaitingRig
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
    Vec3 indexTip{};
    bool indexTipTracked{};
};
struct ControllerFrame {
    std::array<TrackedHand,2> hands{};
    int64_t predictedXrTime{};
    uint64_t referenceEpoch{};
    // Presentation continuity only; never substitutes for raw pose tracking.
    bool presentationFocused{};
    uint64_t presentationEpoch{};
    bool weaponReady{};
    bool supportGrip{};
    bool vehicleControls{};
    // Native occupancy survives iDroid focus and presentation-only frames.
    // Input availability must not move seated eyes back to the chase camera.
    bool vehicleMounted{};
    NativeVehicleView vehicleView{};
    bool contextInteract{};
    // Actual native look input from this XR packet. A vehicle's chase-camera
    // orbit does not set the driver's initial seated facing.
    std::array<float,2> vehicleViewInput{};
    bool nativeActionControls{}; // Native stationary/action aim; no handheld firearm replacement.
    // Occupancy is independent of input focus and of NORMAL_ACTION. Ordinary
    // traversal/context actions must not select a seated body or mounted eye.
    uint16_t nativeActionId{};
    bool mountedStickAiming{};
    std::array<bool,2> mountedHandGrip{};
    bool nativeGamepad{}; // Physical XInput owns actions and native animated hands.
    bool wheelGrip{};
    bool rightWheelGrip{},vehicleWeaponIntent{};
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
    float idroidScreenDepth{.09f},playerHeightOffset{};
    float physicalFloorY{}; // Runtime STAGE floor expressed in this sample's LOCAL space.
    bool physicalFloorTracked{};
    // Display fit relative to the native hologram socket, not the hand rig.
    Vec3 idroidScreenOffset{};
    Quat idroidScreenRotation{};
    // iDroid-only controller contact calibration. The native wrist/socket and
    // cupped fingers move together; ordinary weapon and left-arm fits are separate.
    Vec3 idroidGripOffset{};
    Quat idroidGripRotation{};
    bool handheldMenus{}; // Effective handheld iDroid preference. Pause always uses the world panel.
    bool handheldMenuFree{}; // Native UI-only terminal leaves gameplay input live.
    bool handheldMenuCoexist{}; // Keep the right physical binocular while the LEFT terminal is focused.
    bool handheldIdroidStowed{}; // LEFT hand temporarily owns reload/support; keep the terminal's resource.
    float menuQuadWidth{1.2f},menuQuadDistance{1.3f},menuQuadTilt{-10.f};
    float supportGripRadius{.10f},supportDetachRadius{.30f};
    bool blockFiringNearCover{};
    bool trackedWeaponObstruction{true};
    float handRestCurl{.08f},handTouchCurl{.20f};
    HudMode hudMode{HudMode::binocularsOnly};
    bool enemyWorldMarkers{};
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
    bool avatarEditor{}; // Full native appearance preview/canvas on a quad over retained hospital stereo.
    bool nativeSceneCanvas{}; // Native credit/result/reward canvas; retain the clear stereo world behind it.
    bool scriptedDemo{}; // Native demo state; accept its authored camera without a player-head publication.
    bool scriptedLook{}; // Hospital look lessons retain their interactive native pitch/roll.
    uint64_t headLookLessonKey{}; // Positive, fresh native hospital lesson routed through HMD motion.
    bool authoredCamera{}; // Follow the current native shot, including its position, pitch and roll.
    bool openingSelector{}; // Physical title tape rack owns a native pulse.
    bool openingNativeMenu{}; // Explicit native menu access keeps props optional.
    bool openingBackend{}; // Actual title only; never suppress loading/prologue UI.
    bool cabinPlay{}; // Post-loading helicopter cabin sandbox owns tracked interaction.
    bool cabinDeployHint{}; // Actual ACC, not field transit; render only on the source LEFT forearm.
    bool cabinTransit{}; // Verified passenger of the moving support helicopter.
    std::array<float,2> cabinMove{}; // Title-cabin thumbstick locomotion; does not enter the native gamepad.
    NativeRoomBounds cabinBounds{}; // Sampled native actor envelope, not a walking boundary.
    int openingSelection{-1}; // Hovered/active tape, in openingTapeLabels order.
    CabinBenchFrame openingBench{};
    CabinLayout openingLayout{};
    std::shared_ptr<const CabinStudioFrame> studio;
    bool openingPropsActive{};
    bool cameraWorldFrozen{}; // Acknowledged mod-owned native pause, never a stock menu.
    Pose openingOrigin{};
    Pose openingWorldOrigin{}; // Native anchor captured once, before collision correction.
    bool openingWorldAnchored{};
    PlayerPosture playerPosture{PlayerPosture::unknown};
    bool fullBody{},seatedMode{},cinematicFreeMove{true};
    bool coverAssistance{true};
    std::array<float,2> cinematicMove{};
};
// A platform overlay takes input focus, not ownership of the game scene.
// Keep presentation settings and native scene identity; discard every action.
inline ControllerFrame passiveControllerFrame(ControllerFrame frame,int64_t time,uint64_t epoch){
    frame.hands={};frame.optic={};frame.strikeCurl={};frame.cabinMove={};frame.cinematicMove={};
    frame.presentationFocused=false;
    frame.weaponReady=frame.supportGrip=frame.vehicleControls=frame.nativeActionControls=frame.wheelGrip=false;
    frame.rightWheelGrip=frame.vehicleWeaponIntent=false;
    frame.mountedHandGrip={};frame.mountedStickAiming=false;
    frame.cabinDeployHint=false;
    frame.vehicleViewInput={};
    frame.contextInteract=false;
    frame.allowMotionMelee=frame.allowAnimalTouch=frame.commandControls=frame.equipmentOpen=false;
    frame.equipmentCategory=0;frame.openingSelection=-1;
    frame.predictedXrTime=time;frame.referenceEpoch=epoch;
    return frame;
}
// Read-only evidence copied from the rig's existing native weapon sampling.
// Resource handles identify a runtime resource, not a named weapon definition.
// bodyModel is the accepted player skin; no weapon render model is inferred.
struct WeaponRigDiagnostics {
    uint64_t sampleTime{},supportIdentity{};
    uintptr_t character{},component{},instances{},state{},bodyModel{};
    uint32_t instanceFirst{},playerIndex{},resourceHandle{},flags{},mode{};
    std::array<uint8_t,4> optical{};
    Pose muzzleInGrip{};
    Vec3 nativeAimTarget{}; // Original weapon instance +0x140, before the shot adapter's private copy.
    bool nativeAimTargetRead{};
    Pose sightRear{},sightFront{},sightRearInWrist{},sightFrontInWrist{},sightRearLocal{},sightFrontLocal{};
    bool sightQueryAttempted{},sightRearRead{},sightFrontRead{},sightRearValid{},sightFrontValid{};
    bool sampled{},componentAccepted{},instanceIndexMatched{},resourceAccepted{};
    bool muzzleAttempted{},muzzleMatrixRead{},attachmentValid{},muzzleSocketValid{},muzzleSolved{},opticalRead{};
    uintptr_t magnifierInterface{},magnifierModel{};
    uint32_t magnifierIndex{};
    uint64_t magnifierModelCode{};
};
struct HeadCameraSample {
    struct BodyRig {
        bool valid{},spineClamped{},mounted{};
        uint32_t rejection{}; // 1 source, 2 spine, 3 eye fit, 4 leg, 5 final eyes.
        uint32_t floorQueries{},floorContacts{},headContacts{};
        Pose pelvis{},chest{},head{},eye{};
        Vec3 eyeError{}; // Solved anatomical eyes minus this skin transaction's submitted viewer.
        std::array<Pose,2> feet{};
        std::array<float,2> floorClearance{};
        std::array<float,2> legScale{1.f,1.f};
        float legCalibration{1.f};
        std::array<float,2> nativeFloorClearance{};
    };
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
    uintptr_t anatomicalEyeModel{}; // Matching original skin, retained only by an admitted frozen/menu frame.
    BodyRig body;
    Pose cabinBasePose{}; // Passenger anchor before tracked head motion, from this exact player publication.
    bool cabinBaseTracked{};
    ControllerFrame controllers{};
    uint64_t rigSequence{};
    Pose wristPanel{}; // World-space forearm surface from this skin publication.
    bool wristPanelTracked{};
    bool menuOpen{};
    bool menuIdroid{};
    // Changed only by an observed native menu boundary. Skin and camera must
    // agree on this generation before a new handheld eye pair is admitted.
    uint64_t menuGeneration{};
    bool menuWorldQuad{}; // Independent of wrist tracking and the handheld iDroid preference.
    Pose menuPanel{};
    WeaponScopeSample weaponScope{}; // Same solved weapon/skin publication as this eye pair.
    bool nativeFirearmActive{}; // Native weapon class, excludes throwables/placement and mounted weapons.
    Pose weaponSupportGrip{}; // Authored native support contact in this exact LOCAL rig frame.
    bool weaponSupportGripTracked{},weaponSupportAttached{};
    // Unmodified native steering-wheel palm contacts, joined to these eyes.
    // These are LOCAL controller grip frames, not guessed cab positions.
    std::array<Pose,2> driverWheelGrips{};
    bool driverWheelTracked{},driverWheelHeld{};
    uint8_t driverWheelHands{};
    float driverWheelAxis{};
    std::array<Pose,2> mountedWeaponGrips{}; // Original native anatomical palms, LOCAL space.
    bool mountedWeaponTracked{};
    uint8_t mountedWeaponHands{};
    uint16_t mountedWeaponAction{};
    std::array<float,2> mountedWeaponAxes{};
    // Final native anatomical palm frames from the published first-person
    // skin. Handheld devices use these instead of guessing a palm side from
    // the controller grip alone.
    std::array<Pose,2> renderedPalms{};
    std::array<bool,2> renderedPalmTracked{};
    Pose idroidDevice{}; // Native device root from this solved wrist/socket transaction, WORLD space.
    bool idroidDeviceTracked{};
    WeaponRigDiagnostics weaponRig{}; // Same retained skin/camera publication; not a native draw observation.
    // Cinematic follow state used for this submitted view, before tracked head motion.
    Pose cinematicBasePose{};
    Vec3 cinematicFreeOffset{};
    bool cinematicFollowValid{};
    Quat locomotionHeading{}; // Accepted native heading plus snap, before physical HMD neck look.
    bool locomotionHeadingTracked{};
    // Untouched owned skin from this rig transaction. Keep these distinct
    // from the retargeted body/eye fit: a fitted eye can match a misplaced
    // viewer perfectly while its original native head was elsewhere.
    bool nativeBodySourceTracked{};
    Pose nativeBodySourceRoot{},nativeBodySourceChest{},nativeBodySourceHead{},nativeBodySourceEye{};
    // Exact native projectile view paired with this player publication. The
    // world view remains anchored to Snake; this is never a retained image.
    Pose rocketArmCamera{};
    bool rocketArmGuidance{};
};
// Native listener adapters consume the center-head pose from the camera's
// existing publication, never a newer tracking sample or an individual eye.
std::optional<Pose> trackedListenerPose(const HeadCameraSample& frame,
    HeadCameraStatus status,uint64_t milliseconds);
// Row-vector affine transforms, in native FOX units. The local transform is
// the character's published head bone; the root places that character in world.
std::optional<Vec3> playerHeadPosition(const std::array<float,16>& worldFromPlayer,
                                     const std::array<float,16>& playerFromHead);
// Supplied only by the validated player skin transaction, before retargeting.
// The camera owner's cached HEAD affine is an independent animated frame.
struct NativeBodyHead {
    uintptr_t owner{},model{};
    Pose worldFromHead{};
    Pose worldFromRoot{},worldFromChest{};
    bool bodyFramesTracked{};
    bool wallCover{}; // Same owned native skin transaction; preserve its wall-relative eye footprint.
};
// Experimental render-camera control. Does not change native weapon ballistics.
class HeadCamera {
public:
    void configure(bool enabled,float nativeUnitsPerMeter=1,bool requirePlayerHead=false);
    bool publishPlayerHead(uintptr_t camera,uintptr_t owner,Pose sourceCamera,
                           const std::array<float,16>& worldFromPlayer,
                           const std::array<float,16>& playerFromHead,uint64_t milliseconds,
                           bool verifiedRocketArmGuidance=false);
    void track(Pose head,bool validTracking,uint64_t milliseconds);
    void trackStereo(Pose head,const std::array<EyeView,2>& views,bool validTracking,uint64_t milliseconds,
                     ControllerFrame controllers={});
    void toggle();
    void recenter();
    void setNativeMenuOpen(bool open,bool idroid=false,bool pause=false,bool idroidPaused=false);
    // Present can continue while a native loading screen publishes no camera.
    // Keep the user's VR choice but expose mono menu pixels until that exact
    // camera resumes; this is not permission to adopt a different owner.
    // A publisher timeout alone is not a loading/menu transition.
    void awaitScene(bool nativeLoading);
    void cancel(HeadCameraStop reason=HeadCameraStop::manual);
    HeadCameraSample resolve(uintptr_t camera,Pose nativePose,uint64_t milliseconds,
                            std::optional<NativeBodyHead> nativeHead={});
    // Samples the steady clock while holding the same lock as the tracked pose.
    // A pose published between an earlier timestamp and lock acquisition must
    // not be mistaken for a clock reversal.
    HeadCameraSample resolveCurrent(uintptr_t camera,Pose nativePose);
    HeadCameraSample resolveCurrentForRig(uintptr_t camera,Pose nativePose,
                                         std::optional<NativeBodyHead> nativeHead={});
    bool publishRigFrame(uintptr_t camera,uintptr_t owner,Pose sourceCamera,HeadCameraSample frame);
    std::optional<Pose> openingTrackingOrigin(uint64_t milliseconds) const;
    bool available() const;
    bool active() const;
    HeadCameraStatus status() const;
    bool staleDemoRecoveryReady() const;
    // Read-only diagnostics of the last accepted render-camera publication.
    // This does not resolve a newer pose or claim a submitted final-eye frame.
    std::optional<HeadCameraSample> publishedView() const;
    // Copies the current native skin transaction without resolving another
    // tracking pose. Native effect callbacks can run before camera publication.
    // This is not evidence that its pixels have reached a final eye.
    std::optional<HeadCameraSample> publishedRigFrame(uint64_t milliseconds) const;
private:
    bool joinNativeBodyHeadLocked(uintptr_t camera,Pose sourceCamera,
                                 const NativeBodyHead& nativeHead,uint64_t milliseconds);
    HeadCameraSample resolveLocked(uintptr_t camera,Pose nativePose,uint64_t milliseconds,
                                  bool useRig=true,bool nativeEyeJoined=false);
    Pose followCinematicLocked(Pose nativePose,uint64_t milliseconds,bool advance);
    void cancelLocked(HeadCameraStop reason);
    void suspendLocked(HeadCameraStop reason);
    void awaitPlayerLocked(uint8_t cause);
    mutable std::mutex mutex_;
    Pose head_{}, origin_{},frontEndOrigin_{},frontEndPanel_{};
    Pose lastTitleSource_{},avatarEditorBackdrop_{};
    uintptr_t camera_{},playerOwner_{};
    uint64_t time_{},sequence_{},activation_{};
    uint8_t activationCause_{};
    uint8_t awaitCause_{},awaitJoinFlags_{};uint64_t awaitTime_{},awaitOwnerAge_{};
    uint64_t sceneTransitionActivation_{},sceneTransitionAt_{}; // Intentional native shot/control handoffs only.
    uint64_t trackingEpoch_{};
    uint64_t fieldPostureUntil_{}; // Native helicopter exit may retain its seated/crouched flag.
    float units_{1};
    float seatedHeadY_{};
    bool seatedMode_{};
    float snapYaw_{};
    Vec3 snapTranslation_{}; // Native-yaw local, not a persistent world-space offset.
    bool enabled_{},tracking_{},pending_{},active_{};
    bool stereoTracking_{};
    bool suspended_{};
    bool awaitingPlayer_{};
    bool nativeMenuOpen_{};
    bool nativeIdroidOpen_{};
    bool nativeIdroidPaused_{};
    bool cameraFreezeAnchored_{};
    Pose cameraFreezeNative_{};
    Pose cameraFreezeHead_{};
    uint64_t menuGeneration_{};
    bool awaitingScene_{};
    HeadCameraSample lastView_{};
    uint64_t cameraRenderTime_{}; // Source liveness survives invalidating a previous scene's pixels.
    Pose menuNative_{},menuHead_{},menuPanel_{};
    bool menuAnchored_{};
    uint64_t pausedMenuClosedAt_{};
    bool recenterPending_{};
    bool lastTitleSourceValid_{},avatarEditorBackdropValid_{};
    bool scriptedDemoActive_{};
    Pose headLookLessonAnchor_{};
    uint64_t headLookLessonKey_{};
    struct CinematicFollowState {
        Pose pose{};
        Vec3 rawPosition{};
        float rawYaw{};
        uintptr_t camera{};
        uint64_t time{},activation{};
        bool valid{};
        Vec3 freeOffset{};
    } cinematicFollow_;
    struct DriverViewState {
        float nativeYaw{},rootYaw{},lookYaw{};
        uintptr_t camera{},owner{};
        uint64_t time{},activation{},referenceEpoch{},lookUntil{};
        bool valid{};
        bool passenger{};
        Vec3 headLocalOffset{};
        uintptr_t eyeModel{};
        uint64_t eyeTime{};
        bool eyeSettling{};
    } driverView_;
    VehicleViewHeading vehicleHeading_;
    uintptr_t vehicleHeadingCamera_{},vehicleHeadingOwner_{};
    uint64_t vehicleHeadingEpoch_{};
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
        bool rocketArmGuidance{};
        Quat rootHeading{};
    };
    std::array<PlayerHead,8> playerHeads_{};
    uint64_t playerSequence_{},ownerHeadTime_{};
    StaleDemoRecoveryDwell staleDemoRecoveryDwell_;
    uint64_t recoveryPlayerSequence_{};
    bool requirePlayerHead_{};
};
HeadCamera& headCamera();
}
