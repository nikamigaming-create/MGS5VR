#pragma once
#include "input_bridge.hpp"
#include <algorithm>
#include <cmath>

namespace mgs5vr {
// Pedals transfer away from the firing trigger as soon as the right hand
// requests its gun, before native draw acknowledgement. Ownership transfers
// require release, so a held shot cannot become acceleration on stow.
class VehiclePedalInput {
public:
    GamepadSample update(GamepadSample raw,float vertical,bool weaponOwner,bool available) noexcept {
        if(!available){
            // Another input context owns this sample, but losing that context
            // must not turn a held firing trigger into a pedal on return.
            if(weaponOwner_)triggerReleaseRequired_=true;
            weaponOwner_=false;stickArmed_=false;
            return raw;
        }
        vertical=std::isfinite(vertical)?std::clamp(vertical,-1.f,1.f):0.f;
        if(weaponOwner!=weaponOwner_){
            weaponOwner_=weaponOwner;
            if(weaponOwner)stickArmed_=std::abs(vertical)<.18f;
            else triggerReleaseRequired_=true;
        }
        if(weaponOwner){
            raw.leftTrigger=raw.rightTrigger=0;raw.leftY=0;
            if(std::abs(vertical)<.18f)stickArmed_=true;
            if(stickArmed_){
                const auto amount=static_cast<uint8_t>(std::clamp((std::abs(vertical)-.18f)/.82f,0.f,1.f)*255.f);
                if(vertical>0)raw.rightTrigger=amount;else raw.leftTrigger=amount;
            }
        }else if(triggerReleaseRequired_){
            if(raw.leftTrigger<=24&&raw.rightTrigger<=24)triggerReleaseRequired_=false;
            else raw.leftTrigger=raw.rightTrigger=0;
        }
        return raw;
    }
    void reset() noexcept {*this=VehiclePedalInput{};}
private:
    bool weaponOwner_{},stickArmed_{},triggerReleaseRequired_{};
};
}
