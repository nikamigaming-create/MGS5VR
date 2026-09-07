#include "mgs5vr/input_bridge.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
namespace mgs5vr {
GamepadSample RigEquipment::update(GamepadSample sample,bool modifier,bool allowOptics,uint64_t time,uint64_t pickerDrawTime){
    constexpr uint16_t a=0x1000,b=0x2000,x=0x4000,y=0x8000,rightClick=0x0080;
    constexpr uint16_t categories[]{0x0001,0x0002,0x0008,0x0004};
    if(time<lastTime_)reset();
    lastTime_=time;
    const int sx=sample.rightX,sy=sample.rightY;
    const bool neutral=std::abs(sx)<12000&&std::abs(sy)<12000;
    if(neutral&&!wasNeutral_)neutralSince_=time;
    wasNeutral_=neutral;
    const bool centered=neutral&&time-neutralSince_>=80;
    blockedButtons_&=sample.buttons;
    const bool freshUse=(sample.buttons&(a|rightClick))&&!(blockedButtons_&(a|rightClick));
    const bool freshBack=(sample.buttons&b)&&!(blockedButtons_&b);
    if(centered)blockedStick_=false;
    if(modifier&&!active_){
        blockedStick_=!neutral;categoryChosen_=false;waitBrowseNeutral_=true;
        browseLatched_=false;candidateDirection_=-1;useUntil_=0;
    }
    if(modifier){
        blockedButtons_|=sample.buttons&(a|b|x|y|rightClick);
        if(sample.buttons&x)sample.buttons|=0x0100;
        if(allowOptics&&(sample.buttons&y))sample.buttons|=0x0200;
        sample.buttons&=~(a|b|x|y|rightClick|0x000f);
        sample.rightX=sample.rightY=0;
        if(active_&&freshBack){
            categoryChosen_=false;blockedStick_=!centered;waitBrowseNeutral_=true;
            browseLatched_=false;candidateDirection_=-1;useUntil_=0;
        }
        if(!blockedStick_){
            if(!categoryChosen_){
                if(std::max(std::abs(sx),std::abs(sy))>19660){
                    category_=std::abs(sx)>std::abs(sy)?(sx>0?2u:3u):(sy>0?0u:1u);
                    categoryChosen_=true;waitBrowseNeutral_=true;openedAt_=time;useUntil_=0;
                }
            }else{
                // The expanded native description must have rendered after this
                // category request. Equip/stow animations can defer it; never
                // send early navigation to the gameplay camera in that interval.
                const bool ready=pickerDrawTime>openedAt_&&time>=pickerDrawTime&&time-pickerDrawTime<=150;
                if(!ready){waitBrowseNeutral_=true;browseLatched_=false;candidateDirection_=-1;useUntil_=0;}
                else if(waitBrowseNeutral_){
                    if(centered){waitBrowseNeutral_=false;candidateDirection_=-1;}
                }else if(neutral){
                    if(centered){browseLatched_=false;candidateDirection_=-1;}
                }else if(browseLatched_){
                    sample.rightX=browseX_;sample.rightY=browseY_;
                }else if(std::max(std::abs(sx),std::abs(sy))>19660){
                    // The item picker is an eight-direction radial layout.
                    // One settled flick chooses one card, including diagonals;
                    // thumb wobble cannot select another until centered again.
                    const int direction=(static_cast<int>(std::lround(std::atan2(static_cast<double>(sy),sx)*4/3.141592653589793))+8)%8;
                    if(direction!=candidateDirection_){candidateDirection_=direction;directionSince_=time;}
                    if(time-directionSince_>=60){
                        constexpr int16_t axes[8][2]{{30000,0},{23170,23170},{0,30000},{-23170,23170},
                            {-30000,0},{-23170,-23170},{0,-30000},{23170,-23170}};
                        browseX_=axes[direction][0];browseY_=axes[direction][1];browseLatched_=true;
                        sample.rightX=browseX_;sample.rightY=browseY_;
                    }
                }else candidateDirection_=-1;
            }
        }
        // Holding the trigger alone must not quick-equip the previous category.
        if(categoryChosen_)sample.buttons|=categories[category_];
        if(category_==3&&categoryChosen_&&!waitBrowseNeutral_&&active_&&freshUse)useUntil_=time+100;
    }else{
        useUntil_=0;
        if(active_&&!neutral)blockedStick_=true;
        if(blockedStick_)sample.rightX=sample.rightY=0;
    }
    sample.buttons&=~blockedButtons_;
    if(modifier&&time<useUntil_)sample.buttons|=rightClick;
    active_=modifier;
    return sample;
}
RigInputSample RigInput::update(GamepadSample raw,bool leftGrip,bool rightGrip,TravelMode mode,uint64_t time,uint64_t pickerDrawTime,bool throwing){
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
        return {equipment_.update(raw,rightGrip,false,time,pickerDrawTime),false};
    }
    const bool modifier=raw.leftTrigger>(wristMode_?64:127);
    wristMode_=modifier;
    if(raw.rightTrigger<=24)fireReleaseRequired_=false;
    if(modifier&&raw.rightTrigger>24)fireReleaseRequired_=true;
    const bool ready=rightGrip&&!modifier;
    // Grenade elevation comes from the tracked aim pose. Keep horizontal body
    // turning, but do not also steer the trajectory with native camera pitch.
    if(ready&&throwing)raw.rightY=0;
    raw.leftTrigger=ready?255:0;
    if(modifier||fireReleaseRequired_)raw.rightTrigger=0;
    // The Action Type trigger also performs CQC and throws carried bodies
    // while the weapon is lowered. Let the native action state choose it.
    return {equipment_.update(raw,modifier,false,time,pickerDrawTime),ready};
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
