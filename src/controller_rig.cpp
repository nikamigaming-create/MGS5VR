#include "mgs5vr/controller_rig.hpp"
#include "mgs5vr/arm_ik.hpp"
#include "mgs5vr/head_camera.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/log.hpp"
#include <windows.h>
#include <intrin.h>
#include <MinHook.h>
#include <array>
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
using Shot=void(*)(void*,Vector4*,Vector4*,void*,uint32_t);
Shot originalShot{};
uintptr_t base{};
std::atomic_uintptr_t playerOwner{};
std::atomic_bool enabled{};
std::mutex rigMutex;
uintptr_t calibratedOwner{};
uint64_t activation{},updates{};
std::array<Quat,2> gripFromWrist{};
std::array<Vec3,2> bendHistory{};
struct ShotRig {
    uintptr_t owner{},character{},camera{},model{};
    Pose sourceCamera{},wristWorld{};
    uint64_t trackingSequence{};
} shotRig;
template<class T> bool read(uintptr_t address,T& out){
    SIZE_T count{};return address&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),&out,sizeof(out),&count)&&count==sizeof(out);
}
template<class T> T get(uintptr_t address){T out{};read(address,out);return out;}
Pose bone(const std::array<Quat,512>& q,const std::array<Vector4,512>& p,size_t i){return {q[i],{p[i].x,p[i].y,p[i].z}};}
void replace(std::array<Quat,512>& q,std::array<Vector4,512>& p,size_t i,Pose value){
    q[i]=value.orientation;p[i].x=value.position.x;p[i].y=value.position.y;p[i].z=value.position.z;
}
bool apply(void* context,void* binding){
    const auto owner=playerOwner.load();
    if(!owner||get<uintptr_t>(owner)!=base+0x23b8218)return false;
    const auto character=get<uintptr_t>(owner+0x370),camera=get<uintptr_t>(owner+0x380);
    if(!character||get<uintptr_t>(character)!=base+0x2295210)return false;
    const auto component=get<uintptr_t>(character+0x10),holder=get<uintptr_t>(component+0x10),model=get<uintptr_t>(holder+0x68);
    if(!component||!holder||!model||get<uintptr_t>(model)!=base+0x20f4d90||reinterpret_cast<uintptr_t>(binding)!=model+0xa0)return false;
    const auto driver=reinterpret_cast<uintptr_t>(context),skeleton=get<uintptr_t>(driver+0x10);
    if(get<uintptr_t>(driver)!=base+0x24c1988||!skeleton)return false;
    const auto count=get<uint16_t>(skeleton+0x48);
    if(count<21||count>512||get<int16_t>(skeleton+0x4a)!=0||get<uint32_t>(skeleton+0x60)!=0)return false;
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
    std::array<float,16> matrix{};std::array<float,8> cameraValues{};
    if(!read(reinterpret_cast<uintptr_t>(binding),matrix)||!read(camera+0xf0,cameraValues))return false;
    const auto root=nativeAffinePose(matrix);if(!root)return false;
    const Pose nativeCamera{{cameraValues[0],cameraValues[1],cameraValues[2],cameraValues[3]},
                            {cameraValues[4],cameraValues[5],cameraValues[6]}};
    auto frame=headCamera().resolveCurrentForRig(camera,nativeCamera);
    if(!frame.applied||frame.playerOwner!=owner||!frame.controllers.hands[1].gripTracked)return false;
    const auto renderedHead=compose(*root,bone(q,p,4)).position;
    frame.nativePose.position=frame.nativePose.position+renderedHead-frame.playerHead;
    const auto originalQ=q;const auto originalP=p;
    std::array<Pose,2> grips{};
    for(size_t i=0;i<2;++i)if(frame.controllers.hands[i].gripTracked)
        grips[i]=compose(inverse(*root),nativeTrackedPose(frame.nativePose,frame.headPose,frame.controllers.hands[i].grip));
    std::lock_guard lock(rigMutex);
    constexpr std::array<size_t,2> wrists{8,12},shoulders{6,10};
    if(calibratedOwner!=owner||activation!=frame.activation){
        if(!frame.controllers.hands[0].gripTracked)return false;
        for(size_t i=0;i<2;++i){
            // Experimental neutral-pose orientation calibration. Translation
            // stays at the tracked grip; there is no remembered world offset.
            gripFromWrist[i]=compose(inverse(grips[i]),bone(q,p,wrists[i])).orientation;
            bendHistory[i]=bone(q,p,wrists[i]-1).position-bone(q,p,shoulders[i]).position;
        }
        calibratedOwner=owner;activation=frame.activation;
        log("Controller rig neutral wrist orientation calibrated; weapon/muzzle acceptance pending");
    }
    const auto rightAnimated=bone(q,p,12),leftAnimated=bone(q,p,8);
    const Pose rightTarget=compose(grips[1],Pose{gripFromWrist[1],{}});
    const auto right=solveArm({bone(q,p,10),bone(q,p,11),rightAnimated},rightTarget,bendHistory[1]);
    if(!right)return false;
    replace(q,p,10,right->pose.shoulder);replace(q,p,11,right->pose.elbow);replace(q,p,12,right->pose.wrist);
    bendHistory[1]=right->pose.elbow.position-right->pose.shoulder.position;
    const bool support=frame.controllers.supportRequested;
    if(frame.controllers.hands[0].gripTracked||support){
        // Preserve the game's animated support-hand contact while the native
        // weapon-ready action is held, including relative reload animation.
        const auto target=support?compose(right->pose.wrist,compose(inverse(rightAnimated),leftAnimated))
                                 :compose(grips[0],Pose{gripFromWrist[0],{}});
        const auto left=solveArm({bone(q,p,6),bone(q,p,7),leftAnimated},target,bendHistory[0]);
        if(!left)return false;
        replace(q,p,6,left->pose.shoulder);replace(q,p,7,left->pose.elbow);replace(q,p,8,left->pose.wrist);
        bendHistory[0]=left->pose.elbow.position-left->pose.shoulder.position;
    }
    constexpr std::array<size_t,6> changed{6,7,8,10,11,12};
    std::array<Pose,512> delta{};std::array<bool,512> controlled{};
    for(const auto i:changed){controlled[i]=true;delta[i]=compose(bone(q,p,i),inverse(bone(originalQ,originalP,i)));}
    for(size_t i=0;i<count;++i)if(!controlled[i]){
        auto ancestor=parent[i];
        while(ancestor>=0&&!controlled[static_cast<size_t>(ancestor)])ancestor=parent[static_cast<size_t>(ancestor)];
        if(ancestor>=0)replace(q,p,i,compose(delta[static_cast<size_t>(ancestor)],bone(originalQ,originalP,i)));
    }
    // Full render pose, before native matrix and attachment publication. Finger
    // poses follow their solved wrist. Native position.w metadata is retained.
    frame.playerHead=renderedHead;
    if(!headCamera().publishRigFrame(camera,owner,nativeCamera,frame))return false;
    std::memcpy(reinterpret_cast<void*>(rotations),q.data(),count*16u);
    std::memcpy(reinterpret_cast<void*>(positions),p.data(),count*16u);
    shotRig={owner,character,camera,model,nativeCamera,compose(*root,right->pose.wrist),frame.trackingSequence};
    if(++updates%120==1){
        std::ostringstream s;s<<"Controller rig updates="<<updates<<" tracking="<<frame.trackingSequence
            <<" predicted="<<frame.controllers.predictedXrTime<<" player="<<frame.playerSequence
            <<" right_wrist="<<right->pose.wrist.position.x<<','<<right->pose.wrist.position.y<<','<<right->pose.wrist.position.z
            <<" reach_clamped="<<right->reachClamped;log(s.str());
    }
    return true;
}
void update(void* context,void* binding){
    if(enabled.load()&&headCamera().active())try{apply(context,binding);}catch(...){}
    original(context,binding);
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
}
namespace mgs5vr {
void installControllerRig(uintptr_t imageBase){
    base=imageBase;
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
    enabled.store(true);log("Experimental controller rig installed at verified skin publication RVA 0x1a6caa0 and shot solver 0x1044ff0");
}
void observeControllerRigOwner(uintptr_t owner) noexcept{playerOwner.store(owner);}
void stopControllerRig() noexcept{enabled.store(false);playerOwner.store(0);}
bool controllerRigEnabled() noexcept{return enabled.load();}
}
