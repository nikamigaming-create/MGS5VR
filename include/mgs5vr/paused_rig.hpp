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
    // A handheld opening can be observed after native body AND equipment
    // publication. Rebuilding just the body here detaches the already-published
    // handset. While that native skin is fresh, let the next native body/A8
    // transaction establish the open generation. The source admission rejects
    // a missing rig; it must not label the preceding rig as the new menu.
    // Close boundaries and spatial menus still need the immediate body refresh.
    // A genuinely paused menu retains the existing freshness timeout below.
    // The caller holds the same publication mutex as the native skin job.
    const bool boundary=frame.menuGeneration>owner.menuGeneration;
    const bool freshHandheldOpening=boundary&&frame.menuOpen&&frame.menuIdroid
        &&frame.controllers.handheldMenus&&now>=owner.nativeUpdateTime
        &&now-owner.nativeUpdateTime<=freshnessMilliseconds;
    return owner.camera&&owner.player&&camera==owner.camera
        &&frame.applied&&frame.stereoTracked&&(frame.menuOpen||boundary)
        &&frame.playerOwner==owner.player&&frame.activation==owner.activation
        &&frame.controllers.referenceEpoch==owner.referenceEpoch
        &&frame.controllers.predictedXrTime>0&&frame.controllers.presentationFocused
        &&frame.controllers.presentationEpoch
        &&!frame.controllers.nativeGamepad&&!frame.controllers.loading
        &&!frame.controllers.frontEnd&&!frame.controllers.avatarEditor&&!frame.controllers.authoredCamera
        &&now>=owner.nativeUpdateTime&&!freshHandheldOpening
        &&(boundary||now-owner.nativeUpdateTime>freshnessMilliseconds)
        &&now>=frame.sampleTime&&now-frame.sampleTime<=freshnessMilliseconds;
}
}
