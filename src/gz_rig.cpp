#include "mgs5vr/gz_rig.hpp"
#include "mgs5vr/arm_ik.hpp"
#include "mgs5vr/head_camera.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/idroid_native_hold.hpp"
#include "mgs5vr/log.hpp"
#include <windows.h>
#include <MinHook.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>
#include <stdexcept>

namespace mgs5vr {
namespace {
using SkinFn=uintptr_t(*)(void*,void*);
SkinFn original{};
using AimFn=uintptr_t(*)(void*);
AimFn originalAim{};
using AttachmentFn=void(*)(void*,void*,void*,void*,void*,uint32_t);
AttachmentFn originalAttachment{};
thread_local uintptr_t heldIdroidPoint{};
uintptr_t image{};
std::atomic_bool enabled{};
std::atomic_uintptr_t bodyBinding{};
std::mutex publicationMutex;
uintptr_t sceneCamera{};
uint64_t observedAt{};
GzPlayerHead observed;
std::array<HandPresentationCache,2> handCache;
std::array<ArmTwistState,2> twists;
SupportContact supportContact;
RigContinuityKey supportKey{};
uintptr_t supportWeapon{};
struct PublishedBarrel {
    uintptr_t entity{},model{},weapon{};
    uint64_t time{};
    Pose muzzle{};
    std::array<float,16> wrist{};
};
PublishedBarrel publishedBarrel;
struct V4 {float x,y,z,w;};
template<class T> bool read(uintptr_t p,T& value){
    SIZE_T copied{};
    return p&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(p),&value,sizeof(value),&copied)&&copied==sizeof(value);
}
template<class T> T get(uintptr_t p){T value{};read(p,value);return value;}
bool copy(uintptr_t p,void* output,size_t bytes){
    SIZE_T copied{};return p&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(p),output,bytes,&copied)&&copied==bytes;
}
std::array<float,16> affine(Pose p){
    std::array<float,16> m{};
    const std::array<Vec3,3> axes{Vec3{1,0,0},Vec3{0,1,0},Vec3{0,0,1}};
    for(size_t i=0;i<3;++i){const auto a=rotate(p.orientation,axes[i]);m[i*4]=a.x;m[i*4+1]=a.y;m[i*4+2]=a.z;}
    m[12]=p.position.x;m[13]=p.position.y;m[14]=p.position.z;m[15]=1;return m;
}
uintptr_t idroidPoint(uintptr_t driver){
    const auto count=get<uint32_t>(driver+0x38);
    const auto table=get<uintptr_t>(driver+0x40);
    if(!table||count>128)return 0;
    uintptr_t found{};
    for(uint32_t i=0;i<count;++i){
        const auto point=get<uintptr_t>(table+i*8);
        if(get<uintptr_t>(point)!=image+0x1808708||get<uint64_t>(point+0x10)!=0x1c68632c5c53ull
           ||get<uint16_t>(point+0xa4)!=3||get<int16_t>(point+0xa6)!=12
           ||get<uint32_t>(point+0xac)!=0x20000)continue;
        if(found)return 0;
        found=point;
    }
    return found;
}
void attachment(void* raw,void* palette,void* root,void* remap,void* indices,uint32_t count){
    const auto point=reinterpret_cast<uintptr_t>(raw);
    // GZ variant 3 is independent of TPP's packed variant 4. The native
    // updater consumes +20 as local input and publishes world output there.
    // Touch only this player's selected point inside its own skin call.
    if(!heldIdroidPoint||point!=heldIdroidPoint||get<uint32_t>(point+0xa0)!=1
       ||get<uint32_t>(point+0xac)!=0x20000||get<int16_t>(point+0xa6)!=12){
        originalAttachment(raw,palette,root,remap,indices,count);return;
    }
    const auto local=affine(nativeIdroidLeftBodyInWrist);
    std::memcpy(reinterpret_cast<void*>(point+0x20),local.data(),sizeof(local));
    *reinterpret_cast<int16_t*>(point+0xa6)=8;
    originalAttachment(raw,palette,root,remap,indices,count);
    *reinterpret_cast<int16_t*>(point+0xa6)=12;
    static std::atomic_bool reported{};
    if(!reported.exchange(true))log("GZ native iDroid attachment published from left wrist");
}
std::optional<Pose> muzzleInWrist(uintptr_t* weapon=nullptr){
    // GZ's GetActiveWeaponMuzzleMatrix resolves component 28, its active
    // weapon object, then the weapon Character's authored CNP. Read the same
    // socket without the native query's fallback to the entity origin.
    const auto table=get<uintptr_t>(observed.entity+0x68);
    const auto interface=table?get<uintptr_t>(table+28*8):0;
    if(interface<0x28)return {};
    const auto component=interface-0x28;
    if(get<uintptr_t>(component)!=image+0x1749470||get<uintptr_t>(component+0x30)!=observed.entity)return {};
    const auto kind=get<uint32_t>(component+0x170);
    constexpr std::array<uint32_t,6> offsets{0x178,0x198,0,0x188,0x1a8,0};
    if(kind>=offsets.size()||!offsets[kind])return {};
    const auto slot=component+offsets[kind],object=get<uintptr_t>(slot);
    if(!object||get<uintptr_t>(object)!=image+0x16c30d0)return {};
    const auto entity=get<uintptr_t>(object+0x38),weaponTable=get<uintptr_t>(entity+0x68);
    const auto characterInterface=weaponTable?get<uintptr_t>(weaponTable):0;
    if(characterInterface<0x28)return {};
    const auto character=characterInterface-0x28;
    if(get<uintptr_t>(character)!=image+0x15f7460||get<uintptr_t>(character+0x30)!=entity
       ||!get<uintptr_t>(character+0x1f8)||!get<uint32_t>(character+0x230))return {};
    const auto palette=get<uintptr_t>(observed.model+0xe0);
    alignas(16) std::array<float,16> wrist{},again{},muzzle{};
    if(!palette||!read(palette+12*64,wrist))return {};
    const auto alternate=get<uint8_t>(slot+0xc);
    if(alternate>1)return {};
    using Query=bool(*)(void*,uint64_t,void*,bool);
    const uint64_t socket=alternate?0xf7a333e75173ull:0xb746d955f0a9ull;
    if(!reinterpret_cast<Query>(image+0x2d3090)(reinterpret_cast<void*>(character),socket,muzzle.data(),false)
       ||!read(palette+12*64,again)||again!=wrist||get<uintptr_t>(slot)!=object
       ||get<uint32_t>(component+0x170)!=kind)return {};
    const auto w=nativeAffinePose(wrist),m=nativeAffinePose(muzzle);
    if(!w||!m)return {};
    const auto local=compose(inverse(*w),*m);
    const float distance=dot(local.position,local.position);
    if(!(distance>.0001f&&distance<2.25f))return {};
    if(weapon)*weapon=object;
    return local;
}
uintptr_t aimUpdate(void* raw){
    // ChAimPlugin's native job resolves collision and recoil from the GrCamera
    // basis at +30..60. Give only this call a private basis from the last
    // successfully published native weapon. Never change the shared camera.
    const auto component=reinterpret_cast<uintptr_t>(raw);
    PublishedBarrel barrel;
    {std::lock_guard lock(publicationMutex);barrel=publishedBarrel;}
    const auto now=steadyMilliseconds();
    const auto menu=enabled.load()?readGzMenuState(image,copy):std::nullopt;
    if(!menu||menu->pause||menu->idroid||!barrel.time||now<barrel.time||now-barrel.time>100
       ||get<uintptr_t>(component)!=image+0x1608948||get<uintptr_t>(component+0x30)!=barrel.entity)
        return originalAim(raw);
    const auto table=get<uintptr_t>(barrel.entity+0x68);
    const auto weaponInterface=table?get<uintptr_t>(table+28*8):0;
    const auto aimInterface=table?get<uintptr_t>(table+79*8):0;
    if(aimInterface!=component+0x28||weaponInterface<0x28)return originalAim(raw);
    constexpr std::array<uint32_t,6> offsets{0x178,0x198,0,0x188,0x1a8,0};
    const auto weaponComponent=weaponInterface-0x28;
    const auto kind=get<uint32_t>(weaponComponent+0x170);
    if(get<uintptr_t>(weaponComponent)!=image+0x1749470||kind>=offsets.size()||!offsets[kind]
       ||get<uintptr_t>(weaponComponent+offsets[kind])!=barrel.weapon)return originalAim(raw);
    const auto palette=get<uintptr_t>(barrel.model+0xe0);
    std::array<float,16> wrist{};
    if(!palette||!read(palette+12*64,wrist)||wrist!=barrel.wrist)return originalAim(raw);
    const auto source=get<uintptr_t>(component+0x1a0);
    alignas(16) std::array<unsigned char,0x70> local{};
    const auto offset=get<float>(component+0x120);
    if(!source||!copy(source,local.data(),local.size())||!std::isfinite(offset)||std::abs(offset)>10.f)
        return originalAim(raw);
    auto basis=affine(barrel.muzzle);
    const auto direction=rotate(barrel.muzzle.orientation,{0,0,1});
    basis[12]-=direction.x*offset;basis[13]-=direction.y*offset;basis[14]-=direction.z*offset;
    std::memcpy(local.data()+0x30,basis.data(),sizeof(basis));
    struct RestoreCamera {
        uintptr_t* address;uintptr_t original;
        ~RestoreCamera(){*address=original;}
    } restore{reinterpret_cast<uintptr_t*>(component+0x1a0),source};
    *restore.address=reinterpret_cast<uintptr_t>(local.data());
    const auto result=originalAim(raw);
    static std::atomic_bool reported{};
    if(!reported.exchange(true))log("GZ native aim collision query follows published weapon muzzle");
    return result;
}
struct Restore {
    uintptr_t rotations{},positions{};uint16_t count{};
    std::array<Quat,512> q{};std::array<V4,512> p{};
    ~Restore(){if(count){
        std::memcpy(reinterpret_cast<void*>(rotations),q.data(),count*16u);
        std::memcpy(reinterpret_cast<void*>(positions),p.data(),count*16u);
    }}
};
bool owned(uintptr_t driver,uintptr_t binding){
    const auto& p=observed;
    if(!sceneCamera||!p.model||binding!=p.model+0xa0||get<uintptr_t>(p.player+0x38)!=p.entity
       ||get<uintptr_t>(p.character)!=image+0x15f7460||get<uintptr_t>(p.model)!=image+0x18437c0)return false;
    return get<uintptr_t>(get<uintptr_t>(p.character+0x238))==p.model
        &&get<uintptr_t>(get<uintptr_t>(p.character+0x248))==driver&&get<uintptr_t>(driver)==image+0x1807cf0;
}
bool solve(uintptr_t driver,uintptr_t binding,Restore& restore,bool& barrelReady,uintptr_t& heldPoint){
    const auto skeleton=get<uintptr_t>(driver+0x10);
    const auto count=get<uint16_t>(skeleton+0x48);
    if(!skeleton||count!=138||get<int16_t>(skeleton+0x4a)!=0||get<uint32_t>(skeleton+0x60)!=0)return false;
    const auto qo=get<uint32_t>(skeleton+0x58),po=get<uint32_t>(skeleton+0x5c);
    if(qo<0x90||po<qo+count*16u||po>0x10000)return false;
    std::array<Quat,512> q{};std::array<V4,512> p{};
    std::array<int32_t,512> parents{};std::array<uint32_t,512> names{};
    const auto parentRecords=get<uintptr_t>(observed.model+0xf0);
    if(!copy(skeleton+qo,q.data(),count*16u)||!copy(skeleton+po,p.data(),count*16u)
       ||!copy(get<uintptr_t>(observed.model+0xe8),names.data(),count*4u))return false;
    const auto bone=[&](size_t i){return Pose{q[i],{p[i].x,p[i].y,p[i].z}};};
    const auto put=[&](size_t i,Pose v){q[i]=v.orientation;p[i].x=v.position.x;p[i].y=v.position.y;p[i].z=v.position.z;};
    for(size_t i=0;i<count;++i){
        if(!read(parentRecords+i*32,parents[i])||parents[i]<-1||parents[i]>=static_cast<int32_t>(i)||!valid(bone(i)))return false;
    }
    if(names[4]!=0xc3a88a1b||names[8]!=0x3e2d77e5||names[12]!=0x38b1433c
       ||names[24]!=0x16288b3b||names[34]!=0x83c7c2bb||names[40]!=0x82706889||names[50]!=0x37bbdc2c
       ||parents[6]!=5||parents[7]!=6||parents[8]!=7||parents[10]!=9||parents[11]!=10||parents[12]!=11
       ||parents[24]!=8||parents[30]!=8||parents[34]!=30||parents[40]!=12||parents[46]!=12||parents[50]!=46)return false;
    const auto helpers=armHelperIndices({names.data(),count},{parents.data(),count});
    if(!helpers)return false;
    std::array<float,16> rootMatrix{};std::array<float,8> sourceValues{};
    if(!read(binding,rootMatrix)||!read(observed.camera+0xc0,sourceValues))return false;
    const auto root=nativeAffinePose(rootMatrix);if(!root)return false;
    const Pose source{{sourceValues[0],sourceValues[1],sourceValues[2],sourceValues[3]},
        {sourceValues[4],sourceValues[5],sourceValues[6]}};
    // Verify this is still the selected native player camera, not a previous
    // mode's output. The renderer owns sceneCamera; the skin owns the pose.
    const auto current=readGzPlayerHead(image,source,copy);
    if(!current||current->entity!=observed.entity||current->model!=observed.model||current->camera!=observed.camera)return false;
    if(!headCamera().publishPlayerHead(sceneCamera,observed.entity,source,rootMatrix,affine(bone(4)),steadyMilliseconds()))return false;
    // Join the anatomical eye from this original skin, before any tracked
    // retargeting. A camera-only HEAD publication lacks this body identity.
    const NativeBodyHead nativeHead{observed.entity,observed.model,compose(*root,bone(4)),*root,compose(*root,bone(2)),true};
    auto frame=headCamera().resolveCurrentForRig(sceneCamera,source,nativeHead);
    if(!frame.applied||frame.playerOwner!=observed.entity||frame.controllers.scriptedDemo||frame.controllers.frontEnd)return false;
    const RigContinuityKey key{observed.entity,observed.model,frame.activation,
        frame.controllers.referenceEpoch,frame.controllers.presentationEpoch};
    const auto originalQ=q;const auto originalP=p;
    auto placedQ=q;auto placedP=p;
    const auto nativeBone=[&](size_t i){return Pose{placedQ[i],{placedP[i].x,placedP[i].y,placedP[i].z}};};
    if(parents[1]==0&&parents[2]==1&&parents[3]==2&&parents[4]==3){
        const auto forward=rotate(frame.nativePose.orientation,{0,0,1});
        const float yaw=std::atan2(forward.x,forward.z);
        const Pose upright=compose(inverse(*root),Pose{{0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)},frame.nativePose.position});
        if(frame.controllers.fullBody){
            // Fit lumbar/chest/neck around the fixed eyes. Pelvis, feet and
            // both native leg chains stay untouched; unreachable poses retain
            // their authored torso instead of stretching any segment.
            const Pose targetHead{upright.orientation,upright.position-rotate(upright.orientation,nativeEyeFromHead)};
            const auto headDelta=compose(targetHead,inverse(bone(4)));
            const auto spine=solveSpine({bone(1),bone(2),bone(3)},compose(headDelta,bone(3)));
            if(spine&&!spine->reachClamped){
                const auto chestDelta=compose(spine->pose.elbow,inverse(bone(2)));
                const auto neckDelta=compose(spine->pose.wrist,inverse(bone(3)));
                put(1,spine->pose.shoulder);put(2,spine->pose.elbow);put(3,spine->pose.wrist);
                for(size_t i=4;i<count;++i){
                    bool upper=false,head=false;
                    for(auto ancestor=parents[i];ancestor>=0;ancestor=parents[ancestor]){
                        if(ancestor==3)head=true;
                        if(ancestor==2){upper=true;break;}
                    }
                    if(upper)put(i,compose(head?neckDelta:chestDelta,nativeBone(i)));
                }
            }
        }else if(const auto placement=upperBodyPlacement(bone(2),(bone(6).position+bone(10).position)*.5f,upright)){
            // Hidden torso: use the shared fitted shoulder line for the
            // visible hands/forearms, keeping their authored arm lengths.
            for(size_t i=2;i<count;++i){
                bool upper=i==2;
                for(auto ancestor=parents[i];!upper&&ancestor>=0;ancestor=parents[ancestor])if(ancestor==2)upper=true;
                if(upper)put(i,compose(*placement,nativeBone(i)));
            }
        }
        placedQ=q;placedP=p;
    }
    const auto devicePoint=frame.menuOpen&&frame.menuIdroid&&frame.controllers.handheldMenus
        &&!frame.menuWorldQuad&&!frame.controllers.nativeGamepad?idroidPoint(driver):0;
    const bool idroidHeld=devicePoint&&matchesNativeIdroidLeftHand({names.data(),count},{parents.data(),count});
    if(idroidHeld)for(const auto& joint:nativeIdroidLeftHand){
        const auto offset=get<Vec3>(parentRecords+joint.bone*32+16);
        const auto difference=offset-joint.local.position;
        // Owned GZ hand translations match the native cinematic contact.
        // Preserve GZ's own lengths even when applying those rotations.
        if(!valid(Pose{{},offset})||dot(difference,difference)>.000001f)return false;
    }
    std::array<bool,512> changed{};
    std::array<bool,2> tracked{};
    constexpr std::array<size_t,2> wrists{8,12},index{24,40},little{34,50};
    std::array<Pose,2> palmFromWrist{};
    uintptr_t weapon{};
    const bool binocularHeld=frame.controllers.optic.held&&frame.controllers.optic.pose.tracked
        &&frame.controllers.optic.pose.kind==OpticKind::binocular;
    bool binocularSupport=binocularHeld&&frame.controllers.optic.pose.supportHeld&&!idroidHeld;
    const auto muzzle=frame.controllers.weaponReady&&!binocularHeld?muzzleInWrist(&weapon):std::nullopt;
    for(size_t side=0;side<2;++side){
        const auto wrist=wrists[side];
        const auto palm=anatomicalGrip(bone(wrist),bone(index[side]).position,bone(little[side]).position);
        if(!palm)return false;
        palmFromWrist[side]=compose(inverse(*palm),bone(wrist));
    }
    if(idroidHeld){const auto wrist=nativeIdroidLeftWristFromGrip();if(!wrist)return false;palmFromWrist[0]=*wrist;}
    if(key!=supportKey||weapon!=supportWeapon){supportContact.reset();supportKey=key;supportWeapon=weapon;}
    const auto supportInWrist=compose(inverse(nativeBone(12)),nativeBone(8));
    bool supportAttached=false;
    // The primary wrist publishes the weapon first; acquire the secondary
    // palm only at its actual authored contact using the shared hysteresis.
    for(const size_t side:{1u,0u}){
        const auto wrist=wrists[side];
        const auto& hand=frame.controllers.hands[side];
        const auto presentation=handCache[side].update({hand.grip,hand.aim},
            hand.gripTracked&&hand.aimTracked,frame.controllers.presentationFocused,frame.sampleTime,key);
        if(!presentation||frame.controllers.nativeGamepad)continue;
        auto target=compose(inverse(*root),nativeTrackedPose(frame.nativePose,frame.headPose,presentation->grip));
        if(side==1&&muzzle){
            const auto aimed=weaponAimWithPitch(presentation->aim,frame.controllers.weaponAimPitch);
            if(aimed){
                const auto aim=compose(inverse(*root),nativeTrackedPose(frame.nativePose,frame.headPose,*aimed));
                const auto socket=compose(palmFromWrist[side],*muzzle);
                if(const auto grip=aimedWeaponGrip(target,aim,rotate(socket.orientation,{0,0,1}))){
                    target=*grip;
                    frame.nativeFirearmActive=true;
                    frame.weaponRig.muzzleSolved=true;frame.weaponRig.muzzleInGrip=socket;
                }
            }
        }
        if(binocularHeld&&side==1){
            const auto palm=binocularFaceSafeGrip(frame.headPose,frame.controllers.optic.pose.primaryGrip,frame.controllers.optic.pose);
            target=compose(inverse(*root),nativeTrackedPose(frame.nativePose,frame.headPose,palm));
        }else if(binocularSupport&&side==0){
            target=compose(inverse(*root),nativeTrackedPose(frame.nativePose,frame.headPose,frame.controllers.optic.pose.supportGrip));
        }
        target=compose(target,palmFromWrist[side]);
        if(side==0&&idroidHeld){
            const auto grip=compose(inverse(*root),nativeTrackedPose(frame.nativePose,frame.headPose,presentation->grip));
            const auto held=nativeIdroidLeftHandheldWrist(grip,
                Pose{frame.controllers.idroidGripRotation,frame.controllers.idroidGripOffset});
            if(!held)return false;target=*held;
        }else if(side==0&&!binocularHeld){
            const auto contact=compose(bone(12),supportInWrist);
            const auto contactPalm=compose(contact,inverse(palmFromWrist[0]));
            const auto trackedPalm=compose(target,inverse(palmFromWrist[0]));
            const auto delta=trackedPalm.position-contactPalm.position;
            supportAttached=supportContact.update(muzzle.has_value()&&tracked[1]
                &&frame.controllers.weaponReady&&frame.controllers.supportGrip,true,
                std::sqrt(dot(delta,delta)),frame.sampleTime,
                frame.controllers.supportGripRadius,frame.controllers.supportDetachRadius);
            if(supportAttached)target=contact;
        }
        const ArmBasis basis{get<Vec3>(parentRecords+(wrist-1)*32+16),get<Vec3>(parentRecords+wrist*32+16)};
        for(const auto axis:{basis.upperAxis,basis.forearmAxis})
            if(!valid(Pose{{},axis})||std::abs(axis.y)+std::abs(axis.z)>.001f
               ||(side?-axis.x:axis.x)<.2f||(side?-axis.x:axis.x)>.35f)return false;
        const auto clavicle=wrist-3;
        if(const auto reached=reachingClavicle(bone(clavicle),{bone(wrist-2),bone(wrist-1),bone(wrist)},target)){
            const auto delta=compose(*reached,inverse(bone(clavicle)));
            put(clavicle,*reached);changed[clavicle]=true;
            for(size_t i=wrist-2;i<=wrist;++i)put(i,compose(delta,bone(i)));
        }
        const ArmPose native{bone(wrist-2),bone(wrist-1),bone(wrist)};
        const auto arm=solveArm(native,target,native.elbow.position-native.shoulder.position,&basis);
        if(!arm)return false;
        put(wrist-2,arm->pose.shoulder);put(wrist-1,arm->pose.elbow);put(wrist,arm->pose.wrist);
        changed[wrist-2]=changed[wrist-1]=changed[wrist]=true;tracked[side]=true;
        if(binocularHeld&&side==1){
            const auto renderedPalm=compose(*root,compose(bone(12),inverse(palmFromWrist[1])));
            const auto localPalm=compose(frame.headPose,compose(Pose{{0,1,0,0},{}},
                compose(inverse(frame.nativePose),renderedPalm)));
            const auto optic=attachBinocularToPalm(frame.controllers.optic.pose,localPalm);
            if(!optic)return false;
            frame.controllers.optic.pose=*optic;
            const auto gap=optic->supportGrip.position-frame.controllers.hands[0].grip.position;
            if(dot(gap,gap)>.12f*.12f){binocularSupport=false;frame.controllers.optic.pose.supportHeld=false;}
        }
    }
    std::array<Pose,512> deltas{};
    for(size_t i=0;i<count;++i)if(changed[i])deltas[i]=compose(bone(i),inverse(nativeBone(i)));
    for(size_t i=0;i<count;++i)if(!changed[i]){
        auto ancestor=parents[i];while(ancestor>=0&&!changed[static_cast<size_t>(ancestor)])ancestor=parents[static_cast<size_t>(ancestor)];
        if(ancestor>=0)put(i,compose(deltas[static_cast<size_t>(ancestor)],nativeBone(i)));
    }
    for(size_t side=0;side<2;++side)if(tracked[side]){
        const size_t clavicle=side?9:5;
        const auto local=[&](size_t i){return compose(inverse(bone(static_cast<size_t>(parents[i]))),bone(i)).orientation;};
        const auto twist=twists[side].update(local(clavicle+3),frame.sampleTime,key);
        const auto rotations=armCorrectiveRotations(local(clavicle),local(clavicle+1),local(clavicle+2),twist,side!=0);
        const auto translations=armCorrectiveTranslations(local(clavicle+1),twist,side!=0);
        for(size_t j=0;j<7;++j){
            const auto i=(*helpers)[side*7+j],anchor=static_cast<size_t>(armHelperParents[side*7+j]);
            auto offset=compose(inverse(nativeBone(anchor)),nativeBone(i)).position;
            if(j==1)offset.x=translations[0];if(j==6)offset.y=translations[1];
            if(!valid(Pose{{},offset})||dot(offset,offset)>.16f)return false;
            put(i,compose(bone(anchor),Pose{rotations[j],offset}));
        }
    }
    for(size_t side=0;side<2;++side){
        frame.renderedPalms[side]=compose(*root,compose(bone(wrists[side]),inverse(palmFromWrist[side])));
        frame.renderedPalmTracked[side]=tracked[side];
    }
    if(!tracked[0])supportContact.reset();
    frame.weaponSupportGripTracked=muzzle.has_value()&&tracked[1]&&frame.controllers.weaponReady;
    frame.weaponSupportAttached=frame.weaponSupportGripTracked&&supportAttached;
    if(frame.weaponSupportGripTracked){
        const auto palm=compose(*root,compose(bone(12),compose(supportInWrist,inverse(palmFromWrist[0]))));
        const auto localFromWorld=compose(frame.headPose,compose(Pose{{0,1,0,0},{}},inverse(frame.nativePose)));
        frame.weaponSupportGrip=compose(localFromWorld,palm);
    }
    if(tracked[0]){
        if(const auto panel=forearmPanel(compose(*root,bone(7)),compose(*root,bone(8)),
            rotate(frame.renderedPalms[0].orientation,{-1,0,0}),frame.controllers.wristSurfaceLift)){
            frame.wristPanel=*panel;frame.wristPanelTracked=true;
        }
    }
    // The same named straight finger chains use the shared controller curl
    // policy. A ready firearm retains its authored trigger/support contacts.
    constexpr std::array<std::array<size_t,5>,2> fingers{{{21,24,27,31,34},{37,40,43,47,50}}};
    for(size_t side=0;side<2;++side)if(tracked[side]&&!(side==0&&idroidHeld)&&(binocularHeld||!frame.controllers.weaponReady||(side==0&&!supportAttached))){
        const auto& hand=frame.controllers.hands[side];
        for(size_t finger=0;finger<5;++finger)for(size_t joint=0;joint<3;++joint){
            const auto i=fingers[side][finger]+joint;
            const auto anchor=joint?i-1:finger>=3?(side?46u:30u):wrists[side];
            if(parents[i]!=static_cast<int32_t>(anchor))return false;
            const auto offset=get<Vec3>(parentRecords+i*32+16);
            if(!valid(Pose{{},offset})||dot(offset,offset)<.000025f||dot(offset,offset)>.015f)return false;
            auto curl=freeFingerCurl(static_cast<unsigned>(finger),hand.trigger,hand.squeeze,
                hand.triggerTouched,hand.thumbTouched,frame.controllers.handRestCurl,frame.controllers.handTouchCurl);
            if(binocularHeld&&(side==1||binocularSupport))curl=std::clamp(curl,finger==0?.55f:.72f,finger==0?.65f:.82f);
            const auto rotation=fingerJointRotation(side!=0,static_cast<unsigned>(finger),static_cast<unsigned>(joint),curl);
            if(!rotation)return false;
            put(i,compose(bone(anchor),Pose{*rotation,offset}));
        }
    }
    if(idroidHeld&&tracked[0]){
        for(const auto& joint:nativeIdroidLeftHand)
            put(joint.bone,compose(bone(joint.parent),Pose{joint.local.orientation,get<Vec3>(parentRecords+joint.bone*32+16)}));
        frame.idroidDevice=compose(*root,compose(bone(8),nativeIdroidLeftBodyInWrist));
        frame.idroidDeviceTracked=valid(frame.idroidDevice);
        if(frame.idroidDeviceTracked)heldPoint=devicePoint;
    }
    // Native skeleton Q/P are temporary inputs. The original publisher makes
    // the world palette and held attachments, then Restore returns animation
    // to its authored pose. Legs and position.w remain unchanged.
    if(!headCamera().publishRigFrame(sceneCamera,observed.entity,source,frame))return false;
    static bool aimedReported{};
    if(frame.weaponRig.muzzleSolved&&!aimedReported){aimedReported=true;log("GZ native muzzle socket aligned through tracked firing wrist");}
    restore.rotations=skeleton+qo;restore.positions=skeleton+po;restore.q=originalQ;restore.p=originalP;restore.count=count;
    std::memcpy(reinterpret_cast<void*>(restore.rotations),q.data(),count*16u);
    std::memcpy(reinterpret_cast<void*>(restore.positions),p.data(),count*16u);
    barrelReady=frame.controllers.weaponReady&&frame.weaponRig.muzzleSolved&&tracked[1];
    return true;
}
uintptr_t skin(void* driver,void* binding){
    if(!enabled.load()||reinterpret_cast<uintptr_t>(binding)!=bodyBinding.load())return original(driver,binding);
    std::lock_guard lock(publicationMutex);
    if(!owned(reinterpret_cast<uintptr_t>(driver),reinterpret_cast<uintptr_t>(binding)))return original(driver,binding);
    Restore restore;
    struct AttachmentScope {uintptr_t previous=heldIdroidPoint;~AttachmentScope(){heldIdroidPoint=previous;}} attachmentScope;
    heldIdroidPoint=0;
    bool solved=false,barrelReady=false;
    uintptr_t heldPoint{};
    try{solved=solve(reinterpret_cast<uintptr_t>(driver),reinterpret_cast<uintptr_t>(binding),restore,barrelReady,heldPoint);}catch(...){}
    if(solved)heldIdroidPoint=heldPoint;
    const auto result=original(driver,binding);
    publishedBarrel={};
    if(solved&&barrelReady){
        // Read the original publisher's completed world palette and attachment
        // together. A stowed/untracked frame must revoke the previous barrel.
        uintptr_t weapon{};
        if(const auto local=muzzleInWrist(&weapon)){
            std::array<float,16> wrist{};
            const auto palette=get<uintptr_t>(observed.model+0xe0);
            if(palette&&read(palette+12*64,wrist))if(const auto pose=nativeAffinePose(wrist))
                publishedBarrel={observed.entity,observed.model,weapon,steadyMilliseconds(),compose(*pose,*local),wrist};
        }
    }
    static bool reported{};
    if(solved&&!reported){reported=true;log("GZ tracked native arm skin and attachments published");}
    return result;
}
}
void observeGzRigCamera(uintptr_t identity,const std::optional<GzPlayerHead>& player){
    std::lock_guard lock(publicationMutex);
    if(player){sceneCamera=identity;observed=*player;observedAt=steadyMilliseconds();bodyBinding.store(player->model+0xa0);}
    else{bodyBinding.store(0);sceneCamera=0;observed={};observedAt=0;}
}
GzInputState gzInputState() noexcept{
    GzInputState state;
    if(!enabled.load())return state;
    state.enabled=true;
    state.menu=readGzMenuState(image,copy);
    std::lock_guard lock(publicationMutex);
    // Input ownership follows the native player registry, not the cadence of
    // skin/camera publication. A slow rendered frame must not stow an equipped
    // optic or change the effective controls context. Pose consumers retain
    // their separate same-frame/freshness checks.
    const auto registry=get<uintptr_t>(image+0x1e3b600);
    const auto entry=registry?get<uintptr_t>(registry+0x128):0;
    const auto player=entry?get<uintptr_t>(entry+0x150):0;
    const auto entity=player?get<uintptr_t>(player+0x38):0;
    state.player=sceneCamera&&entity&&entity==observed.entity;
    if(state.player)state.basicAction=readGzBasicAction(image,observed.entity,copy);
    return state;
}
void installGzRig(uintptr_t base){
    constexpr std::array<unsigned char,10> expected{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x30};
    std::array<unsigned char,10> actual{};
    if(!read(base+0xdb41e0,actual)||actual!=expected)throw std::runtime_error("GZ skin publisher signature mismatch");
    image=base;
    constexpr std::array<unsigned char,8> pauseGetter{0x48,0x8b,0x05,0x21,0x77,0x3a,0x01,0xc3};
    constexpr std::array<unsigned char,8> uiGetter{0x48,0x8b,0x05,0x71,0x2f,0x28,0x01,0xc3};
    std::array<unsigned char,8> getter{};
    if(!read(base+0xa60300,getter)||getter!=pauseGetter||!read(base+0xbfb2b0,getter)||getter!=uiGetter)
        throw std::runtime_error("GZ native input owner signature mismatch");
    constexpr std::array<unsigned char,10> cnpQuery{0x48,0x89,0x5c,0x24,0x20,0x48,0x89,0x54,0x24,0x10};
    if(!read(base+0x2d3090,actual)||actual!=cnpQuery)
        throw std::runtime_error("GZ native weapon socket query signature mismatch");
    constexpr std::array<unsigned char,10> aimSignature{0x48,0x8b,0xc4,0x55,0x41,0x54,0x41,0x56,0x48,0x8d};
    if(!read(base+0x326c70,actual)||actual!=aimSignature)
        throw std::runtime_error("GZ native aim job signature mismatch");
    const auto aimTarget=reinterpret_cast<void*>(base+0x326c70);
    constexpr std::array<unsigned char,10> attachmentSignature{0x4c,0x8b,0xdc,0x49,0x89,0x5b,0x10,0x55,0x56,0x57};
    if(!read(base+0xdac4f0,actual)||actual!=attachmentSignature)
        throw std::runtime_error("GZ native attachment publisher signature mismatch");
    const auto attachmentTarget=reinterpret_cast<void*>(base+0xdac4f0);
    if(MH_CreateHook(attachmentTarget,reinterpret_cast<void*>(&attachment),reinterpret_cast<void**>(&originalAttachment))!=MH_OK
       ||MH_EnableHook(attachmentTarget)!=MH_OK)throw std::runtime_error("Cannot install GZ native iDroid adapter");
    if(MH_CreateHook(aimTarget,reinterpret_cast<void*>(&aimUpdate),reinterpret_cast<void**>(&originalAim))!=MH_OK
       ||MH_EnableHook(aimTarget)!=MH_OK)throw std::runtime_error("Cannot install GZ native aim adapter");
    const auto target=reinterpret_cast<void*>(base+0xdb41e0);
    if(MH_CreateHook(target,reinterpret_cast<void*>(&skin),reinterpret_cast<void**>(&original))!=MH_OK
       ||MH_EnableHook(target)!=MH_OK)throw std::runtime_error("Cannot install GZ native skin adapter");
    enabled.store(true);log("GZ native arm adapter installed");
}
void stopGzRig() noexcept {enabled.store(false);bodyBinding.store(0);std::lock_guard lock(publicationMutex);sceneCamera=0;observed={};publishedBarrel={};}
}
