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

// Measured from the supported retail idr0_main0_def FCNP. Both named sockets
// belong to SKL_000_ROOT; the hand connector is not the projection center.
constexpr Pose idroidConnectorInBody{{},{.0000056f,-.092196204f,-.000741f}};
constexpr Pose idroidHologramInBody{{},{0,.043378498f,.012349601f}};
std::optional<Pose> idroidBodyFromConnector(Pose connector) noexcept;
// Keep contact at the grip position, orient the device with the controller's
// own pointing basis. This accommodates runtime grip/aim cant without forcing
// the player to twist their wrist. Both poses must be from the same frame.
std::optional<Pose> idroidGripContact(Pose grip,Pose aim,Pose fit={}) noexcept;
// The native CNP stores its anchor hash in the low word and variant in the
// top 16 bits. Keeping the observed packed value here guards that layout.
constexpr bool matchesNativeIdroidMount(uint64_t name,uint64_t anchor,uint16_t bone) noexcept {
    return name==0x1c68632c5c53ull&&uint32_t(anchor)==0x38b1433c
        &&uint16_t(anchor>>48)==4&&bone==12;
}

// A readable 16:9 hologram rising from the authored projection socket.
// The aim-ray mapping uses these same dimensions.
constexpr float idroidScreenWidth = .45f;
constexpr float idroidScreenHeight = idroidScreenWidth*9.f/16.f;
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
