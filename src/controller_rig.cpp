#include "mgs5vr/controller_rig.hpp"
#include "mgs5vr/idroid_rig.hpp"
#include "mgs5vr/idroid_native_hold.hpp"
#include "mgs5vr/idroid_effect_probe.hpp"
#include "mgs5vr/idroid_attachment.hpp"
#include "mgs5vr/cabin_walk.hpp"
#include "mgs5vr/cabin_studio.hpp"
#include "mgs5vr/arm_ik.hpp"
#include "mgs5vr/head_camera.hpp"
#include "mgs5vr/paused_rig.hpp"
#include "mgs5vr/paused_parts.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/vehicle_weapon.hpp"
#include "mgs5vr/weapon_obstruction.hpp"
#include "mgs5vr/log.hpp"
#include "mgs5vr/motion_melee.hpp"
#include "mgs5vr/animal_interaction.hpp"
#include "mgs5vr/small_animal.hpp"
#include "mgs5vr/player_visibility.hpp"
#include "mgs5vr/optic_markers.hpp"
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
constexpr float fullBodyEyeErrorLimit=.02f;
std::mutex obstructionWitnessMutex;
WeaponObstructionWitness obstructionWitness;
// Opt-in read-only lineage for a failed native skin solve. Shares the bounded
// combat observation lease; never changes actor, camera, rig or input state.
struct SkinSolveProbe {
    uint64_t time{},activation{},tracking{},playerSequence{};
    uintptr_t owner{},camera{},model{},frameOwner{};
    Pose source{};unsigned stage{1},bodyRejection{};bool enabled{},applied{};
    ArmPose arm{};Pose target{};Vec3 hint{},eye{};ArmBasis basis{};
    ArmSurface headSurface{};unsigned armFailure{};
    SkinSolveProbe(uint64_t now,uintptr_t o,uintptr_t c,uintptr_t m)
        :time(now),owner(o),camera(c),model(m),enabled(combatSightLeaseExpiry()>now){}
    ~SkinSolveProbe(){
        if(!enabled)return;
        static thread_local std::array<uint64_t,16> times{},counts{};
        const auto i=std::min<size_t>(stage,15);++counts[i];
        if(times[i]&&time>=times[i]&&time-times[i]<250)return;
        times[i]=time;
        std::ostringstream s;s<<"Combat skin solve stage="<<stage<<" count="<<counts[i]
            <<" sample_ms="<<time<<" thread="<<GetCurrentThreadId()<<" owner="<<owner
            <<" camera="<<camera<<" model="<<model<<" frame_owner="<<frameOwner
            <<" applied="<<applied<<" activation="<<activation<<" tracking="<<tracking
            <<" player_sequence="<<playerSequence<<" body_rejection="<<bodyRejection
            <<" source_p="<<source.position.x<<','<<source.position.y<<','<<source.position.z
            <<" source_q="<<source.orientation.x<<','<<source.orientation.y<<','<<source.orientation.z<<','<<source.orientation.w;
        if(armFailure){
            const auto pose=[&](const char* name,Pose p){s<<' '<<name<<"="<<p.position.x<<','<<p.position.y<<','<<p.position.z<<','
                <<p.orientation.x<<','<<p.orientation.y<<','<<p.orientation.z<<','<<p.orientation.w;};
            s.precision(9);s<<" arm_failure="<<armFailure;
            pose("shoulder",arm.shoulder);pose("elbow",arm.elbow);pose("wrist",arm.wrist);pose("target",target);
            s<<" hint="<<hint.x<<','<<hint.y<<','<<hint.z<<" eye="<<eye.x<<','<<eye.y<<','<<eye.z
                <<" upper_axis="<<basis.upperAxis.x<<','<<basis.upperAxis.y<<','<<basis.upperAxis.z
                <<" forearm_axis="<<basis.forearmAxis.x<<','<<basis.forearmAxis.y<<','<<basis.forearmAxis.z
                <<" head_point="<<headSurface.point.x<<','<<headSurface.point.y<<','<<headSurface.point.z
                <<" head_normal="<<headSurface.normal.x<<','<<headSurface.normal.y<<','<<headSurface.normal.z
                <<" head_clearance="<<headSurface.clearance;
        }
        log(s.str());
    }
};
using Update=void(*)(void*,void*);
Update original{};
struct Vector4{float x{},y{},z{},w{};};
constexpr Quat identityQuaternion() noexcept {
    return {0.f,0.f,0.f,1.f};
}
struct PoseRestore {
    uintptr_t rotations{},positions{};
    uintptr_t stowedMount{};
    uint16_t count{};
    std::array<Quat,512> q{};
    std::array<Vector4,512> p{};
    PausedRigOwner owner{};
    std::optional<HeadCameraSample> attachmentDiagnosticFrame;
    ~PoseRestore(){
        if(count){
            std::memcpy(reinterpret_cast<void*>(rotations),q.data(),count*16u);
            std::memcpy(reinterpret_cast<void*>(positions),p.data(),count*16u);
        }
    }
};
using Shot=void(*)(void*,Vector4*,Vector4*,void*,uint32_t);
Shot originalShot{};
using WeaponObstructionQuery=bool(*)(void*,void*,uint32_t,const Vector4*,const Vector4*,float,bool);
WeaponObstructionQuery originalWeaponObstructionQuery{};
using WeaponObstructionResult=int32_t(*)(void*,void*);
WeaponObstructionResult originalWeaponObstructionResult{};
using ThrowOrigin=void(*)(void*,void*,uint32_t,Vector4*,Vector4*);
using ThrowVelocity=void(*)(void*,void*,uint32_t,Vector4*);
ThrowOrigin originalThrowOrigin{};
ThrowVelocity originalThrowVelocity{};
uintptr_t base{};
std::atomic_uintptr_t playerOwner{};
std::atomic_bool enabled{};
bool groundQueryVerified{};
bool scopeQueryVerified{};
bool scopePartsVerified{};
std::atomic_uintptr_t magnifierOwnerInterface{};
std::atomic_uint32_t magnifierOwnerIndex{};
bool modelTransformVerified{};
bool scopeTrace{};
WeaponScopeZoom scopeZoom;
ScopePoseStabilizer scopeAim;
std::mutex rigMutex;
// A native skin call may recursively publish an attached model. Keep the
// player's solved channels installed until that complete publication returns.
std::recursive_mutex skinPublicationMutex;
struct PausedSkin {
    PausedRigOwner owner{};
    uintptr_t driver{},binding{},skeleton{},model{},matrices{},parents{};
    uint16_t count{};
} pausedSkin;
struct PausedPart {
    uintptr_t model{},matrices{},namesAddress{},parentsAddress{};
    uint16_t count{};
    std::array<uint32_t,512> names{};
    std::array<int32_t,512> parents{};
    std::array<Pose,512> frozen{};
};
std::array<PausedPart,4> pausedParts;
uint64_t pausedSkinRefreshes{};
uintptr_t boundOwner{};
uintptr_t boundModel{};
uint64_t activation{},updates{};
std::array<MotionMelee,3> meleeMotion;
uint64_t meleeTracking{};
std::array<float,2> meleeCurl{};
std::array<WheelSteering,2> wheelSteering;
float wheelSteeringAxis{};
std::array<Vec3,2> bendHistory{};
SupportContact supportContact;
SupportPose supportPose;
float supportBlend{};
float aimBlend{};
Quat guidedOffset=identityQuaternion();
Pose heldSupportOffset{};
uint64_t supportWeapon{};
bool heldCloseSupport{};
uint64_t supportAt{};
std::array<HandPresentationCache,2> handPresentation;
StationaryBodyState stationaryBody;
BodyHeadingState bodyHeading;
uintptr_t legFitOwner{},legFitModel{};
float standingLegScale=1.f;
bool legFitCalibrated{},legFitSeated{};
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
bool vehicleWeaponScope(const HeadCameraSample& frame){
    return frame.controllers.vehicleControls||frame.controllers.vehicleMounted;
}
bool vehicleWeaponAcknowledged(const HeadCameraSample& frame,uint64_t now){
    const auto& c=frame.controllers;
    if(!c.vehicleControls||!c.vehicleWeaponIntent||!c.weaponReady||c.rightWheelGrip
       ||c.nativeGamepad||!c.presentationFocused||c.equipmentOpen||c.commandControls
       ||frame.menuOpen||!c.hands[1].gripTracked||!c.hands[1].aimTracked
       ||now<frame.sampleTime||now-frame.sampleTime>150)return false;
    const auto native=vehicleWeaponState();
    return native.requested&&native.ready&&native.nativeEligible
        &&native.player==frame.playerOwner&&native.player==playerOwner.load()
        &&native.activation==frame.activation&&native.time&&now>=native.time&&now-native.time<=150;
}
struct AssembledSight {
    Pose center{},magnifier{};
    uint64_t centerCode{},magnifierCode{};
    uintptr_t magnifierInterface{},magnifierModel{};
    uint32_t magnifierIndex{};
};
std::optional<AssembledSight> assembledSight(uintptr_t component,uint32_t resource){
    const auto equipment=(resource>>5)&0x7ffu;
    if(!scopePartsVerified||equipment<871||equipment>876)return {};
    const auto service=get<uintptr_t>(component+0x60);
    if(get<uintptr_t>(service)!=base+0x234b840)return {};
    const auto custom=get<uintptr_t>(service+0xf0);
    if(get<uintptr_t>(custom)!=base+0x233ad90
       ||get<uintptr_t>(base+0x233ad90+0x10)!=base+0xd45930)return {};
    const uint32_t weaponIndex=equipment<873?(equipment-871)*3:
        equipment<875?(equipment-873)*3+1:(equipment-875)*3+2;
    using Resolve=uintptr_t(*)(void*,uint32_t,uint32_t,uint32_t*);
    const auto resolve=reinterpret_cast<Resolve>(base+0xd45930);
    const auto transforms=get<uintptr_t>(custom+0x40);
    AssembledSight result;
    for(uint32_t part:{6u,7u}){
        uint32_t index{};
        const auto interface=resolve(reinterpret_cast<void*>(custom),weaponIndex,part,&index);
        if(get<uintptr_t>(interface)!=base+0x22e2970||index>=get<uint32_t>(interface+0x50)||index>=128)continue;
        const auto models=get<uintptr_t>(interface+0x58),model=get<uintptr_t>(models+static_cast<uintptr_t>(index)*8);
        if(get<uintptr_t>(model)!=base+0x20f4d90)continue;
        const auto code=get<uint64_t>(model+0x120);
        const auto count=get<uint16_t>(model+0xf8);
        const auto names=get<uintptr_t>(model+0xe8);
        const bool center=code==0x84a35edc984905ceull&&count==1;
        const bool magnifier=(code==0x84a1df03c6484ee1ull||code==0x84a2233aa7f98e18ull)&&count==2;
        if((!center&&!magnifier)||get<uint32_t>(names)!=0xf08b256e
           ||(magnifier&&get<uint32_t>(names+4)!=0x6ef58063))continue;
        std::array<float,16> root{};
        if(!read(transforms+weaponIndex*0x2c0ull+(part-1)*64ull,root))continue;
        const auto pose=nativeAffinePose(root);if(!pose)continue;
        if(center){result.center=*pose;result.centerCode=code;}
        else{result.magnifier=*pose;result.magnifierCode=code;
            result.magnifierInterface=interface;result.magnifierModel=model;result.magnifierIndex=index;}
        if(get<uintptr_t>(models+static_cast<uintptr_t>(index)*8)!=model)return {};
    }
    if(!result.centerCode||get<uintptr_t>(service+0xf0)!=custom
       ||get<uintptr_t>(component+0x60)!=service)return {};
    return result;
}
uint32_t refreshPausedArmParts(const PausedSkin& body,uintptr_t character){
    if(!modelTransformVerified)return 0;
    // Retail's prosthetic is a separate, player-owned modular model. Its
    // named forearm/wrist/finger matrices normally match the body's palette;
    // the native parts update stops during Pause even when body skin is fresh.
    constexpr uint32_t armGroup=0x4e74fd8c,leftForearm=0x31e2de16,leftWrist=0x3e2d77e5;
    const auto parts=get<uintptr_t>(character+0x610);
    if(get<uintptr_t>(parts)!=base+0x22e56c0||get<uintptr_t>(parts+0x38)!=character)return 0;
    const auto list=get<uintptr_t>(parts+0x48);
    if(get<uintptr_t>(list)!=base+0x2215c78)return 0;
    const auto records=get<uintptr_t>(list+8);const auto count=get<uint32_t>(list+0x10);
    // The animation skeleton and render palette have different counts (178
    // versus 132 for the observed player). The modular model is joined by
    // render-palette names, so its source span must use the model's count.
    const auto sourceCount=get<uint16_t>(body.model+0xf8);
    if(!records||!count||count>32||!sourceCount||sourceCount>512
       ||(get<uint8_t>(body.model+0xfa)&1)==0)return 0;
    std::array<uint32_t,512> sourceNames{};
    std::array<Pose,512> source{};
    const auto nameData=get<uintptr_t>(body.model+0xe8);
    if(!nameData||!body.matrices)return 0;
    for(size_t i=0;i<sourceCount;++i){
        std::array<float,16> matrix{};
        if(!read(nameData+i*4,sourceNames[i])||!read(body.matrices+i*64,matrix))return 0;
        // Unrelated hidden mounts deliberately collapse their matrix. Retain
        // an invalid sentinel so a part referencing that joint is rejected,
        // without blocking the prosthetic on a holstered weapon's transform.
        const auto pose=nativeAffinePose(matrix);
        source[i]=pose?*pose:Pose{{0,0,0,0},{}};
    }
    const auto hasSource=[&](uint32_t name){return std::find(sourceNames.begin(),sourceNames.begin()+sourceCount,name)!=sourceNames.begin()+sourceCount;};
    if(!hasSource(leftForearm)||!hasSource(leftWrist))return 0;
    const auto bodyRoot=nativeAffinePose(get<std::array<float,16>>(body.model+0x40));
    if(!bodyRoot)return 0;
    uint32_t refreshed{};
    for(uint32_t entry=0;entry<count;++entry){
        const auto record=get<uintptr_t>(records+entry*8),renderer=get<uintptr_t>(record);
        if(get<uintptr_t>(renderer)!=base+0x20f9460)continue;
        const auto model=get<uintptr_t>(renderer+0x40);
        if(model==body.model||get<uintptr_t>(model)!=base+0x20f4d90)continue;
        const auto groupCount=get<uint16_t>(model+0x1e8);
        const auto groups=get<uintptr_t>(model+0x180);
        if(!groupCount||groupCount>128||!groups)continue;
        bool arm=false;
        for(size_t g=0;g<groupCount;++g)if(get<uint32_t>(groups+g*4)==armGroup)arm=true;
        const auto boneCount=get<uint16_t>(model+0xf8);
        if(!arm||!boneCount||boneCount>512||(get<uint8_t>(model+0xfa)&1)==0)continue;
        alignas(16) const auto root=get<std::array<float,16>>(model+0x40);
        const auto partRoot=nativeAffinePose(root);if(!partRoot)continue;
        const auto delta=partRoot->position-bodyRoot->position;
        const auto a=partRoot->orientation,b=bodyRoot->orientation;
        if(dot(delta,delta)>.001f*.001f||std::abs(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w)<.99999f)continue;
        const auto matrices=get<uintptr_t>(model+0xe0),names=get<uintptr_t>(model+0xe8),parents=get<uintptr_t>(model+0xf0);
        if(!matrices||!names||!parents)continue;
        auto cached=std::find_if(pausedParts.begin(),pausedParts.end(),[&](const auto& p){return p.model==model;});
        if(cached==pausedParts.end())cached=std::find_if(pausedParts.begin(),pausedParts.end(),[](const auto& p){return !p.model;});
        if(cached==pausedParts.end())continue;
        if(cached->model&&(cached->matrices!=matrices||cached->namesAddress!=names||cached->parentsAddress!=parents||cached->count!=boneCount))*cached={};
        bool consistent=true;
        for(size_t i=0;i<boneCount;++i){
            const auto name=get<uint32_t>(names+i*4);const auto parent=get<int32_t>(parents+i*0x20);
            if(cached->model){if(cached->names[i]!=name||cached->parents[i]!=parent){consistent=false;break;}}
            else {
                const auto pose=nativeAffinePose(get<std::array<float,16>>(matrices+i*64));
                if(!pose){consistent=false;break;}
                cached->names[i]=name;cached->parents[i]=parent;cached->frozen[i]=*pose;
            }
        }
        if(!consistent){*cached={};continue;}
        const auto hasPart=[&](uint32_t name){return std::find(cached->names.begin(),cached->names.begin()+boneCount,name)!=cached->names.begin()+boneCount;};
        if(!hasPart(leftForearm)||!hasPart(leftWrist)){*cached={};continue;}
        cached->model=model;cached->matrices=matrices;cached->namesAddress=names;cached->parentsAddress=parents;cached->count=boneCount;
        std::array<Pose,512> solved{};
        if(!retargetPausedPart({sourceNames.data(),sourceCount},{source.data(),sourceCount},
            {cached->names.data(),boneCount},{cached->frozen.data(),boneCount},
            {cached->parents.data(),boneCount},{solved.data(),boneCount}))continue;
        using SetTransform=void(*)(void*,const float*);
        reinterpret_cast<SetTransform>(base+0x1ce7f0)(reinterpret_cast<void*>(model),root.data());
        for(size_t i=0;i<boneCount;++i){
            const auto& p=solved[i];const auto x=rotate(p.orientation,{1,0,0}),y=rotate(p.orientation,{0,1,0}),z=rotate(p.orientation,{0,0,1});
            const std::array<float,16> matrix{x.x,x.y,x.z,0,y.x,y.y,y.z,0,z.x,z.y,z.z,0,p.position.x,p.position.y,p.position.z,1};
            std::memcpy(reinterpret_cast<void*>(matrices+i*64),matrix.data(),sizeof(matrix));
        }
        ++refreshed;
    }
    return refreshed;
}
bool ownsSkinTarget(void* context,void* binding){
    const auto owner=playerOwner.load();
    if(!owner||get<uintptr_t>(owner)!=base+0x23b8218)return false;
    const auto character=get<uintptr_t>(owner+0x370);
    if(get<uintptr_t>(character)!=base+0x2295210)return false;
    const auto component=get<uintptr_t>(character+0x10),holder=get<uintptr_t>(component+0x10);
    const auto model=get<uintptr_t>(holder+0x68);
    return model&&get<uintptr_t>(model)==base+0x20f4d90
        &&reinterpret_cast<uintptr_t>(binding)==model+0xa0
        &&get<uintptr_t>(holder+0x70)==reinterpret_cast<uintptr_t>(context);
}
std::optional<ArmSurface> groundSurface(Vec3 point,float ceiling,float clearance,float depth=.75f){
    // TPP 1.0.15.4 HumanGroundIk uses this synchronous, internally read-locked
    // query. The 0x800 inclusion layer is set by the player IK factory. Query
    // storage and hit records are caller-owned; no native state is modified.
    if(!groundQueryVerified||!valid(Pose{{},point})||!std::isfinite(ceiling)
       ||point.y>ceiling||ceiling-point.y>3.f||!std::isfinite(depth)||depth<.75f||depth>2.f
       ||!get<uintptr_t>(base+0x2c79710))return {};
    alignas(16) std::array<std::byte,0x250> query{};
    using Init=void*(*)(void*,uint32_t);
    using Ray=uint32_t(*)(void*,uint32_t,const Vector4*,const Vector4*,float);
    using HitValue=void*(*)(const void*,Vector4*);
    reinterpret_cast<Init>(base+0xa0fb50)(query.data(),0);
    const uint64_t groundLayers=0x800;
    std::memcpy(query.data(),&groundLayers,sizeof(groundLayers));
    const uint64_t filter=0x80000006;
    std::memcpy(query.data()+0x18,&filter,sizeof(filter));
    alignas(16) Vector4 start{point.x,ceiling,point.z,0},end{point.x,point.y-depth,point.z,0};
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
struct NativeCollision {bool available{};std::optional<Vec3> point;Vec3 normal{};};
NativeCollision collisionCast(Vec3 start,Vec3 end,float radius=0){
    if(!groundQueryVerified||!valid(Pose{{},start})||!valid(Pose{{},end})
       ||!get<uintptr_t>(base+0x2c79710))return {};
    alignas(16) std::array<std::byte,0x250> query{};
    using Init=void*(*)(void*,uint32_t);
    using Ray=uint32_t(*)(void*,uint32_t,const Vector4*,const Vector4*,float);
    using HitValue=void*(*)(const void*,Vector4*);
    reinterpret_cast<Init>(base+0xa0fb50)(query.data(),0);
    const uint64_t layers=0x800,filter=0x80000006;
    std::memcpy(query.data(),&layers,sizeof(layers));
    std::memcpy(query.data()+0x18,&filter,sizeof(filter));
    alignas(16) const Vector4 from{start.x,start.y,start.z,0},to{end.x,end.y,end.z,0};
    // Native query radius is copied to the shape's +0x20 radius vector by
    // 1B9B160. Zero is the existing ray; positive radii sweep a sphere.
    if(!reinterpret_cast<Ray>(base+0x1b9b130)(query.data(),0x04000700,&from,&to,radius))return {true,{},{}};
    const auto address=reinterpret_cast<uintptr_t>(query.data());
    const auto count=get<uint32_t>(address+0x60);
    const auto index=get<int32_t>(address+0x64);
    if(!count||count>5||index<0||static_cast<uint32_t>(index)>=count)return {};
    alignas(16) Vector4 hit{},normal{};const auto record=query.data()+0x70+index*0x60;
    reinterpret_cast<HitValue>(base+0x1b9a5d0)(record,&hit);
    reinterpret_cast<HitValue>(base+0x1b99910)(record,&normal);
    const Vec3 result{hit.x,hit.y,hit.z};
    return valid(Pose{{},result})?NativeCollision{true,result,{normal.x,normal.y,normal.z}}:NativeCollision{};
}
std::optional<Vec3> collisionRay(Vec3 start,Vec3 end){return collisionCast(start,end).point;}
std::optional<Vec3> moveNativeCabin(Vec3 from,Vec3 to){
    return resolveCabinMovement(from,to,[](Vec3 start,Vec3 end){
        CabinSweep result{true,false};
        const auto delta=end-start;const float squared=dot(delta,delta);
        // Camera clearance, not the native player capsule: one swept head
        // sphere and one shoulder sphere retain the seated cabin viewpoint.
        for(const auto shape:std::array<std::array<float,2>,2>{{{0,.12f},{-.30f,.18f}}}){
            const Vec3 offset{0,shape[0],0};const float radius=shape[1];
            const auto hit=collisionCast(start+offset,end+offset,radius);
            if(!hit.available)return CabinSweep{};
            if(!hit.point)continue;
            const float normalLength=std::sqrt(dot(hit.normal,hit.normal));
            if(!std::isfinite(normalLength)||normalLength<.5f)return CabinSweep{};
            const auto normal=hit.normal*(1.f/normalLength);
            // The native getter returns surface contact, not sphere center.
            const auto center=*hit.point+normal*radius;
            const float fraction=dot(center-start-offset,delta)/std::max(squared,1e-8f);
            // A surface behind the motion cannot block withdrawal/slide.
            if(dot(delta,normal)>=-.00001f)continue;
            if(!result.hit||fraction<result.fraction)result={true,true,fraction,normal};
        }
        return result;
    },[](Vec3 point){
        const auto floor=collisionCast(point+Vec3{0,.05f,0},point-Vec3{0,2.f,0});
        return floor.available&&floor.point&&floor.normal.y>.5f
            &&floor.point->y<point.y-.25f;
    });
}
NativeRoomBounds measureNativeCabin(Pose origin,uint64_t generation,NativeRoomSamples& samples){
    const auto forward=rotate(origin.orientation,{0,0,1});
    if(!std::isfinite(forward.x)||!std::isfinite(forward.z)
       ||forward.x*forward.x+forward.z*forward.z<.0001f)return {};
    const auto yaw=std::atan2(forward.x,forward.z);
    const Pose roomOrigin{{0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)},origin.position};
    NativeRoomBounds result;result.origin=roomOrigin;result.sceneGeneration=generation;
    if(!valid(roomOrigin))return result;
    const auto world=[&](Vec3 local){return compose(roomOrigin,Pose{{},local}).position;};
    constexpr float reach=4.f;
    const auto trace=[&](Vec3 direction)->std::optional<float>{
        const auto hit=collisionRay(world({}),world(direction*reach));
        if(!hit)return {};
        const auto local=compose(inverse(roomOrigin),Pose{{},*hit}).position;
        const float distance=dot(local,direction);
        return std::isfinite(distance)&&distance>.08f&&distance<reach-.02f
            ?std::optional<float>{distance}:std::nullopt;
    };
    // Always sample floor/ceiling, even if an open doorway misses a wall ray.
    // Incomplete samples must not authorize a fabricated enclosing room.
    constexpr std::array<Vec3,6> directions{{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}};
    for(size_t i=0;i<directions.size();++i)samples[i]=trace(directions[i]);
    return nativeRoomBoundsFromSamples(roomOrigin,generation,samples,reach);
}
Pose bone(const std::array<Quat,512>& q,const std::array<Vector4,512>& p,size_t i){return {q[i],{p[i].x,p[i].y,p[i].z}};}
std::optional<Pose> nativeIdroidMount(uintptr_t driver){
    // Live retail comparison: idr0_main0_def's root equals this animated
    // right-hand CNP in wrist space (not the anatomical palm). The asset's
    // CNP_CONNECTOR is a separate grip contact; its hologram is another socket.
    const auto count=get<uint32_t>(driver+0x38);
    const auto points=get<uintptr_t>(driver+0x40);
    if(!points||!count||count>128)return {};
    for(uint32_t i=0;i<count;++i){
        const auto point=get<uintptr_t>(points+i*8);
        if(get<uintptr_t>(point)!=base+0x24c37e8
           ||!matchesNativeIdroidMount(get<uint64_t>(point+0x10),get<uint64_t>(point+0xa0),get<uint16_t>(point+0xa8)))continue;
        std::array<float,16> matrix{};
        if(read(point+0x20,matrix))return nativeAffinePose(matrix);
    }
    return {};
}
void replace(std::array<Quat,512>& q,std::array<Vector4,512>& p,size_t i,Pose value){
    q[i]=value.orientation;p[i].x=value.position.x;p[i].y=value.position.y;p[i].z=value.position.z;
}
Quat blendRotation(Quat a,Quat b,float weight){
    weight=std::clamp(weight,0.f,1.f);
    const float aLengthSquared=a.x*a.x+a.y*a.y+a.z*a.z+a.w*a.w;
    const float bLengthSquared=b.x*b.x+b.y*b.y+b.z*b.z+b.w*b.w;
    constexpr float epsilon=1e-12f;
    if(!std::isfinite(aLengthSquared)||aLengthSquared<=epsilon)a=identityQuaternion();
    if(!std::isfinite(bLengthSquared)||bLengthSquared<=epsilon)b=identityQuaternion();
    if(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w<0)b={-b.x,-b.y,-b.z,-b.w};
    Quat q{a.x+(b.x-a.x)*weight,a.y+(b.y-a.y)*weight,a.z+(b.z-a.z)*weight,a.w+(b.w-a.w)*weight};
    const float lengthSquared=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
    if(!std::isfinite(lengthSquared)||lengthSquared<=epsilon)return identityQuaternion();
    const float length=std::sqrt(lengthSquared);
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
    SkinSolveProbe probe(now,owner,camera,model);
    const auto driver=reinterpret_cast<uintptr_t>(context),skeleton=get<uintptr_t>(driver+0x10);
    if(get<uintptr_t>(holder+0x70)!=driver||get<uintptr_t>(driver)!=base+0x24c1988||!skeleton)return false;
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
    probe.stage=2;
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
    probe.stage=3;probe.source=nativeCamera;
    std::optional<NativeBodyHead> nativeHead;
    const auto nativeHeadNames=get<uintptr_t>(model+0xe8);
    if(nativeHeadNames&&get<uint32_t>(nativeHeadNames+4*sizeof(uint32_t))==0xc3a88a1b
       &&parent[0]==-1&&parent[1]==0&&parent[2]==1&&parent[3]==2&&parent[4]==3){
        // Q/P still contains the native animation under skinPublicationMutex.
        // The owner's cached HEAD affine has a different prone eye basis.
        nativeHead=NativeBodyHead{owner,model,compose(*root,bone(q,p,4)),*root,
            compose(*root,bone(q,p,2)),true};
    }
    auto frame=headCamera().resolveCurrentForRig(camera,nativeCamera,nativeHead);
    probe.applied=frame.applied;probe.frameOwner=frame.playerOwner;probe.activation=frame.activation;
    probe.tracking=frame.trackingSequence;probe.playerSequence=frame.playerSequence;
    frame.nativeBodySourceTracked=false;
    if(frame.applied&&frame.playerOwner==owner&&nativeHead&&frame.anatomicalEyeModel==model){
        frame.nativeBodySourceTracked=true;
        frame.nativeBodySourceRoot=nativeHead->worldFromRoot;
        frame.nativeBodySourceChest=nativeHead->worldFromChest;
        frame.nativeBodySourceHead=nativeHead->worldFromHead;
        frame.nativeBodySourceEye=compose(nativeHead->worldFromHead,Pose{{},nativeEyeFromHead});
    }
    if(frame.applied&&frame.controllers.scriptedDemo){
        // The camera is an observer during a movie, not Snake's body anchor.
        // Leave the entire authored actor skin in its native animation frame.
        if(nativeHead)publishAuthoredPlayerHead(owner,model,nativeHead->worldFromHead,frame.activation);
        updateAuthoredPlayerVisibility();
        publishFullBodyVisibility(owner,model,false);
        if(frame.playerOwner==owner){
            frame.controllers.hands={};frame.controllers.optic={};frame.controllers.weaponReady=frame.controllers.supportGrip=false;
            return headCamera().publishRigFrame(camera,owner,nativeCamera,frame);
        }
        return false;
    }
    if(!frame.applied||frame.playerOwner!=owner||frame.controllers.avatarEditor
       ||(frame.controllers.frontEnd&&!frame.controllers.openingSelector&&!frame.controllers.cabinPlay))return false;
    const bool vehicleWeaponReady=vehicleWeaponAcknowledged(frame,now);
    probe.stage=4;
    if(vehicleWeaponScope(frame)&&!vehicleWeaponReady)
        frame.controllers.weaponReady=frame.controllers.supportGrip=false;
    frame.weaponRig={};
    // Cabin locomotion belongs to the tracked head. A temporarily occluded
    // right controller must not publish an unshifted camera for one frame and
    // snap back to the walking offset when its tracking returns.
    publishNativeCabinBounds(frame);
    const bool rocketIdroid=frame.rocketArmGuidance&&!frame.menuOpen;
    const auto idroidMount=(frame.menuIdroid||rocketIdroid)?nativeIdroidMount(driver):std::nullopt;
    const bool idroidHeld=((frame.menuOpen&&frame.menuIdroid)||rocketIdroid)&&frame.controllers.handheldMenus
        &&!frame.controllers.handheldIdroidStowed
        &&!frame.menuWorldQuad&&(idroidMount.has_value()||rocketIdroid)&&!frame.controllers.nativeGamepad;
    const auto names=get<uintptr_t>(model+0xe8);
    std::array<uint32_t,512> boneNames{};
    const bool namesRead=names&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(names),
        boneNames.data(),count*sizeof(uint32_t),&copied)&&copied==count*sizeof(uint32_t);
    if(idroidHeld&&(!namesRead||!matchesNativeIdroidLeftHand({boneNames.data(),count},{parent.data(),count})))return false;
    std::lock_guard lock(rigMutex);
    const RigContinuityKey continuity{owner,model,frame.activation,frame.controllers.referenceEpoch,
        frame.controllers.presentationEpoch};
    if(legFitOwner!=owner||legFitModel!=model||legFitSeated!=frame.controllers.seatedMode){
        legFitOwner=owner;legFitModel=model;legFitSeated=frame.controllers.seatedMode;
        standingLegScale=1.f;legFitCalibrated=false;
    }
    std::array<std::optional<HandPresentationPose>,2> presentationHands;
    std::array<std::optional<Pose>,2> presentationGrips;
    if(frame.controllers.nativeGamepad){
        for(auto& cache:handPresentation)cache.reset();
        stationaryBody.reset();
    }else for(size_t side=0;side<2;++side){
        const auto& hand=frame.controllers.hands[side];
        presentationHands[side]=handPresentation[side].update({hand.grip,hand.aim},
            hand.gripTracked&&hand.aimTracked,frame.controllers.presentationFocused,frame.sampleTime,continuity);
        if(presentationHands[side])presentationGrips[side]=presentationHands[side]->grip;
    }
    if(!frame.controllers.presentationFocused)stationaryBody.reset();
    if(frame.controllers.nativeGamepad||!presentationGrips[1]){
        scopeAim.reset();
        // Missing presentation does not invalidate a tracked headset. Publish
        // this unchanged native skin with its actual camera transaction; no
        // stale tracked weapon, palm, scope or wrist UI may accompany it.
        frame.controllers.hands={};frame.controllers.optic={};
        frame.controllers.weaponReady=frame.controllers.supportGrip=false;
        if(frame.controllers.nativeGamepad){
            // Real gamepads keep the game's authored hands and weapon poses.
            // The wrist HUD and iDroid can still use those actual skin joints.
            for(size_t side=0;side<2;++side){
                const auto palm=anatomicalGrip(bone(q,p,side?12:8),bone(q,p,side?40:24).position,bone(q,p,side?50:34).position);
                if(palm){frame.renderedPalms[side]=compose(*root,*palm);frame.renderedPalmTracked[side]=true;}
            }
            const auto elbow=compose(*root,bone(q,p,7)),wrist=compose(*root,bone(q,p,8));
            if(const auto panel=forearmPanel(elbow,wrist,rotate(wrist.orientation,{0,1,0}),frame.controllers.wristSurfaceLift)){
                frame.wristPanel=*panel;frame.wristPanelTracked=true;
            }
            if(idroidMount){
                frame.idroidDevice=compose(*root,compose(bone(q,p,12),*idroidMount));
                frame.idroidDeviceTracked=valid(frame.idroidDevice);
            }
        }
        return headCamera().publishRigFrame(camera,owner,nativeCamera,frame);
    }
    const auto renderedHead=compose(*root,bone(q,p,4)).position;
    probe.stage=5;
    const auto originalQ=q;const auto originalP=p;
    std::array<Pose,2> propInputPalms{frame.controllers.hands[0].grip,frame.controllers.hands[1].grip};
    std::array<bool,2> propHands{};
    if(const auto& studio=frame.controllers.studio;studio&&studio->available){
        for(size_t side=0;side<2;++side){
            propHands[side]=studio->cameraHand==static_cast<int>(side)||studio->markerHand==static_cast<int>(side)
                ||std::any_of(studio->papers.begin(),studio->papers.end(),[&](const auto& paper){return paper.hand==static_cast<int>(side);});
            if(studio->cameraHand==static_cast<int>(side))propInputPalms[side]=studioCameraPalm(studio->camera,side);
        }
    }
    std::array<Pose,2> grips{};
    for(size_t i=0;i<2;++i)if(presentationGrips[i])
        grips[i]=compose(inverse(*root),nativeTrackedPose(frame.nativePose,frame.headPose,
            propHands[i]?propInputPalms[i]:*presentationGrips[i]));
    if(frame.controllers.optic.held&&frame.controllers.optic.pose.tracked){
        // Acquire the housing's authored PALM contact. Reusing a gun-like raw
        // controller grip leaves the fingers pointing along the tube instead
        // of enclosing it, despite an apparently correct optical aim ray.
        const auto safe=binocularFaceSafeGrip(frame.headPose,frame.controllers.optic.pose.primaryGrip,frame.controllers.optic.pose);
        grips[1]=compose(inverse(*root),nativeTrackedPose(frame.nativePose,frame.headPose,safe));
    }
    const auto rootInverse=inverse(*root);
    const auto eyeInModel=compose(rootInverse,Pose{{},frame.nativePose.position}).position;
    const float groundCeiling=frame.nativePose.position.y+.05f;
    unsigned groundContacts{},headContacts{};
    const auto modelSurface=[&](Vec3 point,float clearance,bool footQuery=false,float footLift=0)->std::optional<ArmSurface>{
        const auto world=compose(*root,Pose{{},point}).position;
        // An authored prone ankle can be higher than the low viewer. Start
        // its ray above the placed foot without relaxing hand/head queries.
        const float ceiling=footQuery?std::max(groundCeiling,world.y+.05f):groundCeiling;
        const auto surface=groundSurface(world,ceiling,clearance,
            footQuery?std::clamp(.75f+std::max(0.f,footLift),.75f,2.f):.75f);
        if(!surface)return {};
        return ArmSurface{compose(rootInverse,Pose{{},surface->point}).position,
            rotate(rootInverse.orientation,surface->normal),clearance};
    };
    const auto clearWrist=[&](Pose target){
        if(const auto surface=headClearanceSurface(target.position,eyeInModel,.14f)){
            target.position=*outsideArmSurface(target.position,*surface);++headContacts;
        }
        if(const auto surface=modelSurface(target.position,.055f))
            if(const auto point=outsideArmSurface(target.position,*surface)){
                if(dot(*point-target.position,*point-target.position)>1e-8f)++groundContacts;
                target.position=*point;
            }
        return target;
    };
    const auto groundedArm=[&](const ArmPose& animated,Pose target,Vec3 hint,const ArmBasis& basis){
        if(probe.enabled){probe.arm=animated;probe.target=target;probe.hint=hint;probe.basis=basis;probe.eye=eyeInModel;probe.armFailure=0;}
        auto solution=solveArm(animated,target,hint,&basis);
        if(probe.enabled&&!solution)probe.armFailure=1;
        if(solution)if(const auto surface=modelSurface(solution->pose.elbow.position,.06f)){
            if(dot(solution->pose.elbow.position-surface->point,surface->normal)<surface->clearance){
                if(const auto contact=solveArm(animated,target,hint,&basis,&*surface)){
                    solution=contact;++groundContacts;
                }
            }
        }
        if(solution)if(const auto surface=headClearanceSurface(solution->pose.elbow.position,eyeInModel,.12f)){
            const auto contact=solveArm(animated,target,hint,&basis,&*surface);
            if(!contact){if(probe.enabled){probe.armFailure=2;probe.headSurface=*surface;}return std::optional<ArmSolution>{};}solution=contact;++headContacts;
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
    if(idroidHeld){
        const auto wrist=nativeIdroidLeftWristFromGrip();
        if(!wrist)return false;
        gripFromWrist[0]=*wrist;
    }
    if(boundOwner!=owner||boundModel!=model||activation!=frame.activation){
        for(size_t i=0;i<2;++i){
            bendHistory[i]=bone(q,p,wrists[i]-1).position-bone(q,p,shoulders[i]).position;
        }
        boundOwner=owner;boundModel=model;activation=frame.activation;
        supportContact.reset();supportPose.reset();
        supportBlend=0;aimBlend=0;guidedOffset=identityQuaternion();heldSupportOffset={};supportAt=now;
        supportWeapon=0;heldCloseSupport=false;
        for(auto& motion:meleeMotion)motion.reset();
        meleeTracking=0;meleeCurl={};
        for(auto& hand:wheelSteering)hand.reset();
        wheelSteeringAxis=0;wheelMailbox().publish({});
        log("Controller rig uses native anatomical palm frames; no activation-pose wrist calibration");
    }
    const auto rightAnimated=bone(q,p,12),leftAnimated=bone(q,p,8);
    probe.stage=6;
    WheelSample wheel{};wheel.time=frame.sampleTime;
    unsigned wheelHands{};
    for(size_t side=0;side<2;++side){
        const auto animated=side?rightAnimated:leftAnimated;
        const auto nativeContact=compose(*root,compose(animated,inverse(gripFromWrist[side])));
        const auto localContact=compose(frame.headPose,compose(Pose{{0,1,0,0},{}},
            compose(inverse(frame.nativePose),nativeContact)));
        const auto& hand=frame.controllers.hands[side];
        // The right palm can join only while empty. Its squeeze owner is
        // selected by the input layer from this same authored contact.
        const bool available=frame.controllers.vehicleControls&&hand.gripTracked
            &&(!side||(!frame.controllers.weaponReady&&!frame.controllers.vehicleWeaponIntent));
        const auto separation=hand.grip.position-localContact.position;
        if(available&&valid(localContact)&&dot(separation,separation)<.04f)
            wheel.nearHands|=static_cast<uint8_t>(1u<<side);
        const bool squeeze=side?frame.controllers.rightWheelGrip:frame.controllers.wheelGrip;
        const auto sample=wheelSteering[side].update(hand.grip,localContact,available,squeeze,
            frame.sampleTime,frame.activation,rotate(frame.headPose.orientation,{0,0,-1}),35.f,wheelSteeringAxis);
        if(frame.controllers.vehicleControls)frame.driverWheelGrips[side]=localContact;
        if(sample.gripped){
            ++wheelHands;wheel.axis+=sample.axis;wheel.hands|=static_cast<uint8_t>(1u<<side);
            wheel.engaged=wheel.engaged||sample.engaged;
        }
    }
    wheel.gripped=wheelHands!=0;
    if(wheelHands)wheel.axis/=static_cast<float>(wheelHands);
    wheelSteeringAxis=wheel.axis;
    if(frame.controllers.vehicleControls){
        frame.driverWheelTracked=valid(frame.driverWheelGrips[0])&&valid(frame.driverWheelGrips[1]);
        frame.driverWheelHeld=wheel.gripped;frame.driverWheelHands=wheel.hands;frame.driverWheelAxis=wheel.axis;
    }
    {
        // The arm meshes also carry spine and clavicle weights. Reposition
        // the whole upper body with one rigid transform: independent shoulder
        // offsets leave those shared sleeve vertices in different body frames.
        // Neck look and a body turn are separate. The same camera transaction
        // retains locomotion/snap heading before applying physical HMD look.
        auto forward=rotate(frame.nativePose.orientation,{0,0,1});
        if(forward.x*forward.x+forward.z*forward.z<.0001f)
            forward=rotate(nativeCamera.orientation,{0,0,1});
        const float yaw=std::atan2(forward.x,forward.z);
        Quat torso{0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)};
        const auto travelMode=nativeTravelMode();
        const bool mountedBody=travelMode==TravelMode::horse||frame.controllers.vehicleMounted;
        if(frame.locomotionHeadingTracked&&!frame.controllers.frontEnd
           &&!frame.controllers.authoredCamera&&!frame.controllers.cabinPlay
           &&!frame.controllers.cabinTransit&&!frame.controllers.vehicleMounted
           &&travelMode==TravelMode::onFoot){
            std::optional<Quat> handHeading;
            if(frame.controllers.weaponReady&&frame.controllers.supportGrip
               &&supportContact.attached()&&!frame.controllers.optic.held
               &&presentationGrips[0]&&presentationGrips[1]&&!propHands[0]&&!propHands[1]){
                const auto midpoint=(grips[0].position+grips[1].position)*.5f;
                const auto aim=nativeTrackedPose(frame.nativePose,frame.headPose,presentationHands[1]->aim);
                // Only an acquired two-handed weapon hold owns aim-driven
                // torso facing. Bare-hand wrist rotation must not move the
                // shoulders and pull the stationary opposite hand out of reach.
                // Down-at-side, behind-back and crossed-body gestures cannot
                // claim torso facing. A supported weapon can: both grips use
                // the same frame as the viewer and native skin transaction.
                // The hands' midpoint admits the gesture, but must not steer
                // the torso: reaching onto a support socket changes that
                // midpoint and otherwise moves the gun away from the hand.
                // Use the primary tracked aim's heading in the same frame.
                handHeading=trackedTorsoHeading(frame.nativePose,compose(*root,Pose{{},midpoint}).position,aim.orientation);
            }
            const auto heading=bodyHeading.update(frame.locomotionHeading,frame.nativePose.orientation,
                frame.controllers.playerPosture==PlayerPosture::prone,frame.sampleTime,continuity,handHeading);
            if(heading)torso=*heading;
        }else bodyHeading.reset();
        const auto chest=bone(q,p,2);
        if(mountedBody)if(const auto heading=nativeTorsoHeading(compose(*root,chest)))torso=*heading;
        const auto shoulderCenter=(bone(q,p,6).position+bone(q,p,10).position)*.5f;
        auto uprightHead=compose(inverse(*root),Pose{torso,frame.nativePose.position});
        const auto fallbackUprightHead=uprightHead;
        bool fullBody=frame.controllers.fullBody&&nativeHead.has_value()&&frame.anatomicalEyeModel==model
            &&!frame.controllers.frontEnd&&!frame.controllers.authoredCamera
            &&!frame.controllers.cabinTransit
            &&(frame.controllers.cabinPlay||travelMode==TravelMode::onFoot||mountedBody)&&parent[0]==-1&&parent[1]==0&&parent[2]==1&&parent[3]==2&&parent[4]==3
            &&parent[13]==0&&parent[14]==13&&parent[15]==14&&parent[16]==15
            &&parent[17]==0&&parent[18]==17&&parent[19]==18&&parent[20]==19;
        if(frame.controllers.fullBody&&nativeHead&&frame.anatomicalEyeModel!=model)frame.body.rejection=1;
        // Prone uses BodyHeadingState's locomotion heading too: it includes
        // native smooth turning and VR snap yaw, but excludes HMD neck look.
        // Restoring the original skin heading here stranded the crawl body
        // while the view/hands turned. Keep the anatomical camera anchor;
        // fullBodyPlacement rotates the complete skin about that fixed eye,
        // and the placed-foot queries resolve its new terrain contacts.
        auto torsoDelta=fullBody?fullBodyPlacement(chest,bone(originalQ,originalP,4),uprightHead):upperBodyPlacement(chest,shoulderCenter,uprightHead);
        if(!torsoDelta)return false;
        if(fullBody&&!mountedBody){
            // A tracked turn belongs to the complete visible actor. Turning
            // only the chest leaves the native legs facing the opposite way
            // and twists shared garment weights around an unchanged waist.
            // Place the complete render pose at the measured eye, including
            // its pelvis. A pelvis-only pivot makes the eye orbit during a
            // turn, then asks a nearly straight spine to recover that orbit.
            // Native actor/collision and authored gait lengths stay untouched.
            for(size_t i=0;i<count;++i)replace(q,p,i,compose(*torsoDelta,bone(originalQ,originalP,i)));
            // Solve only contact residuals after the complete native posture
            // has acquired the viewer heading and the exact anatomical eye.
            torsoDelta=fullBodyPlacement(bone(q,p,2),bone(q,p,4),uprightHead);
            if(!torsoDelta)return false;
        }
        // Keep the shared sleeve roots together when the native prone head is
        // close to a slope. Moving an individual shoulder tears shared weights.
        float lift{};
        for(const auto i:shoulders){
            const auto world=compose(*root,compose(*torsoDelta,bone(q,p,i))).position;
            if(const auto surface=groundSurface(world,groundCeiling,.09f))
                lift=std::max(lift,(surface->clearance-dot(world-surface->point,surface->normal))/surface->normal.y);
        }
        std::optional<ArmSurface> chestFloor;
        if(lift>0){
            if(fullBody){
                // Shoulder contact constrains the chest, not the eyes.
                // Lifting the neck target displaced the head by 2--4 cm and
                // rejected the entire body in a real prone head-turn run.
                // Bend the measured spine while holding its neck endpoint.
                const auto placedChest=bone(q,p,2).position;
                chestFloor=ArmSurface{placedChest+rotate(rootInverse.orientation,{0,lift,0}),
                    rotate(rootInverse.orientation,{0,1,0}),0};
            }else torsoDelta->position=torsoDelta->position+rotate(rootInverse.orientation,{0,lift,0});
            ++groundContacts;
        }
        const auto fitFullBody=[&](){
            const auto neck=bone(q,p,3);
            const ArmPose nativeSpine{bone(q,p,1),bone(q,p,2),neck};
            const std::array<Pose,2> placedFeet{bone(q,p,15),bone(q,p,19)};
            auto neckTarget=compose(*torsoDelta,neck);
            auto spine=solveSpine(nativeSpine,neckTarget,chestFloor?&*chestFloor:nullptr);
            Vec3 pelvisShift{};
            if(!mountedBody&&!frame.controllers.cabinPlay
               &&(frame.controllers.playerPosture==PlayerPosture::standing
                  ||frame.controllers.playerPosture==PlayerPosture::crouched)){
                // Native aiming animation pitches the head ahead of the neck.
                // Retarget its neutral basis to the torso, keeping the measured
                // eyes fixed. Physical look must not steer this torso fit.
                const Pose headTarget{uprightHead.orientation,
                    uprightHead.position-rotate(uprightHead.orientation,nativeEyeFromHead)};
                const auto neutralNeck=compose(compose(headTarget,inverse(bone(q,p,4))),neck);
                const auto ray=neutralNeck.position-nativeSpine.shoulder.position;
                const float distance=std::sqrt(dot(ray,ray));
                const auto upper=nativeSpine.elbow.position-nativeSpine.shoulder.position;
                const auto lower=nativeSpine.wrist.position-nativeSpine.elbow.position;
                const float reach=std::sqrt(dot(upper,upper))+std::sqrt(dot(lower,lower))-.005f;
                const float advance=std::max(0.f,distance-reach);
                // The pelvis follows only the spine's measured reach deficit;
                // its feet remain at placedFeet for the existing leg solve.
                if(distance>.001f&&advance<=.12f){
                    const Pose shift{{},ray*(advance/distance)};
                    const auto fitted=solveSpine({compose(shift,nativeSpine.shoulder),
                        compose(shift,nativeSpine.elbow),compose(shift,nativeSpine.wrist)},
                        neutralNeck,chestFloor?&*chestFloor:nullptr);
                    if(fitted&&!fitted->reachClamped){
                        spine=fitted;neckTarget=neutralNeck;pelvisShift=shift.position;
                        for(size_t i=0;i<count;++i){auto placed=bone(q,p,i);placed.position=placed.position+pelvisShift;replace(q,p,i,placed);}
                    }
                }
            }
            if(!spine&&chestFloor&&frame.controllers.playerPosture==PlayerPosture::prone){
                // A nearly straight prone spine has no room to arch over a
                // slope. Let the pelvis approach the fixed neck by at most
                // 12 cm, preserving its tiny lumbar pivot and bone lengths.
                // Feet retain their pre-contact footprint for the leg solve.
                auto toward=rotate(root->orientation,neck.position-nativeSpine.shoulder.position);
                const float distance=std::hypot(toward.x,toward.z);
                if(distance>.1f){
                    const auto direction=rotate(rootInverse.orientation,{toward.x/distance,0,toward.z/distance});
                    const auto fit=[&](float amount){
                        const Pose translation{{},direction*amount};
                        return solveSpine({compose(translation,nativeSpine.shoulder),compose(translation,nativeSpine.elbow),
                            compose(translation,nativeSpine.wrist)},neckTarget,&*chestFloor);
                    };
                    float low=0,high=.12f;
                    spine=fit(high);
                    if(spine){
                        for(unsigned step=0;step<8;++step){
                            const float middle=(low+high)*.5f;
                            if(const auto candidate=fit(middle)){spine=candidate;high=middle;}else low=middle;
                        }
                        pelvisShift=direction*high;
                        for(size_t i=0;i<count;++i){auto placed=bone(q,p,i);placed.position=placed.position+pelvisShift;replace(q,p,i,placed);}
                    }
                }
            }
            if(!spine){frame.body.rejection=2;probe.bodyRejection=2;return false;}
            // Descendants still use the untouched native animation baseline.
            // Include the complete eye-centered placement in joint deltas.
            *torsoDelta=compose(spine->pose.elbow,inverse(chest));
            const auto neckDelta=compose(spine->pose.wrist,inverse(bone(originalQ,originalP,3)));
            replace(q,p,1,spine->pose.shoulder);replace(q,p,2,spine->pose.elbow);replace(q,p,3,spine->pose.wrist);
            for(size_t i=3;i<count;++i){
                if(i==3)continue;
                bool upper=false,head=false;
                for(int32_t ancestor=parent[i];ancestor>=0;ancestor=parent[ancestor]){
                    if(ancestor==3)head=true;
                    if(ancestor==2){upper=true;break;}
                }
                if(upper)replace(q,p,i,compose(head?neckDelta:*torsoDelta,bone(originalQ,originalP,i)));
            }
            const auto fittedEye=compose(*root,compose(bone(q,p,4),Pose{{},nativeEyeFromHead}));
            const auto eyeError=fittedEye.position-frame.nativePose.position;
            frame.body.eyeError=eyeError;
            // Shoulder/floor correction must not admit an avatar whose head
            // missed the stationary viewer. Reject before solving the arms,
            // so the established complete hand/prop fallback remains intact.
            if(!valid(fittedEye)||!(dot(eyeError,eyeError)<=fullBodyEyeErrorLimit*fullBodyEyeErrorLimit)){
                frame.body.rejection=3;probe.bodyRejection=3;return false;
            }
            frame.body.valid=true;frame.body.spineClamped=spine->reachClamped;frame.body.mounted=mountedBody;
            frame.body.pelvis=compose(*root,bone(q,p,0));frame.body.chest=compose(*root,bone(q,p,2));
            for(size_t side=0;side<2;++side){
                const size_t hip=side?17:13,knee=hip+1,ankle=hip+2;
                if(mountedBody){
                    // The seat/saddle owns this lower-body animation.
                    // Keep the native pelvis/legs; only the bounded spine and
                    // tracked arms follow the viewer. Grounding these feet
                    // or rotating the hips with head look detaches the rider.
                    frame.body.feet[side]=compose(*root,bone(q,p,ankle));
                    continue;
                }
                // Floor queries belong to the eye-placed native gait,
                // not to feet at their obsolete animation-space location.
                const ArmPose native{bone(q,p,hip),bone(q,p,knee),bone(q,p,ankle)};
                auto contact=placedFeet[side];
                if(frame.controllers.cabinPlay){
                    // The native seated animation projects its feet ahead of
                    // the chair. A room-scale body must support the tracked
                    // pelvis instead; retain a small authored fore/aft offset
                    // and let the existing floor/reach solve bend the knees.
                    auto footWorld=compose(*root,contact);
                    const auto hipWorld=compose(*root,native.shoulder);
                    footWorld.position.x=hipWorld.position.x+(footWorld.position.x-hipWorld.position.x)*.15f;
                    footWorld.position.z=hipWorld.position.z+(footWorld.position.z-hipWorld.position.z)*.15f;
                    contact=compose(rootInverse,footWorld);
                }
                std::optional<ArmSurface> floor;
                // Keep the native support level inside the search when the
                // eye-placed ankle rises; a fixed 75 cm ray lost the floor.
                const auto nativeAnkle=bone(originalQ,originalP,ankle);
                const float footLift=rotate(root->orientation,contact.position-nativeAnkle.position).y;
                if(const auto surface=modelSurface(contact.position,.09f,true,footLift)){
                    floor=surface;++frame.body.floorQueries;
                    // Retain native gait lift, including prone toe posture.
                    // Eye/body yaw can move a foot to a different point on a
                    // slope; preventing only penetration leaves that foot
                    // floating over the new ground. Join both floor queries
                    // to this original/retargeted skin pair instead.
                    if(const auto originalFloor=modelSurface(bone(originalQ,originalP,ankle).position,.09f,true)){
                        const float originalClearance=dot(bone(originalQ,originalP,ankle).position-originalFloor->point,originalFloor->normal);
                        frame.body.nativeFloorClearance[side]=originalClearance;
                        const float placedClearance=dot(contact.position-surface->point,surface->normal);
                        const auto worldNormal=rotate(root->orientation,surface->normal);
                        if(worldNormal.y>.35f){
                            const float verticalCorrection=(std::max(.09f,originalClearance)-placedClearance)/worldNormal.y;
                            contact.position=contact.position+rotate(rootInverse.orientation,{0,verticalCorrection,0});
                        }
                    }
                    if(const auto point=outsideArmSurface(contact.position,*surface)){
                        if(dot(*point-contact.position,*point-contact.position)>1e-8f)++frame.body.floorContacts;
                        contact.position=*point;
                    }
                }
                if(floor&&frame.controllers.physicalFloorTracked&&!legFitCalibrated
                   &&frame.controllers.playerPosture==PlayerPosture::standing){
                    // Calibrate once on entering standing mode. Later head
                    // excursions must not permanently lengthen this body.
                    const auto magnitude=[](Vec3 v){return std::sqrt(dot(v,v));};
                    const float reach=magnitude(native.elbow.position-native.shoulder.position)
                        +magnitude(native.wrist.position-native.elbow.position);
                    const float needed=magnitude(contact.position-native.shoulder.position);
                    if(reach>.2f){standingLegScale=std::clamp(needed/reach*1.005f,1.f,1.5f);legFitCalibrated=true;}
                }
                std::optional<ArmSolution> leg;
                if(floor&&frame.controllers.playerPosture!=PlayerPosture::prone){
                    const auto grounded=solveGroundedLeg(native,contact,
                        frame.controllers.seatedMode?1.f:standingLegScale);
                    if(grounded){leg=grounded->leg;frame.body.legScale[side]=grounded->scale;}
                }else leg=solveArm(native,contact,native.elbow.position-native.shoulder.position);
                frame.body.legCalibration=frame.controllers.seatedMode?1.f:standingLegScale;
                if(!leg){frame.body.rejection=4;probe.bodyRejection=4;return false;}
                replace(q,p,hip,leg->pose.shoulder);replace(q,p,knee,leg->pose.elbow);replace(q,p,ankle,leg->pose.wrist);
                const auto footDelta=compose(leg->pose.wrist,inverse(bone(originalQ,originalP,ankle)));
                for(size_t i=ankle+1;i<count;++i){
                    bool foot=false;for(int32_t ancestor=parent[i];ancestor>=0;ancestor=parent[ancestor])if(ancestor==static_cast<int32_t>(ankle)){foot=true;break;}
                    if(foot)replace(q,p,i,compose(footDelta,bone(originalQ,originalP,i)));
                }
                frame.body.feet[side]=compose(*root,leg->pose.wrist);
                if(floor)frame.body.floorClearance[side]=dot(leg->pose.wrist.position-floor->point,floor->normal);
            }
            return true;
        };
        if(fullBody&&!fitFullBody()){
            // Optional body fitting cannot remove established hands/optics.
            // Discard the partial body transaction and use the complete arm
            // fit until this exact native skeleton has a valid body solution.
            const auto rejection=frame.body.rejection;const auto eyeError=frame.body.eyeError;
            q=originalQ;p=originalP;frame.body={};frame.body.rejection=rejection;frame.body.eyeError=eyeError;fullBody=false;
            publishFullBodyVisibility(owner,model,false);
            torsoDelta=upperBodyPlacement(chest,shoulderCenter,fallbackUprightHead);
            if(!torsoDelta)return false;
            if(lift>0)torsoDelta->position=torsoDelta->position+rotate(rootInverse.orientation,{0,lift,0});
        }
        if(!fullBody)for(const size_t i:{1,2,5,6,7,8,9,10,11,12})replace(q,p,i,compose(*torsoDelta,bone(q,p,i)));
        // Coco's stationary pelvis counter-pivot is a render-pose correction.
        // Never move the native actor/root, change collision, or apply it to
        // a horse, vehicle, menu, authored movie or prone player.
        const bool canAlign=!fullBody&&count>20&&parent[1]==0&&parent[13]==0&&parent[14]==13
            &&parent[17]==0&&parent[18]==17&&!frame.menuOpen&&!frame.controllers.cabinPlay
            &&!frame.controllers.scriptedDemo&&!frame.controllers.authoredCamera
            &&nativeTravelMode()==TravelMode::onFoot&&bone(originalQ,originalP,4).position.y>.6f;
        if(stationaryBody.update(canAlign,root->position,frame.sampleTime,continuity)){
            const StationaryBodyPose body{bone(originalQ,originalP,0).position,
                bone(originalQ,originalP,13).position,bone(originalQ,originalP,17).position,
                bone(originalQ,originalP,6).position,bone(originalQ,originalP,10).position,bone(originalQ,originalP,2)};
            if(const auto lower=stationaryLowerBodyDelta(body,*torsoDelta))for(size_t i=0;i<count;++i){
                if(i==1||i==53)continue;
                bool lowerBone=i==0;
                for(int32_t ancestor=parent[i];!lowerBone&&ancestor>=0;ancestor=parent[ancestor]){
                    if(ancestor==1)break;
                    if(ancestor==0)lowerBone=true;
                }
                if(lowerBone)replace(q,p,i,compose(*lower,bone(q,p,i)));
            }
        }
        for(size_t side=0;side<2;++side){
            // Stable down/out bias; a transient native crouch/reload bend must
            // not leave the elbow trapped above the head in later frames.
            bendHistory[side]=rotate(inverse(*root).orientation,rotate(torso,{side?-.25f:.25f,-1.f,-.15f}));
        }
    }
    const auto fitClavicle=[&](size_t side,Pose target){
        // Grounded prone shoulders and authored mounted contacts retain their
        // existing constraints. The reproduced loss is in the full-body rig.
        if(!frame.body.valid||frame.body.mounted||frame.controllers.playerPosture==PlayerPosture::prone)return;
        const size_t clavicle=side?9:5,shoulder=clavicle+1;
        const auto before=bone(q,p,clavicle);
        const ArmPose arm{bone(q,p,shoulder),bone(q,p,shoulder+1),bone(q,p,shoulder+2)};
        if(const auto fitted=reachingClavicle(before,arm,target)){
            const auto delta=compose(*fitted,inverse(before));
            replace(q,p,clavicle,*fitted);
            for(size_t i=shoulder;i<=shoulder+2;++i)replace(q,p,i,compose(delta,bone(q,p,i)));
        }
    };
    auto wristTarget=compose(grips[1],gripFromWrist[1]);
    auto rightTarget=(wheel.hands&2)?rightAnimated:clearWrist(wristTarget);
    if(!(wheel.hands&2))fitClavicle(1,rightTarget);
    const ArmPose rightAnimatedArm{bone(q,p,10),bone(q,p,11),bone(q,p,12)};
    auto right=groundedArm(rightAnimatedArm,rightTarget,bendHistory[1],armBasis[1]);
    probe.stage=7;
    if(!right)return false;
    replace(q,p,10,right->pose.shoulder);replace(q,p,11,right->pose.elbow);replace(q,p,12,right->pose.wrist);
    bendHistory[1]=right->pose.elbow.position-right->pose.shoulder.position;
    const bool binocularHeld=frame.controllers.optic.held
        &&frame.controllers.optic.pose.tracked
        &&frame.controllers.optic.pose.kind==OpticKind::binocular;
    const auto attachBinocular=[&](){
        // Use this skin's solved palm for the body and its opposite support
        // socket. Raw tracking can differ at a surface or arm reach limit.
        const auto renderedPalm=compose(*root,compose(bone(q,p,12),inverse(gripFromWrist[1])));
        const auto localPalm=compose(frame.headPose,compose(Pose{{0,1,0,0},{}},
            compose(inverse(frame.nativePose),renderedPalm)));
        const auto optic=attachBinocularToPalm(frame.controllers.optic.pose,localPalm);
        if(!optic)return false;
        frame.controllers.optic.pose=*optic;
        const auto supportDistance=optic->supportGrip.position-frame.controllers.hands[0].grip.position;
        if(optic->supportHeld&&dot(supportDistance,supportDistance)>.12f*.12f){
            frame.controllers.optic.pose.supportHeld=false;
            frame.controllers.optic.pose.stability=.35f;
        }
        return true;
    };
    if(binocularHeld&&!attachBinocular())return false;
    const bool binocularSupport=binocularHeld&&frame.controllers.optic.pose.supportHeld;
    if(binocularHeld){aimBlend=0;guidedOffset=identityQuaternion();}
    bool nativeManipulation=false,firearmActive=false,throwableActive=false;
    probe.stage=8;
    std::optional<Vec3> barrelInGrip;
    std::optional<Pose> muzzleInGrip;
    std::optional<Pose> supportWeaponInWrist;
    std::optional<WeaponScopeGeometry> scopeInWrist;
    uint32_t scopeResource{};
    uint64_t weaponSupportIdentity{};
    const auto weaponComponent=get<uintptr_t>(character+0x80);
    auto& weaponDiagnostics=frame.weaponRig;
    weaponDiagnostics.sampled=true;weaponDiagnostics.sampleTime=steadyMilliseconds();
    weaponDiagnostics.character=character;weaponDiagnostics.component=weaponComponent;weaponDiagnostics.bodyModel=model;
    if((!vehicleWeaponScope(frame)||vehicleWeaponReady)&&!frame.controllers.nativeActionControls
       &&get<uintptr_t>(weaponComponent)==base+0x23b3e80&&get<uintptr_t>(weaponComponent+8)==character){
        weaponDiagnostics.componentAccepted=true;
        const auto instances=get<uintptr_t>(weaponComponent+0x38);
        const auto first=get<uint32_t>(instances+0x24),index=get<uint32_t>(owner+0x3a0);
        weaponDiagnostics.instances=instances;weaponDiagnostics.instanceFirst=first;weaponDiagnostics.playerIndex=index;
        if(index>=first&&index-first<=15){
            const auto state=get<uintptr_t>(weaponComponent+0x58)+(index-first)*0x610ull;
            const auto supportResource=get<uint32_t>(state+0x240);
            weaponDiagnostics.instanceIndexMatched=true;weaponDiagnostics.state=state;
            weaponDiagnostics.resourceHandle=supportResource;
            if(supportResource&&(supportResource>>16)!=0xffff)
                weaponSupportIdentity=uint64_t(supportResource)<<32|index;
            weaponDiagnostics.resourceAccepted=weaponSupportIdentity!=0;
            weaponDiagnostics.supportIdentity=weaponSupportIdentity;
            // Observed throughout rifle reload and WU pistol bolt cycling; the
            // native animation owns the support hand during these operations.
            nativeManipulation=(get<uint32_t>(state+0x27c)&0x1c0)==0x40&&(get<uint32_t>(state+0x3c0)&0x04000000)!=0;
            firearmActive=(get<uint32_t>(state+0x27c)&0x1c0)==0x40;
            throwableActive=(get<uint32_t>(state+0x27c)&0x1c0)==0x80;
            const auto flags=get<uint32_t>(state+0x27c),mode=get<uint32_t>(state+0x3c0);
            weaponDiagnostics.flags=flags;weaponDiagnostics.mode=mode;
            std::array<float,4> nativeTarget{};
            if(firearmActive&&read(state+0x140,nativeTarget)
               &&std::isfinite(nativeTarget[0])&&std::isfinite(nativeTarget[1])&&std::isfinite(nativeTarget[2])){
                weaponDiagnostics.nativeAimTarget={nativeTarget[0],nativeTarget[1],nativeTarget[2]};
                weaponDiagnostics.nativeAimTargetRead=true;
            }
            if(!binocularHeld&&frame.controllers.weaponReady&&firearmActive&&!nativeManipulation&&!(flags&0x400000)&&!(mode&0x2000)){
                weaponDiagnostics.muzzleAttempted=true;
                // Use the same authored barrel frame as the native shot path.
                // The vector between animated palms is not the barrel axis,
                // especially while the game changes its support-hand pose.
                alignas(16) std::array<float,16> attachmentMatrix{},muzzleMatrix{};
                using AttachmentGetter=uint32_t(*)(void*,void*,uint32_t);
                reinterpret_cast<AttachmentGetter>(base+0x1042e40)(reinterpret_cast<void*>(weaponComponent),attachmentMatrix.data(),index);
                if(read(state+((flags&0x200)?0x80:0x40),muzzleMatrix)){
                    weaponDiagnostics.muzzleMatrixRead=true;
                    const auto attachment=nativeAffinePose(attachmentMatrix),socket=nativeAffinePose(muzzleMatrix);
                    weaponDiagnostics.attachmentValid=attachment.has_value();weaponDiagnostics.muzzleSocketValid=socket.has_value();
                    if(attachment&&socket){
                        supportWeaponInWrist=*attachment;
                        muzzleInGrip=compose(gripFromWrist[1],compose(*attachment,*socket));
                        weaponDiagnostics.muzzleInGrip=*muzzleInGrip;weaponDiagnostics.muzzleSolved=true;
                        barrelInGrip=rotate(muzzleInGrip->orientation,{0,0,1});
                        // The native weapon initializer at 0x1060d5f queries
                        // this same resource's muzzle, rear and front CNPs.
                        // +0x240 is the accepted resource handle; +0x208 is
                        // the optical part of its copied 0x90-byte gun data.
                        const auto points=get<uintptr_t>(weaponComponent+0x60);
                        std::array<uint8_t,4> optical{};
                        const auto resource=get<uint32_t>(state+0x240);
                        if(scopeQueryVerified&&get<uintptr_t>(points)==base+0x234b840
                           &&get<uintptr_t>(base+0x234b840+0x148)==base+0xdbdc50
                           &&(weaponDiagnostics.opticalRead=read(state+0x208,optical))
                           &&resource&&(resource>>16)!=0xffff){
                            using PointGetter=bool(*)(void*,void*,uint32_t,uint64_t);
                            const auto point=reinterpret_cast<PointGetter>(base+0xdbdc50);
                            alignas(16) std::array<float,16> rear{},front{};
                            // Observe the actual assembled sight pair even when
                            // its optic profile is not supported. These are the
                            // same read-only native queries used by gun setup;
                            // the portal still requires a measured profile.
                            weaponDiagnostics.sightQueryAttempted=true;
                            weaponDiagnostics.sightRearRead=point(reinterpret_cast<void*>(points),rear.data(),resource,0x614d8847090eull);
                            weaponDiagnostics.sightFrontRead=point(reinterpret_cast<void*>(points),front.data(),resource,0x786932704dd6ull);
                            if(weaponDiagnostics.sightRearRead&&weaponDiagnostics.sightFrontRead){
                                const auto rearPose=nativeAffinePose(rear),frontPose=nativeAffinePose(front);
                                weaponDiagnostics.sightRearValid=rearPose.has_value();weaponDiagnostics.sightFrontValid=frontPose.has_value();
                                if(rearPose){weaponDiagnostics.sightRear=*rearPose;weaponDiagnostics.sightRearInWrist=compose(*attachment,*rearPose);}
                                if(frontPose){weaponDiagnostics.sightFront=*frontPose;weaponDiagnostics.sightFrontInWrist=compose(*attachment,*frontPose);}
                                auto geometry=rearPose&&frontPose?nativeWeaponScopeGeometry(*rearPose,*frontPose,optical):std::nullopt;
                                if(!geometry)if(const auto assembled=assembledSight(weaponComponent,resource)){
                                    const uint32_t sight=assembled->magnifierCode==0x84a1df03c6484ee1ull?4u:
                                        assembled->magnifierCode==0x84a2233aa7f98e18ull?5u:2u;
                                    const uint64_t identity=uint64_t(resource)<<32|sight;
                                    const float power=scopeZoom.update(identity,frame.controllers.weaponZoomSequence,
                                        {optical[0],optical[1],optical[2]});
                                    geometry=nativeMagnifierScopeGeometry(assembled->center,assembled->magnifier,
                                        assembled->centerCode,assembled->magnifierCode,optical,power);
                                    if(geometry&&assembled->magnifierModel){
                                        weaponDiagnostics.magnifierInterface=assembled->magnifierInterface;
                                        weaponDiagnostics.magnifierModel=assembled->magnifierModel;
                                        weaponDiagnostics.magnifierIndex=assembled->magnifierIndex;
                                        weaponDiagnostics.magnifierModelCode=assembled->magnifierCode;
                                    }
                                }
                                if(geometry){
                                    geometry->ocular=compose(*attachment,geometry->ocular);
                                    geometry->objective=compose(*attachment,geometry->objective);
                                    scopeInWrist=*geometry;scopeResource=resource;
                                }else if(scopeTrace&&rearPose&&frontPose){
                                    static uint64_t lastUnprofiled{};
                                    const uint64_t key=uint64_t(resource)<<32|uint64_t(optical[0])<<24|uint64_t(optical[1])<<16|uint64_t(optical[2])<<8|optical[3];
                                    if(key!=lastUnprofiled){
                                        lastUnprofiled=key;std::ostringstream s;
                                        s<<"Unprofiled native scope resource="<<std::hex<<resource<<std::dec<<" optical=";
                                        for(auto value:optical)s<<unsigned(value)<<",";
                                        const auto describe=[&](const char* label,Pose p){s<<" "<<label<<"="<<p.position.x<<","<<p.position.y<<","<<p.position.z
                                            <<" q="<<p.orientation.x<<","<<p.orientation.y<<","<<p.orientation.z<<","<<p.orientation.w;};
                                        describe("rear",*rearPose);describe("front",*frontPose);mgs5vr::log(s.str());
                                    }
                                }
                            }
                        }
                        if(weaponDiagnostics.opticalRead)weaponDiagnostics.optical=optical;
                    }
                }
            }
        }
    }
    // OpenXR grip describes a palm, not a gun barrel. Runtime aim accounts
    // for each controller profile's pointing angle. Swing the common wrist
    // owner using the native muzzle socket before solving support contact;
    // changing only the weapon would detach its grip, sights and shot path.
    const auto calibratedWeaponAim=frame.controllers.hands[1].aimTracked
        ?weaponAimWithPitch(frame.controllers.hands[1].aim,frame.controllers.weaponAimPitch):std::nullopt;
    if(!propHands[1]&&barrelInGrip&&calibratedWeaponAim){
        probe.stage=9;
        const auto aim=compose(rootInverse,nativeTrackedPose(frame.nativePose,frame.headPose,*calibratedWeaponAim));
        if(const auto aimed=aimedWeaponGrip(grips[1],aim,*barrelInGrip)){
            grips[1]=*aimed;
            rightTarget=clearWrist(compose(grips[1],gripFromWrist[1]));
            right=groundedArm(rightAnimatedArm,rightTarget,bendHistory[1],armBasis[1]);
            if(!right)return false;
            replace(q,p,10,right->pose.shoulder);replace(q,p,11,right->pose.elbow);replace(q,p,12,right->pose.wrist);
            bendHistory[1]=right->pose.elbow.position-right->pose.shoulder.position;
        }
    }
    // An acquired contact belongs to one native weapon, not to whichever
    // weapon happens to replace it while both grips are still held.
    if(supportWeapon!=weaponSupportIdentity){
        supportContact.reset();supportPose.reset();supportBlend=0;aimBlend=0;
        guidedOffset=identityQuaternion();heldSupportOffset={};heldCloseSupport=false;supportAt=now;
        supportWeapon=weaponSupportIdentity;
    }
    const auto supportOffset=compose(inverse(rightAnimated),leftAnimated);
    const auto nativeSupportInGrip=compose(gripFromWrist[1],compose(supportOffset,inverse(gripFromWrist[0])));
    auto attached=compose(right->pose.wrist,supportOffset);
    // Binocular support is a real tracked left-hand socket, not the firearm
    // support offset. Convert the published LOCAL-frame side cup back into the
    // character root and then use the same anatomical wrist correction as a
    // weapon grip. This keeps the retail binocular in the right palm while
    // the left palm cups the opposite side when the user supports it.
    std::optional<Pose> binocularSupportWrist;
    if(binocularSupport&&frame.controllers.hands[0].gripTracked){
        const auto supportNative=nativeTrackedPose(frame.nativePose,frame.headPose,frame.controllers.optic.pose.supportGrip);
        const auto supportRoot=compose(inverse(*root),supportNative);
        binocularSupportWrist=clearWrist(compose(supportRoot,gripFromWrist[0]));
        attached=*binocularSupportWrist;
    }
    // Test the actual support grip, never controller-to-controller distance.
    // While attached, retain the acquired contact frame through native reload
    // animation and evaluate it in the currently guided weapon frame.
    const bool wasAttached=supportContact.attached();
    const bool compactSupport=wasAttached?heldCloseSupport:closeSupportContact(nativeSupportInGrip.position);
    if(compactSupport){aimBlend=0;guidedOffset=identityQuaternion();}
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
    const bool forwardSupport=calibratedWeaponAim&&withinSupportCone(
        frame.controllers.hands[0].grip.position-primaryHand.grip.position,
        rotate(calibratedWeaponAim->orientation,{0,0,-1}));
    // The configured support grip owns contact, but still requires proximity
    // to the authored socket. Release, inspection and pulling away free the
    // hand. A one-handed reload must not grab a distant tracked controller.
    // A pistol cup is alongside/below the firing palm, not a foregrip in
    // front of it. Socket distance still owns acquisition and release; a
    // wrist-facing pose cannot cancel an explicitly squeezed close cup.
    const bool supportInspect=inspecting&&!compactSupport&&!wasAttached;
    const bool weaponNearSupport=supportContact.update(frame.controllers.supportGrip&&frame.controllers.weaponReady&&firearmActive&&(wasAttached||compactSupport||forwardSupport)&&!supportInspect&&(!nativeManipulation||wasAttached),
        frame.controllers.hands[0].gripTracked,std::sqrt(dot(separation,separation)),now,
        frame.controllers.supportGripRadius,frame.controllers.supportDetachRadius);
    const bool nearSupport=binocularHeld?binocularSupport:weaponNearSupport;
    if(!binocularHeld&&nearSupport&&!wasAttached){heldSupportOffset=supportOffset;heldCloseSupport=compactSupport;}
    const bool support=nearSupport;
    const float step=std::min(now>=supportAt?static_cast<float>(now-supportAt)/180.f:1.f,1.f);supportAt=now;
    supportBlend=std::clamp(supportBlend+(support?step:-step),0.f,1.f);
    // A menu, stow, non-firearm or lost controller releases ownership now.
    // Distance release can blend out, but never toward a new native stow pose.
    if(!binocularHeld&&(!frame.controllers.supportGrip||!frame.controllers.weaponReady||!firearmActive||supportInspect||!frame.controllers.hands[0].gripTracked)){
        supportBlend=0;aimBlend=0;supportPose.reset();
    }
    const auto presentedSupport=binocularSupportWrist
        ?compose(inverse(right->pose.wrist),*binocularSupportWrist)
        :supportPose.update(supportOffset,nearSupport,nativeManipulation,supportWeaponInWrist).value_or(supportOffset);
    if(!binocularHeld&&nearSupport)heldSupportOffset=presentedSupport;
    attached=compose(right->pose.wrist,presentedSupport);
    bool guiding=!binocularHeld&&!compactSupport&&nearSupport&&nativeManipulation&&aimBlend>0;
    if(!compactSupport&&barrelInGrip&&frame.controllers.hands[0].gripTracked&&nearSupport){
        // Guidance follows the acquired native support palm, which can sit
        // below or beside the bore. A hand already on that contact must not
        // rotate the weapon merely because the two axes differ.
        const auto supportInGrip=compose(gripFromWrist[1],compose(presentedSupport,inverse(gripFromWrist[0])));
        if(const auto guided=twoHandSupportGrip(grips[1],grips[0],supportInGrip,1.f)){
            guidedOffset=compose(inverse(grips[1]),*guided).orientation;guiding=true;
        }
    }
    // The firing wrist is the common owner of the visible weapon and muzzle.
    // Swing it toward the support controller while keeping its grip pivot and
    // roll. Native reload contact remains authoritative during manipulation.
    aimBlend=std::clamp(aimBlend+(guiding?step:-step),0.f,1.f);
    if(aimBlend>0){
        probe.stage=10;
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
    if(presentationGrips[0]||support){
        // Preserve the game's animated support-hand contact while the native
        // support is requested or a native reload/bolt cycle is running.
        auto target=attached;
        if(idroidHeld&&presentationGrips[0]){
            // The cinematic's LEFT wrist, palm and fingers own this contact.
            // Handset and hologram are published from the completed wrist below.
            const Pose fit{frame.controllers.idroidGripRotation,frame.controllers.idroidGripOffset};
            if(!valid(fit))return false;
            const auto heldWrist=nativeIdroidLeftHandheldWrist(grips[0],fit);
            if(!heldWrist)return false;
            target=clearWrist(*heldWrist);
        }else if(binocularHeld&&!binocularSupport){
            // A one-handed binocular hold leaves the left hand free and
            // tracked; it must not be pulled toward the firearm support
            // offset merely because the controller is visible.
            target=clearWrist(compose(grips[0],gripFromWrist[0]));
        }else if(presentationGrips[0]&&supportBlend<1){
            const auto tracked=compose(grips[0],gripFromWrist[0]);
            target.position=tracked.position+(attached.position-tracked.position)*supportBlend;
            target.orientation=blendRotation(tracked.orientation,attached.orientation,supportBlend);
        }
        if(wheel.hands&1)target=leftAnimated;
        else if(supportBlend<1)target=clearWrist(target);
        if(!(wheel.hands&1))fitClavicle(0,target);
        const auto left=groundedArm({bone(q,p,6),bone(q,p,7),bone(q,p,8)},target,bendHistory[0],armBasis[0]);
        probe.stage=11;
        if(!left)return false;
        replace(q,p,6,left->pose.shoulder);replace(q,p,7,left->pose.elbow);replace(q,p,8,left->pose.wrist);
        bendHistory[0]=left->pose.elbow.position-left->pose.shoulder.position;
    }
    constexpr std::array<size_t,10> changed{1,2,5,6,7,8,9,10,11,12};
    probe.stage=12;
    std::array<Pose,512> delta{};std::array<bool,512> controlled{};
    for(const auto i:changed){controlled[i]=true;delta[i]=compose(bone(q,p,i),inverse(bone(originalQ,originalP,i)));}
    if(frame.body.valid)for(const size_t i:{0,3,13,14,15,17,18,19}){
        controlled[i]=true;delta[i]=compose(bone(q,p,i),inverse(bone(originalQ,originalP,i)));
    }
    for(size_t i=0;i<count;++i)if(!controlled[i]){
        auto ancestor=parent[i];
        while(ancestor>=0&&!controlled[static_cast<size_t>(ancestor)])ancestor=parent[static_cast<size_t>(ancestor)];
        if(ancestor>=0)replace(q,p,i,compose(delta[static_cast<size_t>(ancestor)],bone(originalQ,originalP,i)));
    }
    // These named corrective joints belong to the verified native arm mesh.
    // Recompute their native local corrections from the solved joints. Neither
    // stale animation corrections nor rigid parent copies match this skin.
    const auto helpers=namesRead?armHelperIndices({boneNames.data(),count},{parent.data(),count}):std::nullopt;
    const bool helpersMatch=helpers.has_value();
    if(helpersMatch)for(size_t side=0;side<2;++side){
        const size_t clavicle=side?9:5;
        const auto local=[&](size_t i){return compose(inverse(bone(q,p,parent[i])),bone(q,p,i)).orientation;};
        const auto corrections=armCorrectiveRotations(local(clavicle),local(clavicle+1),local(clavicle+2),local(clavicle+3),side!=0);
        for(size_t j=0;j<7;++j){
            const auto i=(*helpers)[side*7+j],anchor=static_cast<size_t>(armHelperParents[side*7+j]);
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
        const bool available=frame.controllers.allowMotionMelee&&nativeTravelMode()==TravelMode::onFoot&&!nativeManipulation
            &&frame.controllers.magnification==1&&!frame.controllers.commandControls&&!frame.menuOpen&&frame.controllers.hands[0].trigger<.25f;
        meleeCurl={};
        for(size_t point=0;point<3;++point){
            const size_t side=point==0?0:1;
            const auto& hand=frame.controllers.hands[side];
            auto contact=hand.grip;
            auto rendered=compose(*root,compose(bone(q,p,wrists[side]),inverse(gripFromWrist[side])));
            // Contact follows the rendered palm, which may retain a complete
            // grip/aim pair while either raw pose is unavailable.
            bool allowed=available&&hand.gripTracked&&hand.aimTracked&&(side||supportBlend==0);
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
    // Use the measured cinematic LEFT-hand locals, including the ring/little
    // base. Ordinary equipment animation cups the right hand instead.
    if(idroidHeld&&presentationGrips[0])for(const auto& joint:nativeIdroidLeftHand)
        replace(q,p,joint.bone,compose(bone(q,p,joint.parent),joint.local));
    // Read native locals from the saved pose so updating one joint cannot alter
    // the native reference for its children.
    constexpr std::array<std::array<size_t,5>,2> fingers{{{21,24,27,31,34},{37,40,43,47,50}}};
    unsigned articulatedHands{};
    for(size_t side=0;side<2;++side){
        const auto& hand=frame.controllers.hands[side];
        if(idroidHeld&&side==0){++articulatedHands;continue;}
        // A binocular is a right-hand weapon-style grip even though it has no
        // firearm state. Close the primary hand around the same authored palm
        // socket, and close the optional support hand only after its grip is
        // actually held. This keeps both hands on the device rather than
        // leaving an open/free hand beside a floating model.
        const auto& studio=frame.controllers.studio;
        const bool cameraContact=studio&&studio->available&&studio->cameraHand==static_cast<int>(side);
        const bool markerContact=studio&&studio->available&&studio->markerHand==static_cast<int>(side);
        const bool paperContact=studio&&studio->available&&std::any_of(studio->papers.begin(),studio->papers.end(),
            [&](const auto& paper){return paper.hand==static_cast<int>(side);});
        const bool tapeContact=frame.controllers.openingPropsActive&&std::any_of(
            frame.controllers.openingBench.tapes.begin(),frame.controllers.openingBench.tapes.end(),
            [&](const auto& tape){return tape.hand==static_cast<int>(side);});
        const bool propContact=cameraContact||markerContact||paperContact||tapeContact;
        // The native support wrist is a useful rifle socket even when its
        // current animation leaves the fingers open. An acquired long-gun
        // hold needs a cupped hand; reloads/bolt cycles keep native ownership.
        const bool rifleSupportContact=side==0&&!binocularHeld&&!propContact
            &&firearmActive&&!compactSupport&&!nativeManipulation
            &&frame.controllers.supportGrip&&supportBlend>0;
        const float contact=propContact?0.f:side
            ?((binocularHeld||((frame.controllers.weaponReady)&&(firearmActive||throwableActive)))?1.f:0.f)
            :(binocularSupport?1.f:supportBlend);
        // Native firearm contact owns the authored hand during reloads. The
        // binocular is a VR-only physical item, so its tracked hands still
        // need the authored curl pass at full contact or they remain open and
        // appear to float behind the housing.
        if(!presentationGrips[side]||frame.controllers.vehicleControls
           ||(contact>=1&&!binocularHeld&&!rifleSupportContact))continue;
        for(size_t finger=0;finger<5;++finger)for(size_t joint=0;joint<3;++joint){
            const auto i=fingers[side][finger]+joint;
            const auto anchor=joint?i-1:(finger>=3?(side?46u:30u):wrists[side]);
            if(parent[i]!=static_cast<int32_t>(anchor))return false;
            const auto offset=bindOffset(i);
            if(!valid(Pose{{},offset})||dot(offset,offset)<.000025f||dot(offset,offset)>.015f)return false;
            const auto native=compose(inverse(bone(originalQ,originalP,anchor)),bone(originalQ,originalP,i));
            const float controllerCurl=freeFingerCurl(static_cast<unsigned>(finger),hand.trigger,hand.squeeze,
                hand.triggerTouched,hand.thumbTouched,frame.controllers.handRestCurl,frame.controllers.handTouchCurl);
            const bool opticContact=binocularHeld&&(side!=0||binocularSupport);
            // Cup the top instead of leaving the fingers hovering open above
            // it. The thumb has its own opposing hinge and needs less curl.
            const float propCurl=cameraContact?(finger==0?.50f:finger==1?.42f:.60f)
                :markerContact?(finger==0?.50f:.62f):(finger==0?.35f:finger==1?.42f:.25f);
            const float curl=propContact?propCurl:(opticContact||rifleSupportContact)?std::clamp(controllerCurl,finger==0?.55f:.72f,finger==0?.65f:.82f)
                :std::max(controllerCurl,frame.controllers.strikeCurl[side]);
            const auto rotation=fingerJointRotation(side!=0,static_cast<unsigned>(finger),static_cast<unsigned>(joint),curl);
            if(!rotation)return false;
            // Native firearm contact can be 1 while its weapon is stowed.
            // Blending back to that animation discards every optic curl and
            // leaves an open palm underneath the housing. An optic contact
            // owns the cupped fingers; its wrist still follows the same grip.
            const float nativeContact=(opticContact||rifleSupportContact)?0.f:contact;
            const Pose local{blendRotation(*rotation,native.orientation,nativeContact),offset+(native.position-offset)*nativeContact};
            replace(q,p,i,compose(bone(q,p,anchor),local));
        }
        ++articulatedHands;
    }
    // Animals never displace the controller-owned hands. Detect a stroke from
    // the rendered contact points below; an oversized collision proxy must
    // not hold the wrist away from the fur or redirect the player's fingers.
    frame.body.headContacts=headContacts;
    if(frame.body.valid){
        // Reconstruct from the final skin before body-owned prop joins and
        // visibility admission. A valid spine solve alone does not establish
        // anatomical eye alignment after all descendant publication.
        frame.body.head=compose(*root,bone(q,p,4));
        frame.body.eye=compose(frame.body.head,Pose{{},nativeEyeFromHead});
        frame.body.eyeError=frame.body.eye.position-frame.nativePose.position;
        frame.body.valid=valid(frame.body.eye)
            &&dot(frame.body.eyeError,frame.body.eyeError)<=fullBodyEyeErrorLimit*fullBodyEyeErrorLimit;
        if(!frame.body.valid)frame.body.rejection=5;
    }
    // Full render pose, before native matrix and attachment publication.
    // Native position.w metadata is retained. Publish the final anatomical
    // palm frames with this same skin transaction so handheld displays can
    // choose the palm side from the rendered hand, not a raw controller guess.
    std::array<Pose,2> renderedPalms{};
    std::array<bool,2> renderedPalmTracked{};
    for(size_t side=0;side<2;++side){
        renderedPalms[side]=compose(*root,compose(bone(q,p,wrists[side]),inverse(gripFromWrist[side])));
        renderedPalmTracked[side]=presentationHands[side].has_value()&&valid(renderedPalms[side]);
    }
    frame.renderedPalms=renderedPalms;frame.renderedPalmTracked=renderedPalmTracked;
    // The native skin owns each held prop just as it owns the binocular.
    // Raw tracking may request a palm beyond the arm or through a surface.
    const auto trackingFromNative=compose(frame.headPose,compose(Pose{{0,1,0,0},{}},inverse(frame.nativePose)));
    std::array<Pose,2> localPalms{},inputGrips{};
    for(size_t side=0;side<2;++side){
        localPalms[side]=compose(trackingFromNative,renderedPalms[side]);
        inputGrips[side]=propInputPalms[side];
        frame.controllers.hands[side].indexTip=compose(trackingFromNative,
            compose(*root,bone(q,p,fingers[side][1]+2))).position;
        frame.controllers.hands[side].indexTipTracked=renderedPalmTracked[side];
    }
    if(frame.controllers.studio&&frame.controllers.studio->available){
        auto joined=std::make_shared<CabinStudioFrame>(*frame.controllers.studio);
        joinStudioHands(*joined,inputGrips,localPalms,renderedPalmTracked);
        if(frame.body.valid)joinStudioBody(*joined,
            compose(trackingFromNative,frame.body.pelvis),
            compose(trackingFromNative,compose(*root,bone(q,p,17))));
        frame.controllers.studio=std::move(joined);
    }
    if(frame.controllers.openingPropsActive&&frame.controllers.openingWorldAnchored){
        const auto cabinFromNative=inverse(frame.controllers.openingWorldOrigin);
        for(auto& tape:frame.controllers.openingBench.tapes){
            if(tape.hand<0||tape.hand>1||!renderedPalmTracked[tape.hand])continue;
            const auto input=nativeTrackedPose(frame.nativePose,frame.headPose,inputGrips[tape.hand]);
            const auto correction=compose(renderedPalms[tape.hand],inverse(input));
            tape.pose=compose(cabinFromNative,compose(correction,compose(frame.controllers.openingWorldOrigin,tape.pose)));
        }
    }
    if(idroidMount||(rocketIdroid&&idroidHeld)){
        const size_t side=idroidHeld?0:1;
        frame.idroidDevice=compose(*root,compose(bone(q,p,wrists[side]),
            idroidHeld?nativeIdroidLeftBodyInWrist:*idroidMount));
        frame.idroidDeviceTracked=presentationHands[side].has_value()&&valid(frame.idroidDevice);
    }
    frame.playerHead=renderedHead;
    if(presentationHands[0]&&renderedPalmTracked[0]){
        const auto wrist=compose(*root,bone(q,p,8));
        const auto elbow=compose(*root,bone(q,p,7));
        // Status belongs to the forearm, not the independently articulated
        // palm. The anatomical left palm's -X is the dorsal side; the native
        // wrist bone's authored Y is not that surface after the grip solve.
        // Project the dorsal direction across the solved forearm segment so
        // the text lies along the skin instead of standing above it.
        if(const auto panel=forearmPanel(elbow,wrist,rotate(renderedPalms[0].orientation,{-1,0,0}),frame.controllers.wristSurfaceLift)){
            frame.wristPanel=*panel;
            frame.wristPanelTracked=true;
        }
    }
    if(binocularHeld){
        // The arm solve can constrain the wrist at a surface or reach limit.
        // Publish the device from that exact rendered palm, in this skin's
        // source transaction, so the housing, lens camera and marking ray
        // cannot continue independently at the unconstrained input position.
        if(!attachBinocular())return false;
    }
    frame.nativeFirearmActive=firearmActive&&!binocularHeld&&!propHands[1];
    frame.weaponSupportGripTracked=frame.nativeFirearmActive&&frame.controllers.weaponReady;
    frame.weaponSupportAttached=frame.weaponSupportGripTracked&&nearSupport;
    if(frame.weaponSupportGripTracked){
        const auto supportPalm=compose(*root,compose(right->pose.wrist,
            compose(presentedSupport,inverse(gripFromWrist[0]))));
        frame.weaponSupportGrip=compose(frame.headPose,compose(Pose{{0,1,0,0},{}},
            compose(inverse(frame.nativePose),supportPalm)));
    }
    if(weaponDiagnostics.sightRearValid&&weaponDiagnostics.sightFrontValid){
        const auto localFromWorld=compose(frame.headPose,compose(Pose{{0,1,0,0},{}},inverse(frame.nativePose)));
        const auto localFromWrist=compose(localFromWorld,compose(*root,bone(q,p,12)));
        weaponDiagnostics.sightRearLocal=compose(localFromWrist,weaponDiagnostics.sightRearInWrist);
        weaponDiagnostics.sightFrontLocal=compose(localFromWrist,weaponDiagnostics.sightFrontInWrist);
    }
    magnifierOwnerIndex.store(weaponDiagnostics.magnifierIndex,std::memory_order_relaxed);
    magnifierOwnerInterface.store(weaponDiagnostics.magnifierInterface,std::memory_order_relaxed);
    if(scopeInWrist&&(!frame.menuOpen||(frame.menuIdroid&&frame.controllers.handheldMenus
            &&frame.controllers.handheldMenuFree))){
        // Use the completed skin, including support/reach/collision solve.
        // Never bind the lens to an unconstrained controller or a newer pose.
        const auto wrist=compose(*root,bone(q,p,12));
        const auto localFromWorld=compose(frame.headPose,compose(Pose{{0,1,0,0},{}},inverse(frame.nativePose)));
        const auto localFromWrist=compose(localFromWorld,wrist);
        const uint64_t identity=static_cast<uint64_t>(scopeResource)<<32|scopeInWrist->sight;
        frame.weaponScope={compose(localFromWrist,scopeInWrist->ocular),compose(localFromWrist,scopeInWrist->objective),
            scopeInWrist->radius,frame.controllers.scopeEyeRelief,
            scopeZoom.update(identity,frame.controllers.weaponZoomSequence,scopeInWrist->powers),identity,true,scopeInWrist->dotReticle,scopeInWrist->opaqueAtOnePower};
        const auto rawScope=frame.weaponScope;
        // At full two-hand contact, deliberate support-hand guidance is part
        // of the controller owner, not attachment noise. Follow that same
        // common grip immediately; do not delay it by the x8 filter constant.
        Pose scopeOwner=primaryHand.aim;
        if(aimBlend>0)scopeOwner=compose(localFromWorld,compose(*root,
            compose(grips[1],Pose{blendRotation({},guidedOffset,aimBlend),{}})));
        const auto reach=right->pose.wrist.position-rightTarget.position;
        const bool changingContact=(supportBlend>0&&supportBlend<1)||(aimBlend>0&&aimBlend<1);
        const bool stabilize=frame.controllers.scopePoseStabilization&&primaryHand.aimTracked
            &&primaryHand.gripTracked&&!nativeManipulation&&!changingContact&&dot(reach,reach)<.000004f;
        frame.weaponScope=scopeAim.update(scopeOwner,rawScope,stabilize,frame.sampleTime,
            frame.controllers.referenceEpoch,frame.activation);
        if(scopeTrace&&stabilize){
            static uint64_t statAt{};static float maxDeg{},maxMm{};
            const auto scopeDelta=compose(Pose{rawScope.ocular.orientation,{}},
                inverse(Pose{frame.weaponScope.ocular.orientation,{}})).orientation;
            const auto shift=rawScope.ocular.position-frame.weaponScope.ocular.position;
            maxDeg=std::max(maxDeg,2.f*std::atan2(std::sqrt(scopeDelta.x*scopeDelta.x+scopeDelta.y*scopeDelta.y+scopeDelta.z*scopeDelta.z),std::abs(scopeDelta.w))*57.29578f);
            maxMm=std::max(maxMm,std::sqrt(dot(shift,shift))*1000.f);
            if(now-statAt>=2000){
                statAt=now;std::ostringstream s;s<<"Scope stabilizer (Coco v3): max correction "
                    <<maxDeg<<" deg, "<<maxMm<<" mm (2 s)";log(s.str());maxDeg=maxMm=0;
            }
        }
        static uint64_t reported{};static float reportedPower{};
        if(reported!=identity||reportedPower!=frame.weaponScope.magnification){
            reported=identity;reportedPower=frame.weaponScope.magnification;
            std::ostringstream s;s<<"Physical weapon scope sight="<<scopeInWrist->sight
                <<" resource="<<std::hex<<scopeResource<<std::dec<<" radius="<<scopeInWrist->radius
                <<" power="<<frame.weaponScope.magnification<<" ocular="<<frame.weaponScope.ocular.position.x<<','
                <<frame.weaponScope.ocular.position.y<<','<<frame.weaponScope.ocular.position.z;log(s.str());
        }
        static uint64_t tracedAt{};
        if(scopeTrace&&now-tracedAt>=2000){
            tracedAt=now;std::ostringstream s;s<<"Scope trace ocular=";
            const auto& o=frame.weaponScope.ocular;
            s<<o.position.x<<','<<o.position.y<<','<<o.position.z<<" q="
                <<o.orientation.x<<','<<o.orientation.y<<','<<o.orientation.z<<','<<o.orientation.w;
            for(size_t eye=0;eye<2;++eye){
                const auto eyeLocal=compose(inverse(o),frame.views[eye].pose).position;
                s<<" eye"<<eye<<'='<<eyeLocal.x<<','<<eyeLocal.y<<','<<eyeLocal.z<<" visible="
                    <<weaponScopeEyeVisible(frame.weaponScope,frame.views[eye].pose);
            }
            log(s.str());
        }
    }
    else scopeAim.reset();
    probe.stage=13;
    if(!headCamera().publishRigFrame(camera,owner,nativeCamera,frame))return false;
    probe.stage=14;
    publishFullBodyVisibility(owner,model,frame.body.valid);
    if(idroidAttachmentDiagnosticsActive()){
        // Capture this solve's assigned rig sequence before native skin work.
        // A later render/rig query cannot stand in for this transaction.
        const auto published=headCamera().publishedRigFrame(steadyMilliseconds());
        if(published&&published->applied&&published->rigSequence&&published->playerOwner==owner
            &&published->activation==frame.activation&&published->trackingSequence==frame.trackingSequence
            &&published->playerSequence==frame.playerSequence&&published->sampleTime==frame.sampleTime
            &&published->menuGeneration==frame.menuGeneration
            &&published->controllers.referenceEpoch==frame.controllers.referenceEpoch
            &&published->controllers.presentationEpoch==frame.controllers.presentationEpoch
            &&published->controllers.predictedXrTime==frame.controllers.predictedXrTime)
            restore.attachmentDiagnosticFrame=*published;
    }
    HandContacts handContacts;
    for(size_t side=0;side<2;++side){
        handContacts[side][0]=renderedPalms[side].position;
        for(size_t finger=0;finger<5;++finger)
            handContacts[side][finger+1]=compose(*root,bone(q,p,fingers[side][finger]+2)).position;
    }
    publishAnimalHands(frame,renderedPalms,handContacts);
    publishSmallAnimalHands(frame,renderedPalms,handContacts);
    wheelMailbox().publish(wheel);
    if(wheel.engaged)log("Driver authored wheel contact acquired; hands="+std::to_string(wheel.hands));
    for(const auto& strike:strikes)if(strike)publishMeleeSweep(*strike);
    // The engine may reuse animated helper/finger channels next frame. Only the
    // native publication consumes our solved pose; never feed it back into the
    // animation cache and compound garment transforms on subsequent updates.
    restore.rotations=rotations;restore.positions=positions;restore.q=originalQ;restore.p=originalP;restore.count=count;
    restore.owner={camera,owner,frame.activation,frame.controllers.referenceEpoch,now,frame.menuGeneration};
    // SKL_300_ASRROOT follows the solved pelvis through the descendant pass.
    // Keep the native loadout visible when that full body was admitted. The
    // arms-only fallback still suppresses its unplaced hip stock near the eye.
    if(!frame.body.valid&&count>53&&parent[53]==0&&get<uint32_t>(names+53*4)==0xec70c442)
        restore.stowedMount=get<uintptr_t>(reinterpret_cast<uintptr_t>(binding)+0x40)+53*64;
    std::memcpy(reinterpret_cast<void*>(rotations),q.data(),count*16u);
    std::memcpy(reinterpret_cast<void*>(positions),p.data(),count*16u);
    shotRig={owner,character,camera,model,nativeCamera,compose(*root,right->pose.wrist),frame.trackingSequence};
    if(!frame.controllers.hands[1].gripTracked||(vehicleWeaponScope(frame)&&!vehicleWeaponReady))shotRig={};
    const auto& throwingHand=frame.controllers.hands[1];
    if(shotRig.owner&&throwableActive&&frame.controllers.weaponReady&&!frame.controllers.vehicleControls&&throwingHand.aimTracked){
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
            <<" support_cup="<<compactSupport<<" native_hand_distance="<<std::sqrt(dot(nativeSupportInGrip.position,nativeSupportInGrip.position))
            <<" inspecting="<<inspecting<<" arm_helpers="<<helpersMatch<<" ground_contacts="<<groundContacts
            <<" left_head="<<leftView.x<<','<<leftView.y<<','<<leftView.z
            <<" right_head="<<rightView.x<<','<<rightView.y<<','<<rightView.z;log(s.str());
        if(frame.controllers.vehicleControls){
            const auto nativePalm=compose(*root,compose(leftAnimated,inverse(gripFromWrist[0])));
            const auto target=compose(frame.headPose,compose(Pose{{0,1,0,0},{}},compose(inverse(frame.nativePose),nativePalm)));
            std::ostringstream w;w<<"Driver wheel grip="<<wheel.gripped<<" hands="<<unsigned(wheel.hands)<<" axis="<<wheel.axis
                <<" local_contact="<<target.position.x<<','<<target.position.y<<','<<target.position.z;log(w.str());
        }
    }
    return true;
}
void noteCompletedAttachmentBody(void* context,void* binding,const PoseRestore& restore,bool paused) noexcept {
    const auto& frame=restore.attachmentDiagnosticFrame;
    if(!restore.count||!frame||frame->playerOwner!=restore.owner.player
        ||frame->activation!=restore.owner.activation
        ||frame->controllers.referenceEpoch!=restore.owner.referenceEpoch
        ||frame->menuGeneration!=restore.owner.menuGeneration||!ownsSkinTarget(context,binding))return;
    noteIdroidBodyComplete(reinterpret_cast<uintptr_t>(context),reinterpret_cast<uintptr_t>(binding),*frame,paused);
}
void update(void* context,void* binding){
    // Other skins never acquire the player lock, including work reached by the
    // native player publication. The verified skin path only builds matrices
    // and attachment transforms; it does not dispatch/wait for animation jobs.
    if(!enabled.load()||!headCamera().active()||!ownsSkinTarget(context,binding)){
        applySmallAnimalSkin(reinterpret_cast<uintptr_t>(binding));
        original(context,binding);
        return;
    }
    std::unique_lock publicationLock(skinPublicationMutex);
    PoseRestore restore;
    bool solved=false;
    if(enabled.load()&&headCamera().active())try{solved=apply(context,binding,restore);}catch(...){}
    const bool menuBoundary=restore.count&&hasOwnedRigMenuBoundary(pausedSkin.owner,restore.owner);
    if(restore.count){
        const auto driver=reinterpret_cast<uintptr_t>(context),target=reinterpret_cast<uintptr_t>(binding);
        const auto model=target-0xa0;
        pausedSkin={restore.owner,driver,target,get<uintptr_t>(driver+0x10),model,
            get<uintptr_t>(target+0x40),get<uintptr_t>(model+0xf0),restore.count};
        pausedParts={};
    }else{
        // Unrelated NPC and attachment publications do not hold the player
        // lock over their native work or replace the accepted player target.
        publicationLock.unlock();
    }
    applySmallAnimalSkin(reinterpret_cast<uintptr_t>(binding));
    original(context,binding);
    if(solved)noteCompletedAttachmentBody(context,binding,restore,false);
    const auto& attachmentFrame=restore.attachmentDiagnosticFrame;
    const bool heldIdroid=solved&&attachmentFrame&&((attachmentFrame->menuOpen&&attachmentFrame->menuIdroid)||attachmentFrame->rocketArmGuidance)
        &&attachmentFrame->controllers.handheldMenus&&!attachmentFrame->menuWorldQuad;
    if(menuBoundary||heldIdroid){
        // The native skin can already publish the new menu generation before
        // the prosthetic's separate update runs. That fresh body publication
        // prevents the ordinary paused timeout from repairing this first eye
        // image. Join the exact owned modular arm to this completed palette
        // at the observed entry/exit boundary, under the same player lock.
        // A freely walking iDroid also needs the cupped cinematic finger
        // palette joined to the separately rendered bionic hand each solve.
        const auto character=get<uintptr_t>(restore.owner.player+0x370);
        if(get<uintptr_t>(character)==base+0x2295210)refreshPausedArmParts(pausedSkin,character);
    }
    if(restore.stowedMount){
        auto matrix=get<std::array<float,16>>(restore.stowedMount);
        if(nativeAffinePose(matrix)){
            std::fill_n(matrix.begin(),12,0.f);
            std::memcpy(reinterpret_cast<void*>(restore.stowedMount),matrix.data(),sizeof(matrix));
        }
    }
}
std::optional<HeadCameraSample> weaponObstructionFrame(void* physics,void* query,uint64_t now,
                                                      bool exactCaller,WeaponObstructionMode& policy){
    policy=WeaponObstructionMode::native;
    if(!exactCaller||!enabled.load()||reinterpret_cast<uintptr_t>(query)<0xb0)return {};
    const auto frame=headCamera().publishedRigFrame(now);
    if(!frame||!frame->nativeFirearmActive)return {};
    const auto& owned=frame->weaponRig;
    const auto component=reinterpret_cast<uintptr_t>(query)-0xb0;
    const auto owner=frame->playerOwner;
    if(!owner||!owned.componentAccepted||!owned.instanceIndexMatched||!owned.resourceAccepted
       ||get<uintptr_t>(owner+0x370)!=owned.character
       ||get<uintptr_t>(component)!=base+0x23b3e80||get<uintptr_t>(component+8)!=owned.character
       ||get<uintptr_t>(owned.character+0x80)!=component
       ||get<uintptr_t>(reinterpret_cast<uintptr_t>(physics))!=base+0x24f3760)return {};
    const auto first=get<uint32_t>(get<uintptr_t>(component+0x38)+0x24);
    const auto index=get<uint32_t>(owner+0x3a0);
    if(index<first||index-first>15)return {};
    const auto state=get<uintptr_t>(component+0x58)+(index-first)*0x610ull;
    const auto weaponFlags=get<uint32_t>(state+0x27c);
    const auto mode=get<uint32_t>(state+0x3c0);
    if((weaponFlags&0x1c0)!=0x40||(weaponFlags&0x400000)||(mode&0x2000)
       ||(weaponFlags&0x200)!=(owned.flags&0x200))return {};
    const auto getter=get<uintptr_t>(get<uintptr_t>(owned.character+0x78)+0x238);
    const auto renderFirst=get<uint32_t>(getter+0xc);
    if(get<uintptr_t>(getter)!=base+0x22e6070||index<renderFirst||index-renderFirst>15
       ||get<uintptr_t>(get<uintptr_t>(getter+0x10)+(index-renderFirst)*0xc0ull+0x68)!=owned.bodyModel)
        return {};
    WeaponObstructionSample sample;
    sample.published={owner,owned.component,owned.state,reinterpret_cast<uintptr_t>(physics),
                      owned.playerIndex,owned.resourceHandle};
    sample.current={playerOwner.load(),component,state,get<uintptr_t>(component+0x70),
                    index,get<uint32_t>(state+0x240)};
    sample.frameTime=frame->sampleTime;sample.weaponTime=owned.sampleTime;
    sample.exactCaller=exactCaller;sample.enabled=enabled.load();sample.verified=true;
    sample.ready=frame->controllers.weaponReady;
    sample.tracked=frame->controllers.hands[1].gripTracked&&frame->renderedPalmTracked[1];
    sample.nativeControls=frame->controllers.nativeGamepad||frame->controllers.nativeActionControls;
    sample.vehicleAllowed=!vehicleWeaponScope(*frame)||vehicleWeaponAcknowledged(*frame,now);
    policy=weaponObstructionMode(sample,now,frame->controllers.blockFiringNearCover,
                                 frame->controllers.trackedWeaponObstruction);
    return frame;
}

// With blocking enabled, use the authored tracked barrel for this native
// firearm-sized sweep (radius 0.03 m, or 0.01 m for state+0x3c2 bit 0).
// With blocking disabled, still submit/consume the native asynchronous query;
// only its proximity hit count is ignored by the exact pre-fire consumer below.
bool weaponObstructionQuery(void* physics,void* query,uint32_t flags,
                            const Vector4* start,const Vector4* end,float radius,bool temporary){
    const auto native=[&]{return originalWeaponObstructionQuery(physics,query,flags,start,end,radius,temporary);};
    if(!start||!end)return native();
    WeaponObstructionMode policy;
    const auto frame=weaponObstructionFrame(physics,query,steadyMilliseconds(),
        reinterpret_cast<uintptr_t>(_ReturnAddress())==base+0x1064aae,policy);
    if(!frame||policy!=WeaponObstructionMode::trackedBarrel||!frame->weaponRig.muzzleSolved)return native();
    const auto& owned=frame->weaponRig;
    const auto& palm=frame->renderedPalms[1];
    const auto muzzle=compose(palm,owned.muzzleInGrip);
    if(!valid(palm)||!valid(muzzle)||!std::isfinite(radius)||radius<0.f||radius>1.f)return native();
    const auto forward=rotate(muzzle.orientation,{0,0,1});
    const float barrelLength=dot(muzzle.position-palm.position,forward);
    // Reject inverted/unsupported sockets rather than inventing a weapon
    // length. The rear endpoint lies on the barrel axis at the grip's depth;
    // neither the player's head nor the stock extends this test into cover.
    if(!std::isfinite(barrelLength)||barrelLength<.02f||barrelLength>2.f)return native();
    const auto rear=muzzle.position-forward*barrelLength;
    alignas(16) Vector4 trackedStart{rear.x,rear.y,rear.z,start->w};
    alignas(16) Vector4 trackedEnd{muzzle.position.x,muzzle.position.y,muzzle.position.z,end->w};
    return originalWeaponObstructionQuery(physics,query,flags,&trackedStart,&trackedEnd,radius,temporary);
}

int32_t weaponObstructionResult(void* physics,void* query){
    const bool exactCaller=reinterpret_cast<uintptr_t>(_ReturnAddress())==base+0x103b6d4;
    // This wait and native job cleanup must happen even when cover blocking is
    // off. Do not return early or edit the persistent query/weapon flags.
    const auto nativeCount=originalWeaponObstructionResult(physics,query);
    if(!exactCaller)return nativeCount;
    WeaponObstructionMode policy;
    const auto now=steadyMilliseconds();
    const auto frame=weaponObstructionFrame(physics,query,now,exactCaller,policy);
    const auto applied=weaponObstructionHitCount(nativeCount,policy);
    if(frame){std::lock_guard lock(obstructionWitnessMutex);
        obstructionWitness={now,frame->sampleTime,frame->activation,frame->playerOwner,nativeCount,applied,policy};}
    return applied;
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
    if(vehicleWeaponScope(frame)&&!vehicleWeaponAcknowledged(frame,steadyMilliseconds())){fallback();return;}
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
WeaponObstructionWitness weaponObstructionWitness(){std::lock_guard lock(obstructionWitnessMutex);return obstructionWitness;}
bool refreshPausedControllerRig(uintptr_t camera,Pose nativeCamera) noexcept {
    if(!enabled.load()||!modelTransformVerified)return false;
    try{
        std::unique_lock publicationLock(skinPublicationMutex,std::try_to_lock);
        if(!publicationLock.owns_lock()||!enabled.load())return false;
        const auto target=pausedSkin;
        const auto frame=headCamera().resolveCurrentForRig(camera,nativeCamera);
        if(!mayRefreshPausedRig(target.owner,camera,frame,steadyMilliseconds())
           ||playerOwner.load()!=target.owner.player)return false;
        // Revalidate the complete target chain on every menu frame. A load,
        // model replacement, retired driver or changed palette cannot reuse
        // the cached addresses even if its old allocation is still readable.
        const auto owner=target.owner.player;
        if(get<uintptr_t>(owner)!=base+0x23b8218||get<uintptr_t>(owner+0x380)!=camera)return false;
        const auto character=get<uintptr_t>(owner+0x370);
        if(get<uintptr_t>(character)!=base+0x2295210)return false;
        const auto component=get<uintptr_t>(character+0x10),holder=get<uintptr_t>(component+0x10);
        if(get<uintptr_t>(holder+0x68)!=target.model||get<uintptr_t>(holder+0x70)!=target.driver
           ||get<uintptr_t>(target.model)!=base+0x20f4d90
           ||target.binding!=target.model+0xa0||get<uintptr_t>(target.driver)!=base+0x24c1988
           ||get<uintptr_t>(target.driver+0x10)!=target.skeleton
           ||get<uintptr_t>(target.binding+0x40)!=target.matrices
           ||get<uintptr_t>(target.model+0xf0)!=target.parents
           ||get<uint16_t>(target.skeleton+0x48)!=target.count)return false;
        // apply rechecks the skeleton layout and solves from restored native
        // animation channels. original publishes matrices/attachment transforms
        // only (verified 1.0.15.4 skin routine); it does not run animation time.
        PoseRestore restore;
        if(!apply(reinterpret_cast<void*>(target.driver),reinterpret_cast<void*>(target.binding),restore)||!restore.count)return false;
        // Matrix publication alone leaves the paused renderer's model copy
        // unchanged. Re-submit the same root through the native transform
        // setter, which marks the model dirty without advancing animation.
        alignas(16) std::array<float,16> root{};
        if(!read(target.model+0x40,root)||!nativeAffinePose(root))return false;
        using SetTransform=void(*)(void*,const float*);
        reinterpret_cast<SetTransform>(base+0x1ce7f0)(reinterpret_cast<void*>(target.model),root.data());
        original(reinterpret_cast<void*>(target.driver),reinterpret_cast<void*>(target.binding));
        noteCompletedAttachmentBody(reinterpret_cast<void*>(target.driver),reinterpret_cast<void*>(target.binding),restore,true);
        const auto armParts=refreshPausedArmParts(target,character);
        // This boundary is now backed by freshly rebuilt native matrices.
        // Do not change nativeUpdateTime: no animation tick was advanced.
        pausedSkin.owner.menuGeneration=frame.menuGeneration;
        if(restore.stowedMount){
            auto matrix=get<std::array<float,16>>(restore.stowedMount);
            if(nativeAffinePose(matrix)){
                std::fill_n(matrix.begin(),12,0.f);
                std::memcpy(reinterpret_cast<void*>(restore.stowedMount),matrix.data(),sizeof(matrix));
            }
        }
        if(++pausedSkinRefreshes%300==1)log("Paused controller skin refreshed from tracked camera publication count="+std::to_string(pausedSkinRefreshes)+" modular_arms="+std::to_string(armParts));
        return true;
    }catch(...){return false;}
}
bool controllerThrowReady() noexcept {
    if(!enabled.load()||!headCamera().active())return false;
    std::lock_guard lock(rigMutex);
    return shotRig.throwTracked&&shotRig.owner==playerOwner.load();
}
void refreshNativeWeaponMagnifier(uintptr_t interface,uint32_t index) noexcept {
    if(!enabled.load()||!scopePartsVerified||interface!=magnifierOwnerInterface.load(std::memory_order_relaxed)
       ||index!=magnifierOwnerIndex.load(std::memory_order_relaxed))return;
    try{
        const auto frame=headCamera().publishedRigFrame(steadyMilliseconds());
        if(!frame||!frame->weaponScope.tracked||frame->weaponScope.magnification<=1
           ||!frame->controllers.weaponReady||!frame->weaponRig.magnifierModel
           ||frame->weaponRig.magnifierInterface!=interface||frame->weaponRig.magnifierIndex!=index)return;
        if(vehicleWeaponScope(*frame)&&!vehicleWeaponAcknowledged(*frame,steadyMilliseconds()))return;
        const auto& owned=frame->weaponRig;
        const auto first=get<uint32_t>(get<uintptr_t>(owned.component+0x38)+0x24);
        const auto playerIndex=get<uint32_t>(frame->playerOwner+0x3a0);
        if(frame->playerOwner!=playerOwner.load()||get<uintptr_t>(frame->playerOwner+0x370)!=owned.character
           ||get<uintptr_t>(owned.character+0x80)!=owned.component
           ||get<uintptr_t>(owned.component)!=base+0x23b3e80||get<uintptr_t>(owned.component+8)!=owned.character
           ||playerIndex<first||playerIndex-first>15
           ||owned.state!=get<uintptr_t>(owned.component+0x58)+(playerIndex-first)*0x610ull
           ||get<uint32_t>(owned.state+0x240)!=owned.resourceHandle
           ||get<uintptr_t>(interface)!=base+0x22e2970||index>=get<uint32_t>(interface+0x50))return;
        const auto models=get<uintptr_t>(interface+0x58),model=get<uintptr_t>(models+static_cast<uintptr_t>(index)*8);
        if(model!=owned.magnifierModel||get<uintptr_t>(model)!=base+0x20f4d90
           ||get<uint64_t>(model+0x120)!=owned.magnifierModelCode||get<uint16_t>(model+0xf8)!=2)return;
        const auto names=get<uintptr_t>(model+0xe8),palette=get<uintptr_t>(model+0xe0);
        if(get<uint32_t>(names)!=0xf08b256e||get<uint32_t>(names+4)!=0x6ef58063)return;
        std::array<float,16> root{};
        if(!read(palette,root))return;
        const auto rootPose=nativeAffinePose(root);if(!rootPose)return;
        // Retain this exact native publication's weapon transform. Only the
        // authored magnifier hinge changes; the base, gun and hands do not.
        const auto hinge=compose(*rootPose,Pose{{},{-.024f,.006f,0}});
        auto aligned=root;aligned[12]=hinge.position.x;aligned[13]=hinge.position.y;aligned[14]=hinge.position.z;
        if(get<uintptr_t>(models+static_cast<uintptr_t>(index)*8)!=model||get<uintptr_t>(model+0xe0)!=palette)return;
        SIZE_T written{};
        WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(palette+64),aligned.data(),sizeof(aligned),&written);
    }catch(...){}
}
void installControllerRig(uintptr_t imageBase,bool idroidEffectProbe,bool idroidEffectRetarget){
    base=imageBase;
    wchar_t trace[2]{};
    scopeTrace=GetEnvironmentVariableW(L"MGS5VR_SCOPE_TRACE",trace,2)==1&&trace[0]==L'1';
    const auto matches=[&]<size_t N>(uintptr_t rva,const std::array<unsigned char,N>& bytes){
        std::array<unsigned char,N> actual{};return read(base+rva,actual)&&actual==bytes;
    };
    modelTransformVerified=matches(0x1ce7f0,std::array<unsigned char,12>{
        0x40,0x53,0x48,0x83,0xec,0x20,0x0f,0x28,0x02,0x48,0x8b,0xd9});
    scopeQueryVerified=matches(0xdbdc50,std::array<unsigned char,17>{
        0x4c,0x89,0x4c,0x24,0x20,0x44,0x89,0x44,0x24,0x18,0x55,0x53,0x57,0x48,0x8d,0x6c,0x24});
    log(scopeQueryVerified?"Native named sight-point adapter enabled":"Native sight-point signature differs; weapon scopes disabled");
    scopePartsVerified=matches(0xd45930,std::array<unsigned char,10>{
        0x41,0x83,0xf8,0x0b,0x0f,0x87,0xb9,0,0,0});
    log(scopePartsVerified?"Native assembled magnifier adapter enabled":"Native magnifier part signature differs; flip sights disabled");
    groundQueryVerified=matches(0xa0fb50,std::array<unsigned char,9>{0x48,0x89,0x4c,0x24,0x08,0x48,0x83,0xec,0x18})
        &&matches(0x1b9b130,std::array<unsigned char,10>{0x48,0x83,0xec,0x38,0xf3,0x0f,0x10,0x44,0x24,0x60})
        &&matches(0x1b9b21e,std::array<unsigned char,18>{0xf3,0x0f,0x10,0x84,0x24,0xe0,0,0,0,0x0f,0xc6,0xc0,0,0x0f,0x29,0x44,0x24,0x50})
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
    // Physics interface +0x28 submits an asynchronous query; +0x30 consumes it.
    // Verify both the wrapper and the sole weapon call site before installing
    // a hook which leaves all other native callers byte-for-byte equivalent.
    if(!matches(0x1bf0fc0,std::array<unsigned char,11>{0x40,0x53,0x48,0x83,0xec,0x30,0x0f,0xb6,0x44,0x24,0x70})
       ||!matches(0x1064aa8,std::array<unsigned char,13>{0x44,0x8b,0xc7,0xff,0x50,0x28,0x83,0x8b,0x50,0x03,0,0,1})
       ||!matches(0x1bf1040,std::array<unsigned char,8>{0x48,0x8b,0xca,0xe9,0x48,0xa7,0xfa,0xff})
       ||!matches(0x1b9b790,std::array<unsigned char,10>{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20})
       ||!matches(0x103b6c3,std::array<unsigned char,22>{0x48,0x8b,0x49,0x70,0x48,0x8d,0x96,0xb0,0,0,0,
           0x48,0x8b,0x01,0xff,0x50,0x30,0x85,0xc0,0x0f,0x4f,0xef})
       ||get<uintptr_t>(base+0x24f3760+0x28)!=base+0x1bf0fc0
       ||get<uintptr_t>(base+0x24f3760+0x30)!=base+0x1bf1040)
        throw std::runtime_error("Controller weapon obstruction query signature differs");
    const auto obstructionAddress=reinterpret_cast<void*>(base+0x1bf0fc0);
    status=MH_CreateHook(obstructionAddress,reinterpret_cast<void*>(&weaponObstructionQuery),
                         reinterpret_cast<void**>(&originalWeaponObstructionQuery));
    if(status!=MH_OK)throw std::runtime_error(std::string("Controller weapon obstruction create: ")+MH_StatusToString(status));
    status=MH_EnableHook(obstructionAddress);
    if(status!=MH_OK){MH_RemoveHook(obstructionAddress);throw std::runtime_error(std::string("Controller weapon obstruction enable: ")+MH_StatusToString(status));}
    const auto obstructionResultAddress=reinterpret_cast<void*>(base+0x1bf1040);
    status=MH_CreateHook(obstructionResultAddress,reinterpret_cast<void*>(&weaponObstructionResult),
                         reinterpret_cast<void**>(&originalWeaponObstructionResult));
    if(status!=MH_OK)throw std::runtime_error(std::string("Controller weapon obstruction result create: ")+MH_StatusToString(status));
    status=MH_EnableHook(obstructionResultAddress);
    if(status!=MH_OK){MH_RemoveHook(obstructionResultAddress);throw std::runtime_error(std::string("Controller weapon obstruction result enable: ")+MH_StatusToString(status));}
    log("Weapon cover setting adapter installed for native submit 0x1064aae and result 0x103b6d4");
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
    installSmallAnimalInteraction(base);
    installIdroidEffectProbe(base,idroidEffectProbe,idroidEffectRetarget);
    installIdroidAttachmentDiagnostics(base);
    enabled.store(true);log("Experimental controller rig installed at verified skin publication RVA 0x1a6caa0 and shot solver 0x1044ff0");
}
void observeControllerRigOwner(uintptr_t owner) noexcept{
    if(playerOwner.load()==owner)return;
    std::lock_guard publicationLock(skinPublicationMutex);
    playerOwner.store(owner);pausedSkin={};pausedParts={};
}
void stopControllerRig() noexcept{
    {
        std::lock_guard publicationLock(skinPublicationMutex);
        enabled.store(false);playerOwner.store(0);pausedSkin={};pausedParts={};
    }
    stopIdroidAttachmentDiagnostics();stopIdroidEffectProbe();stopSmallAnimalInteraction();stopMotionMelee();
    {std::lock_guard lock(rigMutex);for(auto& cache:handPresentation)cache.reset();stationaryBody.reset();}
}
bool controllerRigEnabled() noexcept{return enabled.load();}
void publishNativeCabinBounds(HeadCameraSample& frame) noexcept{
    static bool cacheActive{};
    static uint64_t measuredActivation{};
    static uint64_t nextAttempt{};
    static bool reportedInvalid{};
    static NativeRoomBounds cached{};
    static Pose worldOrigin{};
    static bool worldAnchored{};
    static CabinWalkState walk{};
    static CabinWalkState transitWalk{};
    static uint64_t nextWalkReport{};
    const auto& c=frame.controllers;
    if(c.cabinTransit){
        frame.controllers.cabinBounds={};
        frame.controllers.openingWorldAnchored=false;
        if(!frame.cabinBaseTracked){
            transitWalk.lastUpdateMs=steadyMilliseconds();
            return;
        }
        frame.nativePose.position=advanceCabinWalk(transitWalk,frame.nativePose.position,
            frame.nativePose.orientation,c.cabinMove,steadyMilliseconds(),frame.activation,
            moveNativeCabin,.85f,frame.menuOpen&&!c.handheldMenuFree,true,frame.cabinBasePose);
        return;
    }
    transitWalk={};
    if(!c.openingSelector&&!c.cabinPlay){
        cacheActive=false;
        measuredActivation=0;
        nextAttempt=0;
        reportedInvalid=false;
        cached={};
        worldAnchored=false;
        walk={};
        nextWalkReport=0;
        frame.controllers.cabinBounds={};
        return;
    }
    const auto now=steadyMilliseconds();
    if(!cacheActive||measuredActivation!=frame.activation){
        cacheActive=true;
        measuredActivation=frame.activation;
        nextAttempt=0;
        reportedInvalid=false;
        cached={};
        worldAnchored=false;
        walk={};
        nextWalkReport=0;
    }
    if(!worldAnchored){
        worldOrigin=nativeTrackedPose(frame.nativePose,frame.headPose,c.openingOrigin);
        worldAnchored=valid(worldOrigin);
    }
    frame.controllers.openingWorldOrigin=worldOrigin;
    frame.controllers.openingWorldAnchored=worldAnchored;
    if(worldAnchored)frame.controllers.openingLayout=nativeCabinLayout(worldOrigin,frame.activation);
    const auto applyRoomMovement=[&](){
        const auto before=frame.nativePose.position;
        const auto previousOffset=walk.offset;
        frame.nativePose.position=advanceCabinWalk(walk,before,frame.nativePose.orientation,
            c.cabinMove,now,frame.activation,moveNativeCabin,.85f,frame.menuOpen&&!c.handheldMenuFree);
        const auto moved=frame.nativePose.position-(before+previousOffset);
        const float stickMagnitude=std::hypot(c.cabinMove[0],c.cabinMove[1]);
        if((!frame.menuOpen||c.handheldMenuFree)&&stickMagnitude>.2f&&dot(moved,moved)>.000001f&&now>=nextWalkReport){
            std::ostringstream s;s<<"Native title-cabin stick walk axis="<<c.cabinMove[0]<<','<<c.cabinMove[1]
                <<" step="<<moved.x<<','<<moved.y<<','<<moved.z
                <<" head="<<frame.nativePose.position.x<<','<<frame.nativePose.position.y<<','<<frame.nativePose.position.z;
            log(s.str());nextWalkReport=now+500;
        }
    };
    if(validNativeRoomBounds(cached)||now<nextAttempt){
        frame.controllers.cabinBounds=cached;
        applyRoomMovement();
        return;
    }
    const auto origin=worldOrigin;
    NativeRoomSamples samples{};
    const auto measured=measureNativeCabin(origin,frame.activation,samples);
    if(!reportedInvalid){
        std::ostringstream s;s<<"Native cabin rays layer=0x800 origin="<<origin.position.x<<','<<origin.position.y<<','<<origin.position.z
            <<" generation="<<frame.activation;
        constexpr std::array<const char*,6> names{{"+x","-x","ceiling","floor","+z","-z"}};
        for(size_t i=0;i<samples.size();++i){s<<' '<<names[i]<<'=';if(samples[i])s<<*samples[i];else s<<"miss";}
        log(s.str());
    }
    cached=measured;
    frame.controllers.cabinBounds=measured;
    if(validNativeRoomBounds(measured)){
        const auto size=nativeRoomDimensions(measured);
        std::ostringstream s;s<<"Native cabin sampled collision envelope x="<<size.x<<" y="<<size.y<<" z="<<size.z
            <<" local_min="<<measured.min.x<<','<<measured.min.y<<','<<measured.min.z
            <<" local_max="<<measured.max.x<<','<<measured.max.y<<','<<measured.max.z
            <<" generation="<<measured.sceneGeneration;log(s.str());
        const auto ray=collisionCast(origin.position,origin.position-Vec3{0,2.f,0});
        const auto sphere=collisionCast(origin.position,origin.position-Vec3{0,2.f,0},.12f);
        if(ray.point&&sphere.point){
            std::ostringstream cast;cast<<"Native cabin clearance floor ray="<<ray.point->y
                <<" sphere="<<sphere.point->y<<" normal="<<sphere.normal.y
                <<" radius=0.12 camera_sweeps=enabled";log(cast.str());
        }
    }else if(!reportedInvalid){
        log("Native cabin actor envelope unavailable; movement still requires native clearance and floor queries");
        reportedInvalid=true;
    }
    nextAttempt=now+1000;
    applyRoomMovement();
}
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
            const auto mode=travelModeFromPlayerStatus(flags);
            if(mode!=cached){std::ostringstream s;s<<"Native travel mode="<<static_cast<int>(mode)
                <<" player="<<index<<" flags="<<std::hex<<flags[0]<<','<<flags[1];log(s.str());}
            cached=mode;sampledAt=now;return mode;
        }
    }
    return now>=sampledAt&&now-sampledAt<=150?cached:TravelMode::unknown;
}
NativeVehicleView nativeVehicleView() noexcept{
    // Same local double-buffered status as nativeTravelMode. The retail
    // GetGameObjectIdIsRiddenToLocal getter reads its object ID at +b6.
    if(!enabled.load())return {};
    const auto owner=playerOwner.load();
    const auto index=get<uint32_t>(base+0x2bd3fd4);
    if(get<uintptr_t>(owner)!=base+0x23b8218||index>=16||get<uint32_t>(owner+0x3a0)!=index)return {};
    for(unsigned attempt=0;attempt<2;++attempt){
        const auto phase=get<uint32_t>(base+0x2bd3fd0);if(phase>1)return {};
        const auto state=base+0x2bd3ff0+(phase*16ull+index)*0xe0;
        std::array<uint32_t,5> flags{};Vec3 stateRoot{},ownerRoot{};uint16_t ridden{};
        if(get<uintptr_t>(state)!=base+0x2190b28||!read(state+0xc0,flags)
           ||!read(state+0x40,stateRoot)||!read(owner+0x60,ownerRoot)||!read(state+0xb6,ridden))return {};
        const auto difference=stateRoot-ownerRoot;
        if(!valid(Pose{{},difference})||dot(difference,difference)>4)return {};
        NativeVehicleView result{true,(flags[0]&(1u<<7))!=0,(flags[0]&(1u<<26))!=0,false,ridden,0};
        // SendCommand's registry resolves type and instance separately. Read
        // only the occupied TppVehicle2 record, never a nearby guessed vehicle.
        if((ridden>>9)==50){
            const auto registry=get<uintptr_t>(base+0x2c389a0);
            const auto entry=get<uintptr_t>(registry+0x30+50*8);
            const auto entity=get<uintptr_t>(get<uintptr_t>(entry+8)+0x20);
            const auto slot=unsigned(ridden&511);
            const auto records=get<uintptr_t>(entity+0x30);
            const auto active=get<uintptr_t>(get<uintptr_t>(entity+0x1c8)+8);
            Vec3 position{};float heading{};
            if(get<uintptr_t>(registry)==base+0x23073c0&&get<uintptr_t>(entity)==base+0x22ed930
               &&slot<get<uint16_t>(entity+0x1e0)&&records&&active
               &&(get<uint32_t>(active+(slot/32)*4)&(1u<<(slot%32)))
               &&read(records+slot*128ull+0x20,position)&&read(records+slot*128ull+0x2c,heading)
               &&valid(Pose{{},position})&&std::isfinite(heading)
               &&dot(position-ownerRoot,position-ownerRoot)<400
               &&records==get<uintptr_t>(entity+0x30)){
                result.heading=heading;result.headingValid=true;
            }
        }
        if(phase!=get<uint32_t>(base+0x2bd3fd0))continue;
        if(owner!=playerOwner.load()||index!=get<uint32_t>(base+0x2bd3fd4)
           ||ridden!=get<uint16_t>(state+0xb6))return {};
        return result;
    }
    return {};
}
PlayerPosture nativePlayerPosture() noexcept{
    // Live Lua registration was read from the owned game: STAND=4,
    // SQUAT=5, CRAWL=6. Join the same double-buffered local-player table
    // used by AndCheckStatus; animation height never decides the stance.
    if(!enabled.load())return PlayerPosture::unknown;
    const auto owner=playerOwner.load();const auto index=get<uint32_t>(base+0x2bd3fd4);
    if(get<uintptr_t>(owner)!=base+0x23b8218||index>=16||get<uint32_t>(owner+0x3a0)!=index)return PlayerPosture::unknown;
    for(int attempt=0;attempt<2;++attempt){
        const auto phase=get<uint32_t>(base+0x2bd3fd0);if(phase>1)break;
        const auto state=base+0x2bd3ff0+(phase*16ull+index)*0xe0;
        std::array<uint32_t,5> flags{};Vec3 stateRoot{},ownerRoot{};
        if(get<uintptr_t>(state)!=base+0x2190b28||!read(state+0xc0,flags)||!read(state+0x40,stateRoot)||!read(owner+0x60,ownerRoot))break;
        if(phase!=get<uint32_t>(base+0x2bd3fd0))continue;
        const auto delta=stateRoot-ownerRoot;
        if(!valid(Pose{{},delta})||dot(delta,delta)>4||owner!=playerOwner.load()||index!=get<uint32_t>(base+0x2bd3fd4))break;
        switch(flags[0]&0x70u){case 0x10:return PlayerPosture::standing;case 0x20:return PlayerPosture::crouched;case 0x40:return PlayerPosture::prone;default:break;}
    }
    return PlayerPosture::unknown;
}
}
