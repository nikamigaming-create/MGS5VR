#include "mgs5vr/core.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/head_camera.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <string>

using namespace mgs5vr;
static int checks{},failures{};
static void expect(bool ok,const char* name){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<name<<'\n';}}
static bool near(float a,float b){return std::abs(a-b)<0.0001f;}
static bool same(Vec3 a,Vec3 b){return near(a.x,b.x)&&near(a.y,b.y)&&near(a.z,b.z);}
int main(){
    const EyeFov asymmetric{-0.8f,0.9f,0.75f,-0.7f};
    std::array<float,16> projection{1,0,0,0,0,1,0,0,0,0,-0.0002f,1,0,0,0.1f,0};
    expect(setEyeProjection(projection,asymmetric),"native perspective accepts runtime asymmetric field of view");
    const auto ndc=[&](float x,float y,float z){return Vec3{(x*projection[0]+z*projection[8])/z,(y*projection[5]+z*projection[9])/z,(z*projection[10]+projection[14])/z};};
    expect(near(ndc(-std::tan(asymmetric.left)*3,0,3).x,-1),"left eye frustum boundary projects to left edge");
    expect(near(ndc(-std::tan(asymmetric.right)*3,0,3).x,1),"right eye frustum boundary projects to right edge");
    expect(near(ndc(0,std::tan(asymmetric.up)*3,3).y,1),"upper eye frustum boundary projects to upper edge");
    expect(near(ndc(0,std::tan(asymmetric.down)*3,3).y,-1),"lower eye frustum boundary projects to lower edge");
    expect(near(projection[10],-0.0002f)&&near(projection[14],0.1f),"eye optics preserve native depth convention");
    auto ortho=projection;ortho[11]=0;ortho[15]=1;
    expect(!setEyeProjection(ortho,asymmetric),"orthographic pass cannot be mistaken for scene projection");
    std::array<EyeFrame,2> pair{};
    for(uint32_t n=0;n<2;++n)pair[n]={EyeView{Pose{{},{n?0.032f:-0.032f,0,0}},asymmetric},9,31,4,100,n,true,true};
    expect(readyEyePair(pair,4,110),"both native eyes from one simulation and tracking transaction are eligible");
    pair[1].sourceSequence=10;
    expect(!readyEyePair(pair,4,110),"alternate-eye consecutive simulation frames are rejected");
    pair[1].sourceSequence=9;pair[1].trackingSequence=32;
    expect(!readyEyePair(pair,4,110),"different pose generations cannot be submitted as one native frame");
    pair[1].trackingSequence=31;pair[0].joined=false;
    expect(!readyEyePair(pair,4,110),"unjoined native image never receives newer tracking metadata");
    pair[0].joined=true;
    expect(!readyEyePair(pair,5,110),"old activation cannot survive recenter");
    expect(!readyEyePair(pair,4,251),"stale eye images stop submission");
    const auto l=nativeEyePose(Pose{{},{10,20,30}},Pose{},Pose{{},{-0.032f,0,0}});
    const auto r=nativeEyePose(Pose{{},{10,20,30}},Pose{},Pose{{},{0.032f,0,0}});
    expect(near(l.position.x,10.032f)&&near(r.position.x,9.968f),"same-frame eye offsets preserve runtime IPD in FOX camera axes");
    expect(near(l.position.y,r.position.y)&&near(l.position.z,r.position.z),"parallel eye cameras do not introduce toe-in or vertical disparity");
    HeadCamera camera;
    const Pose nativeCamera{{},{10,20,30}};
    camera.track({},true,100);camera.toggle();
    expect(!camera.resolve(1,nativeCamera,100).applied,"native head camera is opt-in");
    camera.configure(true);camera.toggle();
    auto resolved=camera.resolve(1,nativeCamera,100);
    expect(resolved.applied&&same(resolved.nativePose.position,nativeCamera.position),"activation establishes a no-jump head origin");
    camera.track(Pose{{},{1,2,-3}},true,110);
    resolved=camera.resolve(1,nativeCamera,110);
    expect(resolved.applied&&same(resolved.nativePose.position,{9,22,33}),"head translation uses the explicit native camera basis");
    const auto frozen=resolved;
    camera.track(Pose{{},{4,5,-6}},true,120);
    expect(same(frozen.nativePose.position,{9,22,33}),"a captured publication pose remains immutable after newer tracking");
    expect(!camera.resolve(2,nativeCamera,120).applied&&!camera.active(),"camera identity change cancels explicit activation");
    expect(camera.status().reason==HeadCameraStop::cameraChanged,"camera identity cancellation records its reason");
    camera.toggle();camera.resolve(1,nativeCamera,120);
    expect(!camera.resolve(1,nativeCamera,271).applied&&!camera.active(),"stale tracking cancels native camera writes");
    expect(camera.status().reason==HeadCameraStop::staleTracking,"stale tracking cancellation remains observable");
    camera.track({},true,300);camera.toggle();camera.resolve(1,nativeCamera,300);
    camera.track({},false,301);
    expect(!camera.resolve(1,nativeCamera,301).applied,"tracking loss restores the native camera");
    camera.track({},true,400);camera.toggle();camera.resolve(1,nativeCamera,400);
    camera.track(Pose{{0.70710678f,0,0,0.70710678f},{}},true,410);
    resolved=camera.resolve(1,nativeCamera,410);
    expect(resolved.applied&&same(rotate(resolved.nativePose.orientation,{0,0,1}),{0,1,0}),"head pitch rotates in the native camera basis");
    camera.cancel();expect(!camera.active(),"manual cancellation returns control to the native view");
    camera.configure(false);expect(!camera.available(),"disabled camera experiment does not reserve a native input chord");
    camera.configure(true);camera.track({},true,steadyMilliseconds());camera.toggle();
    expect(camera.resolveCurrent(1,nativeCamera).applied,"live camera clock is sampled atomically with tracking");
    HeadCamera rigidCamera;rigidCamera.configure(true);rigidCamera.track({},true,500);rigidCamera.toggle();
    const auto normalized=rigidCamera.resolve(1,Pose{{0,0.60003f,0,0.80004f},{500,300,1300}},500);
    const auto nq=normalized.nativePose.orientation;
    expect(normalized.applied&&std::abs(nq.y*nq.y+nq.w*nq.w-1)<0.0000002f,"kilometer-scale native camera keeps a normalized rigid rotation");
    GamepadMailbox gamepad;
    expect(!gamepad.read(100),"unconnected XR gamepad preserves original input path");
    gamepad.publish({0x1000,0,255,100,-100,0,0},true,100);
    auto input=gamepad.read(200);
    expect(input&&input->buttons==0x1000&&input->rightTrigger==255,"active XR input reaches native gamepad sample");
    input=gamepad.read(351);
    expect(input&&input->buttons==0&&input->rightTrigger==0,"stale XR releases held fire and buttons");
    gamepad.publish({0x1000,255,255,0,0,0,0},false,400);
    input=gamepad.read(400);
    expect(input&&*input==GamepadSample{},"focus loss forces neutral state");
    gamepad.publish({0x1000,255,255,0,0,0,0},true,500);
    expect(gamepad.read(499)==GamepadSample{},"clock mismatch never accepts future input");
    bool fresh=true;
    gamepad.read(499,&fresh);expect(!fresh,"future input cannot suppress a physical controller");
    gamepad.read(501,&fresh);expect(fresh,"fresh active sample owns the virtual controller");
    MenuButton menu;
    expect(menu.update(true,true,100)==0,"menu press does not prematurely open iDroid");
    expect(menu.update(false,true,200)==0x10,"short menu release opens iDroid");
    expect(menu.update(false,true,299)==0x10,"short press survives game frame polling");
    expect(menu.update(false,true,300)==0,"iDroid pulse releases");
    menu.update(true,true,400);
    expect(menu.update(true,true,949)==0,"pause hold threshold is enforced");
    expect(menu.update(true,true,950)==0x20,"long menu press maps native Pause");
    expect(menu.update(true,true,1050)==0,"pause pulse releases while held");
    expect(menu.update(false,true,1200)==0,"long menu release never also opens iDroid");
    menu.update(true,true,1300);menu.update(true,false,1400);
    expect(menu.update(true,true,2100)==0,"focus return cannot trigger a held menu");
    menu.update(false,true,2200);menu.update(true,true,2300);
    expect(menu.update(false,true,2400)==0x10,"fresh press after focus recovery works");
    expect(menu.update(true,true,100)==0,"clock reset cancels pending menu input");
    const Panel panel{Pose{{},{0,0,-2}},2,1,1920,1080};
    auto hit=intersectPanel(panel,{},3);
    expect(hit&&hit->x==960&&hit->y==540&&near(hit->distance,2),"ray reaches exact panel center");
    hit=intersectPanel(panel,Pose{{},{1,-0.5f,0}},3);
    expect(hit&&hit->x==1919&&hit->y==1079,"bottom/right boundary clamps to final source pixel");
    expect(!intersectPanel(panel,Pose{{},{1.01f,0,0}},3),"ray outside panel rejected");
    expect(!intersectPanel(panel,{},1),"ray beyond reach rejected");
    expect(!intersectPanel(panel,Pose{{0,1,0,0},{0,0,-3}},3),"back face rejected");
    expect(!intersectPanel(panel,Pose{{0,0.70710678f,0,0.70710678f},{}},3),"parallel ray rejected");
    expect(!intersectPanel(panel,Pose{{0,0,0,0},{}},3),"invalid controller orientation rejected");
    expect(!intersectPanel(Panel{{},0,1,1,1},{},3),"zero-size panel rejected");
    expect(!intersectPanel(Panel{{},1,1,0,1},{},3),"missing pixel dimensions rejected");
    expect(!intersectPanel(panel,{},std::numeric_limits<float>::infinity()),"unbounded invalid reach rejected");
    for(int n=0;n<100;++n){
        const float a=static_cast<float>(n)*0.06283185f;
        const Pose parent{{0,std::sin(a/2),0,std::cos(a/2)},{1.1f,-0.8f,2.3f}};
        const auto transformed=Panel{compose(parent,panel.pose),2,1,1920,1080};
        const auto h=intersectPanel(transformed,parent,3);
        expect(h&&near(h->u,0.5f)&&near(h->v,0.5f),"ray mapping invariant under world rotation/translation");
        expect(same(compose(parent,inverse(parent)).position,{}),"transform and inverse cancel");
    }
    const Pose tilted{{0.5f,0,0,0.8660254f},{1,1.7f,2}};
    const auto screen=recenteredScreen(tilted,6);
    expect(near(screen.position.y,tilted.position.y)&&near(screen.position.z,-4),"recenter discards head pitch and preserves level screen");
    WristFocus focus;
    expect(focus.update(0,true,true,0.8f,0.5f)==WristState::candidate,"wrist starts dwell");
    expect(focus.update(0.1,true,true,0.8f,0.5f)==WristState::candidate,"wrist cannot open immediately");
    expect(focus.update(0.21,true,true,0.8f,0.5f)==WristState::visible,"wrist opens after dwell");
    expect(focus.update(0.4,true,true,0.6f,0.75f)==WristState::visible,"exit hysteresis prevents boundary flicker");
    expect(focus.update(0.5,true,false,0.8f,0.5f)==WristState::hidden,"blank content immediately hides panel");
    focus.update(1,true,true,0.8f,0.5f);focus.update(1.3,true,true,0.8f,0.5f);
    expect(focus.update(1.5,true,true,0.4f,0.9f)==WristState::cooldown,"leaving expanded bounds closes panel");
    expect(focus.update(1.6,true,true,0.8f,0.5f)==WristState::cooldown,"cooldown rejects immediate reentry");
    expect(focus.update(1.8,true,true,0.8f,0.5f)==WristState::hidden,"cooldown returns to closed state");
    expect(focus.update(1.9,false,true,0.8f,0.5f)==WristState::hidden,"tracking loss is closed");
    expect(focus.update(0.2,true,true,0.8f,0.5f)==WristState::hidden,"clock reset clears dwell");
    PoseHistory history;history.reset(7);
    expect(history.put({{7,1},{},{},{},100}),"pose publication accepted");
    expect(history.find({7,1}).has_value(),"source transaction found");
    expect(!history.find({8,1}),"same sequence in wrong epoch rejected");
    expect(!history.put({{7,1},{},{},{},200}),"duplicate sequence rejected");
    for(uint64_t n=2;n<=65;++n)history.put({{7,n},{},{},{},static_cast<int64_t>(n)*100});
    expect(!history.find({7,1}),"overwritten historical pose rejected instead of using latest");
    expect(history.find({7,65}).has_value(),"newest source transaction retained");
    history.reset(8);expect(!history.find({7,65}),"reset invalidates all prior images");
    expect(!history.put({{7,66},{},{},{},300}),"old producer epoch rejected after reset");
    SkipGate skip;
    expect(!skip.update(Scene::cinematic,1,true,true,true),"held button cannot skip a newly entered cinematic");
    skip.update(Scene::cinematic,1,true,true,false);
    expect(skip.update(Scene::cinematic,1,true,true,true),"fresh press emits one semantic skip");
    expect(!skip.update(Scene::cinematic,1,true,true,true),"held skip does not repeat");
    skip.update(Scene::gameplay,2,true,true,false);
    expect(!skip.update(Scene::gameplay,2,true,true,true),"skip cannot fire during gameplay");
    skip.update(Scene::cinematic,3,false,true,false);
    expect(!skip.update(Scene::cinematic,3,false,true,true),"engine-unready cinematic is not force advanced");
    skip.update(Scene::cinematic,3,true,false,false);
    expect(!skip.update(Scene::cinematic,3,true,false,true),"unfocused controller cannot skip");
    WeaponSockets sockets{Pose{{},{0.1f,-0.02f,0.12f}},Pose{{},{0,0,-0.7f}},true};
    for(int n=0;n<100;++n){
        const float a=static_cast<float>(n)*0.06283185f;
        const Pose grip{{std::sin(a/2),0,0,std::cos(a/2)},{0.2f,1.1f,-0.4f}};
        const auto weapon=solveWeapon(grip,sockets,true);
        expect(weapon&&same(compose(weapon->referenceFromWeapon,sockets.weaponFromGrip).position,grip.position),"weapon authored grip retains contact throughout rotation");
        expect(weapon&&same(compose(weapon->referenceFromWeapon,sockets.weaponFromMuzzle).position,weapon->referenceFromMuzzle.position),"muzzle shares weapon transform chain");
    }
    expect(!solveWeapon({},sockets,false),"tracking loss prevents a fabricated weapon pose");
    sockets.calibrated=false;expect(!solveWeapon({},sockets,true),"uncalibrated weapon cannot silently use guessed sockets");
    std::cout<<checks<<" contract checks, "<<failures<<" failures. These tests do not prove in-game or headset behavior.\n";
    return failures?1:0;
}
