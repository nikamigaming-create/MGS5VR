#include "mgs5vr/input_bridge.hpp"
#include <chrono>
namespace mgs5vr {
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
