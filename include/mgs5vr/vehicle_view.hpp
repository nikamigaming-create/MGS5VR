#pragma once
#include <cmath>
#include <cstdint>

namespace mgs5vr {
struct NativeVehicleView {
    bool valid{},normalAction{},mounted{},headingValid{};
    uint16_t object{65535};
    float heading{};
};

// Keeps the gameplay yaw before HMD and snap composition. Entry/exit animation
// belongs to Snake; only the carrier and deliberate look turn the seated view.
class VehicleViewHeading {
public:
    float update(float native,float snap,NativeVehicleView sample,bool looking,uint64_t time,bool interact=false){
        constexpr float tau=6.283185307f;
        const auto wrap=[](float angle){return std::remainder(angle,tau);};
        if(!std::isfinite(native)||!std::isfinite(snap))return native;
        const bool fresh=initialized_&&time>=time_&&time-time_<=500;
        if(!fresh)lookUntil_=0;
        const bool seated=sample.valid&&(sample.mounted||sample.headingValid);
        const bool wasEngaged=engaged_;
        // A ledge catch can align Snake without an interact press. Capture
        // the preceding normal frame before that authored alignment turns the
        // native camera; the HMD must not inherit the actor's forced rotation.
        if(fresh&&sample.valid&&!sample.normalAction&&normalAction_&&!seated&&!engaged_){
            engaged_=true;pendingAction_=false;sawAction_=true;lookUntil_=0;
        }
        if(interact&&!interact_){
            // Retail begins aligning Snake before ON_VEHICLE changes. Capture
            // the preceding camera transaction at the effective VR Y edge.
            if(!engaged_){
                heading_=initialized_?heading_:wrap(native+offset_);
                engaged_=pendingAction_=true;sawAction_=false;
            }
            lookUntil_=0;
        }
        if(seated&&!seated_){
            if(!engaged_)
                heading_=normalReady_?normalHeading_:sample.headingValid?sample.heading:wrap(native+offset_);
            // Mounted input resets the accumulated on-foot snap. Preserve its
            // contribution once, without copying an HMD-composed eye pose.
            if(initialized_)heading_=wrap(heading_+snap_-snap);
            engaged_=true;pendingAction_=sawAction_=false;carrier_=65535;
        }
        float carrierDelta{};
        if(engaged_){
            if(seated&&sample.headingValid){
                if(carrier_==sample.object&&fresh)
                    carrierDelta=wrap(sample.heading-carrierHeading_);
                carrier_=sample.object;carrierHeading_=sample.heading;
            }else carrier_=65535;
            if(looking)lookUntil_=time+250;
            if(wasEngaged&&fresh&&time>time_&&time<=lookUntil_){
                const float lookDelta=wrap(native-native_)-carrierDelta;
                if(std::abs(lookDelta)<.9f)heading_=wrap(heading_+lookDelta);
            }
            heading_=wrap(heading_+carrierDelta);
            // The native status, not an elapsed animation guess, returns yaw
            // ownership. Retain the offset for subsequent ordinary look.
            if(sample.valid&&!sample.normalAction) sawAction_=true;
            if(sample.valid&&!seated&&sample.normalAction&&(!pendingAction_||sawAction_||looking)){
                offset_=wrap(heading_-native);engaged_=pendingAction_=sawAction_=false;
            }
        }else heading_=wrap(native+offset_);
        if(sample.valid&&!seated&&sample.normalAction){
            normalHeading_=heading_;normalReady_=true;
        }
        native_=native;snap_=snap;time_=time;initialized_=true;interact_=interact;seated_=seated;
        normalAction_=sample.valid&&sample.normalAction;
        return heading_;
    }
    void reset(){*this={};}
private:
    float native_{},snap_{},heading_{},offset_{},normalHeading_{},carrierHeading_{};
    uint64_t time_{},lookUntil_{};
    uint16_t carrier_{65535};
    bool initialized_{},normalReady_{},engaged_{},pendingAction_{},sawAction_{},interact_{},seated_{};
    bool normalAction_{};
};
}
