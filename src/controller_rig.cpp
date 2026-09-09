#include "mgs5vr/controller_rig.hpp"
#include "mgs5vr/arm_ik.hpp"
#include "mgs5vr/head_camera.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/log.hpp"
#include "mgs5vr/motion_melee.hpp"
#include <windows.h>
#include <intrin.h>
#include <MinHook.h>
#include <array>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <cmath>
#include <mutex>
#include <sstream>
#include <stdexcept>

namespace {
using namespace mgs5vr;
using Update=void(*)(void*,void*);
Update original{};
struct Vector4{float x{},y{},z{},w{};};
struct PoseRestore {
    uintptr_t rotations{},positions{};
    uintptr_t stowedMount{};
    uint16_t count{};
    std::array<Quat,512> q{};
    std::array<Vector4,512> p{};
    ~PoseRestore(){
        if(count){
            std::memcpy(reinterpret_cast<void*>(rotations),q.data(),count*16u);
            std::memcpy(reinterpret_cast<void*>(positions),p.data(),count*16u);
        }
    }
};
using Shot=void(*)(void*,Vector4*,Vector4*,void*,uint32_t);
Shot originalShot{};
using ThrowOrigin=void(*)(void*,void*,uint32_t,Vector4*,Vector4*);
using ThrowVelocity=void(*)(void*,void*,uint32_t,Vector4*);
ThrowOrigin originalThrowOrigin{};
ThrowVelocity originalThrowVelocity{};
uintptr_t base{};
std::atomic_uintptr_t playerOwner{};
std::atomic_bool enabled{};
bool groundQueryVerified{};
std::mutex rigMutex;
uintptr_t boundOwner{};
uintptr_t boundModel{};
uint64_t activation{},updates{};
std::array<MotionMelee,3> meleeMotion;
uint64_t meleeTracking{};
std::array<float,2> meleeCurl{};
WheelSteering wheelSteering;
std::array<Vec3,2> bendHistory{};
SupportContact supportContact;
SupportPose supportPose;
float supportBlend{};
float aimBlend{};
Quat guidedOffset{};
Pose heldSupportOffset{};
uint64_t supportAt{};
struct ShotRig {
    uintptr_t owner{},character{},camera{},model{};
    Pose sourceCamera{},wristWorld{};
    uint64_t trackingSequence{};
    Pose palmWorld{},gripFromAim{};
    bool throwTracked{};
} shotRig;
template<class T> bool read(uintptr_t address,T& out){
    SIZE_T count{};return address&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),&out,sizeof(out),&count)&&count==sizeof(out);
}
template<class T> T get(uintptr_t address){T out{};read(address,out);return out;}
std::optional<ArmSurface> groundSurface(Vec3 point,float ceiling,float clearance){
    // TPP 1.0.15.4 HumanGroundIk uses this synchronous, internally read-locked
    // query. The 0x800 inclusion layer is set by the player IK factory. Query
    // storage and hit records are caller-owned; no native state is modified.
    if(!groundQueryVerified||!valid(Pose{{},point})||!std::isfinite(ceiling)
       ||point.y>ceiling||ceiling-point.y>3.f||!get<uintptr_t>(base+0x2c79710))return {};
    alignas(16) std::array<std::byte,0x250> query{};
    using Init=void*(*)(void*,uint32_t);
    using Ray=uint32_t(*)(void*,uint32_t,const Vector4*,const Vector4*,float);
    using HitValue=void*(*)(const void*,Vector4*);
    reinterpret_cast<Init>(base+0xa0fb50)(query.data(),0);
    const uint64_t groundLayers=0x800;
    std::memcpy(query.data(),&groundLayers,sizeof(groundLayers));
    const uint64_t filter=0x80000006;
    std::memcpy(query.data()+0x18,&filter,sizeof(filter));
    alignas(16) Vector4 start{point.x,ceiling,point.z,0},end{point.x,point.y-.75f,point.z,0};
    const auto result=reinterpret_cast<Ray>(base+0x1b9b130)(query.data(),0x04000700,&start,&end,0.f);
    const auto address=reinterpret_cast<uintptr_t>(query.data());
    const auto count=get<uint32_t>(address+0x60);const auto index=get<int32_t>(address+0x64);
    if(!result||!count||count>5||index<0||static_cast<uint32_t>(index)>=count)return {};
    alignas(16) Vector4 hit{},normal{};const void* record=query.data()+0x70+index*0x60;
    // These getters also handle shape-local hit records and their transforms.
    reinterpret_cast<HitValue>(base+0x1b9a5d0)(record,&hit);
    reinterpret_cast<HitValue>(base+0x1b99910)(record,&normal);
    const ArmSurface surface{{hit.x,hit.y,hit.z},{normal.x,normal.y,normal.z},clearance};
    if(!outsideArmSurface(point,surface)||surface.normal.y<.35f
       ||hit.y>start.y+.01f||hit.y<end.y-.01f
       ||std::abs(hit.x-point.x)+std::abs(hit.z-point.z)>.02f)return {};
    return surface;
}
Pose bone(const std::array<Quat,512>& q,const std::array<Vector4,512>& p,size_t i){return {q[i],{p[i].x,p[i].y,p[i].z}};}
void replace(std::array<Quat,512>& q,std::array<Vector4,512>& p,size_t i,Pose value){
    q[i]=value.orientation;p[i].x=value.position.x;p[i].y=value.position.y;p[i].z=value.position.z;
}
Quat blendRotation(Quat a,Quat b,float weight){
    if(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w<0)b={-b.x,-b.y,-b.z,-b.w};
    Quat q{a.x+(b.x-a.x)*weight,a.y+(b.y-a.y)*weight,a.z+(b.z-a.z)*weight,a.w+(b.w-a.w)*weight};
    const float length=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
    return {q.x/length,q.y/length,q.z/length,q.w/length};
}
bool apply(void* context,void* binding,PoseRestore& restore){
    const auto now=steadyMilliseconds();
    const auto owner=playerOwner.load();
    if(!owner||get<uintptr_t>(owner)!=base+0x23b8218)return false;
    const auto character=get<uintptr_t>(owner+0x370),camera=get<uintptr_t>(owner+0x380);
    if(!character||get<uintptr_t>(character)!=base+0x2295210)return false;
    const auto component=get<uintptr_t>(character+0x10),holder=get<uintptr_t>(component+0x10),model=get<uintptr_t>(holder+0x68);
    if(!component||!holder||!model||get<uintptr_t>(model)!=base+0x20f4d90||reinterpret_cast<uintptr_t>(binding)!=model+0xa0)return false;
    const auto driver=reinterpret_cast<uintptr_t>(context),skeleton=get<uintptr_t>(driver+0x10);
    if(get<uintptr_t>(driver)!=base+0x24c1988||!skeleton)return false;
    const auto count=get<uint16_t>(skeleton+0x48);
    if(count<53||count>512||get<int16_t>(skeleton+0x4a)!=0||get<uint32_t>(skeleton+0x60)!=0)return false;
    const auto qo=get<uint32_t>(skeleton+0x58),po=get<uint32_t>(skeleton+0x5c);
    if(qo<0x90||po<qo+count*16u||po>0x10000)return false;
    const auto rotations=skeleton+qo,positions=skeleton+po,parents=get<uintptr_t>(model+0xf0);
    std::array<Quat,512> q{};std::array<Vector4,512> p{};
    std::array<int32_t,512> parent{};SIZE_T copied{};
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(rotations),q.data(),count*16u,&copied)||copied!=count*16u
       ||!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(positions),p.data(),count*16u,&copied)||copied!=count*16u)return false;
    for(size_t i=0;i<count;++i){
        if(!read(parents+i*0x20,parent[i])||parent[i]>=static_cast<int32_t>(i)||parent[i]<-1||!valid(bone(q,p,i)))return false;
    }
    if(parent[6]!=5||parent[7]!=6||parent[8]!=7||parent[10]!=9||parent[11]!=10||parent[12]!=11)return false;
    if(parent[24]!=8||parent[30]!=8||parent[34]!=30||parent[40]!=12||parent[46]!=12||parent[50]!=46)return false;
    const auto bindOffset=[&](size_t i){return get<Vec3>(parents+i*0x20+0x10);};
    const std::array<ArmBasis,2> armBasis{{{bindOffset(7),bindOffset(8)},{bindOffset(11),bindOffset(12)}}};
    // This profile's bind axes are +X on the left and -X on the right. Native
    // flexion is about Y; +Y is the dorsal side of both authored wrist frames.
    for(size_t side=0;side<2;++side)for(const auto axis:{armBasis[side].upperAxis,armBasis[side].forearmAxis})
        if(!valid(Pose{{},axis})||std::abs(axis.y)+std::abs(axis.z)>0.001f
           ||(side?-axis.x:axis.x)<0.2f||(side?-axis.x:axis.x)>0.35f)return false;
    std::array<float,16> matrix{};std::array<float,8> cameraValues{};
    if(!read(reinterpret_cast<uintptr_t>(binding),matrix)||!read(camera+0xf0,cameraValues))return false;
    const auto root=nativeAffinePose(matrix);if(!root)return false;
    const Pose nativeCamera{{cameraValues[0],cameraValues[1],cameraValues[2],cameraValues[3]},
                            {cameraValues[4],cameraValues[5],cameraValues[6]}};
    auto frame=headCamera().resolveCurrentForRig(camera,nativeCamera);
    if(!frame.applied||frame.playerOwner!=owner||!frame.controllers.hands[1].gripTracked)return false;
    const auto renderedHead=compose(*root,bone(q,p,4)).position;
    if(!frame.menuOpen)frame.nativePose.position=frame.nativePose.position+renderedHead-frame.playerHead;
    const auto originalQ=q;const auto originalP=p;
    std::array<Pose,2> grips{};
    for(size_t i=0;i<2;++i)if(frame.controllers.hands[i].gripTracked)
        grips[i]=compose(inverse(*root),nativeTrackedPose(frame.nativePose,frame.headPose,frame.controllers.hands[i].grip));
    std::lock_guard lock(rigMutex);
    const auto rootInverse=inverse(*root);
    const float groundCeiling=frame.nativePose.position.y+.05f;
    unsigned groundContacts{};
    const auto modelSurface=[&](Vec3 point,float clearance)->std::optional<ArmSurface>{
        const auto world=compose(*root,Pose{{},point}).position;
        const auto surface=groundSurface(world,groundCeiling,clearance);
        if(!surface)return {};
        return ArmSurface{compose(rootInverse,Pose{{},surface->point}).position,
            rotate(rootInverse.orientation,surface->normal),clearance};
    };
    const auto clearWrist=[&](Pose target){
        if(const auto surface=modelSurface(target.position,.055f))
            if(const auto point=outsideArmSurface(target.position,*surface)){
                if(dot(*point-target.position,*point-target.position)>1e-8f)++groundContacts;
                target.position=*point;
            }
        return target;
    };
    const auto groundedArm=[&](const ArmPose& animated,Pose target,Vec3 hint,const ArmBasis& basis){
        auto solution=solveArm(animated,target,hint,&basis);
        if(solution)if(const auto surface=modelSurface(solution->pose.elbow.position,.06f)){
            if(dot(solution->pose.elbow.position-surface->point,surface->normal)<surface->clearance){
                if(const auto contact=solveArm(animated,target,hint,&basis,&*surface)){
                    solution=contact;++groundContacts;
                }
            }
        }
        return solution;
    };
    constexpr std::array<size_t,2> wrists{8,12},shoulders{6,10};
    constexpr std::array<size_t,2> indexKnuckles{24,40},littleKnuckles{34,50};
    std::array<Pose,2> gripFromWrist{};
    for(size_t i=0;i<2;++i){
        const auto wrist=bone(q,p,wrists[i]);
        const auto palm=anatomicalGrip(wrist,bone(q,p,indexKnuckles[i]).position,bone(q,p,littleKnuckles[i]).position);
        if(!palm)return false;
        gripFromWrist[i]=compose(inverse(*palm),wrist);
    }
    if(boundOwner!=owner||boundModel!=model||activation!=frame.activation){
        for(size_t i=0;i<2;++i){
            bendHistory[i]=bone(q,p,wrists[i]-1).position-bone(q,p,shoulders[i]).position;
        }
        boundOwner=owner;boundModel=model;activation=frame.activation;
        supportContact.reset();supportPose.reset();
        supportBlend=0;aimBlend=0;guidedOffset={};heldSupportOffset={};supportAt=now;
        for(auto& motion:meleeMotion)motion.reset();
        meleeTracking=0;meleeCurl={};
        wheelSteering.reset();wheelMailbox().publish({});
        log("Controller rig uses native anatomical palm frames; no activation-pose wrist calibration");
    }
    const auto rightAnimated=bone(q,p,12),leftAnimated=bone(q,p,8);
    const auto nativeWheelContact=compose(*root,compose(leftAnimated,inverse(gripFromWrist[0])));
    const auto localWheelContact=compose(frame.headPose,compose(Pose{{0,1,0,0},{}},compose(inverse(frame.nativePose),nativeWheelContact)));
    const auto wheel=wheelSteering.update(frame.controllers.hands[0].grip,localWheelContact,
        frame.controllers.vehicleControls&&frame.controllers.hands[0].gripTracked,
        frame.controllers.hands[0].squeeze>.5f,frame.sampleTime,frame.activation,rotate(frame.headPose.orientation,{0,0,-1}));
    {
        // The arm meshes also carry spine and clavicle weights. Reposition
        // the whole upper body with one rigid transform: independent shoulder
        // offsets leave those shared sleeve vertices in different body frames.
        const auto forward=rotate(nativeCamera.orientation,{0,0,1});
        const float yaw=std::atan2(forward.x,forward.z);
        const Quat torso{0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)};
        const auto chest=bone(q,p,2);
        const auto shoulderCenter=(bone(q,p,6).position+bone(q,p,10).position)*.5f;
        const auto uprightHead=compose(inverse(*root),Pose{torso,frame.nativePose.position});
        auto torsoDelta=upperBodyPlacement(chest,shoulderCenter,uprightHead);
        if(!torsoDelta)return false;
        // Keep the shared sleeve roots together when the native prone head is
        // close to a slope. Moving an individual shoulder tears shared weights.
        float lift{};
        for(const auto i:shoulders){
            const auto world=compose(*root,compose(*torsoDelta,bone(q,p,i))).position;
            if(const auto surface=groundSurface(world,groundCeiling,.09f))
                lift=std::max(lift,(surface->clearance-dot(world-surface->point,surface->normal))/surface->normal.y);
        }
        if(lift>0){torsoDelta->position=torsoDelta->position+rotate(rootInverse.orientation,{0,lift,0});++groundContacts;}
        for(const size_t i:{1,2,5,6,7,8,9,10,11,12})replace(q,p,i,compose(*torsoDelta,bone(q,p,i)));
        for(size_t side=0;side<2;++side){
            // Stable down/out bias; a transient native crouch/reload bend must
            // not leave the elbow trapped above the head in later frames.
            bendHistory[side]=rotate(inverse(*root).orientation,rotate(torso,{side?-.25f:.25f,-1.f,-.15f}));
        }
    }
    auto rightTarget=clearWrist(compose(grips[1],gripFromWrist[1]));
    const ArmPose rightAnimatedArm{bone(q,p,10),bone(q,p,11),bone(q,p,12)};
    auto right=groundedArm(rightAnimatedArm,rightTarget,bendHistory[1],armBasis[1]);
    if(!right)return false;
    replace(q,p,10,right->pose.shoulder);replace(q,p,11,right->pose.elbow);replace(q,p,12,right->pose.wrist);
    bendHistory[1]=right->pose.elbow.position-right->pose.shoulder.position;
    bool nativeManipulation=false,firearmActive=false,throwableActive=false;
    std::optional<Vec3> barrelInGrip;
    std::optional<Pose> muzzleInGrip;
    const auto weaponComponent=get<uintptr_t>(character+0x80);
    if(!frame.controllers.vehicleControls&&get<uintptr_t>(weaponComponent)==base+0x23b3e80&&get<uintptr_t>(weaponComponent+8)==character){
        const auto instances=get<uintptr_t>(weaponComponent+0x38);
        const auto first=get<uint32_t>(instances+0x24),index=get<uint32_t>(owner+0x3a0);
        if(index>=first&&index-first<=15){
            const auto state=get<uintptr_t>(weaponComponent+0x58)+(index-first)*0x610ull;
            // Observed throughout rifle reload and WU pistol bolt cycling; the
            // native animation owns the support hand during these operations.
            nativeManipulation=(get<uint32_t>(state+0x27c)&0x1c0)==0x40&&(get<uint32_t>(state+0x3c0)&0x04000000)!=0;
            firearmActive=(get<uint32_t>(state+0x27c)&0x1c0)==0x40;
            throwableActive=(get<uint32_t>(state+0x27c)&0x1c0)==0x80;
            const auto flags=get<uint32_t>(state+0x27c),mode=get<uint32_t>(state+0x3c0);
            if(frame.controllers.weaponReady&&firearmActive&&!nativeManipulation&&!(flags&0x400000)&&!(mode&0x2000)){
                // Use the same authored barrel frame as the native shot path.
                // The vector between animated palms is not the barrel axis,
                // especially while the game changes its support-hand pose.
                alignas(16) std::array<float,16> attachmentMatrix{},muzzleMatrix{};
                using AttachmentGetter=uint32_t(*)(void*,void*,uint32_t);
                reinterpret_cast<AttachmentGetter>(base+0x1042e40)(reinterpret_cast<void*>(weaponComponent),attachmentMatrix.data(),index);
                if(read(state+((flags&0x200)?0x80:0x40),muzzleMatrix)){
                    const auto attachment=nativeAffinePose(attachmentMatrix),socket=nativeAffinePose(muzzleMatrix);
                    if(attachment&&socket){
                        muzzleInGrip=compose(gripFromWrist[1],compose(*attachment,*socket));
                        barrelInGrip=rotate(muzzleInGrip->orientation,{0,0,1});
                    }
                }
            }
        }
    }
    const auto supportOffset=compose(inverse(rightAnimated),leftAnimated);
    auto attached=compose(right->pose.wrist,supportOffset);
    // Test the actual support grip, never controller-to-controller distance.
    // While attached, retain the acquired contact frame through native reload
    // animation and evaluate it in the currently guided weapon frame.
    const bool wasAttached=supportContact.attached();
    const auto contactPrimary=compose(grips[1],Pose{blendRotation({},guidedOffset,aimBlend),{}});
    auto contactWrist=right->pose.wrist;
    if(aimBlend>0)if(const auto solved=groundedArm(rightAnimatedArm,clearWrist(compose(contactPrimary,gripFromWrist[1])),bendHistory[1],armBasis[1]))
        contactWrist=solved->pose.wrist;
    const auto contactPose=compose(contactWrist,wasAttached?heldSupportOffset:supportOffset);
    const auto attachedGrip=compose(contactPose,inverse(gripFromWrist[0]));
    const auto separation=grips[0].position-attachedGrip.position;
    const auto handSeparation=grips[0].position-grips[1].position;
    const auto towardHead=frame.headPose.position-frame.controllers.hands[0].grip.position;
    const auto dorsal=rotate(frame.controllers.hands[0].grip.orientation,{-1,0,0});
    const bool inspecting=dot(towardHead,towardHead)<0.49f
        &&dot(dorsal,towardHead)>0.5f*std::sqrt(dot(towardHead,towardHead));
    const auto& primaryHand=frame.controllers.hands[1];
    const bool forwardSupport=primaryHand.aimTracked&&withinSupportCone(
        frame.controllers.hands[0].grip.position-primaryHand.grip.position,
        rotate(primaryHand.aim.orientation,{0,0,-1}));
    // Clenching the left controller only articulates its fingers. Contact must
    // dwell at the weapon; inspection, lowering, tracking loss and pulling away
    // release it. A one-handed reload does not grab a distant tracked hand.
    const bool nearSupport=supportContact.update(frame.controllers.weaponReady&&firearmActive&&forwardSupport&&!inspecting&&(!nativeManipulation||wasAttached),
        frame.controllers.hands[0].gripTracked,std::sqrt(dot(separation,separation)),now);
    if(nearSupport&&!wasAttached)heldSupportOffset=supportOffset;
    const bool support=nearSupport;
    const float step=std::min(now>=supportAt?static_cast<float>(now-supportAt)/180.f:1.f,1.f);supportAt=now;
    supportBlend=std::clamp(supportBlend+(support?step:-step),0.f,1.f);
    // A menu, stow, non-firearm or lost controller releases ownership now.
    // Distance release can blend out, but never toward a new native stow pose.
    if(!frame.controllers.weaponReady||!firearmActive||inspecting||!frame.controllers.hands[0].gripTracked){
        supportBlend=0;aimBlend=0;supportPose.reset();
    }
    const auto presentedSupport=supportPose.update(supportOffset,nearSupport,nativeManipulation).value_or(supportOffset);
    attached=compose(right->pose.wrist,presentedSupport);
    bool guiding=nearSupport&&nativeManipulation&&aimBlend>0;
    if(barrelInGrip&&frame.controllers.hands[0].gripTracked&&nearSupport){
        if(const auto guided=twoHandGrip(grips[1],grips[0],*barrelInGrip,1.f)){
            guidedOffset=compose(inverse(grips[1]),*guided).orientation;guiding=true;
        }
    }
    // The firing wrist is the common owner of the visible weapon and muzzle.
    // Swing it toward the support controller while keeping its grip pivot and
    // roll. Native reload contact remains authoritative during manipulation.
    aimBlend=std::clamp(aimBlend+(guiding?step:-step),0.f,1.f);
    if(aimBlend>0){
        const auto guided=compose(grips[1],Pose{blendRotation({},guidedOffset,aimBlend),{}});
        rightTarget=clearWrist(compose(guided,gripFromWrist[1]));
        right=groundedArm(rightAnimatedArm,rightTarget,bendHistory[1],armBasis[1]);
        if(!right)return false;
        replace(q,p,10,right->pose.shoulder);replace(q,p,11,right->pose.elbow);replace(q,p,12,right->pose.wrist);
        bendHistory[1]=right->pose.elbow.position-right->pose.shoulder.position;
        attached=compose(right->pose.wrist,presentedSupport);
    }
    if(supportBlend>=1){
        // Lift the held weapon and its support contact together; do not slide
        // the fully attached left palm away from the authored weapon grip.
        const auto contact=clearWrist(attached);
        const auto shift=contact.position-attached.position;
        if(dot(shift,shift)>1e-8f){
            rightTarget.position=rightTarget.position+shift;
            if(const auto raised=groundedArm(rightAnimatedArm,rightTarget,bendHistory[1],armBasis[1])){
                right=raised;
                replace(q,p,10,right->pose.shoulder);replace(q,p,11,right->pose.elbow);replace(q,p,12,right->pose.wrist);
                bendHistory[1]=right->pose.elbow.position-right->pose.shoulder.position;
                attached=compose(right->pose.wrist,presentedSupport);
            }
        }
    }
    if(frame.controllers.hands[0].gripTracked||support){
        // Preserve the game's animated support-hand contact while the native
        // support is requested or a native reload/bolt cycle is running.
        auto target=attached;
        if(frame.controllers.hands[0].gripTracked&&supportBlend<1){
            const auto tracked=compose(grips[0],gripFromWrist[0]);
            target.position=tracked.position+(attached.position-tracked.position)*supportBlend;
            target.orientation=blendRotation(tracked.orientation,attached.orientation,supportBlend);
        }
        if(wheel.gripped)target=leftAnimated;
        else if(supportBlend<1)target=clearWrist(target);
        const auto left=groundedArm({bone(q,p,6),bone(q,p,7),bone(q,p,8)},target,bendHistory[0],armBasis[0]);
        if(!left)return false;
        replace(q,p,6,left->pose.shoulder);replace(q,p,7,left->pose.elbow);replace(q,p,8,left->pose.wrist);
        bendHistory[0]=left->pose.elbow.position-left->pose.shoulder.position;
    }
    constexpr std::array<size_t,10> changed{1,2,5,6,7,8,9,10,11,12};
    std::array<Pose,512> delta{};std::array<bool,512> controlled{};
    for(const auto i:changed){controlled[i]=true;delta[i]=compose(bone(q,p,i),inverse(bone(originalQ,originalP,i)));}
    for(size_t i=0;i<count;++i)if(!controlled[i]){
        auto ancestor=parent[i];
        while(ancestor>=0&&!controlled[static_cast<size_t>(ancestor)])ancestor=parent[static_cast<size_t>(ancestor)];
        if(ancestor>=0)replace(q,p,i,compose(delta[static_cast<size_t>(ancestor)],bone(originalQ,originalP,i)));
    }
    // These named corrective joints belong to the verified native arm mesh.
    // Recompute their native local corrections from the solved joints. Neither
    // stale animation corrections nor rigid parent copies match this skin.
    constexpr std::array<uint32_t,14> helperNames{0x8cb42ff9,0x17c46537,0x668bcff7,0xf8ae9203,0x9ccbd1fd,0xc7a9a0c4,0x6cae37b1,
        0x82901b42,0x4b89fc94,0x18f26b1e,0x24ce95fb,0x0831f646,0x9bed7bf0,0xa4b3e85d};
    constexpr std::array<int32_t,14> helperParents{5,6,6,7,7,7,8,9,10,10,11,11,11,12};
    const auto names=get<uintptr_t>(model+0xe8);
    bool helpersMatch=count>110;
    for(size_t j=0;helpersMatch&&j<helperNames.size();++j)
        helpersMatch=parent[97+j]==helperParents[j]&&get<uint32_t>(names+(97+j)*4)==helperNames[j];
    if(helpersMatch)for(size_t side=0;side<2;++side){
        const size_t clavicle=side?9:5;
        const auto local=[&](size_t i){return compose(inverse(bone(q,p,parent[i])),bone(q,p,i)).orientation;};
        const auto corrections=armCorrectiveRotations(local(clavicle),local(clavicle+1),local(clavicle+2),local(clavicle+3),side!=0);
        for(size_t j=0;j<7;++j){
            const auto i=97+side*7+j,anchor=static_cast<size_t>(helperParents[side*7+j]);
            // Retain the native shoulder-slide and wrist-bulge translations;
            // these channels are animated, not constant asset bind offsets.
            const auto offset=compose(inverse(bone(originalQ,originalP,anchor)),bone(originalQ,originalP,i)).position;
            if(!valid(Pose{{},offset})||dot(offset,offset)>0.16f)return false;
            replace(q,p,i,compose(bone(q,p,anchor),Pose{corrections[j],offset}));
        }
    }
    std::array<std::optional<MeleeSweep>,3> strikes{};
    if(meleeTracking!=frame.trackingSequence){
        meleeTracking=frame.trackingSequence;
        const bool available=nativeTravelMode()==TravelMode::onFoot&&!nativeManipulation
            &&frame.controllers.magnification==1&&!frame.controllers.commandControls&&!frame.menuOpen&&frame.controllers.hands[0].trigger<.25f;
        meleeCurl={};
        for(size_t point=0;point<3;++point){
            const size_t side=point==0?0:1;
            const auto& hand=frame.controllers.hands[side];
            auto contact=hand.grip;
            auto rendered=compose(*root,compose(bone(q,p,wrists[side]),inverse(gripFromWrist[side])));
            bool allowed=available&&hand.gripTracked&&(side||supportBlend==0);
            if(point==2){
                allowed=allowed&&frame.controllers.weaponReady&&firearmActive&&muzzleInGrip.has_value();
                if(muzzleInGrip){contact=compose(contact,*muzzleInGrip);rendered=compose(rendered,*muzzleInGrip);}
            }else if(side&&frame.controllers.weaponReady
                &&(throwableActive||(firearmActive&&muzzleInGrip)))allowed=false;
            const auto motion=meleeMotion[point].update(frame.headPose,contact,allowed,
                frame.sampleTime,frame.controllers.referenceEpoch,point==2);
            if(point<2)meleeCurl[side]=motion.curl;
            if(!motion.strike)continue;
            const auto toWorld=[&](Vec3 p){return nativeTrackedPose(frame.nativePose,frame.headPose,Pose{{},p}).position;};
            const auto correction=rendered.position-toWorld(motion.end);
            strikes[point]=MeleeSweep{owner,character,toWorld(motion.start)+correction,rendered.position,
                frame.sampleTime,frame.activation,static_cast<unsigned>(point),motion.started};
        }
    }
    frame.controllers.strikeCurl=meleeCurl;
    // Free fingers follow controller curls. Preserve authored contact around a
    // weapon and during reloads, including when the support hand is blending.
    // Read native locals from the saved pose so updating one joint cannot alter
    // the native reference for its children.
    constexpr std::array<std::array<size_t,5>,2> fingers{{{21,24,27,31,34},{37,40,43,47,50}}};
    unsigned articulatedHands{};
    for(size_t side=0;side<2;++side){
        const auto& hand=frame.controllers.hands[side];
        const float contact=side?((frame.controllers.weaponReady&&(firearmActive||throwableActive))?1.f:0.f):supportBlend;
        if(!hand.gripTracked||frame.controllers.vehicleControls||contact>=1)continue;
        for(size_t finger=0;finger<5;++finger)for(size_t joint=0;joint<3;++joint){
            const auto i=fingers[side][finger]+joint;
            const auto anchor=joint?i-1:(finger>=3?(side?46u:30u):wrists[side]);
            if(parent[i]!=static_cast<int32_t>(anchor))return false;
            const auto offset=bindOffset(i);
            if(!valid(Pose{{},offset})||dot(offset,offset)<.000025f||dot(offset,offset)>.015f)return false;
            const auto native=compose(inverse(bone(originalQ,originalP,anchor)),bone(originalQ,originalP,i));
            const float controllerCurl=finger==0?std::max(hand.squeeze,hand.thumbTouched?.65f:0.f)
                :finger==1?std::max(hand.trigger,hand.triggerTouched?.12f:0.f):hand.squeeze;
            const float curl=std::max(controllerCurl,frame.controllers.strikeCurl[side]);
            const auto rotation=fingerJointRotation(side!=0,static_cast<unsigned>(finger),static_cast<unsigned>(joint),curl);
            if(!rotation)return false;
            const Pose local{blendRotation(*rotation,native.orientation,contact),offset+(native.position-offset)*contact};
            replace(q,p,i,compose(bone(q,p,anchor),local));
        }
        ++articulatedHands;
    }
    // Full render pose, before native matrix and attachment publication.
    // Native position.w metadata is retained.
    frame.playerHead=renderedHead;
    if(frame.controllers.hands[0].gripTracked){
        const auto wrist=compose(*root,bone(q,p,8));
        const auto elbow=compose(*root,bone(q,p,7));
        const auto surface=helpersMatch?compose(*root,bone(q,p,102)):wrist;
        if(const auto panel=forearmPanel(elbow,wrist,rotate(surface.orientation,{0,1,0}))){
            frame.wristPanel=*panel;frame.wristPanelTracked=true;
        }
    }
    if(!headCamera().publishRigFrame(camera,owner,nativeCamera,frame))return false;
    wheelMailbox().publish(wheel);
    if(wheel.engaged)log("Driver left hand gripped authored wheel contact");
    for(const auto& strike:strikes)if(strike)publishMeleeSweep(*strike);
    // The engine may reuse animated helper/finger channels next frame. Only the
    // native publication consumes our solved pose; never feed it back into the
    // animation cache and compound garment transforms on subsequent updates.
    restore.rotations=rotations;restore.positions=positions;restore.q=originalQ;restore.p=originalP;restore.count=count;
    // SKL_300_ASRROOT is the authored hip holster, independent of the held
    // weapon's wrist socket. Suppress only its published render scale in VR.
    // The native animation channels and held-weapon transform remain intact.
    if(count>53&&parent[53]==0&&get<uint32_t>(names+53*4)==0xec70c442)
        restore.stowedMount=get<uintptr_t>(reinterpret_cast<uintptr_t>(binding)+0x40)+53*64;
    std::memcpy(reinterpret_cast<void*>(rotations),q.data(),count*16u);
    std::memcpy(reinterpret_cast<void*>(positions),p.data(),count*16u);
    shotRig={owner,character,camera,model,nativeCamera,compose(*root,right->pose.wrist),frame.trackingSequence};
    const auto& throwingHand=frame.controllers.hands[1];
    if(throwableActive&&frame.controllers.weaponReady&&!frame.controllers.vehicleControls&&throwingHand.aimTracked){
        shotRig.palmWorld=compose(shotRig.wristWorld,inverse(gripFromWrist[1]));
        shotRig.gripFromAim=compose(inverse(throwingHand.grip),throwingHand.aim);
        shotRig.throwTracked=true;
    }
    if(++updates%120==1){
        const auto headFromModel=compose(inverse(frame.nativePose),*root);
        const auto leftView=compose(headFromModel,bone(q,p,8)).position;
        const auto rightView=compose(headFromModel,bone(q,p,12)).position;
        std::ostringstream s;s<<"Controller rig updates="<<updates<<" tracking="<<frame.trackingSequence
            <<" predicted="<<frame.controllers.predictedXrTime<<" player="<<frame.playerSequence
            <<" right_wrist="<<right->pose.wrist.position.x<<','<<right->pose.wrist.position.y<<','<<right->pose.wrist.position.z
            <<" reach_clamped="<<right->reachClamped<<" support="<<supportBlend<<" support_aim="<<aimBlend<<" articulated_hands="<<articulatedHands
            <<" support_near="<<nearSupport<<" support_distance="<<std::sqrt(dot(separation,separation))
            <<" hand_distance="<<std::sqrt(dot(handSeparation,handSeparation))
            <<" inspecting="<<inspecting<<" arm_helpers="<<helpersMatch<<" ground_contacts="<<groundContacts
            <<" left_head="<<leftView.x<<','<<leftView.y<<','<<leftView.z
            <<" right_head="<<rightView.x<<','<<rightView.y<<','<<rightView.z;log(s.str());
        if(frame.controllers.vehicleControls){
            const auto nativePalm=compose(*root,compose(leftAnimated,inverse(gripFromWrist[0])));
            const auto target=compose(frame.headPose,compose(Pose{{0,1,0,0},{}},compose(inverse(frame.nativePose),nativePalm)));
            std::ostringstream w;w<<"Driver wheel grip="<<wheel.gripped<<" axis="<<wheel.axis
                <<" local_contact="<<target.position.x<<','<<target.position.y<<','<<target.position.z;log(w.str());
        }
    }
    return true;
}
void update(void* context,void* binding){
    PoseRestore restore;
    if(enabled.load()&&headCamera().active())try{apply(context,binding,restore);}catch(...){}
    original(context,binding);
    if(restore.stowedMount){
        auto matrix=get<std::array<float,16>>(restore.stowedMount);
        if(nativeAffinePose(matrix)){
            std::fill_n(matrix.begin(),12,0.f);
            std::memcpy(reinterpret_cast<void*>(restore.stowedMount),matrix.data(),sizeof(matrix));
        }
    }
}
// The native shot solver reads the rendered wrist, then converges the shot onto
// a camera target. Supply a private copy of that target on the rendered barrel
// axis. The live weapon state, ammo, shot seed and projectile handling stay native.
void shot(void* context,Vector4* origin,Vector4* direction,void* state,uint32_t index){
    const bool firing=reinterpret_cast<uintptr_t>(_ReturnAddress())==base+0x1041219;
    auto fallback=[&]{originalShot(context,origin,direction,state,index);};
    if(!enabled.load()||!headCamera().active()){fallback();return;}
    ShotRig rig;{std::lock_guard lock(rigMutex);rig=shotRig;}
    const auto component=reinterpret_cast<uintptr_t>(context),weapon=reinterpret_cast<uintptr_t>(state);
    if(!rig.owner||get<uintptr_t>(component)!=base+0x23b3e80
       ||get<uintptr_t>(component+8)!=rig.character||get<uintptr_t>(rig.character+0x80)!=component){fallback();return;}
    const auto frame=headCamera().resolveCurrent(rig.camera,rig.sourceCamera);
    const auto instances=get<uintptr_t>(component+0x38);
    const auto first=get<uint32_t>(instances+0x24);
    const auto flags=get<uint32_t>(weapon+0x27c),mode=get<uint32_t>(weapon+0x3c0);
    if(!frame.applied||frame.playerOwner!=rig.owner||frame.trackingSequence!=rig.trackingSequence
       ||!frame.controllers.hands[1].gripTracked||get<uintptr_t>(component)!=base+0x23b3e80
       ||get<uintptr_t>(component+8)!=rig.character||get<uintptr_t>(rig.character+0x80)!=component
       ||index!=get<uint32_t>(rig.owner+0x3a0)||index<first||index-first>15
       ||weapon!=get<uintptr_t>(component+0x58)+(index-first)*0x610ull
       ||(flags&0x1c0)!=0x40||(flags&0x400000)||(mode&0x2000)){
        if(firing)log("Controller aim unavailable: unmatched rig or unsupported weapon mode; using native shot");fallback();return;
    }
    const auto render=get<uintptr_t>(rig.character+0x78),getter=get<uintptr_t>(render+0x238);
    const auto renderFirst=get<uint32_t>(getter+0xc);
    if(get<uintptr_t>(getter)!=base+0x22e6070||index<renderFirst||index-renderFirst>15
       ||get<uintptr_t>(get<uintptr_t>(getter+0x10)+(index-renderFirst)*0xc0ull+0x68)!=rig.model){fallback();return;}
    alignas(16) std::array<float,16> wristMatrix{},attachmentMatrix{},muzzleMatrix{};
    using WristGetter=bool(*)(void*,uint32_t,uint32_t,void*);
    using AttachmentGetter=uint32_t(*)(void*,void*,uint32_t);
    if(!reinterpret_cast<WristGetter>(base+0xaf0ea0)(reinterpret_cast<void*>(getter),index,12,wristMatrix.data())){fallback();return;}
    reinterpret_cast<AttachmentGetter>(base+0x1042e40)(context,attachmentMatrix.data(),index);
    if(!read(weapon+((flags&0x200)?0x80:0x40),muzzleMatrix)){fallback();return;}
    const auto wrist=nativeAffinePose(wristMatrix),attachment=nativeAffinePose(attachmentMatrix),socket=nativeAffinePose(muzzleMatrix);
    if(!wrist||!attachment||!socket){if(firing)log("Controller aim unavailable: invalid socket; using native shot");fallback();return;}
    const auto error=wrist->position-rig.wristWorld.position;
    const auto a=wrist->orientation,b=rig.wristWorld.orientation;
    const float alignment=std::abs(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w);
    if(dot(error,error)>0.0009f||alignment<0.999f){
        if(firing){std::ostringstream s;s<<"Controller aim unavailable: wrist publication differs distance="<<std::sqrt(dot(error,error))<<" orientation="<<alignment<<"; using native shot";log(s.str());}fallback();return;
    }
    const auto muzzle=compose(*wrist,compose(*attachment,*socket));
    const auto forward=rotate(muzzle.orientation,{0,0,1}); // FOX weapon sockets point along +Z.
    alignas(16) std::array<std::byte,0x610> privateState{};
    if(!read(weapon,privateState)){fallback();return;}
    Vector4 target{};std::memcpy(&target,privateState.data()+0x140,sizeof(target));
    const auto point=muzzle.position+forward*1000.f;
    target.x=point.x;target.y=point.y;target.z=point.z;
    std::memcpy(privateState.data()+0x140,&target,sizeof(target));
    originalShot(context,origin,direction,privateState.data(),index);
    const Vec3 ray{direction->x,direction->y,direction->z};
    // Keep the native solver's direction (including its convergence policy).
    // Projectiles now start at the weapon's authored muzzle socket.
    if(valid(Pose{{},ray})&&dot(ray,ray)>0.9f&&dot(ray,ray)<1.1f){
        origin->x=muzzle.position.x;origin->y=muzzle.position.y;origin->z=muzzle.position.z;
        if(firing){std::ostringstream s;s<<"Controller shot tracking="<<frame.trackingSequence<<" rig="<<frame.rigSequence
            <<" barrel_dot="<<dot(ray,forward)<<" muzzle="<<origin->x<<','<<origin->y<<','<<origin->z
            <<" direction="<<direction->x<<','<<direction->y<<','<<direction->z;log(s.str());}
    }
}
struct PendingThrow {
    void* context{};void* state{};uint32_t index{};
    uintptr_t velocityCaller{};
    uint64_t time{},tracking{},rig{};
    PointThrow solution{};float velocityW{};
    Vector4* origin{};
};
thread_local PendingThrow pendingThrow;
std::optional<PendingThrow> controllerThrow(void* context,void* state,uint32_t index,uintptr_t velocityCaller){
    if(!enabled.load()||!headCamera().active())return {};
    ShotRig rig;{std::lock_guard lock(rigMutex);rig=shotRig;}
    if(!rig.throwTracked||!rig.owner||rig.owner!=playerOwner.load())return {};
    const auto component=reinterpret_cast<uintptr_t>(context),weapon=reinterpret_cast<uintptr_t>(state);
    if(get<uintptr_t>(component)!=base+0x23b3e80||get<uintptr_t>(component+8)!=rig.character
       ||get<uintptr_t>(rig.character+0x80)!=component||get<uintptr_t>(rig.owner+0x370)!=rig.character)return {};
    const auto first=get<uint32_t>(get<uintptr_t>(component+0x38)+0x24);
    if(index!=get<uint32_t>(rig.owner+0x3a0)||index<first||index-first>15
       ||weapon!=get<uintptr_t>(component+0x58)+(index-first)*0x610ull
       ||(get<uint32_t>(weapon+0x27c)&0x1c0)!=0x80)return {};
    const auto frame=headCamera().resolveCurrent(rig.camera,rig.sourceCamera);
    if(!frame.applied||frame.playerOwner!=rig.owner||frame.trackingSequence!=rig.trackingSequence
       ||!frame.controllers.weaponReady||frame.controllers.vehicleControls
       ||!frame.controllers.hands[1].gripTracked||!frame.controllers.hands[1].aimTracked)return {};
    // This verified velocity helper reads only context+8/+60, character+78,
    // and render+8/+290 before consulting the unchanged equipment database.
    // Give it caller-owned copies with neutral pitch/yaw. Never patch the live
    // camera or shared player angles just to change grenade throw strength.
    alignas(16) std::array<std::byte,0x68> privateContext{};
    alignas(16) std::array<std::byte,0x80> privateCharacter{};
    alignas(16) std::array<std::byte,0x298> privateRender{};
    std::array<float,2> angles{};
    if(!read(component,privateContext)||!read(rig.character,privateCharacter))return {};
    const auto characterAddress=reinterpret_cast<uintptr_t>(privateCharacter.data());
    const auto renderAddress=reinterpret_cast<uintptr_t>(privateRender.data());
    const auto anglesAddress=reinterpret_cast<uintptr_t>(angles.data());
    std::memcpy(privateContext.data()+8,&characterAddress,8);
    std::memcpy(privateCharacter.data()+0x78,&renderAddress,8);
    std::memcpy(privateRender.data()+8,&index,4);
    std::memcpy(privateRender.data()+0x290,&anglesAddress,8);
    alignas(16) Vector4 velocity{};
    originalThrowVelocity(privateContext.data(),state,index,&velocity);
    const auto solution=pointThrow(rig.palmWorld,rig.gripFromAim,{velocity.x,velocity.y,velocity.z});
    if(!solution)return {};
    return PendingThrow{context,state,index,velocityCaller,steadyMilliseconds(),frame.trackingSequence,
        frame.rigSequence,*solution,velocity.w};
}
void throwOrigin(void* context,void* state,uint32_t index,Vector4* origin,Vector4* rotation){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    originalThrowOrigin(context,state,index,origin,rotation);
    pendingThrow={};
    // The two native request paths both consume origin immediately followed
    // by velocity. Keep that pair on one published rig generation and thread.
    const auto velocityCaller=caller==base+0x1061eac?base+0x1061ebe:
        caller==base+0x1087b16?base+0x1087b28:0;
    if(!velocityCaller)return;
    if(const auto request=controllerThrow(context,state,index,velocityCaller)){
        pendingThrow=*request;
        pendingThrow.origin=origin;
    }
}
void throwVelocity(void* context,void* state,uint32_t index,Vector4* velocity){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const auto request=pendingThrow;pendingThrow={};
    if(request.context!=context||request.state!=state||request.index!=index||request.velocityCaller!=caller
       ||!request.origin){originalThrowVelocity(context,state,index,velocity);return;}
    // Both verified callers retain the origin in their own request storage and
    // call velocity immediately next. Publish the pair here: an unmatched call
    // leaves the original origin intact, including after a scheduling delay.
    request.origin->x=request.solution.origin.x;request.origin->y=request.solution.origin.y;request.origin->z=request.solution.origin.z;
    *velocity={request.solution.velocity.x,request.solution.velocity.y,request.solution.velocity.z,request.velocityW};
    static std::atomic_uint64_t requests{};
    const auto count=++requests;
    if(count%120==1||caller==base+0x1087b28){
        std::ostringstream s;s<<"Controller throw requests="<<count<<" caller="<<std::hex<<caller-base<<std::dec
            <<" tracking="<<request.tracking<<" rig="<<request.rig
            <<" origin="<<request.solution.origin.x<<','<<request.solution.origin.y<<','<<request.solution.origin.z
            <<" velocity="<<velocity->x<<','<<velocity->y<<','<<velocity->z;log(s.str());
    }
}
}
namespace mgs5vr {
bool controllerThrowReady() noexcept {
    if(!enabled.load()||!headCamera().active())return false;
    std::lock_guard lock(rigMutex);
    return shotRig.throwTracked&&shotRig.owner==playerOwner.load();
}
void installControllerRig(uintptr_t imageBase){
    base=imageBase;
    const auto matches=[&]<size_t N>(uintptr_t rva,const std::array<unsigned char,N>& bytes){
        std::array<unsigned char,N> actual{};return read(base+rva,actual)&&actual==bytes;
    };
    groundQueryVerified=matches(0xa0fb50,std::array<unsigned char,9>{0x48,0x89,0x4c,0x24,0x08,0x48,0x83,0xec,0x18})
        &&matches(0x1b9b130,std::array<unsigned char,10>{0x48,0x83,0xec,0x38,0xf3,0x0f,0x10,0x44,0x24,0x60})
        &&matches(0x1b9a5d0,std::array<unsigned char,10>{0x4c,0x8b,0xdc,0x48,0x81,0xec,0x98,0,0,0})
        &&matches(0x1b99910,std::array<unsigned char,7>{0x48,0x81,0xec,0x88,0,0,0});
    log(groundQueryVerified?"Controller rig native ground-query signatures verified":"Controller rig ground contacts disabled: native query signature differs");
    constexpr std::array<unsigned char,13> expected{0x48,0x89,0x5c,0x24,0x18,0x57,0x48,0x81,0xec,0xc0,0,0,0};
    std::array<unsigned char,expected.size()> actual{};auto address=reinterpret_cast<void*>(base+0x1a6caa0);
    if(!read(base+0x1a6caa0,actual)||actual!=expected)throw std::runtime_error("Controller rig skin publication signature differs");
    auto status=MH_CreateHook(address,reinterpret_cast<void*>(&update),reinterpret_cast<void**>(&original));
    if(status!=MH_OK)throw std::runtime_error(std::string("Controller rig create: ")+MH_StatusToString(status));
    status=MH_EnableHook(address);
    if(status!=MH_OK){MH_RemoveHook(address);throw std::runtime_error(std::string("Controller rig enable: ")+MH_StatusToString(status));}
    constexpr std::array<unsigned char,10> shotExpected{0x48,0x8b,0xc4,0x55,0x56,0x57,0x41,0x56,0x41,0x57};
    std::array<unsigned char,10> shotActual{};
    if(!read(base+0x1044ff0,shotActual)||shotActual!=shotExpected)throw std::runtime_error("Controller shot solver signature differs");
    const auto shotAddress=reinterpret_cast<void*>(base+0x1044ff0);
    status=MH_CreateHook(shotAddress,reinterpret_cast<void*>(&shot),reinterpret_cast<void**>(&originalShot));
    if(status!=MH_OK)throw std::runtime_error(std::string("Controller shot create: ")+MH_StatusToString(status));
    status=MH_EnableHook(shotAddress);
    if(status!=MH_OK)throw std::runtime_error(std::string("Controller shot enable: ")+MH_StatusToString(status));
    if(!matches(0x1037ba0,std::array<unsigned char,13>{0x48,0x8b,0xc4,0x55,0x53,0x56,0x57,0x41,0x54,0x41,0x56,0x41,0x57})
       ||!matches(0x1039020,std::array<unsigned char,9>{0x48,0x8b,0xc4,0x55,0x53,0x56,0x57,0x41,0x56}))
        throw std::runtime_error("Controller throw helper signatures differ");
    const auto originAddress=reinterpret_cast<void*>(base+0x1037ba0),velocityAddress=reinterpret_cast<void*>(base+0x1039020);
    status=MH_CreateHook(originAddress,reinterpret_cast<void*>(&throwOrigin),reinterpret_cast<void**>(&originalThrowOrigin));
    if(status!=MH_OK)throw std::runtime_error(std::string("Controller throw origin create: ")+MH_StatusToString(status));
    status=MH_CreateHook(velocityAddress,reinterpret_cast<void*>(&throwVelocity),reinterpret_cast<void**>(&originalThrowVelocity));
    if(status!=MH_OK){MH_RemoveHook(originAddress);throw std::runtime_error(std::string("Controller throw velocity create: ")+MH_StatusToString(status));}
    status=MH_QueueEnableHook(originAddress);
    if(status==MH_OK)status=MH_QueueEnableHook(velocityAddress);
    if(status==MH_OK)status=MH_ApplyQueued();
    if(status!=MH_OK){MH_RemoveHook(originAddress);MH_RemoveHook(velocityAddress);throw std::runtime_error(std::string("Controller throw enable: ")+MH_StatusToString(status));}
    log("Controller throw origin and velocity adapters installed");
    installMotionMelee(base);
    enabled.store(true);log("Experimental controller rig installed at verified skin publication RVA 0x1a6caa0 and shot solver 0x1044ff0");
}
void observeControllerRigOwner(uintptr_t owner) noexcept{playerOwner.store(owner);}
void stopControllerRig() noexcept{stopMotionMelee();enabled.store(false);playerOwner.store(0);}
bool controllerRigEnabled() noexcept{return enabled.load();}
TravelMode nativeTravelMode() noexcept{
    // PlayerStatus's own Lua readers select this double-buffered local player
    // table (0x53c990 / 0x53ca40). Registration at 0x5547cc assigns ON_HORSE
    // bit 25 and ON_VEHICLE bit 26. No game status is written here.
    static std::mutex mutex;
    static TravelMode cached{TravelMode::unknown};
    static uint64_t sampledAt{};
    std::lock_guard lock(mutex);
    const auto now=steadyMilliseconds();
    if(!enabled.load())return TravelMode::unknown;
    const auto owner=playerOwner.load();
    if(get<uintptr_t>(owner)==base+0x23b8218){
        const auto index=get<uint32_t>(base+0x2bd3fd4);
        if(index<16&&get<uint32_t>(owner+0x3a0)==index)for(int attempt=0;attempt<2;++attempt){
            const auto phase=get<uint32_t>(base+0x2bd3fd0);
            if(phase>1)break;
            const auto state=base+0x2bd3ff0+(phase*16ull+index)*0xe0;
            std::array<uint32_t,5> flags{};Vec3 stateRoot{},ownerRoot{};
            if(get<uintptr_t>(state)!=base+0x2190b28||!read(state+0xc0,flags)
                ||!read(state+0x40,stateRoot)||!read(owner+0x60,ownerRoot))break;
            if(phase!=get<uint32_t>(base+0x2bd3fd0))continue;
            const auto difference=stateRoot-ownerRoot;
            if(!valid(Pose{{},difference})||dot(difference,difference)>4)break;
            const auto mode=(flags[1]&(1u<<5))?TravelMode::unknown:
                (flags[0]&(1u<<26))?TravelMode::vehicle:(flags[0]&(1u<<25))?TravelMode::horse:TravelMode::onFoot;
            if(mode!=cached){std::ostringstream s;s<<"Native travel mode="<<static_cast<int>(mode)
                <<" player="<<index<<" flags="<<std::hex<<flags[0]<<','<<flags[1];log(s.str());}
            cached=mode;sampledAt=now;return mode;
        }
    }
    return now>=sampledAt&&now-sampledAt<=150?cached:TravelMode::unknown;
}
}
