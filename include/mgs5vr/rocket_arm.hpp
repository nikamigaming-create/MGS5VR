#pragma once
#include <cstdint>
#include "core.hpp"
#include <cmath>
#include <algorithm>
#include <array>

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
// forearmPanel: +X elbow->wrist, +Z dorsal. The oval flips up 60 degrees
// from the dorsal plane, facing back toward the pilot above the socket.
// Its near-edge hinge stays 25 mm above the wrist centerline. The complete
// mount follows forearm translation/roll; it never faces the head separately.
inline std::optional<Pose> rocketArmDisplay(Pose socket,float width,float aspect) noexcept {
    if(!valid(socket)||!std::isfinite(width)||width<.1f||width>.35f
       ||!std::isfinite(aspect)||aspect<.5f||aspect>3.f)return {};
    const float halfHeight=width/(2*aspect);
    return compose(socket,Pose{Quat{.35355339f,-.35355339f,-.61237244f,.61237244f},
        {halfHeight*.5f,0,.025f+halfHeight*.8660254f}});
}
inline Pose rocketArmGuidancePose(Pose socket,Pose phantomHand) noexcept {
    // The absent hand still articulates at the wrist. Keep its full rotation,
    // including pronation, instead of reducing it to the elbow->wrist axis.
    return {phantomHand.orientation,socket.position};
}
inline std::array<float,2> rocketArmInput(std::array<float,2> gyro,std::array<float,2> stick) noexcept {
    if(!std::isfinite(stick[0])||!std::isfinite(stick[1])||std::hypot(stick[0],stick[1])<=.15f)stick={};
    for(size_t i=0;i<gyro.size();++i)
        gyro[i]=std::clamp((std::isfinite(gyro[i])?gyro[i]:0.f)+stick[i],-1.f,1.f);
    return gyro;
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
class RocketArmSteering {
public:
    std::array<float,2> update(Pose hand,Pose head,bool active,
                              uintptr_t owner,uint64_t activation,uint64_t referenceEpoch,uint64_t time=0,
                              float sensitivity=2.f,bool wristTwist=true) noexcept {
        if(!active||!owner||!valid(hand)||!valid(head)){
            // Camera/skin publication can cross an XR input poll. Suppress
            // input on that poll, but don't recenter guidance at the moved
            // arm on every reacquisition. Owner/epoch changes and prolonged
            // loss still require a new neutral; no stale aim is replayed.
            if(!time||!lastActive_||time<lastActive_||time-lastActive_>150
               ||(owner&&owner!=owner_)||activation!=activation_||referenceEpoch!=epoch_)*this={};
            return {};
        }
        if(time&&lastActive_&&(time<lastActive_||time-lastActive_>150))*this={};
        lastActive_=time;
        if(!owner_||owner_!=owner||activation_!=activation||epoch_!=referenceEpoch){
            owner_=owner;activation_=activation;epoch_=referenceEpoch;neutral_=hand;
            const auto forward=rotate(head.orientation,{0,0,-1});
            const float yaw=std::atan2(-forward.x,-forward.z);
            basis_=Pose{{0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)},{}};
            return {};
        }
        const auto delta=compose(compose(inverse(basis_),compose(hand,inverse(neutral_))),basis_);
        // Shortest-arc rotation vector in the launch head's level frame:
        // pitch raises the rocket; yaw and wrist twist turn it horizontally.
        // Native flight has two steering axes, not a separate roll channel.
        auto q=delta.orientation;
        if(q.w<0)q={-q.x,-q.y,-q.z,-q.w};
        const float length=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z);
        const float scale=length>1e-6f?2.f*std::atan2(length,q.w)/length:2.f;
        const float gain=std::isfinite(sensitivity)?std::clamp(sensitivity,.25f,4.f):2.f;
        const auto axis=[gain](float angle){return std::abs(angle)<.04f?0.f:std::clamp(angle*gain,-.65f,.65f);};
        return {axis(-(q.y+(wristTwist?q.z:0.f))*scale),axis(q.x*scale)};
    }
private:
    Pose neutral_{},basis_{};
    uintptr_t owner_{};
    uint64_t activation_{},epoch_{},lastActive_{};
};
}
