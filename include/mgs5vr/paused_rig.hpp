#pragma once
#include "head_camera.hpp"

namespace mgs5vr {
// A retained native animation target is usable only by the accepted player's
// current menu camera. This does not authorize advancing game simulation.
struct PausedRigOwner {
    uintptr_t camera{},player{};
    uint64_t activation{},referenceEpoch{},nativeUpdateTime{};
};
inline bool mayRefreshPausedRig(const PausedRigOwner& owner,uintptr_t camera,
                               const HeadCameraSample& frame,uint64_t now) noexcept {
    constexpr uint64_t freshnessMilliseconds=150;
    return owner.camera&&owner.player&&camera==owner.camera
        &&frame.applied&&frame.stereoTracked&&frame.menuOpen
        &&frame.playerOwner==owner.player&&frame.activation==owner.activation
        &&frame.controllers.referenceEpoch==owner.referenceEpoch
        &&frame.controllers.predictedXrTime>0&&frame.controllers.hands[1].gripTracked
        &&!frame.controllers.nativeGamepad&&!frame.controllers.loading
        &&!frame.controllers.frontEnd&&!frame.controllers.avatarEditor&&!frame.controllers.authoredCamera
        &&now>=owner.nativeUpdateTime&&now-owner.nativeUpdateTime>freshnessMilliseconds
        &&now>=frame.sampleTime&&now-frame.sampleTime<=freshnessMilliseconds;
}
}
