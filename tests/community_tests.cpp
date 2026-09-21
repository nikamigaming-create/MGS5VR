#include "mgs5vr/controls.hpp"
#include "mgs5vr/arm_ik.hpp"
#include "mgs5vr/head_camera.hpp"
#include "mgs5vr/idroid_rig.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/native_controls.hpp"
#include <cmath>
#include <iostream>
#include <sstream>
using namespace mgs5vr;
namespace {
int failures{},checks{};
void expect(bool value,const char* message){++checks;if(!value){++failures;std::cerr<<message<<'\n';}}
bool near(float a,float b){return std::abs(a-b)<.0001f;}
}
int main(){
    const Vec3 nativeBarrel{0,-.5f,-.8660254f};
    const Pose palm{{},{.2f,-.25f,-.4f}},aim{{},{.2f,-.2f,-.5f}};
    const auto pointed=aimedWeaponGrip(palm,aim,nativeBarrel);
    expect(pointed&&dot(rotate(pointed->orientation,nativeBarrel),Vec3{0,0,-1})>.9999f,
        "one-handed native barrel follows controller aim instead of palm pitch");
    expect(pointed&&near(pointed->position.x,palm.position.x)&&near(pointed->position.y,palm.position.y)
        &&near(pointed->position.z,palm.position.z),"aim correction preserves the tracked palm pivot");
    const Pose rotated{{0,0,.70710678f,.70710678f},{}};
    const auto rolled=aimedWeaponGrip(compose(rotated,palm),compose(rotated,aim),nativeBarrel);
    expect(rolled&&pointed&&dot(rotate(rolled->orientation,{1,0,0}),rotate(compose(rotated,*pointed).orientation,{1,0,0}))>.9999f,
        "aim correction retains controller roll and a common coordinate frame");
    expect(!aimedWeaponGrip(palm,aim,{})&&!aimedWeaponGrip(palm,aim,{0,0,1}),
        "invalid or backwards native muzzle cannot flip the wrist");
    GamepadOwnership owner;GamepadSample real{},drift{};drift.leftX=4000;drift.rightY=-5000;
    expect(owner.update(true,{},true,false),"connected Xbox pad owns neutral input by default");
    expect(owner.update(true,drift,true,false),"small stick drift cannot switch owners");
    expect(!owner.update(true,{},true,true),"fresh wand input takes over a neutral gamepad");
    real.buttons=0x1000;
    expect(owner.update(true,real,true,true)&&owner.update(true,{},true,true),"Xbox button takes over and held wand cannot steal input back");
    owner.update(true,{},true,false);
    expect(!owner.update(true,{},true,true),"released and pressed wand can take over again");
    expect(owner.update(true,{},false,false),"XR focus loss returns physical pad immediately");
    expect(!owner.update(false,{},true,false),"disconnect releases physical ownership");
    NativeControls native;ControlBindings defaults;PhysicalControls neutral;
    defaults.update(neutral,ControlContext::menus,100);
    expect(native.update(defaults,neutral,true).changed&&native.selected(),"cinema mode selects normal native controller layout");
    defaults.update(neutral,ControlContext::nativeButtons,120);native.update(defaults,neutral,true);
    neutral.buttons[0]=1;neutral.leftStick={.5f,.6f};neutral.rightStick={-.4f,.8f};
    defaults.update(neutral,ControlContext::nativeButtons,140);const auto ordinary=native.update(defaults,neutral,true);
    expect((ordinary.gamepad.buttons&0x1000)&&ordinary.gamepad.leftX>0&&ordinary.gamepad.rightY>0,"cinema retains native A and both analog sticks");
    expect(native.update(defaults,neutral,false).changed&&!native.selected(),"return to 3D restores the previous wand layout");
    ControlBindings controls;
    expect(controls.setting("settings.handheld_menus")==0,"handheld menus require explicit opt-in");
    const auto load=[&](const char* text){std::istringstream input(text);return controls.load(input).empty();};
    expect(load("[settings]\nplayer_height_offset_cm=12\nright_hand_roll_degrees=-15\nweapon_smoothing_ms=45\nsupport_grip_radius_cm=14\nsupport_detach_radius_cm=35\n"),"community tuning loads");
    expect(!load("[settings]\nsupport_detach_radius_cm=15\n")&&controls.setting("settings.support_detach_radius_cm")==35,"invalid radius pair preserves prior settings");
    expect(!load("[settings]\nhand_rest_curl_percent=30\nhand_touch_curl_percent=20\n"),"touch cannot open a resting hand");
    expect(!load("[settings]\nright_hand_x_cm=nan\n")&&!load("[settings]\nweapon_smoothing_ms=151\n"),"invalid fit and smoothing refused");
    PhysicalControls input{};controls.update(input,ControlContext::menus,100);controls.update(input,ControlContext::menus,110);
    input.buttons[4]=input.buttons[1]=1;controls.update(input,ControlContext::menus,120);controls.update(input,ControlContext::menus,680);
    expect(controls.active("system.toggle_vr")&&!controls.active("system.pause")&&!controls.active("menus.back"),"3D chord owns Menu and B without native menu actions");
    controls.update(input,ControlContext::menus,900);expect(!controls.active("system.toggle_vr"),"held 3D chord cannot oscillate presentation");
    input={};controls.update(input,ControlContext::menus,1000);controls.update(input,ControlContext::menus,1010);
    input.buttons[4]=1;controls.update(input,ControlContext::menus,1020);input={};controls.update(input,ControlContext::menus,1100);
    expect(controls.active("system.idroid"),"original Menu tap survives the new chord");
    expect(near(freeFingerCurl(0,0,0,false,false),.08f)&&near(freeFingerCurl(1,0,0,false,false),.08f),"relaxed thumb and index curl");
    expect(near(freeFingerCurl(0,0,0,false,true),.20f)&&near(freeFingerCurl(1,0,0,true,false),.20f),"capacitive thumb and index curl");
    expect(near(freeFingerCurl(1,.8f,0,true,false),.8f)&&near(freeFingerCurl(3,0,1,false,false),1),"physical trigger and grip still close fully");
    SupportContact support;
    expect(!support.update(true,true,.12f,100,.14f,.35f)&&support.update(true,true,.12f,251,.14f,.35f),"custom acquisition radius has dwell");
    expect(support.update(true,true,.34f,300,.14f,.35f)&&!support.update(true,true,.36f,320,.14f,.35f),"custom detach radius owns release");
    WeaponGripSmoothing smooth;
    Pose head{},start{{},{0,0,-.4f}},target{{},{.04f,0,-.4f}};
    smooth.update(head,start,true,50,100,1);const auto filtered=smooth.update(head,target,true,50,120,1);
    expect(filtered.position.x>0&&filtered.position.x<target.position.x,"configured smoothing damps small grip motion");
    const auto direct=smooth.update(head,target,true,0,130,1);expect(near(direct.position.x,target.position.x),"zero smoothing is immediate");
    smooth.update(head,start,true,50,200,1);
    expect(near(smooth.update(head,target,true,50,400,1).position.x,target.position.x),"tracking gaps reset smoothing without dragging a stale weapon");
    HeadCamera camera;camera.configure(true);
    const std::array<EyeView,2> eyes{{{Pose{{},{-.032f,0,0}},{-.7f,.7f,.7f,-.7f}},{Pose{{},{.032f,0,0}},{-.7f,.7f,.7f,-.7f}}}};
    ControllerFrame frame;frame.predictedXrTime=1;frame.referenceEpoch=1;frame.authoredCamera=true;frame.frontEnd=true;
    frame.hands[1].gripTracked=true;frame.weaponReady=true;frame.cabinMove={0,1};
    camera.trackStereo({},eyes,true,100,frame);camera.toggle();const auto before=camera.resolve(1,{},100);
    auto passive=passiveControllerFrame(frame,2,1);
    expect(passive.frontEnd&&passive.authoredCamera&&!passive.hands[1].gripTracked&&!passive.weaponReady&&!passive.allowAnimalTouch&&passive.cabinMove[1]==0,"focus loss preserves scene and clears input");
    camera.trackStereo({},eyes,true,1000,passive);const auto unfocused=camera.resolve(1,{},1000);
    expect(before.applied&&unfocused.applied&&before.activation==unfocused.activation&&!camera.status().suspended,"platform menu does not suspend a valid headset or change scene generation");
    frame.hands={};frame.predictedXrTime=3;
    camera.trackStereo({},eyes,true,2000,frame);const auto idle=camera.resolve(1,{},2000);
    expect(idle.applied&&idle.activation==before.activation,"both inactive controllers leave valid stereo running");
    HeadCamera height;height.configure(true);ControllerFrame fit;fit.playerHeightOffset=.12f;
    height.trackStereo({},eyes,true,100,fit);height.toggle();const auto raised=height.resolve(1,{},100);
    height.trackStereo({},eyes,true,110,fit);const auto again=height.resolve(1,{},110);
    expect(raised.applied&&again.applied&&near(raised.nativePose.position.y,.12f)&&near(again.nativePose.position.y,.12f),"height offset applies once without accumulating");
    HeadCameraSample menu;menu.applied=menu.stereoTracked=true;menu.activation=1;menu.controllers.hands[1].gripTracked=true;
    menu.controllers.hands[1].grip.position={0,0,-.4f};const auto close=trackedIdroidPose(menu);
    menu.controllers.idroidScreenDepth=.08f;const auto projected=trackedIdroidPose(menu);
    expect(close&&projected&&near(dot(projected->screen.position-close->screen.position,rotate(close->screen.orientation,{0,0,1})),.08f)
        &&near(projected->body.position.z,close->body.position.z),"iDroid depth moves projection while handset stays in palm");
    menu.controllers.hands={};menu.controllers.nativeGamepad=true;menu.renderedPalmTracked[1]=true;menu.renderedPalms[1]={{},{.1f,0,-.4f}};
    expect(trackedIdroidPose(menu).has_value(),"physical gamepad iDroid remains on the native animated palm without wands");
    HeadCamera panelCamera;panelCamera.configure(true);ControllerFrame panelInput;
    panelCamera.trackStereo({},eyes,true,100,panelInput);panelCamera.toggle();panelCamera.resolve(1,{},100);
    panelCamera.setNativeMenuOpen(true,true);const auto panelBefore=panelCamera.resolve(1,{},101);
    panelCamera.trackStereo({{},{.12f,0,0}},eyes,true,110,panelInput);const auto panelAfter=panelCamera.resolve(1,{{},{1,0,0}},110);
    expect(panelBefore.applied&&panelAfter.applied&&panelCamera.active()&&panelAfter.menuIdroid,
        "paused quad retains active 3D and fresh head tracking");
    expect(near(panelBefore.menuPanel.position.x,panelAfter.menuPanel.position.x)
        &&near(panelBefore.menuPanel.position.z,panelAfter.menuPanel.position.z)
        &&!near(panelBefore.nativePose.position.x,panelAfter.nativePose.position.x),
        "paused panel stays in world while the head moves around it");
    expect(near(panelBefore.menuPanel.position.y,-.20f)
        &&near(rotate(panelBefore.menuPanel.orientation,{0,0,1}).y,std::sin(10.f*.0174532925f)),
        "paused quad sits below eye level and tilts up toward the viewer");
    panelCamera.setNativeMenuOpen(false);panelInput.handheldMenus=true;
    panelCamera.trackStereo({},eyes,true,120,panelInput);panelCamera.resolve(1,{},120);panelCamera.setNativeMenuOpen(true,true);
    const auto handheldBefore=panelCamera.resolve(1,{},121),handheldAfter=panelCamera.resolve(1,{{},{.2f,0,0}},122);
    expect(handheldBefore.applied&&handheldAfter.applied&&near(handheldAfter.nativePose.position.x-handheldBefore.nativePose.position.x,.2f),
        "opt-in handheld mode retains the live native camera");
    std::cout<<checks<<" community checks; "<<failures<<" failures\n";return failures?1:0;
}
