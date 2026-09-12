#include "mgs5vr/core.hpp"
#include "mgs5vr/arm_ik.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/motion_melee.hpp"
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
    {
        SnapTurn snap;
        expect(near(snap.update(1,0,true),0),"held stick cannot snap on activation");
        snap.update(0,0,true);
        expect(near(snap.update(1,0,true),-.523598776f),"right flick requests one thirty-degree right turn");
        expect(near(snap.update(1,0,true),0),"holding the turn stick cannot repeat the snap");
        snap.update(0,0,true);
        expect(near(snap.update(0,1,true),0)&&near(snap.update(1,0,true),0),"vertical flick does not pitch or spill into a snap");
        snap.update(0,0,true);snap.update(0,0,false);
        expect(near(snap.update(-1,0,true),0),"menu exit requires a fresh neutral stick");
        snap.update(0,0,true);
        expect(near(snap.update(-1,0,true),.523598776f),"left flick turns left after centering");
    }
    {
        WheelSteering steering;const Pose contact{{},{-.2f,-.3f,.5f}};
        steering.update(contact,contact,true,false,100,1);
        auto wheel=steering.update(contact,contact,true,true,111,1);
        expect(wheel.gripped&&wheel.engaged&&wheel.axis==0,"fresh near-wheel grip acquires without steering jump");
        auto turn=contact;turn.orientation={0,0,-.258819f,.965926f};
        wheel=steering.update(turn,contact,true,true,122,1);
        expect(wheel.gripped&&!wheel.engaged&&std::abs(wheel.axis-.5f)<.001f,"clockwise hand roll turns the wheel right");
        const auto lookAway=steering.update(turn,contact,true,true,127,1,Vec3{1,0,0});
        expect(near(lookAway.axis,wheel.axis),"head turns while holding cannot change the steering basis");
        turn.orientation={0,0,.258819f,.965926f};
        expect(std::abs(steering.update(turn,contact,true,true,129,1).axis+.5f)<.001f,"counterclockwise hand roll turns left");
        expect(!steering.update(turn,contact,true,false,133,1).gripped,"releasing the grip releases the wheel");
        turn.position.x+=1;
        expect(!steering.update(turn,contact,true,true,144,1).gripped,"distant squeezing cannot snap to the wheel");
        steering.update(contact,contact,true,false,155,1);
        steering.update(contact,contact,true,true,166,1);
        expect(!steering.update(contact,contact,false,true,177,1).gripped,"tracking or vehicle loss releases the wheel");
        expect(!steering.update(contact,contact,true,true,188,1).gripped,"held grip cannot reattach after tracking loss");
        RumbleMailbox rumble;rumble.publish({.4f,.2f,100});
        expect(rumble.read(110).low==.4f&&rumble.read(351).low==0,"native rumble expires without a new game sample");
    }
    {
        OpticSelection selection;
        expect(!selection.update(true,false),"ordinary weapon grip does not equip binoculars");
        expect(selection.update(true,true),"explicit optic chord equips");
        expect(selection.update(true,true),"held equip chord does not toggle repeatedly");
        selection.update(true,false);
        expect(!selection.update(true,true),"fresh equip chord stows the optic");
        selection.update(false,true);
        expect(!selection.update(true,true),"tracking recovery cannot turn a held chord into an equip");
        selection.update(true,false);selection.update(true,true);
        expect(!selection.update(true,false,true),"B stows the device without a native camera action");

        RigOptics optics;
        GamepadSample input{};
        auto view=optics.update(input,true,false);
        expect(view.magnification==1&&!view.exclusive,"unheld optics leave gun handling alone");
        view=optics.update(input,true,true);
        expect(view.magnification==2&&view.exclusive&&!view.nativeActive&&view.gamepad.buttons==0,
            "one-handed carry has full 2x power without entering a native camera mode");
        GamepadSample browse{};browse.leftY=24000;browse.rightX=12000;browse.buttons=0x80;
        view=optics.update(browse,true,true);
        expect(view.magnification==4&&view.gamepad.leftY==24000&&view.gamepad.rightX==12000
            &&view.gamepad.buttons==0,"one-handed zoom preserves walking and turn");
        expect(optics.update(browse,true,true).magnification==4,"held R3 cannot cycle twice");
        GamepadSample mark{};mark.rightTrigger=255;
        view=optics.update(mark,true,true);
        expect(view.markRequested&&view.gamepad.buttons==0&&view.gamepad.rightTrigger==0,
            "right trigger requests an optic mark without firing or opening Commands");
        expect(!optics.update(mark,true,true).markRequested,"held trigger cannot repeat a mark");
        optics.update({},true,true);
        expect(optics.update(mark,true,true).markRequested,"released trigger allows the next mark");
        GamepadSample clear{};clear.rightY=-32767;
        view=optics.update(clear,true,true);
        expect(view.clearRequested&&!view.markRequested&&view.gamepad.rightY==0,
            "down flick removes an optic mark without pitching the camera");
        expect(!optics.update(clear,true,true).clearRequested,"held down cannot remove a second mark");
        optics.update({},true,true);
        expect(optics.update(clear,true,true).clearRequested,"centering rearms optic clearing");
        expect(!optics.update(clear,true,false).clearRequested,"stowed binoculars cannot clear marks");
        expect(!optics.update(clear,true,true).clearRequested,"regripping a held down stick requires neutral");
        view=optics.update(mark,true,false);
        expect(view.exclusive&&view.gamepad.rightTrigger==0&&view.gamepad.buttons==0,
            "stowing with trigger held cannot discharge the restored firearm");
        expect(!optics.update({},true,false).exclusive,"release returns ordinary gameplay input");
        expect(optics.update({},true,true).magnification==4,"carrying and regripping retain the selected zoom");
        GamepadSample intel{};intel.buttons=0x1000;
        view=optics.update(intel,true,true,true);
        expect(!view.nativeActive&&view.gamepad.buttons==0,
            "eye relief cannot enter an authored native camera mode or leak A into stance");
        expect(optics.update({},true,true,false).gamepad.buttons==0,
            "lowering the physical optic releases native scanning");
        view=optics.update(mark,false,false);
        expect(view.gamepad.buttons==0&&view.gamepad.rightTrigger==0,
            "tracking loss never injects native B or a weapon trigger");
    }
    {
        const Pose head{{},{0,1.6f,0}};
        const std::array<EyeView,2> eyes{{
            {Pose{{},{-.032f,1.6f,0}},EyeFov{-.8f,.8f,.7f,-.7f}},
            {Pose{{},{.032f,1.6f,0}},EyeFov{-.8f,.8f,.7f,-.7f}}}};
        const Pose left{{},{-.30f,1.25f,.18f}},right{{},{0,1.54f,.18f}};
        const Pose aim{{},{}};
        expect(solveBinocularPose(left,right,aim,aim,true,true,true,true,true,true).has_value(),
            "a right primary hand publishes one authored binocular frame");
        const auto single=solveBinocularPose(
            Pose{{},{-.4f,1.2f,.2f}},Pose{{},{.03f,1.3f,.2f}},
            Pose{{},{-.4f,1.2f,.2f}},Pose{{},{.03f,1.3f,.2f}},
            false,true,false,true,false,true);
        expect(single&&single->primaryRight&&!single->supportHeld
            &&single->ray.tracked
            &&near(single->ray.origin.x,single->rightEyepiece.position.x)
            &&near(single->ray.origin.y,single->rightEyepiece.position.y)
            &&near(single->ray.origin.z,single->rightEyepiece.position.z)
            &&near(dot(single->ray.direction,rotate(single->rightEyepiece.orientation,{0,0,-1})),1.f),
            "a single right-hand optic stays attached and publishes its calibrated carry ray");
        const auto primaryAperture=compose(single->body,Pose{{0,0,0,1},binocularOcularCenter}).position;
        expect(near(single->rightEyepiece.position.x,primaryAperture.x)
            &&near(single->rightEyepiece.position.y,primaryAperture.y)
            &&near(single->rightEyepiece.position.z,primaryAperture.z),
            "the dominant eye relief point is the real retail ocular, without a synthetic side offset");
        expect(near(dot(single->ray.direction,rotate(single->renderBody.orientation,{0,0,1})),-1.f),
            "the optical ray enters the housing opposite the outward ocular normal");
        const Pose rotatedGrip{{0,.258819f,0,.965926f},{.03f,1.3f,.2f}};
        const Pose differentAim{{0,-.258819f,0,.965926f},{.03f,1.3f,.2f}};
        const auto gripOwned=solveBinocularPose({},rotatedGrip,{},differentAim,
            false,true,false,true,false,true);
        const auto gripReference=solveBinocularPose({},rotatedGrip,{},rotatedGrip,
            false,true,false,true,false,true);
        expect(gripOwned&&gripReference
            &&std::abs(gripOwned->body.orientation.x-gripReference->body.orientation.x)<.0001f
            &&std::abs(gripOwned->body.orientation.y-gripReference->body.orientation.y)<.0001f
            &&std::abs(gripOwned->body.orientation.z-gripReference->body.orientation.z)<.0001f
            &&std::abs(gripOwned->body.orientation.w-gripReference->body.orientation.w)<.0001f
            &&std::abs(gripOwned->body.position.x-gripReference->body.position.x)<.0001f
            &&std::abs(gripOwned->body.position.y-gripReference->body.position.y)<.0001f
            &&std::abs(gripOwned->body.position.z-gripReference->body.position.z)<.0001f,
            "optic housing frame is owned by the right grip rather than the aim pose");
        OpticGate gate;
        auto sample=gate.update(head,eyes,left,right,aim,aim,true,true,true,true,true,true,true,100,1);
        expect(!sample.active&&!sample.aligned,"a device held below the face cannot open binocular mode");
        const Pose nearLeft{{},{-.30f,1.25f,.18f}},nearRight{{},{0,1.54f,-.01f}};
        const auto nearPose=solveBinocularPose(nearLeft,nearRight,nearLeft,nearRight,
            true,true,true,true,true,true);
        std::array<EyeView,2> nearEyes{};
        if(nearPose){
            nearEyes={{{nearPose->leftEyepiece,eyes[0].fov},{nearPose->rightEyepiece,eyes[1].fov}}};
        }
        sample=gate.update(head,nearEyes,nearLeft,nearRight,nearLeft,nearRight,true,true,true,true,true,true,true,111,1);
        expect(sample.active&&sample.opened&&sample.pose.kind==OpticKind::binocular,
            "binocular mode opens only when both authored eyepieces meet the two eyes");
        sample=gate.update(head,nearEyes,nearLeft,nearRight,nearLeft,nearRight,true,true,true,true,true,true,true,122,1);
        expect(sample.active&&!sample.opened,"held eye relief does not re-open or flicker the native optic state");
        const Pose loweredRight{{},{0,1.54f,.3f}};
        sample=gate.update(head,nearEyes,nearLeft,loweredRight,nearLeft,loweredRight,true,true,true,true,true,true,true,133,1);
        expect(sample.closed&&!sample.active,"leaving eye relief closes the optic instead of retaining zoom");

    Pose physicalHead{{},{}};
    std::array<EyeView,2> physicalViews{{
        {Pose{{},{-.032f,0,0}},EyeFov{-.8f,.8f,.7f,-.7f}},
        {Pose{{},{.032f,0,0}},EyeFov{-.8f,.8f,.7f,-.7f}}
    }};
    OpticGate physicalGate;
    const Pose physicalLeft{{},{-.30f,-.35f,-.35f}},physicalRight{{.61595203f,-.00760909f,.07982154f,.78369236f},{0,-.06f,-.08f}};
    const auto physicalPose=solveBinocularPose(physicalLeft,physicalRight,
        physicalLeft,physicalRight,true,true,true,true,true,true);
    if(physicalPose){
        physicalViews={{{physicalPose->leftEyepiece,physicalViews[0].fov},
                        {physicalPose->rightEyepiece,physicalViews[1].fov}}};
    }
    auto aimed=physicalGate.update(physicalHead,physicalViews,
        physicalLeft,physicalRight,physicalLeft,physicalRight,
        true,true,true,true,true,true,true,1,1);
    const auto nearVec=[](Vec3 a,Vec3 b){return dot(a-b,a-b)<1e-8f;};
    expect(aimed.active,"a valid paired device pose activates the optic gate");
    const auto camera2=binocularSceneView(aimed.pose,2);
    const auto camera4=binocularSceneView(aimed.pose,4);
    expect(camera2&&camera4&&near(std::tan(camera2->fov.right),2*std::tan(camera4->fov.right)),
        "4x renders twice the angular detail of 2x in the dedicated lens camera");
    const auto objectiveAdvance=camera2?camera2->pose.position-aimed.pose.ray.origin:Vec3{};
    expect(camera2&&dot(objectiveAdvance,aimed.pose.ray.direction)>.10f
        &&nearVec(objectiveAdvance,aimed.pose.ray.direction*dot(objectiveAdvance,aimed.pose.ray.direction))
        &&nearVec(rotate(camera2->pose.orientation,{0,0,-1}),aimed.pose.ray.direction),
        "lens camera clears the housing at the front glass on the same marking axis");
    auto supportController=compose(aimed.pose.body,Pose{{},binocularSupportSocket});
    supportController.position.x-=.025f;
    const auto supported=solveBinocularPose(supportController,physicalRight,
        supportController,physicalRight,true,true,true,true,true,true);
    expect(supported&&supported->supportHeld&&nearVec(supported->body.position,aimed.pose.body.position)
        &&nearVec(compose(inverse(supported->body),supported->supportGrip).position,binocularSupportSocket),
        "a nearby supporting hand cups the side without pulling the primary grip");
    supportController.position.x-=.25f;
    const auto detached=solveBinocularPose(supportController,physicalRight,
        supportController,physicalRight,true,true,true,true,true,true);
    expect(detached&&!detached->supportHeld&&nearVec(detached->body.position,aimed.pose.body.position),
        "pulling the support hand away releases its cup and keeps one-handed carry");
    auto moved=aimed.pose;moved.rightEyepiece.position.x+=.25f;
    const auto carriedCamera=binocularSceneView(moved,2);
    expect(carriedCamera&&camera2&&near(carriedCamera->pose.position.x-camera2->pose.position.x,.25f),
        "the lens camera follows hand motion independently of either HMD eye");
    {
        const Pose reference{{.61595203f,-.00760909f,.07982154f,.78369236f},{.10f,0,-.04f}};
        const auto close=solveBinocularPose(physicalLeft,reference,physicalLeft,reference,true,true,true,true,false,true);
        const auto safe=close?binocularFaceSafeGrip(physicalHead,reference,*close):reference;
        const auto protectedPose=solveBinocularPose(physicalLeft,safe,physicalLeft,safe,true,true,true,true,false,true);
        expect(protectedPose&&protectedPose->rightEyepiece.position.z<=-.0449f,
            "bringing the housing through the eye stops it outside the face");
        const auto unchanged=protectedPose?binocularFaceSafeGrip(physicalHead,safe,*protectedPose):reference;
        expect(nearVec(unchanged.position,safe.position),"face contact is stable and does not push the palm again each frame");
        auto carry=reference;carry.position.z=-.4f;
        const auto carryPose=solveBinocularPose(physicalLeft,carry,physicalLeft,carry,true,true,true,true,false,true);
        expect(carryPose&&nearVec(binocularFaceSafeGrip(physicalHead,carry,*carryPose).position,carry.position),
            "face clearance leaves normal one-handed carry at the tracked palm");
    }
    expect(!binocularSceneView(aimed.pose,std::numeric_limits<float>::quiet_NaN())
        &&!binocularSceneView(aimed.pose,0),"invalid optical powers cannot reach native projection");
    const auto originalHead=physicalHead;const auto originalViews=physicalViews;
    expect(validateBinocularViews(aimed,physicalHead,physicalViews),
        "active binoculars accept their physical eyepiece pair without changing stereo origins");
    expect(nearVec(physicalHead.position,originalHead.position)
        &&nearVec(physicalViews[0].pose.position,originalViews[0].pose.position)
        &&nearVec(physicalViews[1].pose.position,originalViews[1].pose.position),
        "active binoculars preserve the head-derived stereo transaction");
        sample=gate.update(head,eyes,nearLeft,nearRight,aim,aim,true,false,true,false,true,true,true,144,1);
        expect(!sample.active&&!sample.pose.tracked,"lost primary hand tracking fails closed");
    }
    {
        RigCommands commands;
        GamepadSample input{0x4000,220,0,0,20000,25000,0};
        auto result=commands.update(input,true,1000,990);
        expect(result.active&&result.gamepad.buttons==0x100&&result.gamepad.leftY==20000&&result.gamepad.rightX==0,
            "Commands preserves walking but consumes navigation held during opening");
        input.buttons=0;input.rightX=0;commands.update(input,true,1020,1010);
        input.rightY=25000;result=commands.update(input,true,1030,1020);
        expect(result.active&&result.gamepad.rightY==25000&&result.gamepad.rightX==0,
            "X can be released and upward command navigation stays upward");
        input.rightTrigger=255;result=commands.update(input,true,1040,1030);
        expect(result.gamepad.buttons==0x180&&result.gamepad.rightTrigger==0,"command confirm cannot fire the weapon");
        expect(result.gamepad.rightY==25000,"native command confirmation includes its selected direction");
        result=commands.update(input,true,1150,1140);
        expect(result.gamepad.buttons==0x100,"holding command confirm cannot repeat it");
        expect(result.gamepad.rightX==0&&result.gamepad.rightY==0,"held selection cannot turn the camera after command confirmation");
        input.leftTrigger=0;result=commands.update(input,true,1160,1150);
        expect(!result.active&&result.exclusive&&result.gamepad.rightTrigger==0&&result.gamepad.rightY==0&&result.gamepad.leftY==20000,
            "release closes commands without leaking held fire or turning");
        commands.update({},true,1170,1160);input={0x4000,220};commands.update(input,true,1180,1170);
        commands.suspend();expect(!commands.update(input,true,1190,1180).active,"tracking or focus loss requires a fresh commands chord");
    }
    {
        MotionMelee melee;const Pose head{{},{0,1.6f,0}};
        const uint64_t actorTag=(uint64_t{1}<<63)|(uint64_t{1}<<59);
        for(uint64_t index=0;index<512;++index){
            expect(protectedMotionMeleeTarget(actorTag|(19u<<9)|index),"every D-Dog ID ignores automatic hand/weapon strikes");
            expect(protectedMotionMeleeTarget(actorTag|(20u<<9)|index),"puppies ignore automatic hand/weapon strikes");
        }
        for(uint64_t type:{13u,18u,39u,40u,41u,42u})
            expect(protectedMotionMeleeTarget(actorTag|(type<<9)),"companion horse and Quiet types ignore automatic strikes");
        for(uint64_t type=28;type<=34;++type)
            for(uint64_t index=0;index<512;++index)
                expect(protectedMotionMeleeTarget(actorTag|(type<<9)|index),"non-hostile wildlife cannot be hit by an automatic hand strike");
        expect(!protectedMotionMeleeTarget(actorTag|(2u<<9)),"enemy soldiers retain native motion-melee processing");
        expect(!protectedMotionMeleeTarget(actorTag|(25u<<9)),"hostile Quiet encounter retains native rules");
        expect(!protectedMotionMeleeTarget(uint64_t{19}<<9),"untagged scene geometry is not a companion");
        Pose fist{{},{-.22f,1.25f,-.2f}};
        melee.update(head,fist,true,1000,1);
        unsigned starts{},sweeps{};
        for(unsigned n=1;n<=18;++n){
            fist.position.z=-.2f-.018f*n;
            const auto hit=melee.update(head,fist,true,1000+11*n,1);
            starts+=hit.started;sweeps+=hit.strike;
            if(hit.strike)expect(hit.curl==1&&same(hit.end,fist.position),"motion strike closes the fist at its actual contact point");
        }
        expect(starts==1&&sweeps>1,"one button-free punch keeps a contact window without repeating its stroke");
        fist.position.z+=.04f;
        expect(!melee.update(head,fist,true,1209,1).strike,"returning a fist does not strike again");
        expect(!melee.update(head,fist,true,1800,1).strike,"tracking gaps rebaseline rather than inventing velocity");
        fist.position.z-=.5f;
        expect(!melee.update(head,fist,true,1811,1).strike,"tracking teleport cannot punch");
        expect(!melee.update(head,fist,true,1822,2).strike,"recenter starts a new motion baseline");
        melee.reset();Pose walkingHead=head;fist.position={-.22f,1.25f,-.2f};
        for(unsigned n=0;n<25;++n){
            walkingHead.position.z=-.03f*n;fist.position.z=walkingHead.position.z-.2f;
            expect(!melee.update(walkingHead,fist,true,2000+11*n,1).strike,"walking moves head and fist together without a punch");
        }
        melee.reset();fist.position={-.22f,1.25f,-.2f};
        for(unsigned n=0;n<18;++n){
            auto duck=head;duck.position.y+=.02f*n;
            expect(!melee.update(duck,fist,true,3000+11*n,1).strike,"head motion alone cannot turn a stationary hand into a strike");
        }
        melee.reset();unsigned weaponStarts{};
        for(unsigned n=0;n<22;++n){
            const float angle=1.2f-.045f*n;
            const Pose tip{{},{0,1.6f-std::sin(angle)*.6f,-.2f-std::cos(angle)*.6f}};
            weaponStarts+=melee.update(head,tip,true,4000+11*n,1,true).started;
        }
        expect(weaponStarts==1,"swinging an authored weapon tip can strike while its grip pivot stays still");
        expect(!melee.update(head,fist,false,4300,1).strike,"menu and native manipulation disable motion strikes");
    }
    {
        const Pose palm{{},{2,3,4}};
        const auto straight=pointThrow(palm,{},Vec3{3,4,0});
        expect(straight&&same(straight->origin,palm.position)&&same(straight->velocity,{0,0,-5}),
            "grenade starts at rendered palm and uses aim -Z with native speed");
        const Pose aimPitch{{.70710678f,0,0,.70710678f},{.1f,0,-.1f}};
        const auto upward=pointThrow(palm,aimPitch,Vec3{0,0,5});
        expect(upward&&same(upward->origin,palm.position)&&same(upward->velocity,{0,5,0}),
            "controller aim pitch raises grenade arc without moving its palm origin");
        const Pose turned{{0,.70710678f,0,.70710678f},{-2,8,1}};
        const auto side=pointThrow(turned,{},Vec3{0,5,0});
        expect(side&&same(side->origin,turned.position)&&same(side->velocity,{-5,0,0}),
            "translated and rotated rendered hand controls grenade launch in world space");
        expect(!pointThrow(palm,{},{}),"zero native grenade speed cannot create a bogus arc");
        expect(!pointThrow(palm,{},Vec3{0,std::numeric_limits<float>::quiet_NaN(),0}),"invalid grenade velocity is rejected");
        expect(!pointThrow(Pose{{0,0,0,0},{}},{},Vec3{1,0,0}),"invalid hand transform is rejected for throwing");
    }

    const EyeFov asymmetric{-0.8f,0.9f,0.75f,-0.7f};
    std::array<float,16> projection{1,0,0,0,0,1,0,0,0,0,-0.0002f,1,0,0,0.1f,0};
    expect(setEyeProjection(projection,asymmetric),"native perspective accepts runtime asymmetric field of view");
    {
        const Pose head{{0,.258819f,0,.965926f},{3,2,1}};
        std::array<EyeView,2> eyes{{{head,asymmetric},{head,{-asymmetric.right,-asymmetric.left,asymmetric.up,asymmetric.down}}}};
        auto visibility=projection;
        expect(widenVisibilityProjection(visibility,head,eyes),"visibility can enclose both asymmetric eyes in the source-head frame");
        expect(near(visibility[8],0)&&near(visibility[9],0)&&std::abs(visibility[0])<std::abs(projection[0])
            &&visibility[10]==projection[10]&&visibility[14]==projection[14],
            "visibility adds symmetric coverage while preserving native depth");
        for(const auto& eye:eyes)for(float angle:{eye.fov.left,eye.fov.right})
            expect(std::abs(std::tan(angle)*visibility[0])<1,"both eye edges fit inside the visibility frustum");
        auto repeated=visibility;
        expect(widenVisibilityProjection(repeated,head,eyes)&&repeated==visibility,"repeated visibility preparation cannot keep expanding coverage");
        eyes[1].pose.orientation={0,0,0,0};
        expect(!widenVisibilityProjection(repeated,head,eyes)&&repeated==visibility,"invalid eye tracking leaves native visibility untouched");
    }
    const auto ndc=[&](float x,float y,float z){return Vec3{(x*projection[0]+z*projection[8])/z,(y*projection[5]+z*projection[9])/z,(z*projection[10]+projection[14])/z};};
    expect(near(ndc(-std::tan(asymmetric.left)*3,0,3).x,-1),"left eye frustum boundary projects to left edge");
    expect(near(ndc(-std::tan(asymmetric.right)*3,0,3).x,1),"right eye frustum boundary projects to right edge");
    expect(near(ndc(0,std::tan(asymmetric.up)*3,3).y,1),"upper eye frustum boundary projects to upper edge");
    expect(near(ndc(0,std::tan(asymmetric.down)*3,3).y,-1),"lower eye frustum boundary projects to lower edge");
    expect(near(projection[10],-0.0002f)&&near(projection[14],0.1f),"eye optics preserve native depth convention");
    const EyeFov opticalLeft{-.9424778f,.6981317f,.8726646f,-.8552113f};
    const EyeFov opticalRight{-opticalLeft.right,-opticalLeft.left,opticalLeft.up,opticalLeft.down};
    for(const auto eye:{opticalLeft,opticalRight})for(float power:{1.f,2.f,4.f}){
        const auto narrow=opticalFov(eye,power),restored=narrow?opticalFov(*narrow,1/power):std::nullopt;
        expect(narrow&&restored&&near(std::tan(narrow->right)*power,std::tan(eye.right))
            &&near(restored->left,eye.left)&&near(restored->right,eye.right)
            &&near(restored->up,eye.up)&&near(restored->down,eye.down),"stereo magnification scales rays and preserves each eye's asymmetric optical center");
    }
    expect(!opticalFov(opticalLeft,0)&&!opticalFov(opticalLeft,5)
        &&!opticalFov(opticalLeft,std::numeric_limits<float>::quiet_NaN()),"invalid magnification cannot enter scene projection");
    const auto completeFov=enclosingEyeFov(opticalLeft);
    expect(completeFov&&near(completeFov->left,-completeFov->right)&&near(completeFov->up,-completeFov->down),
        "lighting coverage has a centered enclosing render field");
    const auto cropLeft=eyeImageRegion(*completeFov,opticalLeft,1280,720);
    const auto cropRight=eyeImageRegion(*completeFov,opticalRight,1280,720);
    expect(cropLeft&&cropRight&&cropLeft->x==0&&cropRight->x>0&&cropLeft->width==cropRight->width,
        "left and right runtime optical centers select opposite texture regions");
    const auto rayFromPixel=[](EyeFov f,double x,double y,double w,double h){
        return Vec3{float(std::tan(f.left)+(std::tan(f.right)-std::tan(f.left))*x/w),
            float(std::tan(f.up)-(std::tan(f.up)-std::tan(f.down))*y/h),-1};};
    for(const auto& region:{*cropLeft,*cropRight})for(float u:{0.f,.13f,.5f,1.f})for(float v:{0.f,.37f,1.f}){
        const auto sourceRay=rayFromPixel(*completeFov,region.x+u*region.width,region.y+v*region.height,1280,720);
        const auto displayedRay=rayFromPixel(region.fov,u,v,1,1);
        expect(same(sourceRay,displayedRay),"cropped pixels retain their exact original angular ray");
    }
    expect(cropLeft->fov.left<=opticalLeft.left&&cropLeft->fov.right>=opticalLeft.right
        &&cropLeft->fov.up>=opticalLeft.up&&cropLeft->fov.down<=opticalLeft.down,
        "integer rounding covers every requested eye ray");
    expect(!eyeImageRegion(opticalLeft,*completeFov,1280,720),"missing rendered coverage is rejected rather than stretched");
    expect(!eyeImageRegion(*completeFov,opticalLeft,0,720),"zero-size eye cannot produce a projection region");
    const auto sameSize=eyeImageRegion(opticalLeft,opticalLeft,1280,720);
    expect(sameSize&&sameSize->x==0&&sameSize->y==0&&sameSize->width==1280&&sameSize->height==720,
        "unchanged optics retain the full texture");
    const std::array<float,16> identityMatrix{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    const EyeFov squareEye{-0.785398163f,0.785398163f,0.785398163f,-0.785398163f};
    const auto panelClip=[](const std::array<float,16>& m,float x,float y){
        const float w=x*m[3]+y*m[7]+m[15];
        return Vec3{(x*m[0]+y*m[4]+m[12])/w,(x*m[1]+y*m[5]+m[13])/w,(x*m[2]+y*m[6]+m[14])/w};};
    const Pose spatialPanel{{0,1,0,0},{0,0,2}};
    const auto uiProjection=uiPanelProjection(identityMatrix,identityMatrix,squareEye,spatialPanel,0.8f,0.4f);
    expect(uiProjection&&same(panelClip(*uiProjection,1,1),{0.2f,0.1f,0.015f}),
           "native UI corners project at the physical panel size and distance");
    auto leftView=identityMatrix,rightView=identityMatrix;leftView[12]=-0.032f;rightView[12]=0.032f;
    const auto leftPanel=uiPanelProjection(identityMatrix,leftView,squareEye,spatialPanel,0.8f,0.4f);
    const auto rightPanel=uiPanelProjection(identityMatrix,rightView,squareEye,spatialPanel,0.8f,0.4f);
    expect(leftPanel&&rightPanel&&near(panelClip(*leftPanel,0,0).x-panelClip(*rightPanel,0,0).x,0.032f),
           "forearm UI has real per-eye disparity rather than a shared flat overlay");
    expect(!uiPanelProjection(identityMatrix,identityMatrix,squareEye,spatialPanel,0,0.4f),
           "invalid physical UI extent cannot replace native projection");
    const std::array<Pose,2> panelEyes{Pose{{},{-.032f,0,0}},Pose{{},{.032f,0,0}}};
    expect(panelFacesBothEyes(Pose{{},{0,0,-.4f}},panelEyes),"front-facing wrist display is visible to the stereo pair");
    expect(!panelFacesBothEyes(Pose{{0,1,0,0},{0,0,-.4f}},panelEyes),"back-facing wrist display is hidden in both eyes");
    const Pose edgePanel{{0,.70710678f,0,.70710678f},{0,0,-.4f}};
    expect(!panelFacesBothEyes(edgePanel,panelEyes),"plane separating the eyes cannot become a monocular wrist display");
    auto invalidEyes=panelEyes;invalidEyes[1].position.x=std::numeric_limits<float>::quiet_NaN();
    expect(!panelFacesBothEyes(Pose{{},{0,0,-.4f}},invalidEyes),"invalid eye pose cannot expose half of the wrist display");
    auto ortho=projection;ortho[11]=0;ortho[15]=1;
    expect(!setEyeProjection(ortho,asymmetric),"orthographic pass cannot be mistaken for scene projection");
    std::array<EyeFrame,2> pair{};
    for(uint32_t n=0;n<2;++n)pair[n]={EyeView{Pose{{},{n?0.032f:-0.032f,0,0}},asymmetric},9,31,4,100,n,true,true,asymmetric};
    expect(readyEyePair(pair,4,110),"both native eyes from one simulation and tracking transaction are eligible");
    pair[0].magnification=2;
    expect(!readyEyePair(pair,4,110),"different zoom powers cannot be submitted as one stereo pair");
    pair[1].magnification=2;
    expect(readyEyePair(pair,4,110),"matching zoom powers retain independent stereo eye poses");
    pair[0].magnification=pair[1].magnification=1;
    pair[1].sourceSequence=10;
    expect(!readyEyePair(pair,4,110),"alternate-eye consecutive simulation frames are rejected");
    pair[1].sourceSequence=9;pair[1].trackingSequence=32;
    expect(!readyEyePair(pair,4,110),"different pose generations cannot be submitted as one native frame");
    pair[1].trackingSequence=31;pair[0].joined=false;
    expect(!readyEyePair(pair,4,110),"unjoined native image never receives newer tracking metadata");
    pair[0].joined=true;
    expect(!readyEyePair(pair,5,110),"old activation cannot survive recenter");
    expect(!readyEyePair(pair,4,251),"stale eye images stop submission");
    expect(readyEyePair(pair,4,400,500),"an already accepted pair can cover a bounded runtime stall with its original poses");
    expect(!readyEyePair(pair,4,601,500),"presentation recovery never retains eyes beyond half a second");
    expect(!readyEyePair(pair,5,400,500),"presentation recovery cannot cross activation generations");
    expect(!readyEyePair(pair,4,400,501),"unbounded presentation retention is rejected");
    const auto l=nativeEyePose(Pose{{},{10,20,30}},Pose{},Pose{{},{-0.032f,0,0}});
    const auto r=nativeEyePose(Pose{{},{10,20,30}},Pose{},Pose{{},{0.032f,0,0}});
    expect(near(l.position.x,10.032f)&&near(r.position.x,9.968f),"same-frame eye offsets preserve runtime IPD in FOX camera axes");
    expect(near(l.position.y,r.position.y)&&near(l.position.z,r.position.z),"parallel eye cameras do not introduce toe-in or vertical disparity");
    HeadCamera camera;
    const std::array<float,16> playerRoot{0,0,-1,0,0,1,0,0,1,0,0,0,500,300,1300,1};
    auto headBone=std::array<float,16>{1,0,0,0,0,1,0,0,0,0,1,0,0,1.6f,0.1f,1};
    const auto headPoint=playerHeadPosition(playerRoot,headBone);
    expect(headPoint&&same(*headPoint,{500.1f,301.6f,1300}),"animated head position uses local-then-world transform order");
    auto invalidRoot=playerRoot;invalidRoot[0]=std::numeric_limits<float>::quiet_NaN();
    expect(!playerHeadPosition(invalidRoot,headBone),"nonfinite player root cannot move the VR camera");
    invalidRoot=playerRoot;invalidRoot[2]=1;
    expect(!playerHeadPosition(invalidRoot,headBone),"reflected player transform cannot reverse stereo handedness");
    invalidRoot=playerRoot;invalidRoot[5]=2;
    expect(!playerHeadPosition(invalidRoot,headBone),"scaled player transform requires a separate world-scale contract");
    HeadCamera firstPerson;firstPerson.configure(true,1,true);firstPerson.track({},true,100);
    const Pose thirdPerson{{},{500,303,1305}};
    expect(firstPerson.publishPlayerHead(11,22,thirdPerson,playerRoot,headBone,100),"native player head joins its camera publication");
    firstPerson.toggle();auto firstView=firstPerson.resolve(11,thirdPerson,100);
    expect(firstView.applied&&same(firstView.nativePose.position,*headPoint)&&firstView.playerOwner==22&&firstView.playerSequence==1,
           "VR starts at the player head even when the native camera is behind the player");
    headBone[13]=0.3f;const Pose lowered{{},{500,301,1303}};
    firstPerson.track(Pose{{},{0.2f,0.1f,-0.1f}},true,110);
    firstPerson.publishPlayerHead(11,22,lowered,playerRoot,headBone,110);
    firstView=firstPerson.resolve(11,lowered,110);
    expect(firstView.applied&&same(firstView.nativePose.position,{499.9f,300.4f,1300.1f}),
           "prone head height and six-axis tracking do not inherit the third-person boom");
    const auto savedHeadView=firstView;
    auto mismatchedCamera=lowered;mismatchedCamera.position.z+=1;
    expect(!firstPerson.resolve(11,mismatchedCamera,110).applied&&firstPerson.status().reason==HeadCameraStop::playerHeadUnavailable,
           "unmatched camera generation stops instead of attaching an unrelated player pose");
    const auto menuActivation=firstPerson.status().activation;
    expect(!firstPerson.active()&&!firstPerson.status().pending&&firstPerson.status().awaitingPlayer,
           "missing player publication exposes native menu pixels and controls while retaining VR intent");
    firstPerson.track(Pose{{},{0.3f,0.1f,-0.1f}},true,120);
    firstPerson.publishPlayerHead(12,23,lowered,playerRoot,headBone,120);
    expect(!firstPerson.resolve(12,lowered,120).applied&&firstPerson.status().awaitingPlayer,
           "an unrelated camera and actor cannot resume VR after a menu");
    firstPerson.publishPlayerHead(11,23,lowered,playerRoot,headBone,120);
    expect(!firstPerson.resolve(11,lowered,120).applied&&firstPerson.status().awaitingPlayer,
           "recycled camera address cannot attach VR to a different player owner");
    firstPerson.publishPlayerHead(11,22,lowered,playerRoot,headBone,120);
    const auto resumedPlayer=firstPerson.resolve(11,lowered,120);
    expect(resumedPlayer.applied&&!firstPerson.status().awaitingPlayer
        &&firstPerson.status().activation==menuActivation+1
        &&same(resumedPlayer.nativePose.position,{499.8f,300.4f,1300.1f}),
           "the same live player resumes automatically with original head origin and a fresh eye generation");
    firstPerson.resolve(11,mismatchedCamera,120);firstPerson.toggle();
    expect(!firstPerson.status().awaitingPlayer&&!firstPerson.active(),"manual disable cancels automatic menu return");
    expect(!firstPerson.resolve(11,lowered,120).applied,"fresh gameplay cannot undo a manual VR disable");
    firstPerson.toggle();firstPerson.resolve(11,lowered,120);
    firstPerson.setNativeMenuOpen(true);
    const auto menuView=firstPerson.resolve(11,lowered,120);
    expect(menuView.applied&&menuView.menuOpen&&firstPerson.active()&&!firstPerson.status().awaitingPlayer,
           "iDroid retains the native stereo viewpoint with a world-space menu panel");
    firstPerson.track(Pose{{},{0.4f,0.1f,-0.1f}},true,125);
    const auto movedMenu=firstPerson.resolve(11,mismatchedCamera,125);
    expect(movedMenu.applied&&same(movedMenu.menuPanel.position,menuView.menuPanel.position)
        &&near(std::abs(movedMenu.nativePose.position.x-menuView.nativePose.position.x),.1f),
           "leaning moves the menu world camera while the panel stays anchored and native iDroid camera animation is ignored");
    expect(!firstPerson.resolve(12,lowered,125).applied&&firstPerson.active(),
           "an unrelated native camera cannot borrow the menu viewpoint");
    firstPerson.setNativeMenuOpen(false);
    expect(firstPerson.resolve(11,lowered,125).applied,"closing the native iDroid state restores the same tracked player");
    firstPerson.setNativeMenuOpen(true);firstPerson.toggle();firstPerson.setNativeMenuOpen(false);
    expect(!firstPerson.resolve(11,lowered,125).applied,"manual disable inside iDroid prevents automatic return");
    expect(same(savedHeadView.nativePose.position,{499.9f,300.4f,1300.1f}),"published player-eye frame remains immutable");
    firstPerson.track({},true,300);firstPerson.toggle();
    expect(!firstPerson.resolve(11,lowered,300).applied,"stale player head cannot survive a fresh headset sample");
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
    const auto activationBeforeStall=camera.status().activation;
    expect(!camera.resolve(1,nativeCamera,271).applied&&camera.active()&&camera.status().suspended,"stale tracking suspends camera writes while preserving VR intent");
    expect(camera.status().reason==HeadCameraStop::staleTracking,"stale tracking suspension remains observable");
    camera.track(Pose{{},{5,5,-6}},true,300);resolved=camera.resolve(1,nativeCamera,300);
    expect(resolved.applied&&same(resolved.nativePose.position,{9,20,30})&&!camera.status().suspended
        &&camera.status().activation==activationBeforeStall,"fresh tracking resumes the same origin and stereo activation without a third-person fallback");
    camera.track({},false,301);
    expect(!camera.resolve(1,nativeCamera,301).applied&&camera.active()&&camera.status().suspended,"tracking loss suspends rendering without submitting a theatre view");
    camera.toggle();expect(!camera.active()&&!camera.status().suspended,"manual disable still works while tracking is suspended");
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
    HeadCamera handsCamera;handsCamera.configure(true);
    const std::array<EyeView,2> rigEyes{{{Pose{{},{-0.032f,0,0}},EyeFov{-0.7f,0.7f,0.7f,-0.7f}},
                                      {Pose{{},{0.032f,0,0}},EyeFov{-0.7f,0.7f,0.7f,-0.7f}}}};
    {
        HeadCamera title;title.configure(true);ControllerFrame input;input.frontEnd=true;input.referenceEpoch=1;
        title.trackStereo({},rigEyes,true,100,input);title.toggle();
        const auto entry=title.resolve(1,nativeCamera,100);
        auto orbit=nativeCamera;orbit.position.x+=2;orbit.orientation={0,.70710678f,0,.70710678f};
        const auto stationary=title.resolve(1,orbit,110);
        expect(entry.applied&&stationary.applied&&same(entry.nativePose.position,stationary.nativePose.position)
            &&same(rotate(entry.nativePose.orientation,{0,0,1}),rotate(stationary.nativePose.orientation,{0,0,1})),
            "Title camera orbit cannot move or turn the tracked player");
        title.trackStereo(Pose{{},{.2f,0,0}},rigEyes,true,120,input);
        const auto lean=title.resolve(1,orbit,120);
        expect(lean.applied&&!same(lean.nativePose.position,entry.nativePose.position),
            "physical head translation remains live in the wrist Title menu");
        title.cancel();input.frontEnd=false;title.trackStereo({},rigEyes,true,130,input);title.toggle();
        expect(same(title.resolve(2,orbit,130).nativePose.position,orbit.position),
            "entering gameplay after Title binds the new player camera");
    }
    {
        HeadCamera snaps;snaps.configure(true);ControllerFrame input;
        snaps.trackStereo({},rigEyes,true,100,input);snaps.toggle();snaps.resolve(1,nativeCamera,100);
        const Pose lean{{},{.3f,.1f,-.2f}};
        snaps.trackStereo(lean,rigEyes,true,110,input);
        const auto before=snaps.resolve(1,nativeCamera,110);
        input.snapYaw=-.523598776f;snaps.trackStereo(lean,rigEyes,true,120,input);
        const auto after=snaps.resolve(1,nativeCamera,120);
        const Quat turn{0,std::sin(input.snapYaw*.5f),0,std::cos(input.snapYaw*.5f)};
        expect(before.applied&&after.applied&&same(before.nativePose.position,after.nativePose.position),
            "snap pivots around the leaned physical head without teleporting it");
        expect(same(rotate(after.nativePose.orientation,{0,0,1}),rotate(turn,rotate(before.nativePose.orientation,{0,0,1}))),
            "snap rotates around world up without adding pitch or roll");
        expect(same(snaps.resolve(1,nativeCamera,121).nativePose.position,after.nativePose.position),
            "repeated native publications do not accumulate the snap pivot correction");
        snaps.setNativeMenuOpen(true);
        const auto menu=snaps.resolve(1,nativeCamera,122);
        expect(menu.menuOpen&&same(menu.nativePose.position,after.nativePose.position)
            &&same(rotate(menu.nativePose.orientation,{0,0,1}),rotate(after.nativePose.orientation,{0,0,1})),
            "opening a native menu does not apply the snap angle a second time");
        snaps.setNativeMenuOpen(false);
        expect(same(snaps.resolve(1,nativeCamera,123).nativePose.position,after.nativePose.position),
            "closing a native menu restores the same snapped gameplay viewpoint");
    }
    ControllerFrame hands;hands.referenceEpoch=1;hands.predictedXrTime=9000;
    {
        const auto yaw=[](float a){return Quat{0,std::sin(a*.5f),0,std::cos(a*.5f)};};
        const auto pitch=[](float a){return Quat{std::sin(a*.5f),0,0,std::cos(a*.5f)};};
        const auto roll=[](float a){return Quat{0,0,std::sin(a*.5f),std::cos(a*.5f)};};
        const Pose basis{{0,1,0,0},{}};
        const Pose stock=compose(Pose{yaw(-1.9f),{10,20,30}},Pose{pitch(-.35f),{}});
        Pose initial=compose(Pose{yaw(.3f),{.2f,1.6f,-.1f}},compose(Pose{pitch(-.4f),{}},Pose{roll(.12f),{}}));
        HeadCamera level;level.configure(true);ControllerFrame input;input.referenceEpoch=1;
        level.trackStereo(initial,rigEyes,true,100,input);level.toggle();level.resolve(1,stock,100);
        Pose moved{};
        for(int i=0;i<7;++i){
            moved=compose(Pose{yaw(i*.8f),{.5f,1.65f,-.4f}},Pose{pitch(.2f),{}});
            input.snapYaw=i%2?-.523598776f:0;
            level.trackStereo(moved,rigEyes,true,110+i*10,input);
            const auto frame=level.resolve(1,stock,110+i*10);
            const auto worldFromTracking=compose(compose(frame.nativePose,basis),inverse(moved));
            expect(frame.applied&&same(rotate(worldFromTracking.orientation,{0,1,0}),{0,1,0}),
                "tilted activation and native camera cannot tilt gravity during head or snap turns");
        }
        const auto before=level.resolve(1,stock,171);
        level.recenter();const auto centered=level.resolve(1,stock,172);
        expect(centered.applied&&same(centered.nativePose.position,stock.position)
            &&same(rotate(centered.nativePose.orientation,{0,0,1}),rotate(before.nativePose.orientation,{0,0,1})),
            "recenter brings the player under the head without changing the current facing or physical pitch");
        expect(centered.activation>before.activation,"recenter invalidates earlier eye and rig publications");
        input.referenceEpoch=2;
        const Pose shifted=compose(Pose{yaw(.65f),{2,0,3}},moved);
        level.trackStereo({},rigEyes,false,175,{});
        level.trackStereo(shifted,rigEyes,true,180,input);
        const auto rebased=level.resolve(1,stock,180);
        expect(rebased.applied&&same(rebased.nativePose.position,centered.nativePose.position)
            &&same(rotate(rebased.nativePose.orientation,{0,0,1}),rotate(centered.nativePose.orientation,{0,0,1})),
            "runtime recenter preserves gameplay and snap heading across LOCAL epochs even after lost focus/tracking");
    }
    {
        OpticStabilizer steady;OpticPose optic;optic.tracked=true;optic.rightEyepiece.position={0,0,-.08f};
        Pose grip{{},{.1f,-.02f,-.14f}};
        steady.update({},grip,optic,true,100,1);
        auto jitter=grip;jitter.position.x+=.004f;
        const auto damped=steady.update({},jitter,optic,true,111,1);
        expect(damped.position.x>grip.position.x&&damped.position.x<grip.position.x+.002f,
            "small close-eye grip tremor is damped before housing and scene attachment");
        const Pose head{{0,.258819f,0,.9659258f},{.1f,.03f,.02f}};
        auto attached=compose(head,damped);optic.rightEyepiece=compose(head,Pose{{},{0,0,-.08f}});
        const auto follows=steady.update(head,attached,optic,true,122,1);
        expect(same(follows.position,attached.position),"steady eyepiece follows head movement without filtering the headset");
        optic.rightEyepiece.position={0,0,-.4f};
        expect(same(steady.update({},grip,optic,true,133,1).position,grip.position),"lowered optics immediately return to direct tracked control");
        expect(same(steady.update({},jitter,optic,true,144,2).position,jitter.position),"tracking epoch resets never reuse an old stabilized grip");
    }
    hands.hands[1]={Pose{{},{0.2f,-0.3f,-0.4f}},Pose{{},{0.2f,-0.2f,-0.5f}},true,true};
    HeadCamera menuEpoch;menuEpoch.configure(true,1,true);
    auto menuHands=hands;
    menuEpoch.trackStereo({},rigEyes,true,100,menuHands);
    menuEpoch.publishPlayerHead(11,22,thirdPerson,playerRoot,headBone,100);
    menuEpoch.toggle();menuEpoch.resolve(11,thirdPerson,100);menuEpoch.setNativeMenuOpen(true);
    const Pose menuLean{{},{.2f,.1f,-.3f}};
    menuEpoch.trackStereo(menuLean,rigEyes,true,105,menuHands);
    const auto beforeMenuRebase=menuEpoch.resolve(11,thirdPerson,105);
    menuHands.referenceEpoch=2;menuHands.predictedXrTime=10000;
    menuEpoch.trackStereo(Pose{{0,.258819f,0,.9659258f},{3,1,-2}},rigEyes,true,110,menuHands);
    const auto rebasedMenu=menuEpoch.resolve(11,thirdPerson,110);
    expect(!menuEpoch.status().awaitingPlayer&&!menuEpoch.status().pending&&menuEpoch.active()
        &&rebasedMenu.menuOpen&&same(rebasedMenu.nativePose.position,beforeMenuRebase.nativePose.position)
        &&same(rebasedMenu.menuPanel.position,beforeMenuRebase.menuPanel.position),
           "a reference-space change inside iDroid rebases the live stereo menu viewpoint");
    menuEpoch.setNativeMenuOpen(false);
    menuEpoch.publishPlayerHead(11,22,thirdPerson,playerRoot,headBone,110);
    const auto afterMenuRebase=menuEpoch.resolve(11,thirdPerson,110);
    expect(afterMenuRebase.applied&&same(afterMenuRebase.nativePose.position,beforeMenuRebase.nativePose.position)
        &&same(rotate(afterMenuRebase.nativePose.orientation,{0,0,1}),rotate(beforeMenuRebase.nativePose.orientation,{0,0,1})),
           "closing iDroid after a reference-space change preserves the gameplay viewpoint");
    handsCamera.trackStereo({},rigEyes,true,100,hands);handsCamera.toggle();
    const auto joinedHands=handsCamera.resolve(1,nativeCamera,100);
    expect(joinedHands.applied&&joinedHands.controllers.hands[1].gripTracked&&joinedHands.controllers.predictedXrTime==9000,
           "grip, aim and eyes retain one predicted-time snapshot");
    hands.hands[1].grip.position.x+=0.1f;hands.predictedXrTime=10000;
    handsCamera.trackStereo({},rigEyes,true,110,hands);
    expect(near(joinedHands.controllers.hands[1].grip.position.x,0.2f),"a newer controller sample cannot mutate a source frame");
    expect(!handsCamera.resolve(1,nativeCamera,261).controllers.hands[1].gripTracked,"stale eyes cannot expose a live weapon pose");
    hands.hands[1].grip.orientation.w=0;
    handsCamera.trackStereo({},rigEyes,true,270,hands);
    auto validAimOnly=handsCamera.resolve(1,nativeCamera,270);
    expect(!validAimOnly.controllers.hands[1].gripTracked&&validAimOnly.controllers.hands[1].aimTracked,"grip and aim validity remain independent");
    const Pose held{{},{0.2f,-0.3f,-0.4f}},movingHead{{0,0.258819f,0,0.965926f},{0.1f,0.05f,0}};
    const Pose basis{{0,1,0,0},{}};
    const auto movedNativeHead=compose(nativeCamera,compose(compose(basis,movingHead),inverse(basis)));
    expect(same(nativeTrackedPose(movedNativeHead,movingHead,held).position,nativeTrackedPose(nativeCamera,{},held).position),
           "head motion does not steer a stationary controller");
    const auto nativeRoot=nativeAffinePose(playerRoot);
    HeadCamera rigCamera;rigCamera.configure(true,1,true);
    hands.hands[1].grip.orientation.w=1;hands.predictedXrTime=10000;
    rigCamera.trackStereo({},rigEyes,true,300,hands);rigCamera.toggle();
    rigCamera.publishPlayerHead(1,22,thirdPerson,playerRoot,headBone,300);
    auto rigFrame=rigCamera.resolve(1,thirdPerson,300);rigFrame.nativePose.position.y+=0.1f;
    expect(rigCamera.publishRigFrame(1,22,thirdPerson,rigFrame),"native skin publication latches its exact tracking input");
    hands.predictedXrTime=11000;hands.hands[1].grip.position.x+=0.1f;
    rigCamera.trackStereo({},rigEyes,true,310,hands);
    const auto renderedRig=rigCamera.resolve(1,thirdPerson,310);
    expect(renderedRig.applied&&renderedRig.rigSequence&&renderedRig.controllers.predictedXrTime==10000
        &&same(renderedRig.nativePose.position,rigFrame.nativePose.position),"eyes use the pose that drove the native rig despite newer tracking");
    auto audioStatus=rigCamera.status();
    const auto audioPose=trackedListenerPose(renderedRig,audioStatus,310);
    expect(audioPose&&same(audioPose->position,rigFrame.nativePose.position)
        &&same(rotate(audioPose->orientation,{0,0,1}),rotate(rigFrame.nativePose.orientation,{0,0,1})),
        "listener uses the rendered center-head position and orientation despite newer tracking");
    expect(!trackedListenerPose(renderedRig,audioStatus,299)&&!trackedListenerPose(renderedRig,audioStatus,451),
        "future or stale camera publications cannot update the native audio listener");
    ++audioStatus.activation;
    expect(!trackedListenerPose(renderedRig,audioStatus,310),"recenter rejects the previous listener publication");
    audioStatus=rigCamera.status();audioStatus.suspended=true;
    expect(!trackedListenerPose(renderedRig,audioStatus,310),"tracking suspension releases listener ownership");
    audioStatus=rigCamera.status();audioStatus.active=false;
    expect(!trackedListenerPose(renderedRig,audioStatus,310),"theatre mode leaves native audio in control");
    audioStatus=rigCamera.status();audioStatus.pending=true;
    expect(!trackedListenerPose(renderedRig,audioStatus,310),"pending VR activation cannot move audio ahead of the image");
    audioStatus=rigCamera.status();audioStatus.enabled=false;
    expect(!trackedListenerPose(renderedRig,audioStatus,310),"disabled integration cannot retain listener ownership");
    auto badAudioFrame=renderedRig;badAudioFrame.nativePose.orientation.w=std::numeric_limits<float>::quiet_NaN();
    expect(!trackedListenerPose(badAudioFrame,rigCamera.status(),310),"invalid rotation cannot reach the audio engine");
    badAudioFrame=renderedRig;badAudioFrame.applied=false;
    expect(!trackedListenerPose(badAudioFrame,rigCamera.status(),310),"unmodified camera cannot receive a tracked listener");
    badAudioFrame=renderedRig;badAudioFrame.stereoTracked=false;
    expect(!trackedListenerPose(badAudioFrame,rigCamera.status(),310),"missing stereo tracking leaves the native listener unchanged");
    auto changedNativeCamera=thirdPerson;changedNativeCamera.position.x+=0.1f;
    rigCamera.publishPlayerHead(1,22,changedNativeCamera,playerRoot,headBone,311);
    expect(!rigCamera.resolve(1,changedNativeCamera,311).applied&&rigCamera.status().reason==HeadCameraStop::rigFrameMismatch,
           "a rig from another native camera publication is withheld");
    expect(nativeRoot&&same(rotate(nativeRoot->orientation,{1,0,0}),{0,0,-1}),"row affine decoding retains native handedness");
    const Pose chestPose{{.38268343f,0,0,.92387953f},{0,1.2f,0}};
    const Vec3 shoulderCenter{0,1.4f,.03f};
    const Pose uprightHead{{0,.70710678f,0,.70710678f},{2,1.7f,3}};
    const auto torsoPlacement=upperBodyPlacement(chestPose,shoulderCenter,uprightHead);
    expect(torsoPlacement&&same(compose(*torsoPlacement,Pose{{},shoulderCenter}).position,{1.84f,1.52f,3}),
           "shoulders stay behind and below the eyes after native yaw");
    if(torsoPlacement){
        const auto placedChest=compose(*torsoPlacement,chestPose);
        expect(same(rotate(placedChest.orientation,{0,1,0}),{0,1,0}),"native crouch pitch does not tilt the VR shoulder frame");
        const Vec3 a{-.2f,1.4f,.03f},b{.2f,1.4f,.03f};
        const auto movedA=compose(*torsoPlacement,Pose{{},a}).position,movedB=compose(*torsoPlacement,Pose{{},b}).position;
        expect(near(dot(movedA-movedB,movedA-movedB),.16f),"shared upper-body placement preserves shoulder width");
        const auto skinBefore=a*.35f+b*.65f;
        expect(same(movedA*.35f+movedB*.65f,compose(*torsoPlacement,Pose{{},skinBefore}).position),
               "sleeve vertices shared by chest and clavicle keep one rigid placement");
    }
    auto invalidChest=chestPose;invalidChest.orientation.w=std::numeric_limits<float>::quiet_NaN();
    expect(!upperBodyPlacement(invalidChest,shoulderCenter,uprightHead),"invalid torso pose cannot reach skin publication");
    const ArmPose arm{{{},{}},{{},{0.2f,-0.15f,0}},{{},{0.4f,0,0}}};
    const ArmBasis armBasis{arm.elbow.position-arm.shoulder.position,arm.wrist.position-arm.elbow.position};
    for(int i=0;i<100;++i){
        const float angle=static_cast<float>(i)*0.06283185f;
        const Pose target{{0,0,std::sin(angle/2),std::cos(angle/2)},{0.4f*std::cos(angle),0.4f*std::sin(angle),0.1f}};
        const auto solved=solveArm(arm,target,{0,-1,-1},&armBasis);
        expect(solved&&same(solved->pose.wrist.position,target.position),"reachable wrist follows controller translation");
        if(solved){const auto upper=solved->pose.elbow.position-solved->pose.shoulder.position,lower=solved->pose.wrist.position-solved->pose.elbow.position;
            expect(near(dot(upper,upper),0.0625f)&&near(dot(lower,lower),0.0625f),"arm IK preserves both authored segment lengths");
            expect(same(rotate(compose(Pose{solved->pose.elbow.orientation,{}},inverse(Pose{arm.elbow.orientation,{}})).orientation,
                               arm.wrist.position-arm.elbow.position),lower),"forearm twist retains the solved bone axis");}
    }
    const ArmPose alignedArm{{{},{}},{{},{0,-.3f,0}},{{},{0,-.3f,-.25f}}};
    // A prone wrist can remain above the floor while the unconstrained elbow
    // bends through it. Exercise contact on rotated slopes, not just flat Y=0.
    for(int i=-4;i<=4;++i){
        const float angle=static_cast<float>(i)*.1f;
        const Pose slope{{0,0,std::sin(angle/2),std::cos(angle/2)},{12,3,-7}};
        const auto transform=[&](Vec3 point){return compose(slope,Pose{{},point}).position;};
        const ArmSurface surface{transform({}),rotate(slope.orientation,{0,1,0}),.06f};
        ArmPose grounded{compose(slope,Pose{{},{0,.10f,0}}),
            compose(slope,Pose{{},{.20f,-.05f,0}}),compose(slope,Pose{{},{.40f,.10f,0}})};
        const Pose target{slope.orientation,transform({.40f,.10f,0})};
        const auto free=solveArm(grounded,target,rotate(slope.orientation,{0,-1,0}));
        const auto constrained=solveArm(grounded,target,rotate(slope.orientation,{0,-1,0}),nullptr,&surface);
        expect(free&&dot(free->pose.elbow.position-surface.point,surface.normal)<0,
               "prone regression fixture actually places the free elbow below ground");
        expect(constrained&&dot(constrained->pose.elbow.position-surface.point,surface.normal)>=.0599f,
               "native contact keeps the elbow above a sloped surface");
        if(constrained){
            const auto u=constrained->pose.elbow.position-grounded.shoulder.position;
            const auto lowerSegment=constrained->pose.wrist.position-constrained->pose.elbow.position;
            expect(near(dot(u,u),.0625f)&&near(dot(lowerSegment,lowerSegment),.0625f)&&same(constrained->pose.wrist.position,target.position),
                   "contact preserves arm lengths and an unobstructed tracked wrist");
        }
        const auto hand=outsideArmSurface(transform({.4f,-.2f,0}),surface);
        expect(hand&&near(dot(*hand-surface.point,surface.normal),.06f),"penetrating wrist stops at native surface clearance");
        const auto clearHand=outsideArmSurface(target.position,surface);
        expect(clearHand&&same(*clearHand,target.position),"contact does not move an unobstructed wrist");
    }
    expect(!outsideArmSurface({},ArmSurface{{},{},.05f}),"missing collision normal cannot invent a floor");
    const ArmBasis alignedBasis{{0,-.3f,0},{0,0,-.25f},{0,0,-1},{1,0,0}};
    const Quat wristRoll{0,0,.70710678f,.70710678f};
    const auto twisted=solveArm(alignedArm,Pose{wristRoll,alignedArm.wrist.position},{0,-1,0},&alignedBasis);
    expect(twisted&&same(rotate(twisted->pose.elbow.orientation,{1,0,0}),{1,0,0}),
           "tracked wrist roll leaves the native elbow hinge untwisted");
    for(int i=0;i<=720;++i){
        const float angle=static_cast<float>(i)*0.034906585f;
        const Quat roll{0,0,std::sin(angle/2),std::cos(angle/2)};
        auto animated=alignedArm;
        animated.elbow.orientation={0,std::sin(angle*.73f),0,std::cos(angle*.73f)};
        animated.wrist.orientation={std::sin(angle*.21f),0,0,std::cos(angle*.21f)};
        const auto solved=solveArm(animated,Pose{roll,alignedArm.wrist.position},{0,-1,0},&alignedBasis);
        expect(solved&&same(rotate(solved->pose.elbow.orientation,{1,0,0}),{1,0,0})
            &&same(rotate(solved->pose.wrist.orientation,{1,0,0}),rotate(roll,{1,0,0})),
               "repeated wrist turns cannot accumulate twist at the elbow or lose the tracked hand");
    }
    const auto axisRotation=[](unsigned axis,float degrees){
        const float angle=degrees*.00872664626f,s=std::sin(angle);
        return Quat{axis==0?s:0,axis==1?s:0,axis==2?s:0,std::cos(angle)};
    };
    for(const bool right:{false,true}){
        const auto corrections=armCorrectiveRotations(axisRotation(2,20),axisRotation(0,80),
            axisRotation(1,-60),axisRotation(0,100),right);
        expect(same(rotate(corrections[1],{0,1,0}),rotate(axisRotation(0,right?-60.f:-56.f),{0,1,0})),
               "shoulder correctives preserve the native left/right twist weights");
        expect(same(rotate(corrections[3],{1,0,0}),rotate(axisRotation(1,33),{1,0,0})),
               "elbow corrective counter-rotates flexion instead of folding the sleeve");
        expect(same(rotate(corrections[4],{0,1,0}),rotate(axisRotation(0,35),{0,1,0}))
            &&same(rotate(corrections[5],{0,1,0}),rotate(axisRotation(0,75),{0,1,0})),
               "forearm helper joints distribute 100 degrees of wrist twist at 35 and 75 percent");
    }
    const Pose watchElbow{{},{-.3f,-.3f,-.4f}},watchWrist{{},{0,-.3f,-.4f}};
    const auto watch=forearmPanel(watchElbow,watchWrist,{0,1,0});
    expect(watch&&same(rotate(watch->orientation,{1,0,0}),{1,0,0})
        &&same(rotate(watch->orientation,{0,0,1}),{0,1,0})
        &&same(rotate(watch->orientation,{0,1,0}),{0,0,-1}),
        "left watch reads left to right along the forearm with its top away from the wearer");
    const Pose watchMove{{0,.70710678f,0,.70710678f},{3,1,2}};
    const auto movedWatch=forearmPanel(compose(watchMove,watchElbow),compose(watchMove,watchWrist),rotate(watchMove.orientation,{0,1,0}));
    expect(watch&&movedWatch&&same(movedWatch->position,compose(watchMove,*watch).position),
           "forearm HUD remains attached through character translation and turning");
    expect(!forearmPanel(watchElbow,watchWrist,{1,0,0}),"undefined forearm normal cannot produce a face HUD");
    SupportContact support;
    const Vec3 heldSeparation{-.08f,0,-.24f},withdrawnSeparation{-.34f,-.06f,.04f},primaryForward{0,0,-1};
    expect(withinSupportCone(heldSeparation,primaryForward),"a hand at the forward rifle grip can guide the barrel");
    expect(!withinSupportCone(withdrawnSeparation,primaryForward),"withdrawing alongside the firing hand releases even when the gun follows it");
    const Quat supportTurn{0,.70710678f,0,.70710678f};
    expect(withinSupportCone(rotate(supportTurn,heldSeparation),rotate(supportTurn,primaryForward))
        &&!withinSupportCone(rotate(supportTurn,withdrawnSeparation),rotate(supportTurn,primaryForward)),
        "support release follows controller aim through body turns");
    expect(!withinSupportCone({},primaryForward)&&!withinSupportCone(heldSeparation,{}),"invalid support directions cannot acquire a hand");
    expect(!support.update(true,true,.07f,100)&&support.update(true,true,.07f,250)
        &&!support.update(withinSupportCone(withdrawnSeparation,primaryForward),true,.07f,300),
        "a close guided contact cannot retain a sideways withdrawn hand");
    support.reset();
    SupportPose supportPose;
    const Pose acquiredSupport{{},{.25f,0,.12f}},animatedReload{{},{.15f,.2f,.05f}},stowingSupport{{},{-.4f,2.f,-.3f}};
    expect(!supportPose.update(acquiredSupport,false,false),"a free hand has no acquired support pose");
    auto presentedSupport=supportPose.update(acquiredSupport,true,false);
    expect(presentedSupport&&same(presentedSupport->position,acquiredSupport.position),"support acquires the current weapon contact");
    presentedSupport=supportPose.update(stowingSupport,true,false);
    expect(presentedSupport&&same(presentedSupport->position,acquiredSupport.position),"ordinary animation changes cannot move acquired support into the sky");
    presentedSupport=supportPose.update(animatedReload,true,true);
    expect(presentedSupport&&same(presentedSupport->position,animatedReload.position),"an attached native reload can animate contact");
    presentedSupport=supportPose.update(stowingSupport,false,false);
    expect(presentedSupport&&same(presentedSupport->position,animatedReload.position),"release retains the last contact rather than chasing stow animation");
    presentedSupport=supportPose.update(acquiredSupport,true,false);
    expect(presentedSupport&&same(presentedSupport->position,acquiredSupport.position),"new contact reacquires its own weapon pose");
    supportPose.reset();
    expect(!supportPose.update(stowingSupport,false,false),"menu or tracking reset clears support presentation");
    expect(!support.update(true,true,.25f,1000)&&!support.update(true,true,.25f,1200),"a free palm away from the weapon support grip stays one-handed");
    expect(!support.update(true,true,.09f,1300)&&!support.update(true,true,.09f,1400)
        &&support.update(true,true,.09f,1450),"support engages only after dwelling at the actual weapon grip");
    expect(support.update(true,true,.19f,1500),"a small movement at the acquired grip retains support");
    expect(!support.update(true,true,.21f,1600)&&!support.update(true,true,.19f,1800),"pulling away releases support without edge chatter");
    support.update(true,true,.05f,2000);
    expect(support.update(true,true,.05f,2150)&&!support.update(true,false,.05f,2160),"lost hand tracking releases support");
    support.update(true,true,.05f,2300);
    expect(support.update(true,true,.05f,2450)&&!support.update(false,true,.05f,2460),"lowering or inspecting releases support");
    expect(!support.update(true,true,.15f,2600),"selection does not preserve a sticky two-hand latch");
    support.update(true,true,.05f,2700);
    expect(!support.update(true,true,.05f,2800)&&!support.update(true,true,.05f,2750),"a regressed contact clock resets the dwell");
    RigInput travelInput;
    GamepadSample triggerOnly{};triggerOnly.rightTrigger=255;
    const auto cqc=travelInput.update(triggerOnly,false,false,TravelMode::onFoot);
    expect(cqc.gamepad.rightTrigger==255&&cqc.gamepad.leftTrigger==0&&!cqc.weaponReady,
           "lowered weapon retains the native attack/CQC trigger without aiming");
    const auto readyGun=travelInput.update(triggerOnly,true,true,TravelMode::onFoot);
    expect(readyGun.gamepad.leftTrigger==255&&readyGun.gamepad.rightTrigger==255&&readyGun.weaponReady,
           "right grip readies the firearm; left grip no longer forces support");
    expect(travelInput.update(triggerOnly,true,true,TravelMode::vehicle).gamepad==GamepadSample{},
           "entering a vehicle cannot carry firing input into accelerator or mounted attack");
    travelInput.update({},false,false,TravelMode::vehicle);
    const auto accelerator=travelInput.update(triggerOnly,false,false,TravelMode::vehicle);
    expect(accelerator.gamepad.rightTrigger==255&&accelerator.gamepad.leftTrigger==0&&!accelerator.weaponReady,
           "vehicle accelerator works independently of the weapon-ready grip and brake");
    GamepadSample brakeOnly{};brakeOnly.leftTrigger=255;
    const auto brake=travelInput.update(brakeOnly,false,false,TravelMode::vehicle);
    expect(brake.gamepad.leftTrigger==255&&brake.gamepad.rightTrigger==0&&brake.gamepad.buttons==0,
           "vehicle braking/reversing is not consumed by the equipment modifier");
    expect(travelInput.update({},true,false,TravelMode::vehicle).gamepad.buttons==0,
           "gripping the wheel cannot fire a vehicle weapon");
    expect(travelInput.update(GamepadSample{0x4000},false,false,TravelMode::vehicle).gamepad.buttons==0x0100,
           "X reaches the native vehicle weapon or radio independently of the wheel grip");
    expect(travelInput.update(brakeOnly,false,true,TravelMode::onFoot).gamepad==GamepadSample{},
           "exiting a vehicle releases brake and grip inputs before restoring foot controls");
    travelInput.update({},false,false,TravelMode::onFoot);
    expect(travelInput.update(triggerOnly,false,true,TravelMode::onFoot).weaponReady,
           "on-foot weapon controls resume after neutral input following exit");
    expect(travelInput.update(triggerOnly,false,true,TravelMode::unknown).gamepad==GamepadSample{},
           "unavailable native travel state cannot guess accelerator versus firearm input");
    travelInput.update({},false,false,TravelMode::vehicle);
    travelInput.suspend();
    expect(travelInput.update(triggerOnly,false,false,TravelMode::vehicle).gamepad==GamepadSample{},
           "regaining XR focus cannot resume a held accelerator");
    travelInput.update({},false,false,TravelMode::vehicle);
    expect(travelInput.update(triggerOnly,false,false,TravelMode::vehicle).gamepad.rightTrigger==255,
           "vehicle input resumes after neutral input following focus loss");
    const auto extended=solveArm(arm,Pose{{},{3,0,0}},{0,-1,0});
    expect(extended&&extended->reachClamped&&near(extended->pose.wrist.position.x,0.499f),"unreachable grip cannot stretch the native limb");
    expect(!solveArm(ArmPose{},Pose{},{}),"missing skeleton geometry cannot produce an arm");
    const Vec3 indexKnuckle{0,0.03f,-0.085f},littleKnuckle{0,-0.03f,-0.075f};
    const auto palm=anatomicalGrip({},indexKnuckle,littleKnuckle);
    expect(palm&&near(palm->position.z,-0.044f),"grip origin is inside the palm rather than at the wrist");
    if(palm){
        const Pose moved{{0,0.70710678f,0,0.70710678f},{3,2,1}};
        const auto movedPalm=anatomicalGrip(moved,compose(moved,Pose{{},indexKnuckle}).position,compose(moved,Pose{{},littleKnuckle}).position);
        const auto expectedPalm=compose(moved,*palm);
        expect(movedPalm&&same(movedPalm->position,expectedPalm.position)
            &&same(rotate(movedPalm->orientation,{0,0,-1}),rotate(expectedPalm.orientation,{0,0,-1})),
            "anatomical grip is invariant under native character or animation rotation");
        const Pose tracked{{0.258819f,0,0,0.9659258f},{0.2f,-0.2f,-0.4f}};
        const auto target=compose(tracked,inverse(*palm));
        expect(same(compose(target,*palm).position,tracked.position),"tracked grip places the palm at the controller, including the wrist offset");
    }
    expect(!anatomicalGrip({},indexKnuckle,indexKnuckle)&&!anatomicalGrip({},Vec3{0,0,2},Vec3{0,1,2}),
           "degenerate or non-hand landmark geometry cannot steer the rig");
    RigEquipment equipment;
    uint64_t equipmentTime=1000;
    const auto equipmentStep=[&](GamepadSample sample,bool modifier,bool rendered=true,uint64_t elapsed=100){
        equipmentTime+=elapsed;
        return equipment.update(sample,modifier,false,equipmentTime,rendered?equipmentTime:0);
    };
    GamepadSample equipmentHeld{0x0040,255,128,17000,30000,0,0};
    auto mapped=equipmentStep(equipmentHeld,true);
    expect(mapped.buttons==0x0040&&mapped.leftX==17000&&mapped.leftY==30000,
        "trigger alone never quick-equips the previous category and preserves movement");
    for(const auto direction:std::array<GamepadSample,4>{{{0,0,0,-30000,22000,0,30000},{0,0,0,-30000,22000,0,-30000},
            {0,0,0,-30000,22000,30000,0},{0,0,0,-30000,22000,-30000,0}}}){
        equipment.reset();equipmentStep({},true);
        const uint16_t category=direction.rightY>0?1:direction.rightY<0?2:direction.rightX>0?8:4;
        mapped=equipmentStep(direction,true);
        expect(mapped.buttons==category&&mapped.rightX==0&&mapped.rightY==0&&mapped.leftX==-30000,
            "one category request matches its direction without browsing or interrupting movement");
        const auto openedAt=equipmentTime;
        equipmentTime+=100;
        mapped=equipment.update(direction,true,false,equipmentTime,openedAt);
        expect(mapped.rightX==0&&mapped.rightY==0&&equipment.phase()==2,
            "a picker draw from before the request cannot unlock navigation");
        for(unsigned frame=0;frame<8;++frame){
            mapped=equipmentStep(direction,true,false);
            expect(mapped.buttons==category&&mapped.rightX==0&&mapped.rightY==0,
                "equip/stow delays keep held navigation out of the gameplay camera");
        }
        equipmentStep({},true);equipmentStep({},true);
        expect(equipment.phase()==3,"fresh expanded UI and a centered stick unlock browsing");
        for(const auto browse:std::array<GamepadSample,8>{{{0,0,0,0,0,30000,0},{0,0,0,0,0,23170,23170},
                {0,0,0,0,0,0,30000},{0,0,0,0,0,-23170,23170},{0,0,0,0,0,-30000,0},
                {0,0,0,0,0,-23170,-23170},{0,0,0,0,0,0,-30000},{0,0,0,0,0,23170,-23170}}}){
            mapped=equipmentStep(browse,true);
            expect(mapped.rightX==0&&mapped.rightY==0,"an unsettled flick cannot skip a card");
            mapped=equipmentStep(browse,true,true,60);
            expect(mapped.buttons==category&&mapped.rightX==browse.rightX&&mapped.rightY==browse.rightY,
                "all eight visible card directions retain their axes and signs");
            auto wobble=browse;wobble.rightX=static_cast<int16_t>(-browse.rightX);wobble.rightY=static_cast<int16_t>(-browse.rightY);
            expect(equipmentStep(wobble,true)==mapped,"one continuous gesture cannot jump to another card");
            equipmentStep({},true,true,1);
            expect(equipmentStep(wobble,true,true,1)==mapped,"a one-frame center bounce cannot double-select");
            equipmentStep({},true);equipmentStep({},true);
        }
        mapped=equipmentStep(direction,true,false);
        expect(mapped.rightX==0&&mapped.rightY==0&&equipment.phase()==2,
            "missing UI closes navigation instead of turning the world");
        mapped=equipmentStep(direction,true);
        expect(mapped.rightX==0&&mapped.rightY==0,"UI recovery cannot replay the held direction");
        equipmentStep({},true);equipmentStep({},true);
    }
    equipmentHeld={0x1000};
    expect(equipmentStep(equipmentHeld,true).buttons==0x0084,"A uses the selected item without clicking its navigation stick");
    expect(equipmentStep(equipmentHeld,true).buttons==0x0004,"holding Use does not repeatedly toggle an item");
    expect(equipmentStep(equipmentHeld,false).buttons==0,"held Use cannot become crouch after closing");
    equipmentStep({},false);equipmentStep({},true);equipmentStep({0,0,0,0,0,-30000,0},true);
    equipmentStep({},true);equipmentStep({},true);
    expect(equipmentStep({0x0080},true).buttons==0x0084,"the existing right-stick item-use click remains available");
    equipmentStep({},true);
    expect(equipmentStep({0x2000},true).buttons==0,"B returns to category choice without quick-equipping anything");
    mapped=equipmentStep({0x2000,0,0,0,0,0,-30000},true);
    expect(mapped.buttons==2&&mapped.rightY==0,"a new downward flick chooses secondary after Back");
    expect(equipmentStep({0x2000,0,0,0,0,0,-30000},false).buttons==0,
        "back and navigation remain consumed on close");
    equipment.reset();equipmentHeld={0x8000};
    expect(equipmentStep(equipmentHeld,true).buttons==0,"reserved optics cannot open a scope or select equipment");
    expect(equipmentStep(equipmentHeld,false).buttons==0,"reserved optics cannot leak an action on release");
    equipmentStep({},false);
    expect(equipmentStep(equipmentHeld,false).buttons==0x8000,"ordinary Y resumes after release");
    equipment.reset();equipmentHeld={0,0,0,30000,0,30000,0};
    expect(equipmentStep(equipmentHeld,true).buttons==0,"opening during a turn cannot choose a category");
    expect(equipmentStep(equipmentHeld,true).rightX==0,"the old turning gesture remains consumed");
    equipmentStep({},true);equipmentStep({},true);
    expect(equipmentStep(equipmentHeld,true).buttons==8,"a fresh right flick chooses support after centering");
    RigInput wristInput;
    GamepadSample walkingSelection{0,255,255,15000,28000,0,0};
    auto selectionInput=wristInput.update(walkingSelection,true,true,TravelMode::onFoot);
    expect(selectionInput.gamepad.leftX==15000&&selectionInput.gamepad.leftY==28000&&selectionInput.gamepad.buttons==0
        &&selectionInput.gamepad.leftTrigger==0&&selectionInput.gamepad.rightTrigger==0&&!selectionInput.weaponReady,
        "selection frees the wrist, lowers the weapon and consumes fire while movement continues");
    walkingSelection.leftTrigger=100;
    expect(wristInput.update(walkingSelection,false,true,TravelMode::onFoot).gamepad.buttons==0,"trigger hysteresis keeps the picker open through a partial release");
    walkingSelection.leftTrigger=0;
    selectionInput=wristInput.update(walkingSelection,false,true,TravelMode::onFoot);
    expect(selectionInput.gamepad.buttons==0&&selectionInput.gamepad.rightTrigger==0&&selectionInput.weaponReady&&selectionInput.gamepad.leftY==28000,
        "release closes selection and restores the ready grip without firing a held trigger");
    walkingSelection.rightTrigger=0;wristInput.update(walkingSelection,false,true,TravelMode::onFoot);
    walkingSelection.rightTrigger=255;
    expect(wristInput.update(walkingSelection,false,true,TravelMode::onFoot).gamepad.rightTrigger==255,"a fresh trigger pull fires after the picker closes");
    RigInput grenadeInput;
    const GamepadSample grenadeStick{0,0,0,12000,19000,21000,-25000};
    auto grenadeMapped=grenadeInput.update(grenadeStick,false,true,TravelMode::onFoot,1000,0,true);
    expect(grenadeMapped.weaponReady&&grenadeMapped.gamepad.rightY==0&&grenadeMapped.gamepad.rightX==21000
        &&grenadeMapped.gamepad.leftY==19000,"readied grenade uses hand elevation while retaining walking and body turning");
    expect(grenadeInput.update(grenadeStick,false,true,TravelMode::onFoot,1100,0,false).gamepad.rightY==-25000,
        "ordinary weapon look remains available outside grenade mode");
    const Pose primaryGrip{{},{.2f,1.2f,-.3f}},supportGrip{{},{.5f,1.2f,-.6f}};
    const auto guidedGrip=twoHandGrip(primaryGrip,supportGrip,{0,0,-.3f},1);
    expect(guidedGrip&&same(guidedGrip->position,primaryGrip.position)
        &&same(rotate(guidedGrip->orientation,{0,0,-1}),{.70710678f,0,-.70710678f}),
        "support controller guides aim while the primary palm stays fixed");
    const auto unguidedGrip=twoHandGrip(primaryGrip,supportGrip,{0,0,-.3f},0);
    expect(unguidedGrip&&same(rotate(unguidedGrip->orientation,{0,0,-1}),{0,0,-1}),"zero support influence preserves one-handed aim");
    const auto sidewaysBarrel=twoHandGrip(primaryGrip,supportGrip,{1,0,0},1);
    expect(sidewaysBarrel&&same(sidewaysBarrel->position,primaryGrip.position)
        &&same(rotate(sidewaysBarrel->orientation,{1,0,0}),{.70710678f,0,-.70710678f}),
        "a barrel offset from the palm axes follows the support controller without moving the firing grip");
    expect(!twoHandGrip(primaryGrip,primaryGrip,{0,0,-.3f},1)
        &&!twoHandGrip(primaryGrip,Pose{{},{.2f,1.2f,.1f}},{0,0,-.3f},1),"coincident or crossed hands cannot flip the weapon");
    for(unsigned finger=0;finger<5;++finger)for(unsigned joint=0;joint<3;++joint){
        const auto leftOpen=fingerJointRotation(false,finger,joint,0),rightOpen=fingerJointRotation(true,finger,joint,0);
        expect(leftOpen&&rightOpen&&same(rotate(*leftOpen,{1,0,0}),{1,0,0})&&same(rotate(*rightOpen,{-1,0,0}),{-1,0,0}),
            "released controller fingers retain the authored straight bind pose");
        const auto leftCurl=fingerJointRotation(false,finger,joint,1),rightCurl=fingerJointRotation(true,finger,joint,1);
        expect(leftCurl&&rightCurl&&(finger
            ?rotate(*leftCurl,{1,0,0}).y<0&&rotate(*rightCurl,{-1,0,0}).y<0
            :rotate(*leftCurl,{1,0,0}).z<0&&rotate(*rightCurl,{-1,0,0}).z<0),
            "fingers curl into the palm while mirrored thumbs close across it");
    }
    for(const bool rightHand:{false,true}){
        const float side=rightHand?-1.f:1.f;
        Pose thumb{{},{0,0,.03f}};
        const Vec3 segments[]{{side*.04f,-.01f,.008f},{side*.03f,-.01f,0},{side*.02f,0,0}};
        for(unsigned joint=0;joint<3;++joint){
            thumb=compose(thumb,Pose{*fingerJointRotation(rightHand,0,joint,1),{}});
            thumb.position=thumb.position+rotate(thumb.orientation,segments[joint]);
        }
        expect(side*thumb.position.x>.02f&&thumb.position.z<0&&thumb.position.z>-.06f&&near(thumb.position.y,-.02f),
            "closed thumb stays in front of the wrist and crosses the palm without folding backward");
    }
    expect(!fingerJointRotation(false,5,0,1)&&!fingerJointRotation(false,0,3,1)&&!fingerJointRotation(false,0,0,-1),"invalid finger channels are rejected");
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
    {
        MenuButton utility;
        expect(utility.update(true,true,100,true)==0&&utility.recentered(),"left grip plus Menu requests recenter without iDroid");
        expect(utility.update(true,true,800,true)==0&&!utility.recentered(),"held recenter chord neither repeats nor opens Pause");
        expect(utility.update(false,true,850)==0,"recenter release cannot leak into iDroid");
        utility.update(true,true,1000);
        expect(utility.update(false,true,1100)==0x10,"ordinary Menu tap still opens iDroid after recenter");
    }
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
