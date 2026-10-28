#include "mgs5vr/gz_recon.hpp"
#include "mgs5vr/gz_player.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/recon.hpp"
#include "mgs5vr/log.hpp"
#include <windows.h>
#include <MinHook.h>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
#include <stdexcept>

namespace mgs5vr {
namespace {
uintptr_t image{};
std::atomic_bool enabled{};
std::mutex frameMutex;
HeadCameraSample latest;
using Update=void(*)(void*);
Update original{};
Update originalEvents{};
struct RadioQuery {uintptr_t entity{};uint64_t activation{},time{};};
RadioQuery radioQuery;
ReconDwell dwell;
ReconClearPolicy clearPolicy;
std::array<uintptr_t,256> recordCharacters{};
uintptr_t priorEntity{},priorRecord{};
uint64_t priorActivation{},consumed{},consumedClear{};
bool wasHeld{};
bool copy(uintptr_t address,void* output,size_t bytes){
    SIZE_T copied{};
    return address&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),output,bytes,&copied)&&copied==bytes;
}
template<class T>T get(uintptr_t address){T value{};copy(address,&value,sizeof(value));return value;}
template<size_t N>bool matches(uintptr_t base,uintptr_t rva,const std::array<unsigned char,N>& bytes){
    std::array<unsigned char,N> actual{};
    return copy(base+rva,actual.data(),actual.size())&&actual==bytes;
}
struct NativePoolLock {
    uintptr_t address{};
    int state{-1};
    explicit NativePoolLock(uintptr_t pool):address(pool+0x808){
        using Lock=void(*)(uintptr_t,int*);
        reinterpret_cast<Lock>(image+0xd85860)(address,&state);
    }
    ~NativePoolLock(){
        if(state>=0){int result{};using Unlock=void(*)(uintptr_t,int*);
            reinterpret_cast<Unlock>(image+0xd858a0)(address,&result);}
    }
    NativePoolLock(const NativePoolLock&)=delete;
    NativePoolLock& operator=(const NativePoolLock&)=delete;
};
std::array<float,16> affine(Pose pose){
    std::array<float,16> result{};
    const std::array<Vec3,3> axes{Vec3{1,0,0},Vec3{0,1,0},Vec3{0,0,1}};
    for(size_t i=0;i<3;++i){const auto v=rotate(pose.orientation,axes[i]);result[i*4]=v.x;result[i*4+1]=v.y;result[i*4+2]=v.z;}
    result[12]=pose.position.x;result[13]=pose.position.y;result[14]=pose.position.z;result[15]=1;return result;
}
struct NativeScope {
    uintptr_t owner{},viewport{},camera{};
    uint8_t scan{},radioScan{};
    std::array<float,5> parameters{};
    explicit NativeScope(uintptr_t value):owner(value),viewport(get<uintptr_t>(value+0x7b0)),camera(get<uintptr_t>(value+0x7b8)),scan(get<uint8_t>(value+0x7a0)){
        copy(value+0x778,parameters.data(),sizeof(parameters));
        radioScan=get<uint8_t>(value+0x7a1);
    }
    ~NativeScope(){
        std::memcpy(reinterpret_cast<void*>(owner+0x778),parameters.data(),sizeof(parameters));
        *reinterpret_cast<uint8_t*>(owner+0x7a0)=scan;
        *reinterpret_cast<uint8_t*>(owner+0x7a1)=radioScan;
        *reinterpret_cast<uintptr_t*>(owner+0x7b0)=viewport;
        *reinterpret_cast<uintptr_t*>(owner+0x7b8)=camera;
    }
};
void update(void* raw){
    const auto owner=reinterpret_cast<uintptr_t>(raw);
    if(!enabled.load()||get<uintptr_t>(owner)!=image+0x16ce448){original(raw);return;}
    HeadCameraSample frame;
    {std::lock_guard lock(frameMutex);frame=latest;}
    const auto now=steadyMilliseconds();const auto status=headCamera().status();
    const auto menu=enabled.load()?readGzMenuState(image,copy):std::nullopt;
    const auto entity=get<uintptr_t>(owner+0x30);
    if(!entity||frame.playerOwner!=entity){original(raw);return;}
    const auto& optic=frame.controllers.optic;
    const bool held=enabled.load()&&frame.applied&&status.active&&!status.suspended
        &&frame.activation==status.activation&&frame.playerOwner==entity
        &&menu&&!menu->pause&&!menu->idroid
        &&optic.held&&optic.pose.tracked&&optic.pose.ray.tracked&&optic.pose.kind==OpticKind::binocular;
    if(!held){
        if(wasHeld)*reinterpret_cast<uint8_t*>(owner+0x7d9)=1;
        wasHeld=false;dwell.reset();original(raw);return;
    }
    // A delayed render sample suspends acquisition, not the physical equip
    // session. It must not repeatedly flush otherwise valid native results.
    if(now<frame.sampleTime||now-frame.sampleTime>150){dwell.reset();original(raw);return;}
    const auto table=get<uintptr_t>(entity+0x68);
    const auto registry=get<uintptr_t>(image+0x1e3b600);
    const auto entry=registry?get<uintptr_t>(registry+0x128):0;
    const auto player=entry?get<uintptr_t>(entry+0x150):0;
    const auto parameters=get<uintptr_t>(image+0x1e3b8e8);
    const auto viewport=get<uintptr_t>(owner+0x7b0),camera=get<uintptr_t>(owner+0x7b8);
    const float distance=parameters?get<float>(parameters+0xb0):0;
    const float radioDistance=parameters?get<float>(parameters+0xb4):0;
    const float range=parameters?get<float>(parameters+0xb8):0;
    const float power=frame.controllers.magnification;
    if(!table||get<uintptr_t>(table+0x50*8)!=owner+0x28||!player||get<uintptr_t>(player+0x38)!=entity
       ||get<uintptr_t>(viewport)!=image+0x1841918||get<uintptr_t>(camera)!=image+0x1841378
       ||!std::isfinite(distance)||distance<=0||distance>2000||!std::isfinite(range)||range<=0||range>2
       ||!std::isfinite(radioDistance)||radioDistance<=0||radioDistance>2000
       ||!std::isfinite(power)||power<1||power>4){
        wasHeld=false;dwell.reset();original(raw);return;
    }
    const auto sight=nativeTrackedPose(frame.nativePose,frame.headPose,optic.pose.rightEyepiece);
    if(!valid(sight)){wasHeld=false;dwell.reset();original(raw);return;}
    // GZ GrCamera uses +Z forward; the OpenXR optic uses -Z. Only this
    // native query receives private world/view matrices. The shared scene
    // camera, native projection, collision buffers and job ordering survive.
    const auto nativeCamera=compose(sight,Pose{{0,1,0,0},{}});
    alignas(16) std::array<unsigned char,0x520> localViewport{};
    alignas(16) std::array<unsigned char,0x70> localCamera{};
    if(!copy(viewport,localViewport.data(),localViewport.size())||!copy(camera,localCamera.data(),localCamera.size())){
        wasHeld=false;dwell.reset();original(raw);return;
    }
    const auto world=affine(nativeCamera),view=affine(inverse(nativeCamera));
    std::memcpy(localCamera.data()+0x30,world.data(),sizeof(world));
    std::memcpy(localViewport.data()+0x280,view.data(),sizeof(view));
    {
        NativeScope restore(owner);
        *reinterpret_cast<uintptr_t*>(owner+0x7b0)=reinterpret_cast<uintptr_t>(localViewport.data());
        *reinterpret_cast<uintptr_t*>(owner+0x7b8)=reinterpret_cast<uintptr_t>(localCamera.data());
        *reinterpret_cast<uint8_t*>(owner+0x7a0)=1;
        *reinterpret_cast<uint8_t*>(owner+0x7a1)=1;
        if(!wasHeld)*reinterpret_cast<uint8_t*>(owner+0x7d9)=1;
        // Native binocular zoom publishes its optical power to homing +780
        // (64abd0/64ad20). The query multiplies the base marking distance by
        // that power; use the same power that renders the physical lens.
        const std::array<float,5> scanParameters{distance,radioDistance,power,range,range};
        std::memcpy(reinterpret_cast<void*>(owner+0x778),scanParameters.data(),sizeof(scanParameters));
        original(raw);
    }
    // Publish query identity/freshness for the native mission-event job.
    // Do not invoke the radio Candidate/Play events from a homing job.
    {std::lock_guard guard(frameMutex);radioQuery={entity,frame.activation,wasHeld?now:0};}
    // The original job consumes completed native collision queries before
    // submitting its next pair. A new optical session discards eligibility
    // from the preceding camera; it never sets a visibility result to true.
    const auto pool=get<uintptr_t>(image+0x1e687d0);
    if(!pool){dwell.reset();return;}
    NativePoolLock lock(pool);
    const auto count=get<uint32_t>(pool+0x800);
    if(count>256){dwell.reset();return;}
    clearPolicy.scene(entity,pool);
    if(!wasHeld||priorEntity!=entity||priorActivation!=frame.activation){
        using Clear=void(*)(void*,bool);
        for(uint32_t i=0;i<count;++i)if(const auto record=get<uintptr_t>(pool+i*8))
            reinterpret_cast<Clear>(image+0xabb480)(reinterpret_cast<void*>(record),false);
        wasHeld=true;priorEntity=entity;priorActivation=frame.activation;priorRecord=0;dwell.reset();
        consumed=frame.controllers.opticMarkSequence;
        consumedClear=frame.controllers.opticClearSequence;
        log("GZ physical optic native visibility query active");return;
    }
    const bool requested=frame.controllers.opticMarkSequence!=consumed;
    const bool clearing=frame.controllers.opticClearSequence!=consumedClear;
    consumed=frame.controllers.opticMarkSequence;
    consumedClear=frame.controllers.opticClearSequence;
    const bool automatic=optic.active&&frame.controllers.binocularAutoMark;
    if(!automatic&&!requested&&!clearing){dwell.reset();return;}
    const auto forward=rotate(sight.orientation,{0,0,-1});
    uintptr_t selected{},character{};uint16_t index{};float nearest=distance*power;
    using Marked=bool(*)(uintptr_t*,uint8_t);
    for(uint32_t i=0;i<count;++i){
        const auto record=get<uintptr_t>(pool+i*8);if(!record)continue;
        auto target=get<uintptr_t>(record+0x10);if(!target)continue;
        if(recordCharacters[i]!=target){recordCharacters[i]=target;clearPolicy.explicitMark(static_cast<uint16_t>(i));}
        const bool marked=reinterpret_cast<Marked>(image+0xab60e0)(&target,1)&&reinterpret_cast<Marked>(image+0xab60e0)(&target,2);
        if(clearing){if(!marked)continue;}
        else if(marked||(get<uint8_t>(record+0x48)&1)==0||(!requested&&!clearPolicy.automaticAllowed(static_cast<uint16_t>(i))))continue;
        const auto points=get<uintptr_t>(record+8);const auto size=get<uint32_t>(record);
        if(!points||size>256)continue;
        for(uint32_t j=0;j<size;++j){
            const auto position=get<Vec3>(points+j*16);if(!valid(Pose{{},position}))continue;
            const auto delta=position-sight.position;const float along=dot(delta,forward);
            if(along<1||along>=nearest)continue;
            const auto perpendicular=delta-forward*along;
            if(dot(perpendicular,perpendicular)>.65f*.65f)continue;
            selected=record;character=target;index=static_cast<uint16_t>(i);nearest=along;
        }
    }
    if(clearing){
        dwell.reset();
        if(selected){
            // The native DisableMarker path accepts the handle's name and a
            // layer mask. It clears only the player's two acquired layers.
            using Clear=void(*)(uint64_t,uint8_t);
            reinterpret_cast<Clear>(image+0xab4e60)(get<uint64_t>(character),3);
            clearPolicy.clear(index);
            log("GZ physical optic native character clear index="+std::to_string(index));
        }
        return;
    }
    if(priorRecord!=selected){priorRecord=selected;dwell.reset();}
    const bool acquired=automatic&&dwell.update(selected?std::optional<uint16_t>{index}:std::nullopt,
        frame.sampleTime,frame.activation,frame.controllers.binocularMarkDwellMs);
    if(!selected||(!requested&&!acquired))return;
    if(requested)clearPolicy.explicitMark(index);
    using Commit=void(*)(void*,uintptr_t);
    reinterpret_cast<Commit>(image+0x6c2250)(reinterpret_cast<void*>(player),character);
    const bool marked=reinterpret_cast<Marked>(image+0xab60e0)(&character,1)&&reinterpret_cast<Marked>(image+0xab60e0)(&character,2);
    if(marked)dwell.suppress();
    log("GZ physical optic native character mark index="+std::to_string(index)+" marked="+std::to_string(marked));
}
void updateIntelEvents(void* raw){
    if(!enabled.load()||reinterpret_cast<uintptr_t>(raw)!=get<uintptr_t>(image+0x1de94b8)){
        originalEvents(raw);return;
    }
    static bool reported{};
    if(!reported){reported=true;log("GZ physical optic native intel event update active");}
    static uintptr_t ownedController{},ownedBinocular{};
    static uint64_t consumedIntel{},ownedTarget{};
    HeadCameraSample frame;RadioQuery query;
    {std::lock_guard lock(frameMutex);frame=latest;query=radioQuery;}
    const auto now=steadyMilliseconds();const auto status=headCamera().status();
    const auto menu=readGzMenuState(image,copy);
    const auto registry=get<uintptr_t>(image+0x1e3b600);
    const auto entry=registry?get<uintptr_t>(registry+0x128):0;
    const auto player=entry?get<uintptr_t>(entry+0x150):0;
    const auto entity=player?get<uintptr_t>(player+0x38):0;
    const auto table=entity?get<uintptr_t>(entity+0x68):0;
    const auto slot=table?get<uintptr_t>(table+0x79*8):0;
    const auto binocular=slot>=0x28?slot-0x28:0;
    const auto controller=get<uintptr_t>(image+0x1e68660);
    const bool validOwner=get<uintptr_t>(binocular)==image+0x16d3050&&get<uintptr_t>(binocular+0x30)==entity;
    const auto& optic=frame.controllers.optic;
    const bool held=validOwner&&controller&&frame.applied&&status.active&&!status.suspended
        &&frame.activation==status.activation&&frame.playerOwner==entity
        &&menu&&!menu->pause&&!menu->idroid&&optic.active&&optic.held&&optic.pose.tracked
        &&optic.pose.kind==OpticKind::binocular;
    const bool fresh=held&&now>=frame.sampleTime&&now-frame.sampleTime<=150
        &&query.entity==entity&&query.activation==frame.activation&&query.time&&now>=query.time&&now-query.time<=150;
    const bool request=frame.controllers.opticIntelSequence!=consumedIntel;
    consumedIntel=frame.controllers.opticIntelSequence;
    using Check=bool(*)();
    using Candidate=bool(*)(void*);
    if(fresh&&!reinterpret_cast<Check>(image+0x534620)()&&!reinterpret_cast<Check>(image+0x534670)()){
        const bool available=reinterpret_cast<Candidate>(image+0x6fa4a0)(reinterpret_cast<void*>(binocular));
        const auto target=get<uint64_t>(binocular+0x170);
        if(ownedController!=controller||ownedTarget!=target){
            log("GZ physical optic native intel candidate="+std::to_string(target)+" available="+std::to_string(available));
        }
        ownedController=controller;ownedBinocular=binocular;ownedTarget=target;
        // The same mission restriction and radio-busy checks precede the
        // native PlayButton event in GZ's binocular input action.
        const auto mission=get<uintptr_t>(image+0x1e1eb50);
        if(request&&available&&mission&&!(get<uint32_t>(mission+0x150)&8)
           &&reinterpret_cast<Check>(image+0xab7870)()){
            using Play=void(*)(uint64_t);
            reinterpret_cast<Play>(image+0xab7e20)(target);
            log("GZ physical optic native intel request="+std::to_string(target));
        }
    }else if(!held&&ownedController){
        // Do not conceal a candidate subsequently owned by native binoculars.
        if(validOwner&&ownedBinocular==binocular&&ownedController==controller
           &&get<uint64_t>(binocular+0x170)==ownedTarget&&(get<uint8_t>(binocular+0x50)&0x40)){
            int result{};using Hide=void(*)(uintptr_t,int*);
            reinterpret_cast<Hide>(image+0xab7740)(controller,&result);
        }
        ownedController=ownedBinocular=0;ownedTarget=0;
    }
    originalEvents(raw);
}
}
void installGzRecon(uintptr_t base){
    if(!matches(base,0x6df0c0,std::array<unsigned char,6>{0x40,0x57,0x48,0x83,0xec,0x50})
       ||!matches(base,0x6c2250,std::array<unsigned char,8>{0x48,0x89,0x54,0x24,0x10,0x55,0x56,0x57})
       ||!matches(base,0xabb480,std::array<unsigned char,6>{0x40,0x53,0x48,0x83,0xec,0x20})
       ||!matches(base,0xab4e60,std::array<unsigned char,6>{0x40,0x53,0x48,0x83,0xec,0x30})
       ||!matches(base,0x14fbd0,std::array<unsigned char,6>{0x40,0x56,0x48,0x83,0xec,0x40})
       ||!matches(base,0x6fa4a0,std::array<unsigned char,7>{0x40,0x56,0x57,0x41,0x56,0x48,0x83})
       ||!matches(base,0xab7e20,std::array<unsigned char,7>{0x48,0x89,0x4c,0x24,0x08,0x48,0x81}))
        throw std::runtime_error("GZ native recon signature mismatch");
    image=base;const auto target=reinterpret_cast<void*>(base+0x6df0c0);
    if(MH_CreateHook(target,reinterpret_cast<void*>(&update),reinterpret_cast<void**>(&original))!=MH_OK
       ||MH_EnableHook(target)!=MH_OK)throw std::runtime_error("Cannot install GZ native recon adapter");
    // GZ's event job (14fd60, VT 15afb08) owns this pump. Both binocular UI
    // updates are inactive outside the authored mode; the mission event job
    // remains scheduled and dispatches Candidate/PlayButton in native order.
    const auto eventPump=reinterpret_cast<void*>(base+0x14fbd0);
    if(MH_CreateHook(eventPump,reinterpret_cast<void*>(&updateIntelEvents),reinterpret_cast<void**>(&originalEvents))!=MH_OK
       ||MH_EnableHook(eventPump)!=MH_OK)throw std::runtime_error("Cannot install GZ native intel adapter");
    enabled.store(true);log("GZ native physical-optic acquisition adapter installed");
}
void publishGzReconFrame(const HeadCameraSample& frame){if(enabled.load()){std::lock_guard lock(frameMutex);latest=frame;}}
void stopGzRecon() noexcept {enabled.store(false);}
}
