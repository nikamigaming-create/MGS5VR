#pragma once
#include "head_camera.hpp"

namespace mgs5vr {
// A retained native animation target is usable only by the accepted player's
// current menu camera or its observed entry/exit boundary. This does not
// authorize advancing game simulation.
struct PausedRigOwner {
    uintptr_t camera{},player{};
    uint64_t activation{},referenceEpoch{},nativeUpdateTime{},menuGeneration{};
};
constexpr bool hasOwnedRigMenuBoundary(const PausedRigOwner& before,const PausedRigOwner& current) noexcept {
    return before.camera&&before.player&&before.camera==current.camera&&before.player==current.player
        &&before.activation==current.activation&&before.referenceEpoch==current.referenceEpoch
        &&current.menuGeneration>before.menuGeneration;
}
inline bool mayRefreshPausedRig(const PausedRigOwner& owner,uintptr_t camera,
                               const HeadCameraSample& frame,uint64_t now) noexcept {
    // Admission permits a solve attempt, not an untracked native write.
    // The rig must still produce a complete current or retained presentation
    // pair for this exact player/model before publishing any skin matrices.
    constexpr uint64_t freshnessMilliseconds=150;
    // Native menu entry/exit can change the camera after the last animation
    // job in this frame. Rebuild that same owned skin at the new camera now;
    // waiting for the ordinary paused timeout exposes a mismatched arm pose.
    // The caller holds the same publication mutex as the native skin job.
    const bool boundary=frame.menuGeneration>owner.menuGeneration;
    return owner.camera&&owner.player&&camera==owner.camera
        &&frame.applied&&frame.stereoTracked&&(frame.menuOpen||boundary)
        &&frame.playerOwner==owner.player&&frame.activation==owner.activation
        &&frame.controllers.referenceEpoch==owner.referenceEpoch
        &&frame.controllers.predictedXrTime>0&&frame.controllers.presentationFocused
        &&frame.controllers.presentationEpoch
        &&!frame.controllers.nativeGamepad&&!frame.controllers.loading
        &&!frame.controllers.frontEnd&&!frame.controllers.avatarEditor&&!frame.controllers.authoredCamera
        &&now>=owner.nativeUpdateTime&&(boundary||now-owner.nativeUpdateTime>freshnessMilliseconds)
        &&now>=frame.sampleTime&&now-frame.sampleTime<=freshnessMilliseconds;
}
}
