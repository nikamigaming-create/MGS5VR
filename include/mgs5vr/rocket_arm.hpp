#pragma once
#include <cstdint>
#include "core.hpp"
#include <cmath>

namespace mgs5vr {
// TPP 1.0.15.4: the native owner publishes its selected camera producer and
// the producer's mode before the camera setter. Mode 10 is preparation;
// mode 11 is the observed guided-flight branch (native category 14).
// Do not infer flight from distance, equipment selection or a trigger edge.
struct RocketArmCameraOwner {
    uintptr_t owner{},camera{},linkedCamera{},producer{},selectedProducer{},producerOwner{};
    uint32_t family{},mode{};
    int32_t overrideMode{};
    bool typesVerified{},readerVerified{};
};
constexpr bool rocketArmCameraOwned(const RocketArmCameraOwner& s) noexcept {
    return s.readerVerified&&s.typesVerified&&s.owner&&s.camera&&s.camera==s.linkedCamera
        &&s.producer&&s.producer==s.selectedProducer&&s.producerOwner==s.owner+0x20
        &&s.family==2&&s.mode==11&&s.overrideMode==-1;
}
inline std::optional<Pose> rocketArmRenderPose(Pose native) noexcept {
    if(!valid(native))return {};
    // Native flight interpolation accumulates small quaternion length error.
    // A rigid inverse amplifies it at kilometre-scale world coordinates.
    // Preserve the raw publication for owner joins; normalize only this replay.
    auto& q=native.orientation;
    const double length=std::sqrt(double(q.x)*q.x+double(q.y)*q.y+double(q.z)*q.z+double(q.w)*q.w);
    q={float(q.x/length),float(q.y/length),float(q.z/length),float(q.w/length)};
    return native;
}
}
