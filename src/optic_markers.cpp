#include "mgs5vr/optic_markers.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/log.hpp"
#include <windows.h>
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
using Update=void(*)(void*,void*,const void*);
Update original{};
uintptr_t base{};
std::atomic_bool enabled{};
std::mutex mutex;
HeadCameraSample latest;
OpticWaypoints waypoints;
uint64_t consumed{},consumedClear{};
struct alignas(16) Vector {float x{},y{},z{},w{};};

template<class T> T read(uintptr_t address){
    T value{};SIZE_T size{};
    if(address)ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),&value,sizeof(value),&size);
    return size==sizeof(value)?value:T{};
}
template<size_t N> bool matches(uintptr_t address,const std::array<unsigned char,N>& expected){
    return read<std::array<unsigned char,N>>(address)==expected;
}

std::optional<Vector> surface(const HeadCameraSample& frame){
    const auto& optic=frame.controllers.optic;
    if(!optic.held||!optic.pose.tracked||!optic.pose.ray.tracked
       ||optic.pose.kind!=OpticKind::binocular||!read<uintptr_t>(base+0x2c79710))return {};
    const auto camera=nativeTrackedPose(frame.nativePose,frame.headPose,optic.pose.rightEyepiece);
    const auto direction=rotate(camera.orientation,{0,0,-1});
    if(!valid(camera)||!valid(Pose{{},direction}))return {};
    // Same synchronous collision-query ABI used by native ground IK, with
    // UiUpdateMarker's all-layer visibility filter. Cast from the objective
    // side of the housing; a sky miss never creates a floating waypoint.
    const auto origin=camera.position+direction*.15f;
    const auto endpoint=origin+direction*1500.f;
    alignas(16) std::array<std::byte,0x250> query{};
    using Init=void*(*)(void*,uint32_t);
    using Ray=uint32_t(*)(void*,uint32_t,const Vector*,const Vector*,float);
    using Position=void*(*)(const void*,Vector*);
    reinterpret_cast<Init>(base+0xa0fb50)(query.data(),0);
    const uint64_t layers=~uint64_t{},filter=0x80000004;
    std::memcpy(query.data(),&layers,sizeof(layers));
    std::memcpy(query.data()+0x18,&filter,sizeof(filter));
    const Vector start{origin.x,origin.y,origin.z,0},end{endpoint.x,endpoint.y,endpoint.z,0};
    if(!reinterpret_cast<Ray>(base+0x1b9b130)(query.data(),0x04000700,&start,&end,0.f))return {};
    const auto address=reinterpret_cast<uintptr_t>(query.data());
    const auto count=read<uint32_t>(address+0x60);const auto index=read<int32_t>(address+0x64);
    if(!count||count>5||index<0||static_cast<uint32_t>(index)>=count)return {};
    Vector hit;
    reinterpret_cast<Position>(base+0x1b9a5d0)(query.data()+0x70+index*0x60,&hit);
    const Vec3 offset=Vec3{hit.x,hit.y,hit.z}-origin;
    const float along=dot(offset,direction);
    const auto residual=offset-direction*along;
    if(!valid(Pose{{},{hit.x,hit.y,hit.z}})||along<.05f||along>1500.1f||dot(residual,residual)>.04f)return {};
    return hit;
}

void snapshot(uintptr_t markers,uint64_t now,uint64_t activation){
    OpticWaypoints result;result.sampleTime=now;result.activation=activation;
    // Native insertion/removal uses 0x30-byte records in the marker pool.
    // +0x28 carries the actual rotating A..Z label, not the array index.
    const auto pool=read<uintptr_t>(markers+0x90);
    const auto records=read<uintptr_t>(pool+0x10);
    const auto first=read<uint32_t>(pool+0x18);
    const auto count=read<uint32_t>(markers+0xac);
    if(records&&first<65536&&count<=5)for(uint32_t i=0;i<count;++i){
        const auto address=records+(static_cast<uintptr_t>(first)+i)*0x30;
        const auto position=read<Vec3>(address+0x10);
        const auto label=read<uint16_t>(address+0x28)&0x1f;
        if((read<uint8_t>(address+0x1f)&2)&&label>=1&&label<=26&&valid(Pose{{},position}))
            result.points[result.count++]={position,static_cast<uint8_t>(label)};
    }
    const auto actors=read<std::array<std::byte,0x3b7*0x30>>(read<uintptr_t>(markers+0x88));
    for(size_t i=0;i<0x3b7&&result.count<result.points.size();++i){
        const auto* record=actors.data()+i*0x30;
        // Type 1 is the native person-marker pool. Bit 1 is acquired;
        // bit 2 excludes inactive actors. Read positions again every update.
        const auto flags=std::to_integer<uint8_t>(record[0x1f]);
        if(std::to_integer<uint8_t>(record[0x1e])!=1||(flags&7)!=3)continue;
        Vec3 position{};std::memcpy(&position,record+0x10,sizeof(position));
        if(valid(Pose{{},position})&&dot(position,position)>1)
            result.points[result.count++]={position,0};
    }
    std::lock_guard lock(mutex);waypoints=result;
}

bool markPerson(uintptr_t services,uintptr_t markers,const HeadCameraSample& frame,const std::optional<Vector>& hit,bool acquire){
    const auto camera=nativeTrackedPose(frame.nativePose,frame.headPose,frame.controllers.optic.pose.rightEyepiece);
    const auto forward=rotate(camera.orientation,{0,0,-1});
    const float surfaceDistance=hit?dot(Vec3{hit->x,hit->y,hit->z}-camera.position,forward):1000.f;
    const auto records=read<uintptr_t>(markers+0x88);
    const auto actors=read<std::array<std::byte,0x3b7*0x30>>(records);
    uintptr_t selected{};uint16_t id{};float best=1e6f;
    for(size_t i=0;i<0x3b7;++i){
        const auto* record=actors.data()+i*0x30;
        const auto flags=std::to_integer<uint8_t>(record[0x1f]);
        if(std::to_integer<uint8_t>(record[0x1e])!=1||(flags&5)!=1)continue;
        if(!acquire&&!(flags&2))continue;
        Vec3 position{};std::memcpy(&position,record+0x10,sizeof(position));
        if(!valid(Pose{{},position})||dot(position,position)<1)continue;
        // This native visibility query hits world geometry, but can pass
        // through a character. Intersect the person's marker capsule with
        // the aim ray, and use the world hit only as the occlusion limit.
        const auto offset=position-camera.position;const float along=dot(offset,forward);
        if(along<1||along>1000||(acquire&&along>surfaceDistance+.35f))continue;
        const auto relative=camera.position+forward*along-position;
        if(relative.x*relative.x+relative.z*relative.z>.65f*.65f||relative.y< -1.8f||relative.y>.5f)continue;
        const float score=along;
        if(score>=best)continue;
        best=score;selected=records+i*0x30;std::memcpy(&id,record+0x1c,sizeof(id));
    }
    if(!selected)return false;
    // Same GameObject command and payload as TppMarker.Enable/DisableMarker. The
    // owning native component publishes the tag and keeps its campaign state.
    struct Command {uint64_t name{0x73f1d2c50cda};uint8_t layers{3},flags{1};uint16_t pad{};uint32_t event{};float value{};uint32_t tail{};} command;
    command.flags=acquire?1:0;
    const auto objects=read<uintptr_t>(services+0x60);const auto table=read<uintptr_t>(objects);
    using Send=void*(*)(void*,int32_t*,uint32_t,Command*);
    const auto send=read<uintptr_t>(table+0x38);if(!send)return false;
    int32_t result{-1};reinterpret_cast<Send>(send)(reinterpret_cast<void*>(objects),&result,id,&command);
    const bool marked=(read<uint8_t>(selected+0x1f)&2)!=0;
    std::ostringstream message;message<<"Physical optic native person "<<(acquire?"mark":"clear")<<" id="<<id<<" result="<<result<<" acquired="<<marked;
    log(message.str());
    return marked==acquire;
}

bool clearWaypoint(uintptr_t markers,const HeadCameraSample& frame){
    const auto camera=nativeTrackedPose(frame.nativePose,frame.headPose,frame.controllers.optic.pose.rightEyepiece);
    const auto forward=rotate(camera.orientation,{0,0,-1});
    const auto pool=read<uintptr_t>(markers+0x90);
    const auto records=read<uintptr_t>(pool+0x10);
    const auto first=read<uint32_t>(pool+0x18),count=read<uint32_t>(markers+0xac);
    if(!records||first>=65536||!count||count>5)return false;
    uint32_t selected=count;float best=.015f*.015f;
    for(uint32_t i=0;i<count;++i){
        const auto record=records+(static_cast<uintptr_t>(first)+i)*0x30;
        if(!(read<uint8_t>(record+0x1f)&2))continue;
        const auto offset=read<Vec3>(record+0x10)-camera.position;
        const float along=dot(offset,forward);if(along<.1f)continue;
        const auto lateral=offset-forward*along;
        const float score=dot(lateral,lateral)/(along*along);
        if(score<best){best=score;selected=i;}
    }
    if(selected==count)return false;
    // Native single-marker removal compacts the pool and updates map/HUD state.
    using Remove=void(*)(void*,uint32_t);
    reinterpret_cast<Remove>(base+0x5c6250)(reinterpret_cast<void*>(markers),selected);
    const auto remaining=read<uint32_t>(markers+0xac);
    std::ostringstream message;message<<"Physical optic native waypoint clear index="<<selected<<" count="<<remaining;
    log(message.str());return remaining+1==count;
}

void update(void* job,void* input,const void* output){
    // Execute on the native marker-update job, never on the XR/render thread.
    original(job,input,output);
    if(!enabled.load())return;
    HeadCameraSample frame;bool requested{},clearRequested{};
    {std::lock_guard lock(mutex);
        frame=latest;
        requested=frame.controllers.opticMarkSequence&&frame.controllers.opticMarkSequence!=consumed;
        if(requested)consumed=frame.controllers.opticMarkSequence;
        clearRequested=frame.controllers.opticClearSequence&&frame.controllers.opticClearSequence!=consumedClear;
        if(clearRequested)consumedClear=frame.controllers.opticClearSequence;
    }
    const auto status=headCamera().status();const auto now=steadyMilliseconds();
    if(!frame.applied||frame.menuOpen||!status.active||frame.activation!=status.activation
       ||now<frame.sampleTime||now-frame.sampleTime>150)return;
    using Services=uintptr_t(*)();
    const auto services=reinterpret_cast<Services>(base+0xbff050)();
    const auto ui=read<uintptr_t>(services+0x98);
    const auto markers=read<uintptr_t>(ui+0x80);
    if(read<uintptr_t>(markers)!=base+0x21addf0)return;
    snapshot(markers,now,status.activation);
    if((!requested&&!clearRequested)||!frame.controllers.optic.held||!frame.controllers.optic.pose.ray.tracked)return;
    if(clearRequested){
        if(clearWaypoint(markers,frame)||markPerson(services,markers,frame,{},false)){
            snapshot(markers,now,status.activation);rumbleMailbox().publish({0,.25f,now});
        }
        return;
    }
    const auto hit=surface(frame);
    if(markPerson(services,markers,frame,hit,true)){
        snapshot(markers,now,status.activation);rumbleMailbox().publish({0,.45f,now});return;
    }
    if(!hit){log("Physical optic mark: no visible surface on aim ray");return;}
    // TppMarker2System's native waypoint insertion method. Its own capacity,
    // replacement and marker-publication behavior remain authoritative.
    using Place=void(*)(void*,const Vector*);
    reinterpret_cast<Place>(base+0x5c53a0)(reinterpret_cast<void*>(markers),&*hit);
    snapshot(markers,now,status.activation);
    rumbleMailbox().publish({0,.35f,now});
    std::ostringstream message;message<<"Physical optic native waypoint sequence="<<consumed
        <<" position="<<hit->x<<','<<hit->y<<','<<hit->z
        <<" count="<<read<uint32_t>(markers+0xac);
    log(message.str());
}
}

namespace mgs5vr {
void installOpticMarkers(uintptr_t moduleBase){
    constexpr std::array<unsigned char,9> updateEntry{0x48,0x85,0xd2,0x0f,0x84,0x99,0x02,0,0};
    constexpr std::array<unsigned char,10> placeEntry{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20};
    constexpr std::array<unsigned char,6> removeEntry{0x40,0x53,0x48,0x83,0xec,0x20};
    constexpr std::array<unsigned char,9> initEntry{0x48,0x89,0x4c,0x24,0x08,0x48,0x83,0xec,0x18};
    constexpr std::array<unsigned char,10> rayEntry{0x48,0x83,0xec,0x38,0xf3,0x0f,0x10,0x44,0x24,0x60};
    constexpr std::array<unsigned char,10> positionEntry{0x4c,0x8b,0xdc,0x48,0x81,0xec,0x98,0,0,0};
    if(!matches(moduleBase+0x930710,updateEntry)||!matches(moduleBase+0x5c53a0,placeEntry)
       ||!matches(moduleBase+0x5c6250,removeEntry)
       ||!matches(moduleBase+0xa0fb50,initEntry)||!matches(moduleBase+0x1b9b130,rayEntry)
       ||!matches(moduleBase+0x1b9a5d0,positionEntry)
       ||read<uintptr_t>(moduleBase+0x21ade40)!=moduleBase+0x5c53a0
       ||read<uintptr_t>(moduleBase+0x21ade58)!=moduleBase+0x5c6250)
        throw std::runtime_error("Native optic marker signature mismatch");
    base=moduleBase;
    const auto address=reinterpret_cast<void*>(base+0x930710);
    const auto created=MH_CreateHook(address,reinterpret_cast<void*>(&update),reinterpret_cast<void**>(&original));
    if(created!=MH_OK)throw std::runtime_error(MH_StatusToString(created));
    if(MH_EnableHook(address)!=MH_OK)throw std::runtime_error("Cannot enable native optic markers");
    enabled.store(true);log("Physical optic trigger connected to native waypoint placement");
}
void publishOpticMarkerFrame(const HeadCameraSample& frame){std::lock_guard lock(mutex);latest=frame;}
OpticWaypoints opticWaypoints(){std::lock_guard lock(mutex);return waypoints;}
void stopOpticMarkers() noexcept {enabled.store(false);}
}
