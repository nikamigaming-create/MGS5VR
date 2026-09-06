#include "mgs5vr/input_bridge.hpp"
#include <chrono>
#include <cstdlib>
namespace mgs5vr {
GamepadSample RigEquipment::update(GamepadSample sample,bool modifier){
    constexpr uint16_t x=0x4000,y=0x8000,stickClick=0x0040;
    const int sx=sample.leftX,sy=sample.leftY;
    blockedButtons_&=sample.buttons;
    if(std::abs(sx)<12000&&std::abs(sy)<12000)blockedStick_=false;
    if(modifier){
        blockedButtons_|=sample.buttons&(x|y|stickClick);
        blockedStick_=true;
        if(sample.buttons&x)sample.buttons|=0x0100;
        if(sample.buttons&y)sample.buttons|=0x0200;
        if(std::abs(sx)>19660||std::abs(sy)>19660){
            if(std::abs(sy)>=std::abs(sx))sample.buttons|=sy>0?0x0001:0x0002;
            else sample.buttons|=sx>0?0x0008:0x0004;
        }
        sample.buttons&=~(x|y|stickClick);
    }
    sample.buttons&=~blockedButtons_;
    if(modifier||blockedStick_)sample.leftX=sample.leftY=0;
    return sample;
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
