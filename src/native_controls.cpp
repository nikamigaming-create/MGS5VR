#include "mgs5vr/native_controls.hpp"
#include <cmath>
#include <atomic>

namespace mgs5vr {
namespace {std::atomic_uint64_t idroidCloseRequestedAt{};}
void requestNativeIdroidClose(bool requested) noexcept {
    idroidCloseRequestedAt.store(requested?steadyMilliseconds():0);
}
bool takeNativeIdroidClose() noexcept {
    const auto requested=idroidCloseRequestedAt.exchange(0),now=steadyMilliseconds();
    return requested&&now>=requested&&now-requested<=1000;
}
bool IdroidBackRecovery::update(bool idroidOpen,bool back,bool available,uint64_t time) noexcept {
    if(!available||!idroidOpen||time<lastTime_){suspend();lastTime_=time;return false;}
    lastTime_=time;
    if(!back){held_=sent_=releaseRequired_=false;return false;}
    if(releaseRequired_)return false;
    if(!held_){held_=true;since_=time;}
    if(!sent_&&time-since_>=750){sent_=true;return true;}
    return false;
}
GamepadSample cabinTitleGamepad(GamepadSample sample,bool spatialTitle,bool confirm) noexcept {
    if(!spatialTitle)return sample;
    GamepadSample result{};
    if(confirm)result.buttons=0x1000;
    return result;
}
GamepadSample nativeMenuGamepad(const ControlBindings& bindings,const PhysicalControls& physical,NativeMenuInput mode){
    GamepadSample pad;
    struct Button {std::string_view name;uint16_t mask;};
    constexpr Button buttons[]{
        {"menus.confirm",0x1000},{"menus.back",0x2000},{"menus.action_x",0x4000},{"menus.action_y",0x8000},
        {"menus.previous_tab",0x100},{"menus.next_tab",0x200},{"menus.left_click",0x40},{"menus.right_click",0x80},
        {"menus.dpad_up",1},{"menus.dpad_down",2},{"menus.dpad_left",4},{"menus.dpad_right",8}};
    for(const auto& [name,mask]:buttons)if(bindings.active(name))pad.buttons|=mask;
    if(mode!=NativeMenuInput::cinematic){
        pad.leftTrigger=static_cast<uint8_t>(bindings.value("menus.left_trigger")*255);
        pad.rightTrigger=static_cast<uint8_t>(bindings.value("menus.right_trigger")*255);
    }
    if(mode!=NativeMenuInput::cinematic){
        const auto move=bindings.axis(mode==NativeMenuInput::menu?"axes.menu":"axes.move",physical);
        const auto look=bindings.axis(mode==NativeMenuInput::scriptedScene?"axes.native_look":"axes.map",physical);
        pad.leftX=static_cast<int16_t>(move[0]*32767);pad.leftY=static_cast<int16_t>(move[1]*32767);
        pad.rightX=static_cast<int16_t>(look[0]*32767);pad.rightY=static_cast<int16_t>(look[1]*32767);
    }
    return pad;
}
NativeControlSample NativeControls::update(const ControlBindings& bindings,const PhysicalControls& physical){
    NativeControlSample result;
    const bool toggle=bindings.active("system.native_buttons");
    if(toggle&&!priorToggle_){selected_=!selected_;releaseRequired_=true;resetDpad();result.changed=true;}
    priorToggle_=toggle;result.selected=selected_;
    if(releaseRequired_){
        bool neutral=true;
        for(size_t n=0;n<11;++n)neutral=neutral&&std::isfinite(physical.buttons[n])&&physical.buttons[n]<=.09f;
        for(const auto stick:{physical.leftStick,physical.rightStick})
            for(const float axis:stick)neutral=neutral&&std::isfinite(axis)&&std::abs(axis)<.18f;
        if(neutral)releaseRequired_=false;
        result.exclusive=true;return result;
    }
    if(!selected_)return result;
    result.exclusive=true;
    auto& pad=result.gamepad;
    for(const auto& button:nativeButtonDefinitions())if(bindings.active(button.name))pad.buttons|=button.mask;
    pad.leftTrigger=static_cast<uint8_t>(bindings.value("native.left_trigger")*255);
    pad.rightTrigger=static_cast<uint8_t>(bindings.value("native.right_trigger")*255);
    const auto move=bindings.axis("axes.native_move",physical),look=bindings.axis("axes.native_look",physical);
    pad.leftX=static_cast<int16_t>(move[0]*32767);pad.leftY=static_cast<int16_t>(move[1]*32767);
    pad.rightX=static_cast<int16_t>(look[0]*32767);pad.rightY=static_cast<int16_t>(look[1]*32767);
    // Menu + one right-stick flick holds the chosen D-pad button. After
    // centering, the same stick is free to browse the game's native equipment
    // cards while Menu keeps that D-pad button held. Releasing Menu releases it.
    // Direct remapped D-pad bindings also work without this optional modifier.
    const bool shift=bindings.active("native.dpad_hold")&&!(pad.buttons&0x0030);
    if(!shift){resetDpad();return result;}
    const bool neutral=std::abs(int(pad.rightX))<6000&&std::abs(int(pad.rightY))<6000;
    if(!shiftHeld_){resetDpad();shiftHeld_=true;waitDirectionNeutral_=!neutral;}
    const auto direction=static_cast<uint16_t>(pad.buttons&0x000f);
    pad.buttons&=~uint16_t{0x000f};
    if(!dpad_){
        if(neutral)waitDirectionNeutral_=false;
        if(!waitDirectionNeutral_&&direction){dpad_=direction;waitBrowseNeutral_=true;}
        pad.rightX=pad.rightY=0;
    }else if(waitBrowseNeutral_){
        if(neutral)waitBrowseNeutral_=false;
        pad.rightX=pad.rightY=0;
    }
    pad.buttons|=dpad_;
    return result;
}
}
