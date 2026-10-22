#pragma once
#include "core.hpp"
#include <algorithm>
#include <cmath>

namespace mgs5vr {
// A native adapter must supply current, source-matched authored contacts and
// a stable actor/seat identity. This helper never identifies a mount or fires it.
struct MountedGripOwner {
    uintptr_t player{},mount{};
    uint64_t activation{},referenceEpoch{};
    uint64_t generation{}; // New native action admission, including a new emplacement on the same actor.
    bool operator==(const MountedGripOwner&) const = default;
};
struct MountedGripSample {
    bool held{},acquired{};
    Pose snappedHand{},requestedAim{};
};
class MountedGrip {
public:
    MountedGripSample update(MountedGripOwner owner,Pose hand,Pose contact,Pose aim,
            bool available,bool squeeze,uint64_t now,float acquireRadius,float releaseRadius,bool rebase=false) noexcept {
        const bool identity=owner.player&&owner.mount&&owner.activation&&owner.referenceEpoch;
        const bool dimensions=std::isfinite(acquireRadius)&&std::isfinite(releaseRadius)
            &&acquireRadius>0&&releaseRadius>=acquireRadius;
        const bool discontinuity=sampled_&&(now<time_||now-time_>250||owner!=owner_);
        if(!available||!identity||!dimensions||!valid(hand)||!valid(contact)||!valid(aim)||discontinuity){
            held_=false;pressed_=squeeze;armed_=!squeeze;
            owner_=owner;time_=now;sampled_=true;return {};
        }
        owner_=owner;time_=now;sampled_=true;
        if(!squeeze){held_=pressed_=false;armed_=true;return {};}
        const bool edge=armed_&&!pressed_;pressed_=true;
        const auto distance=hand.position-contact.position;
        bool acquired=false;
        if(edge&&!held_&&dot(distance,distance)<=acquireRadius*acquireRadius){
            held_=acquired=true;armed_=false;
            // Capture orientation at contact: acquisition itself cannot turn
            // the native gun, even if the physical controller is misaligned.
            startHand_=hand.orientation;startAim_=aim.orientation;
        }
        if(held_&&dot(distance,distance)>releaseRadius*releaseRadius){held_=false;armed_=false;}
        if(!held_)return {};
        if(rebase){startHand_=hand.orientation;startAim_=aim.orientation;}
        const auto delta=compose(Pose{hand.orientation,{}},inverse(Pose{startHand_,{}}));
        const auto requested=compose(delta,Pose{startAim_,{}}).orientation;
        // The rendered hand follows the current native grip, not its old world
        // position. The adapter applies the request through native aim limits.
        return {true,acquired,contact,Pose{requested,aim.position}};
    }
    void reset() noexcept {*this=MountedGrip{};}
private:
    MountedGripOwner owner_{};
    Quat startHand_{},startAim_{};
    uint64_t time_{};
    bool sampled_{},held_{},pressed_{},armed_{};
};
// Native axes retain the game's mechanical limits. A physical grip requests
// an aim direction, never a world transform for the emplacement itself.
inline std::array<float,2> mountedAimAxes(Pose current,Pose requested) noexcept {
    if(!valid(current)||!valid(requested))return {};
    const auto a=rotate(current.orientation,{0,0,1}),b=rotate(requested.orientation,{0,0,1});
    const auto horizontalA=std::hypot(a.x,a.z),horizontalB=std::hypot(b.x,b.z);
    if(horizontalA<.01f||horizontalB<.01f)return {};
    const float yaw=std::remainder(std::atan2(b.x,b.z)-std::atan2(a.x,a.z),6.283185307f);
    const float pitch=std::atan2(b.y,horizontalB)-std::atan2(a.y,horizontalA);
    const auto axis=[](float error){
        if(!std::isfinite(error)||std::abs(error)<.012f)return 0.f;
        // Retail mortar input has a dead zone even after the VR resolver.
        return std::copysign(std::clamp(std::abs(error)*2.f,.45f,.65f),error);
    };
    return {-axis(yaw),axis(pitch)};
}
// A held controller requests changes, not correction of native ready-camera
// animation. Latch each axis only after physical motion and brake when the
// request is reached or crossed; delayed native camera motion must not restart
// an opposite-direction correction with an unchanged hand.
class MountedAimServo {
public:
    std::array<float,2> update(Pose current,Pose requested,bool rebase=false) noexcept {
        const auto angles=[](Pose pose){
            const auto f=rotate(pose.orientation,{0,0,1});
            return std::array<float,2>{std::atan2(f.x,f.z),std::atan2(f.y,std::hypot(f.x,f.z))};
        };
        if(!valid(current)||!valid(requested)){reset();return {};}
        const auto target=angles(requested),actual=angles(current);
        if(!sampled_||rebase){target_=target;active_={};sampled_=true;return {};}
        auto axes=mountedAimAxes(current,requested);
        for(size_t i=0;i<2;++i){
            const float change=std::remainder(target[i]-target_[i],6.283185307f);
            const float error=std::remainder(target[i]-actual[i],6.283185307f);
            if(std::abs(change)>=.012f){target_[i]=target[i];active_[i]=true;error_[i]=error;}
            if(active_[i]&&(std::abs(error)<.012f||error*error_[i]<=0))active_[i]=false;
            if(!active_[i])axes[i]=0;
            error_[i]=error;
        }
        return axes;
    }
    void reset() noexcept {*this=MountedAimServo{};}
private:
    std::array<float,2> target_{},error_{};
    std::array<bool,2> active_{};
    bool sampled_{};
};
}
