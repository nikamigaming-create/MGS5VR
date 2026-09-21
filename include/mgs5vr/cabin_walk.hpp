#pragma once

#include "room_bounds.hpp"
#include "input_bridge.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace mgs5vr {

// The title camera has no native right-stick yaw owner. Turn its tracked
// frame around the HMD using the same accumulated yaw as on-foot snap turn.
// Entering a cabin, changing modes or regaining focus requires a neutral stick.
class CabinTurn {
public:
    float update(std::array<float,2> axis,int mode,float snapDegrees,bool available,
        uint64_t nowMs,uint64_t generation) noexcept {
        if(!available||!generation||!std::isfinite(snapDegrees)){
            reset();return 0;
        }
        if(generation_!=generation||mode_!=mode||nowMs<time_||nowMs-time_>150){
            reset();generation_=generation;mode_=mode;time_=nowMs;
        }
        const float dt=std::min(static_cast<float>(nowMs-time_)*.001f,.05f);
        time_=nowMs;
        const auto snap=snap_.update(axis[0],axis[1],mode==0)*snapDegrees/30.f;
        const auto smooth=smooth_.update(axis[0],axis[1],mode==1);
        // Right is negative world-Y in FOX camera space. A full deflection
        // turns 90 degrees per second, independent of render frequency.
        return snap-static_cast<float>(smooth)/32767.f*1.570796327f*dt;
    }
    void reset(){snap_.reset();smooth_.reset();time_=generation_=0;mode_=-1;}
private:
    SnapTurn snap_;
    NativeSmoothTurn smooth_;
    uint64_t time_{},generation_{};
    int mode_{-1};
};

struct CabinWalkState {
    Vec3 offset{};
    Vec3 position{};
    uint64_t lastUpdateMs{};
    uint64_t sceneGeneration{};
};

struct CabinSweep {
    bool available{},hit{};
    float fraction{1};
    Vec3 normal{};
};

// Move and slide the camera's native swept clearance volumes. A complete
// support query is required at the resolved endpoint, including open doors.
// The six rays sampled for actor eligibility are not a movement boundary.
template<class Sweep,class Supported>
std::optional<Vec3> resolveCabinMovement(Vec3 from,Vec3 target,Sweep sweep,Supported supported) {
    if(!valid(Pose{{},from})||!valid(Pose{{},target}))return {};
    auto position=from;
    for(unsigned pass=0;pass<3;++pass){
        const auto delta=target-position;
        const float length=std::sqrt(dot(delta,delta));
        if(length<.0001f)break;
        const auto result=sweep(position,target);
        if(!result.available)return {};
        if(!result.hit){position=target;break;}
        const float normalLength=std::sqrt(dot(result.normal,result.normal));
        if(!std::isfinite(result.fraction)||!std::isfinite(normalLength)||normalLength<.5f)return {};
        const auto normal=result.normal*(1.f/normalLength);
        const float fraction=std::clamp(result.fraction-.002f/length,0.f,1.f);
        position=position+delta*fraction;
        const auto remaining=target-position;
        target=position+remaining-normal*std::min(dot(remaining,normal),0.f);
    }
    return supported(position)?std::optional<Vec3>{position}:std::nullopt;
}

template<class Resolve>
Vec3 advanceCabinWalk(CabinWalkState& state,Vec3 basePosition,Quat heading,
    std::array<float,2> axis,uint64_t nowMs,uint64_t sceneGeneration,Resolve resolve,
    float metersPerSecond=.85f) {
    if(!valid(Pose{heading,basePosition})||!sceneGeneration
       ||!std::isfinite(metersPerSecond)||metersPerSecond<0){
        state={};
        return basePosition;
    }
    if(state.sceneGeneration!=sceneGeneration){
        state={Vec3{},basePosition,nowMs,sceneGeneration};
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
    if(const auto constrained=resolve(state.position,target);constrained&&valid(Pose{{},*constrained}))
        state.position=*constrained;
    state.offset=state.position-basePosition;
    return state.position;
}

} // namespace mgs5vr
