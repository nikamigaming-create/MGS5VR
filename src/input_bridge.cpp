#include "mgs5vr/input_bridge.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
namespace mgs5vr {
WheelSample WheelSteering::update(Pose tracked,Pose contact,bool available,bool squeeze,uint64_t time,uint64_t epoch,Vec3 forward){
    if(!available||!valid(tracked)||!valid(contact)||!epoch){reset();held_=squeeze;return {};}
    if(time<time_||epoch!=epoch_){reset();held_=squeeze;}
    time_=time;epoch_=epoch;
    if(!squeeze){held_=gripped_=false;return {};}
    const auto separation=tracked.position-contact.position;
    bool engaged{};
    if(!held_&&dot(separation,separation)<.04f&&valid(Pose{{},forward})&&std::abs(dot(forward,forward)-1)<.01f){
        gripped_=engaged=true;start_=tracked;axis_=forward;
    }
    held_=true;
    const auto travel=tracked.position-start_.position;
    if(dot(travel,travel)>.36f)gripped_=false;
    if(!gripped_)return {};
    auto rotation=compose(Pose{tracked.orientation,{}},inverse(Pose{start_.orientation,{}})).orientation;
    if(rotation.w<0)rotation={-rotation.x,-rotation.y,-rotation.z,-rotation.w};
    // Signed twist in LOCAL around the forward axis captured at acquisition.
    // Engine +Z versus XR -Z cannot invert steering; subsequent head turns
    // cannot rotate the steering basis. Grip pitch does not create a singularity.
    const float angle=2*std::atan2(dot(Vec3{rotation.x,rotation.y,rotation.z},axis_),rotation.w);
    const float axis=std::clamp(angle/1.04719755f,-1.f,1.f);
    return {std::abs(axis)<.035f?0.f:axis,true,engaged,time};
}
void WheelMailbox::publish(WheelSample sample){std::lock_guard lock(mutex_);sample_=sample;}
WheelSample WheelMailbox::read(uint64_t time) const{std::lock_guard lock(mutex_);return time>=sample_.time&&time-sample_.time<=100?sample_:WheelSample{};}
WheelMailbox& wheelMailbox(){static WheelMailbox box;return box;}
void RumbleMailbox::publish(RumbleSample sample){std::lock_guard lock(mutex_);sample_=sample;}
RumbleSample RumbleMailbox::read(uint64_t time) const{std::lock_guard lock(mutex_);return time>=sample_.time&&time-sample_.time<=250?sample_:RumbleSample{};}
RumbleMailbox& rumbleMailbox(){static RumbleMailbox box;return box;}
OpticsInput RigOptics::update(GamepadSample raw,bool available){
    if(!available){reset();return {raw};}
    constexpr uint16_t y=0x8000,click=0x0080,b=0x2000;
    const bool chord=raw.leftTrigger>127&&(raw.buttons&y),pressed=raw.buttons&click,back=raw.buttons&b;
    if(active_&&(raw.buttons&0x0030)){
        active_=false;releaseRequired_=true;
        return {GamepadSample{static_cast<uint16_t>(raw.buttons&0x0030)},1,true};
    }
    if(chord&&!priorChord_){active_=!active_;power_=0;if(!active_)releaseRequired_=true;}
    else if(active_&&back&&!priorBack_){active_=false;releaseRequired_=true;}
    else if(active_&&pressed&&!priorClick_)power_=(power_+1)%2;
    priorChord_=chord;priorClick_=pressed;priorBack_=back;
    if(active_){
        // The optical mode owns fire, equipment and stance inputs. Walking and
        // horizontal turning remain available, and head tracking stays live.
        raw.buttons=0;raw.leftTrigger=raw.rightTrigger=0;raw.rightY=0;
        return {raw,power_?4.f:2.f,true};
    }
    if(releaseRequired_){
        if(!raw.buttons&&raw.leftTrigger<=24&&raw.rightTrigger<=24
            &&std::abs(int(raw.rightX))<6000&&std::abs(int(raw.rightY))<6000)releaseRequired_=false;
        return {{},1,true};
    }
    return {raw};
}
CommandsInput RigCommands::update(GamepadSample raw,bool available,uint64_t time,uint64_t drawTime){
    if(time<lastTime_){reset();releaseRequired_=true;}
    lastTime_=time;
    const bool chord=raw.leftTrigger>127&&(raw.buttons&0x4000);
    const bool freshChord=chord&&!priorChord_;
    priorChord_=chord;
    const bool confirm=raw.rightTrigger>127||(raw.buttons&0x0080);
    const bool neutral=std::abs(int(raw.rightX))<6000&&std::abs(int(raw.rightY))<6000;
    if(!available&&active_){active_=false;releaseRequired_=true;}
    if(releaseRequired_){
        if(!raw.buttons&&raw.leftTrigger<=24&&raw.rightTrigger<=24&&neutral)releaseRequired_=false;
        return {GamepadSample{0,0,0,raw.leftX,raw.leftY},false,true};
    }
    if(!available)return {raw};
    if(freshChord&&!active_){
        active_=true;openedAt_=time;confirmHeld_=confirm;stickBlocked_=true;confirmUntil_=0;confirmed_=false;
    }
    if(!active_)return {raw};
    const auto menus=static_cast<uint16_t>(raw.buttons&0x0030);
    if(raw.leftTrigger<64||(raw.buttons&0x2000)||menus){
        active_=false;releaseRequired_=true;confirmUntil_=0;
        return {GamepadSample{menus,0,0,raw.leftX,raw.leftY},false,true};
    }
    const bool ready=drawTime>openedAt_&&time>=drawTime&&time-drawTime<=150;
    if(!ready)stickBlocked_=true;
    else if(neutral)stickBlocked_=false;
    if(ready&&!stickBlocked_&&confirm&&!confirmHeld_&&!confirmed_){confirmUntil_=time+100;confirmed_=true;}
    confirmHeld_=confirm;
    GamepadSample result{static_cast<uint16_t>(0x0100|((ready&&time<confirmUntil_)?0x0080:0)),0,0,raw.leftX,raw.leftY};
    // Native Call resolves direction and R3 in the same input packet. Keep
    // the selection through its bounded confirm pulse, then consume it even
    // if the user keeps holding the stick after the native menu disappears.
    if(ready&&!stickBlocked_&&(!confirmed_||time<confirmUntil_)){result.rightX=raw.rightX;result.rightY=raw.rightY;}
    return {result,true,true};
}
MotionStrike MotionMelee::update(Pose head,Pose hand,bool available,uint64_t time,uint64_t epoch,bool weapon){
    MotionStrike result;
    if(!available||!valid(head)||!valid(hand)||!epoch){reset();return result;}
    // Keep a fixed LOCAL orientation while subtracting head translation. Head
    // rotation by itself must not sweep a stationary controller through targets.
    const auto relative=hand.position-head.position;
    if(!primed_||epoch!=epoch_||time<=previousTime_||time-previousTime_>80){
        reset();previous_=relative;previousContact_=hand.position;previousTime_=time;epoch_=epoch;primed_=true;return result;
    }
    const float seconds=float(time-previousTime_)/1000.f;
    const auto delta=relative-previous_;const float distance=std::sqrt(dot(delta,delta));
    const float speed=distance/seconds;
    const auto physical=hand.position-previousContact_;
    const auto previous=previous_;previous_=relative;previousContact_=hand.position;previousTime_=time;
    const float reach=std::sqrt(dot(relative,relative));
    if(distance>.3f||speed>8.f||reach>(weapon?1.9f:1.15f)||reach<.12f){moving_=striking_=false;latched_=true;return result;}
    if(speed<.45f){moving_=striking_=false;if(time>=cooldownUntil_)latched_=false;return result;}
    const auto forward=rotate(head.orientation,{0,0,-1});
    const bool inFront=dot(relative,forward)>.08f;
    // Forward jabs and downward strikes toward a nearby grounded target both
    // qualify. Return strokes and sweeping across the wrist menu do not.
    const bool outbound=dot(delta,relative)>0&&inFront;
    if(!outbound){moving_=striking_=false;return result;}
    if(striking_){
        if(time-startedAt_>260||speed<.8f){striking_=false;return result;}
        return {true,false,1,head.position+previous,hand.position};
    }
    if(latched_||time<cooldownUntil_)return {false,false,time<cooldownUntil_?1.f:0.f};
    // A head-only duck must not turn a stationary fist into a downward strike.
    if(dot(physical,physical)<seconds*seconds*.8f*.8f){moving_=false;return result;}
    if(!moving_){if(speed<1.15f)return result;moving_=true;start_=previous;startedAt_=time;}
    const auto stroke=relative-start_;
    if(time-startedAt_>260){moving_=false;return result;}
    result.curl=1;
    if(speed>=1.3f&&dot(stroke,stroke)>=.0225f){
        result.strike=result.started=true;result.start=head.position+previous;result.end=hand.position;
        moving_=false;striking_=latched_=true;cooldownUntil_=time+300;
    }
    return result;
}
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
        const auto wheel=wheelMailbox().read(time);
        if(leftGrip&&wheel.gripped)raw.leftX=static_cast<int16_t>(wheel.axis*32767);
        // Left grip belongs to the physical wheel. X retains native vehicle
        // attack/call, while both triggers retain their pedal meanings.
        if(raw.buttons&0x4000){raw.buttons&=~0x4000;raw.buttons|=0x0100;}
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
