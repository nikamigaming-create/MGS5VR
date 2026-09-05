#pragma once
#include "core.hpp"
#include "stereo.hpp"
#include <mutex>

namespace mgs5vr {
enum class HeadCameraStop { none, manual, trackingLost, staleTracking, clockMismatch, cameraChanged, matrixMismatch };
struct HeadCameraStatus { bool enabled{},active{},pending{}; HeadCameraStop reason{}; uint64_t cancellations{},activation{}; };
struct HeadCameraSample {
    Pose nativePose{}, headPose{};
    uint64_t trackingSequence{}, activation{};
    bool applied{};
    EyeFrame eye{};
    std::array<EyeView,2> views{};
    uint64_t sampleTime{};
    bool stereoTracked{};
};
// Experimental render-camera control. Does not change native weapon ballistics.
class HeadCamera {
public:
    void configure(bool enabled,float nativeUnitsPerMeter=1);
    void track(Pose head,bool validTracking,uint64_t milliseconds);
    void trackStereo(Pose head,const std::array<EyeView,2>& views,bool validTracking,uint64_t milliseconds);
    void toggle();
    void cancel(HeadCameraStop reason=HeadCameraStop::manual);
    HeadCameraSample resolve(uintptr_t camera,Pose nativePose,uint64_t milliseconds);
    // Samples the steady clock while holding the same lock as the tracked pose.
    // A pose published between an earlier timestamp and lock acquisition must
    // not be mistaken for a clock reversal.
    HeadCameraSample resolveCurrent(uintptr_t camera,Pose nativePose);
    bool available() const;
    bool active() const;
    HeadCameraStatus status() const;
private:
    HeadCameraSample resolveLocked(uintptr_t camera,Pose nativePose,uint64_t milliseconds);
    void cancelLocked(HeadCameraStop reason);
    mutable std::mutex mutex_;
    Pose head_{}, origin_{};
    uintptr_t camera_{};
    uint64_t time_{},sequence_{},activation_{};
    float units_{1};
    bool enabled_{},tracking_{},pending_{},active_{};
    bool stereoTracking_{};
    std::array<EyeView,2> views_{};
    HeadCameraStop reason_{};
    uint64_t cancellations_{};
};
HeadCamera& headCamera();
}
