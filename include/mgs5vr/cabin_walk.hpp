#pragma once

#include "room_bounds.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace mgs5vr {

struct CabinWalkState {
    Vec3 offset{};
    uint64_t lastUpdateMs{};
    uint64_t sceneGeneration{};
};

// Move the tracked view inside the sampled native cabin envelope. This is a
// conservative point-envelope constraint, not a swept capsule or whole-room
// navigation guarantee. Unknown/incomplete native collision fails closed.
inline Vec3 advanceCabinWalk(CabinWalkState& state,const NativeRoomBounds& bounds,
    Vec3 basePosition,Quat heading,std::array<float,2> axis,uint64_t nowMs,
    uint64_t sceneGeneration,float metersPerSecond=.65f,float clearance=.30f) noexcept {
    if(!validNativeRoomBounds(bounds)||!valid(Pose{heading,basePosition})||!sceneGeneration
       ||!std::isfinite(metersPerSecond)||metersPerSecond<0
       ||!std::isfinite(clearance)||clearance<0){
        state={};
        return basePosition;
    }
    if(state.sceneGeneration!=sceneGeneration){
        state={Vec3{},nowMs,sceneGeneration};
        return basePosition;
    }
    float dt=0;
    if(nowMs>state.lastUpdateMs)
        dt=std::min(static_cast<float>(nowMs-state.lastUpdateMs)*.001f,.05f);
    state.lastUpdateMs=nowMs;

    float x=std::isfinite(axis[0])?std::clamp(axis[0],-1.f,1.f):0.f;
    float y=std::isfinite(axis[1])?std::clamp(axis[1],-1.f,1.f):0.f;
    const float magnitude=std::hypot(x,y);
    float gain=0;
    if(magnitude>.18f){
        const float bounded=std::min(magnitude,1.f);
        gain=(bounded-.18f)/.82f;
        x=x/magnitude*gain;
        y=y/magnitude*gain;
    }else x=y=0;

    const auto forward=rotate(heading,{0,0,1});
    const float horizontalLength=std::hypot(forward.x,forward.z);
    if(!std::isfinite(horizontalLength)||horizontalLength<.001f)
        return basePosition+state.offset;
    const float yaw=std::atan2(forward.x,forward.z);
    const Quat yawOnly{0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)};
    // FOX camera space faces +Z with right along -X. Matching only forward
    // makes the sideways stick run opposite the user's hand.
    const auto step=rotate(yawOnly,{-x,0,y})*(metersPerSecond*dt);
    const auto target=basePosition+state.offset+step;
    // Horizontal walking clearance must not lower a seated viewer simply
    // because the real cabin ceiling is less than 30 cm above their eye.
    const auto constrained=nativeRoomClamp(bounds,target,Vec3{clearance,0,clearance});
    if(!nativeRoomContains(bounds,nativeRoomLocal(bounds,constrained))){
        state.offset={};
        return basePosition;
    }
    state.offset=constrained-basePosition;
    return constrained;
}

} // namespace mgs5vr
