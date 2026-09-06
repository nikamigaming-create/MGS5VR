#include "mgs5vr/controller_rig.hpp"
#include "mgs5vr/arm_ik.hpp"
#include "mgs5vr/head_camera.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/log.hpp"
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
uintptr_t base{};
std::atomic_uintptr_t playerOwner{};
std::atomic_bool enabled{};
std::mutex rigMutex;
uintptr_t boundOwner{};
uint64_t activation{},updates{};
std::array<Vec3,2> bendHistory{};
float supportBlend{};
uint64_t supportAt{};
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
    constexpr std::array<size_t,2> indexKnuckles{24,40},littleKnuckles{34,50};
    std::array<Pose,2> gripFromWrist{};
    for(size_t i=0;i<2;++i){
        const auto wrist=bone(q,p,wrists[i]);
        const auto palm=anatomicalGrip(wrist,bone(q,p,indexKnuckles[i]).position,bone(q,p,littleKnuckles[i]).position);
        if(!palm)return false;
        gripFromWrist[i]=compose(inverse(*palm),wrist);
    }
    if(boundOwner!=owner||activation!=frame.activation){
        for(size_t i=0;i<2;++i){
            bendHistory[i]=bone(q,p,wrists[i]-1).position-bone(q,p,shoulders[i]).position;
        }
        boundOwner=owner;activation=frame.activation;
        supportBlend=0;supportAt=now;
        log("Controller rig uses native anatomical palm frames; no activation-pose wrist calibration");
    }
    const auto rightAnimated=bone(q,p,12),leftAnimated=bone(q,p,8);
    {
        // The retail prone animation puts shoulders above/around the eye. Use
        // a standing upper-body frame at the tracked head for the visible VR
        // arms, retaining native limb lengths and moving clavicle helpers too.
        const auto forward=rotate(nativeCamera.orientation,{0,0,1});
        const float yaw=std::atan2(forward.x,forward.z);
        const Quat torso{0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)};
        const auto span=bone(q,p,6).position-bone(q,p,10).position;
        const float halfWidth=std::clamp(std::sqrt(dot(span,span))*.5f,.15f,.23f);
        for(size_t side=0;side<2;++side){
            const auto shoulder=shoulders[side];
            const auto anchorWorld=frame.nativePose.position+rotate(torso,{side? -halfWidth:halfWidth,-.22f,-.06f});
            const auto anchor=compose(inverse(*root),Pose{{},anchorWorld}).position;
            const auto shift=anchor-bone(q,p,shoulder).position;
            for(size_t i=shoulder-1;i<=shoulder+2;++i){auto b=bone(q,p,i);b.position=b.position+shift;replace(q,p,i,b);}
            // Stable down/out bias; a transient native crouch/reload bend must
            // not leave the elbow trapped above the head in later frames.
            bendHistory[side]=rotate(inverse(*root).orientation,rotate(torso,{side?-.25f:.25f,-1.f,-.15f}));
        }
    }
    const Pose rightTarget=compose(grips[1],gripFromWrist[1]);
    const auto right=solveArm({bone(q,p,10),bone(q,p,11),bone(q,p,12)},rightTarget,bendHistory[1],true);
    if(!right)return false;
    replace(q,p,10,right->pose.shoulder);replace(q,p,11,right->pose.elbow);replace(q,p,12,right->pose.wrist);
    bendHistory[1]=right->pose.elbow.position-right->pose.shoulder.position;
    bool nativeManipulation=false;
    const auto weaponComponent=get<uintptr_t>(character+0x80);
    if(get<uintptr_t>(weaponComponent)==base+0x23b3e80&&get<uintptr_t>(weaponComponent+8)==character){
        const auto instances=get<uintptr_t>(weaponComponent+0x38);
        const auto first=get<uint32_t>(instances+0x24),index=get<uint32_t>(owner+0x3a0);
        if(index>=first&&index-first<=15){
            const auto state=get<uintptr_t>(weaponComponent+0x58)+(index-first)*0x610ull;
            // Observed throughout rifle reload and WU pistol bolt cycling; the
            // native animation owns the support hand during these operations.
            nativeManipulation=(get<uint32_t>(state+0x27c)&0x1c0)==0x40&&(get<uint32_t>(state+0x3c0)&0x04000000)!=0;
        }
    }
    const bool support=frame.controllers.supportRequested||nativeManipulation;
    const float step=std::min(now>=supportAt?static_cast<float>(now-supportAt)/120.f:1.f,1.f);supportAt=now;
    supportBlend=std::clamp(supportBlend+(support?step:-step),0.f,1.f);
    if(frame.controllers.hands[0].gripTracked||support){
        // Preserve the game's animated support-hand contact while the native
        // support is requested or a native reload/bolt cycle is running.
        const auto attached=compose(right->pose.wrist,compose(inverse(rightAnimated),leftAnimated));
        auto target=attached;
        if(frame.controllers.hands[0].gripTracked&&supportBlend<1){
            const auto tracked=compose(grips[0],gripFromWrist[0]);
            target.position=tracked.position+(attached.position-tracked.position)*supportBlend;
            auto a=tracked.orientation,b=attached.orientation;
            if(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w<0)b={-b.x,-b.y,-b.z,-b.w};
            Quat blended{a.x+(b.x-a.x)*supportBlend,a.y+(b.y-a.y)*supportBlend,a.z+(b.z-a.z)*supportBlend,a.w+(b.w-a.w)*supportBlend};
            const float n=std::sqrt(blended.x*blended.x+blended.y*blended.y+blended.z*blended.z+blended.w*blended.w);
            target.orientation={blended.x/n,blended.y/n,blended.z/n,blended.w/n};
        }
        const auto left=solveArm({bone(q,p,6),bone(q,p,7),bone(q,p,8)},target,bendHistory[0],true);
        if(!left)return false;
        replace(q,p,6,left->pose.shoulder);replace(q,p,7,left->pose.elbow);replace(q,p,8,left->pose.wrist);
        bendHistory[0]=left->pose.elbow.position-left->pose.shoulder.position;
    }
    constexpr std::array<size_t,8> changed{5,6,7,8,9,10,11,12};
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
    if(frame.controllers.hands[0].gripTracked){
        const auto wrist=compose(*root,bone(q,p,8));
        const auto elbow=compose(*root,bone(q,p,7));
        const auto palm=compose(wrist,inverse(gripFromWrist[0]));
        frame.wristPanel=compose(palm,Pose{{0,-0.70710678f,0,0.70710678f},{}});
        frame.wristPanel.position=wrist.position+(elbow.position-wrist.position)*0.35f
            +rotate(frame.wristPanel.orientation,{0,0,0.025f});
        frame.wristPanelTracked=true;
    }
    if(!headCamera().publishRigFrame(camera,owner,nativeCamera,frame))return false;
    // The engine may reuse animated helper/finger channels next frame. Only the
    // native publication consumes our solved pose; never feed it back into the
    // animation cache and compound garment transforms on subsequent updates.
    restore.rotations=rotations;restore.positions=positions;restore.q=originalQ;restore.p=originalP;restore.count=count;
    // SKL_300_ASRROOT is the authored hip holster, independent of the held
    // weapon's wrist socket. Suppress only its published render scale in VR.
    // The native animation channels and held-weapon transform remain intact.
    const auto names=get<uintptr_t>(model+0xe8);
    if(count>53&&parent[53]==0&&get<uint32_t>(names+53*4)==0xec70c442)
        restore.stowedMount=get<uintptr_t>(reinterpret_cast<uintptr_t>(binding)+0x40)+53*64;
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
