#pragma once
#include "core.hpp"
#include "stereo.hpp"
#include <mutex>

namespace mgs5vr {
enum class HeadCameraStop { none, manual, trackingLost, staleTracking, clockMismatch, cameraChanged, matrixMismatch, playerHeadUnavailable, rigFrameMismatch };
struct HeadCameraStatus { bool enabled{},active{},pending{}; HeadCameraStop reason{}; uint64_t cancellations{},activation{}; bool suspended{},awaitingPlayer{},nativeMenuOpen{}; };
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
    bool vehicleControls{};
    unsigned equipmentCategory{}; // 0 closed/choosing; 1..4 native category.
    float magnification{1};
    std::array<float,2> strikeCurl{};
    bool commandControls{};
};
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
    Pose menuPanel{};
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
    void setNativeMenuOpen(bool open);
    void cancel(HeadCameraStop reason=HeadCameraStop::manual);
    HeadCameraSample resolve(uintptr_t camera,Pose nativePose,uint64_t milliseconds);
    // Samples the steady clock while holding the same lock as the tracked pose.
    // A pose published between an earlier timestamp and lock acquisition must
    // not be mistaken for a clock reversal.
    HeadCameraSample resolveCurrent(uintptr_t camera,Pose nativePose);
    HeadCameraSample resolveCurrentForRig(uintptr_t camera,Pose nativePose);
    bool publishRigFrame(uintptr_t camera,uintptr_t owner,Pose sourceCamera,HeadCameraSample frame);
    bool available() const;
    bool active() const;
    HeadCameraStatus status() const;
private:
    HeadCameraSample resolveLocked(uintptr_t camera,Pose nativePose,uint64_t milliseconds,bool useRig=true);
    void cancelLocked(HeadCameraStop reason);
    void suspendLocked(HeadCameraStop reason);
    void awaitPlayerLocked();
    mutable std::mutex mutex_;
    Pose head_{}, origin_{};
    uintptr_t camera_{},playerOwner_{};
    uint64_t time_{},sequence_{},activation_{};
    float units_{1};
    bool enabled_{},tracking_{},pending_{},active_{};
    bool stereoTracking_{};
    bool suspended_{};
    bool awaitingPlayer_{};
    bool nativeMenuOpen_{};
    HeadCameraSample lastView_{};
    Pose menuNative_{},menuHead_{},menuPanel_{};
    bool menuAnchored_{};
    std::array<EyeView,2> views_{};
    ControllerFrame controllers_{};
    struct RigFrame { uintptr_t camera{},owner{}; Pose sourceCamera{}; HeadCameraSample sample{}; } rig_;
    uint64_t rigSequence_{};
    HeadCameraStop reason_{};
    uint64_t cancellations_{};
    struct PlayerHead {
        uintptr_t camera{},owner{};
        Pose sourceCamera{};
        Vec3 position{};
        uint64_t time{},sequence{};
    };
    std::array<PlayerHead,8> playerHeads_{};
    uint64_t playerSequence_{};
    bool requirePlayerHead_{};
};
HeadCamera& headCamera();
}
