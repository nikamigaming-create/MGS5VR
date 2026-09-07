#include "mgs5vr/input_bridge.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
namespace mgs5vr {
GamepadSample RigEquipment::update(GamepadSample sample,bool modifier,bool allowOptics){
    constexpr uint16_t b=0x2000,x=0x4000,y=0x8000,rightClick=0x0080;
    constexpr uint16_t categories[]{0x0001,0x0002,0x0008,0x0004};
    const int sx=sample.rightX,sy=sample.rightY;
    const bool neutral=std::abs(sx)<12000&&std::abs(sy)<12000;
    blockedButtons_&=sample.buttons;
    const bool freshUse=(sample.buttons&rightClick)&&!(blockedButtons_&rightClick);
    const bool freshBack=(sample.buttons&b)&&!(blockedButtons_&b);
    if(!(sample.buttons&rightClick)||!modifier||!active_)useHeld_=false;
    if(neutral)blockedStick_=false;
    if(neutral)waitBrowseNeutral_=false;
    if(modifier&&!active_){blockedStick_=!neutral;categoryChosen_=false;waitBrowseNeutral_=false;}
    if(modifier){
        blockedButtons_|=sample.buttons&(b|x|y|rightClick);
        if(sample.buttons&x)sample.buttons|=0x0100;
        if(allowOptics&&(sample.buttons&y))sample.buttons|=0x0200;
        sample.buttons&=~(b|x|y|rightClick|0x000f);
        sample.rightX=sample.rightY=0;
        if(active_&&freshBack){categoryChosen_=false;blockedStick_=!neutral;waitBrowseNeutral_=false;useHeld_=false;}
        if(!blockedStick_){
            if(!categoryChosen_){
                if(std::max(std::abs(sx),std::abs(sy))>19660){
                    category_=std::abs(sx)>std::abs(sy)?(sx>0?2u:3u):(sy>0?0u:1u);
                    categoryChosen_=true;waitBrowseNeutral_=true;useHeld_=false;
                }
            }else if(!waitBrowseNeutral_){
                // Match the displayed native menu. Up must never become
                // horizontal browsing, and choosing a category must not also
                // skip a card before the controller has returned to neutral.
                sample.rightX=static_cast<int16_t>(sx);
                sample.rightY=static_cast<int16_t>(sy);
            }
        }
        sample.buttons|=categories[category_];
        // The native item-card Use action is a right-stick click. Consume the
        // same held click on close so it cannot become an unrelated action.
        if(category_==3&&categoryChosen_&&!waitBrowseNeutral_&&active_&&freshUse)useHeld_=true;
        if(useHeld_)sample.buttons|=rightClick;
    }else{
        if(active_&&!neutral)blockedStick_=true;
        if(blockedStick_)sample.rightX=sample.rightY=0;
    }
    sample.buttons&=~(blockedButtons_&~(useHeld_?rightClick:0));
    active_=modifier;
    return sample;
}
RigInputSample RigInput::update(GamepadSample raw,bool leftGrip,bool rightGrip,TravelMode mode){
    if(mode==TravelMode::unknown){releaseRequired_=true;wristMode_=false;equipment_.reset();return {};}
    if(mode!=mode_){
        if(mode_!=TravelMode::unknown)releaseRequired_=true;
        mode_=mode;wristMode_=false;equipment_.reset();
    }
    if(releaseRequired_){
        if(raw.buttons||raw.leftTrigger>24||raw.rightTrigger>24||leftGrip||rightGrip
           ||std::abs(static_cast<int>(raw.leftX))>6000||std::abs(static_cast<int>(raw.leftY))>6000
           ||std::abs(static_cast<int>(raw.rightX))>6000||std::abs(static_cast<int>(raw.rightY))>6000)return {};
        releaseRequired_=false;
    }
    if(mode==TravelMode::vehicle){
        if(leftGrip)raw.buttons|=0x0100;
        return {equipment_.update(raw,rightGrip,false),false};
    }
    const bool modifier=raw.leftTrigger>(wristMode_?64:127);
    wristMode_=modifier;
    if(raw.rightTrigger<=24)fireReleaseRequired_=false;
    if(modifier&&raw.rightTrigger>24)fireReleaseRequired_=true;
    const bool ready=rightGrip&&!modifier;
    raw.leftTrigger=ready?255:0;
    if(modifier||fireReleaseRequired_)raw.rightTrigger=0;
    // The Action Type trigger also performs CQC and throws carried bodies
    // while the weapon is lowered. Let the native action state choose it.
    return {equipment_.update(raw,modifier,false),ready};
}
uint16_t MenuButton::update(bool pressed,bool active,uint64_t time){
    if(!active||time<lastTime_){
        held_=false;longSent_=false;pulse_=0;releaseRequired_=pressed;lastTime_=time;return 0;
    }
    lastTime_=time;
    if(releaseRequired_){if(!pressed)releaseRequired_=false;return 0;}
    if(pressed&&!held_){held_=true;longSent_=false;pressedAt_=time;}
    if(pressed&&held_&&!longSent_&&time-pressedAt_>=550){
        pulse_=0x0020;pulseUntil_=time+100;longSent_=true;
    }
    if(!pressed&&held_){
        if(!longSent_){pulse_=0x0010;pulseUntil_=time+100;}
        held_=false;
    }
    if(time>=pulseUntil_)pulse_=0;
    return pulse_;
}
void GamepadMailbox::publish(GamepadSample sample,bool active,uint64_t time){
    std::lock_guard guard(mutex_);sample_=sample;active_=active;connected_=connected_||active;timestamp_=time;
}
std::optional<GamepadSample> GamepadMailbox::read(uint64_t time,bool* freshActive) const {
    std::lock_guard guard(mutex_);
    if(freshActive)*freshActive=false;
    if(!connected_)return {};
    if(!active_||time<timestamp_||time-timestamp_>250)return GamepadSample{};
    if(freshActive)*freshActive=true;
    return sample_;
}
GamepadMailbox& gamepadMailbox(){static GamepadMailbox box;return box;}
uint64_t steadyMilliseconds(){return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
}
