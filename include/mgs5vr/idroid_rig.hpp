#pragma once

#include "head_camera.hpp"

namespace mgs5vr {

// A real, hand-carried iDroid.  The display is a separate pose so native
// pixels can be emitted from the front of the housing without a head-locked
// replacement surface.
struct IdroidPose {
    Pose body{};
    Pose screen{};
};

// A readable hand-held hologram, still 16:9 and centered on the native palm
// attachment. The aim-ray mapping uses these same dimensions.
constexpr float idroidScreenWidth = .30f;
constexpr float idroidScreenHeight = .16875f;
constexpr uint32_t idroidScreenPixelWidth = 1280;
constexpr uint32_t idroidScreenPixelHeight = 720;
constexpr float idroidRayMaxDistance = 2.f;

// The iDroid pointer is the controller's normal OpenXR aim pose.  It is
// deliberately separate from the grip pose that places the device housing.
// This keeps the projected ray at the same origin/direction as the rest of
// first-person aiming instead of silently starting at the screen or grip.
struct IdroidRayHit {
    Hit hit{};
    Pose aim{};
};

std::optional<IdroidPose> trackedIdroidPose(const HeadCameraSample& frame) noexcept;
std::optional<IdroidRayHit> trackedIdroidRay(const HeadCameraSample& frame) noexcept;

}
