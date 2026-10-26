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
            bool available,bool squeeze,uint64_t now,float acquireRadius,float releaseRadius,bool rebase=false,
            float forearmLength=0.f) noexcept {
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
            startHand_=hand.orientation;startAim_=aim.orientation;startPosition_=hand.position;
            // Use this actor's authored forearm length for the spatial control
            // lever. It is an input gesture, not an invented emplacement pivot.
            lever_=std::isfinite(forearmLength)&&forearmLength>=.1f&&forearmLength<=1.f?forearmLength:0.f;
        }
        if(held_&&dot(distance,distance)>releaseRadius*releaseRadius){held_=false;armed_=false;}
        if(!held_)return {};
        if(rebase){startHand_=hand.orientation;startAim_=aim.orientation;startPosition_=hand.position;}
        const auto delta=compose(Pose{hand.orientation,{}},inverse(Pose{startHand_,{}}));
        Pose spatial{};
        if(lever_>0){
            const auto displacement=rotate(inverse(Pose{startAim_,{}}).orientation,hand.position-startPosition_);
            // Native aim is +Z. Lateral/vertical handle movement contributes
            // yaw/elevation; pushing along the barrel must not introduce yaw.
            const float yaw=std::atan2(displacement.x,lever_);
            const float pitch=-std::atan2(displacement.y,std::hypot(lever_,displacement.x));
            spatial=compose(Pose{{0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)},{}},
                Pose{{std::sin(pitch*.5f),0,0,std::cos(pitch*.5f)},{}});
        }
        const auto requested=compose(delta,compose(Pose{startAim_,{}},spatial)).orientation;
        // The rendered hand follows the current native grip, not its old world
        // position. The adapter applies the request through native aim limits.
        return {true,acquired,contact,Pose{requested,aim.position}};
    }
    void reset() noexcept {*this=MountedGrip{};}
private:
    MountedGripOwner owner_{};
    Quat startHand_{},startAim_{};
    Vec3 startPosition_{};
    float lever_{};
    uint64_t time_{};
    bool sampled_{},held_{},pressed_{},armed_{};
};
// Stick trim changes the requested bearing while either hand owns a native
// handle. It never turns the VR reference space or recenters the physical hand.
// Keep at most eight degrees of pending stick travel at native mechanical stops.
class MountedAimTrim {
public:
    Pose update(Pose current,Pose handRequest,std::array<float,2> stick,uint64_t now,bool rebase=false) noexcept {
        if(!valid(current)||!valid(handRequest)){reset();return handRequest;}
        const auto angles=[](Pose p){const auto f=rotate(p.orientation,{0,0,1});
            return std::array<float,2>{std::atan2(f.x,f.z),std::atan2(f.y,std::hypot(f.x,f.z))};};
        const auto actual=angles(current),physical=angles(handRequest);
        if(!sampled_||rebase||now<time_||now-time_>250){
            reset();sampled_=true;time_=now;previous_=physical;return handRequest;
        }
        const float seconds=float(now-time_)*.001f;time_=now;
        for(size_t i=0;i<2;++i){
            const float input=std::isfinite(stick[i])?std::clamp(stick[i],-1.f,1.f):0.f;
            const bool active=std::abs(input)>.15f;
            if(active){
                constexpr float trimRadiansPerSecond=.35f; // About 20 degrees/s at full stick.
                const float step=(i==0?-input:input)*trimRadiansPerSecond*seconds;
                const float error=std::remainder(physical[i]+offset_[i]-actual[i],6.283185307f);
                // Do not accumulate trim beyond a stop, or unwind a deliberate
                // physical request merely because it is farther from the gun.
                if(step*error<=0||std::abs(error)<.14f)
                    offset_[i]=std::remainder(offset_[i]+step,6.283185307f);
            }else if(sticking_[i]){
                // Stop on release at the attained bearing; preserve physical
                // movement on this very frame instead of pulling back later.
                offset_[i]=std::remainder(actual[i]-previous_[i],6.283185307f);
            }
            sticking_[i]=active;previous_[i]=physical[i];
        }
        const float yaw=physical[0]+offset_[0];
        const float pitch=std::clamp(physical[1]+offset_[1],-1.55334f,1.55334f);
        return compose(Pose{{0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)},handRequest.position},
            Pose{{-std::sin(pitch*.5f),0,0,std::cos(pitch*.5f)},{}});
    }
    void reset() noexcept {*this=MountedAimTrim{};}
private:
    std::array<float,2> offset_{},previous_{};
    std::array<bool,2> sticking_{};
    uint64_t time_{};
    bool sampled_{};
};
// Native axes retain the game's mechanical limits. A physical grip requests
// an aim direction, never a world transform for the emplacement itself.
inline std::array<float,2> mountedAimAxes(Pose current,Pose requested,bool machineGun=false) noexcept {
    if(!valid(current)||!valid(requested))return {};
    const auto a=rotate(current.orientation,{0,0,1}),b=rotate(requested.orientation,{0,0,1});
    const auto horizontalA=std::hypot(a.x,a.z),horizontalB=std::hypot(b.x,b.z);
    if(horizontalA<.01f||horizontalB<.01f)return {};
    const float yaw=std::remainder(std::atan2(b.x,b.z)-std::atan2(a.x,a.z),6.283185307f);
    const float pitch=std::atan2(b.y,horizontalB)-std::atan2(a.y,horizontalA);
    const auto axis=[machineGun](float error){
        if(!std::isfinite(error)||std::abs(error)<.012f)return 0.f;
        // The MG must catch the hand rapidly; retain the mortar's slower
        // response. Both use native limits and brake on reaching the target.
        return std::copysign(machineGun?std::clamp(std::abs(error)*8.f,.45f,1.f)
            :std::clamp(std::abs(error)*2.f,.45f,.65f),error);
    };
    return {-axis(yaw),axis(pitch)};
}
// A held controller requests changes, not correction of native ready-camera
// animation. Latch each axis only after physical motion and brake when the
// request is reached or crossed; delayed native camera motion must not restart
// an opposite-direction correction with an unchanged hand.
class MountedAimServo {
public:
    std::array<float,2> update(Pose current,Pose requested,bool rebase=false,bool machineGun=false) noexcept {
        const auto angles=[](Pose pose){
            const auto f=rotate(pose.orientation,{0,0,1});
            return std::array<float,2>{std::atan2(f.x,f.z),std::atan2(f.y,std::hypot(f.x,f.z))};
        };
        if(!valid(current)||!valid(requested)){reset();return {};}
        const auto target=angles(requested),actual=angles(current);
        if(!sampled_||rebase){target_=target;active_={};sampled_=true;return {};}
        auto axes=mountedAimAxes(current,requested,machineGun);
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
