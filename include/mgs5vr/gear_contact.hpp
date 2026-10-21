#pragma once
#include <cstdint>

namespace mgs5vr {
struct GearContactResult {
    bool pulse{}, grab{};
    uint64_t item{};
};
// Candidate eligibility only. The caller owns native acquire/return and must
// supply an exact loadout item key, a fresh source and bounded spatial reach.
class GearContact {
public:
    GearContactResult update(uint64_t item, bool available, bool touching,
            bool withinReach, bool grip, uint64_t now) noexcept {
        const bool discontinuity=sampled_&&(now<last_||now-last_>250);
        last_=now;sampled_=true;
        if(!available||!item||!withinReach||discontinuity){
            candidate_=0;contact_=false;pressed_=grip;armed_=!grip;
            return {};
        }
        if(item!=candidate_){candidate_=0;contact_=false;}
        const bool edge=armed_&&grip&&!pressed_;
        pressed_=grip;
        if(!grip)armed_=true;
        GearContactResult result;
        if(touching){
            result.pulse=!contact_;
            candidate_=item;lastTouch_=now;
        }
        contact_=touching;
        if(candidate_&&(!touching&&(now<lastTouch_||now-lastTouch_>=1000)))candidate_=0;
        if(candidate_&&edge){
            result.grab=true;result.item=candidate_;
            armed_=false;
        }
        result.item=candidate_;
        return result;
    }
private:
    uint64_t candidate_{},lastTouch_{},last_{};
    bool sampled_{},contact_{},pressed_{},armed_{};
};
}
