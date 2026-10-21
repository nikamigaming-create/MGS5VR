#pragma once
#include "core.hpp"
#include <cmath>

namespace mgs5vr {
// A native adapter must supply current, source-matched authored contacts and
// a stable actor/seat identity. This helper never identifies a mount or fires it.
struct MountedGripOwner {
    uintptr_t player{},mount{};
    uint64_t activation{},referenceEpoch{};
    bool operator==(const MountedGripOwner&) const = default;
};
struct MountedGripSample {
    bool held{},acquired{};
    Pose snappedHand{},requestedAim{};
};
class MountedGrip {
public:
    MountedGripSample update(MountedGripOwner owner,Pose hand,Pose contact,Pose aim,
            bool available,bool squeeze,uint64_t now,float acquireRadius,float releaseRadius) noexcept {
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
        const auto delta=compose(Pose{hand.orientation,{}},inverse(Pose{startHand_,{}}));
        const auto requested=compose(delta,Pose{startAim_,{}}).orientation;
        // The rendered hand follows the current native grip, not its old world
        // position. The adapter applies the request through native aim limits.
        return {true,acquired,contact,Pose{requested,aim.position}};
    }
private:
    MountedGripOwner owner_{};
    Quat startHand_{},startAim_{};
    uint64_t time_{};
    bool sampled_{},held_{},pressed_{},armed_{};
};
}
