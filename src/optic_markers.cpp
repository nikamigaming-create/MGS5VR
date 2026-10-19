#include "mgs5vr/optic_markers.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/log.hpp"
#include "mgs5vr/recon.hpp"
#include "mgs5vr/optic_events.hpp"
#include "mgs5vr/optic_radio.hpp"
#include <windows.h>
#include <MinHook.h>
#include <array>
#include <atomic>
#include <cstring>
#include <cmath>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <intrin.h>

namespace {
using namespace mgs5vr;
using Update=void(*)(void*,void*,const void*);
Update original{};
using RadioCommit=void(*)(void*);
RadioCommit originalRadioCommit{};
uintptr_t base{};
std::atomic_bool enabled{};
std::mutex mutex;
HeadCameraSample latest;
OpticWaypoints waypoints;
CombatSightSnapshot combatSight;
std::atomic_uint64_t combatSightExpiry{};
std::atomic_uint16_t combatMarker{0xffff};
// Keep a finite segment outside the muzzle, including genuine close encounters.
constexpr float combatSightMinimumRange=.25f;
CoverProbeRequest coverRequest;
CoverProbeSnapshot coverResult;
uint64_t consumed{},consumedClear{};
uint64_t consumedIntel{};
ReconDwell reconDwell;
ReconClearPolicy reconClears;
std::atomic_bool resetReconClears{};
OpticUseEdge opticUseEdge;
OpticEventQueue opticEvents;
OpticEvent queuedUse;
void publishEvent(OpticEvent event){
    std::lock_guard lock(mutex);
    if(event.kind==OpticEventKind::raised)queuedUse={};
    opticEvents.push(event);
}
struct alignas(16) Vector {float x{},y{},z{},w{};};
using NativeRay=uint32_t(*)(void*,uint32_t,const Vector*,const Vector*,float,uint32_t);
NativeRay originalNativeRay{};
std::atomic_uint64_t rayTraceWindow{};
std::atomic_uint32_t rayTraceCount{};

template<class T> T read(uintptr_t address){
    T value{};SIZE_T size{};
    if(address)ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),&value,sizeof(value),&size);
    return size==sizeof(value)?value:T{};
}
template<size_t N> bool matches(uintptr_t address,const std::array<unsigned char,N>& expected){
    return read<std::array<unsigned char,N>>(address)==expected;
}

uint32_t observeNativeRay(void* query,uint32_t flags,const Vector* from,const Vector* to,float radius,uint32_t tag){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const auto expiry=combatSightExpiry.load(std::memory_order_relaxed);
    const auto now=expiry?steadyMilliseconds():0;
    bool trace=false;
    if(expiry>now&&combatMarker.load(std::memory_order_relaxed)!=0xffff){
        auto window=rayTraceWindow.load(std::memory_order_relaxed);
        if(now>=window+1000&&rayTraceWindow.compare_exchange_strong(window,now))rayTraceCount.store(0);
        trace=rayTraceCount.fetch_add(1,std::memory_order_relaxed)<128;
    }
    const auto address=reinterpret_cast<uintptr_t>(query);
    const auto start=trace?read<Vector>(reinterpret_cast<uintptr_t>(from)):Vector{};
    const auto end=trace?read<Vector>(reinterpret_cast<uintptr_t>(to)):Vector{};
    const auto layers=trace?read<uint64_t>(address):0;
    const auto filter=trace?read<uint64_t>(address+0x18):0;
    // Preserve every native argument, result and query write. The observation
    // runs only after the synchronous query has released its native scene lock.
    const auto result=originalNativeRay(query,flags,from,to,radius,tag);
    if(trace){
        std::ostringstream out;out<<"NativeRayProbe ms="<<now<<" caller="
            <<(caller>=base&&caller<base+0x208d000?caller-base:0)
            <<" thread="<<GetCurrentThreadId()<<" flags="<<flags<<" layers="<<layers<<" filter="<<filter
            <<" radius="<<radius<<" tag="<<tag<<" from="<<start.x<<','<<start.y<<','<<start.z
            <<" to="<<end.x<<','<<end.y<<','<<end.z<<" result="<<result
            <<" count="<<read<uint32_t>(address+0x60)<<" index="<<read<int32_t>(address+0x64);
        log(out.str());
    }
    return result;
}

std::optional<Vector> surface(const HeadCameraSample& frame,bool* queryValid=nullptr){
    if(queryValid)*queryValid=false;
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
    if(!reinterpret_cast<Ray>(base+0x1b9b130)(query.data(),0x04000700,&start,&end,0.f)){
        if(queryValid)*queryValid=true; // A valid sky miss, not an unavailable query.
        return {};
    }
    const auto address=reinterpret_cast<uintptr_t>(query.data());
    const auto count=read<uint32_t>(address+0x60);const auto index=read<int32_t>(address+0x64);
    if(!count||count>5||index<0||static_cast<uint32_t>(index)>=count)return {};
    Vector hit;
    reinterpret_cast<Position>(base+0x1b9a5d0)(query.data()+0x70+index*0x60,&hit);
    const Vec3 offset=Vec3{hit.x,hit.y,hit.z}-origin;
    const float along=dot(offset,direction);
    const auto residual=offset-direction*along;
    if(!valid(Pose{{},{hit.x,hit.y,hit.z}})||along<.05f||along>1500.1f||dot(residual,residual)>.04f)return {};
    if(queryValid)*queryValid=true;
    return hit;
}

// Reuse the signature-verified native world query and its visibility filter.
// This must stay on the marker-update job, rather than the pipe/XR thread.
std::optional<Vector> combatSurface(Vec3 origin,CombatSightTarget& target,bool projectileComparison=false){
    const auto endpoint=target.aim;
    auto& queryValid=target.queryValid;queryValid=false;target.queryReason=1;
    const auto delta=endpoint-origin;const float length=std::sqrt(dot(delta,delta));
    if(!read<uintptr_t>(base+0x2c79710))return {};
    target.queryReason=2;
    if(!valid(Pose{{},origin})||!valid(Pose{{},endpoint})
       ||!std::isfinite(length)||length<combatSightMinimumRange||length>300.f)return {};
    alignas(16) std::array<std::byte,0x250> query{};
    using Init=void*(*)(void*,uint32_t);
    using Ray=uint32_t(*)(void*,uint32_t,const Vector*,const Vector*,float);
    using Position=void*(*)(const void*,Vector*);
    reinterpret_cast<Init>(base+0xa0fb50)(query.data(),0);
    // The optional comparison reproduces the actual accepted ALM48 sky-shot
    // query (retained 20261018T110942700836Z). It never supplies admission.
    const uint64_t layers=projectileComparison?0x40:~uint64_t{};
    const uint64_t filter=projectileComparison?0x2:0x80000004;
    std::memcpy(query.data(),&layers,sizeof(layers));
    std::memcpy(query.data()+0x18,&filter,sizeof(filter));
    const Vector start{origin.x,origin.y,origin.z,0},end{endpoint.x,endpoint.y,endpoint.z,0};
    target.rayResult=reinterpret_cast<Ray>(base+0x1b9b130)(query.data(),projectileComparison?0x700:0x04000700,&start,&end,0.f);
    if(!target.rayResult){
        target.queryReason=3;queryValid=true;return {};
    }
    const auto address=reinterpret_cast<uintptr_t>(query.data());
    const auto count=read<uint32_t>(address+0x60);const auto index=read<int32_t>(address+0x64);
    target.hitCount=count;target.hitIndex=index;target.queryReason=4;
    if(!count||count>5||index<0||static_cast<uint32_t>(index)>=count)return {};
    Vector hit;
    const auto* record=query.data()+0x70+index*0x60;
    reinterpret_cast<Position>(base+0x1b9a5d0)(record,&hit);
    if(target.aimKind==2){
        std::memcpy(target.hitRecordWords.data(),record,0x60);
        // The verified native position getter uses record+0x58 as the
        // shape-to-world matrix and adds its translation at matrix+0x30.
        // Read the diagnostic matrix without executing an unknown owner API.
        const auto transform=target.hitRecordWords[11];SIZE_T bytes{};
        if(transform)ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(transform),
            target.hitTransform.data(),sizeof(target.hitTransform),&bytes);
        target.hitTransformRead=bytes==sizeof(target.hitTransform)
            &&std::all_of(target.hitTransform.begin(),target.hitTransform.end(),[](float v){return std::isfinite(v);});
        if(!target.hitTransformRead)target.hitTransform={};
    }
    const auto direction=delta*(1.f/length),offset=Vec3{hit.x,hit.y,hit.z}-origin;
    const float along=dot(offset,direction);const auto residual=offset-direction*along;
    target.queryReason=5;
    if(!valid(Pose{{},{hit.x,hit.y,hit.z}})||!std::isfinite(along)||!std::isfinite(dot(residual,residual)))return {};
    target.rawSurface={hit.x,hit.y,hit.z};target.rawAlong=along;target.rawLateralSquared=dot(residual,residual);
    target.queryReason=6;if(along<0.f)return {};
    target.queryReason=7;if(along>length+.1f)return {};
    target.queryReason=8;if(dot(residual,residual)>.04f)return {};
    target.queryReason=9;queryValid=true;return hit;
}

void sampleCombatSight(uintptr_t markers,const HeadCameraSample& frame,uint64_t now){
    if(now>=combatSightExpiry.load())return;
    {std::lock_guard lock(mutex);
        if(combatSight.activation==frame.activation&&combatSight.presentationEpoch==frame.controllers.presentationEpoch
           &&now>=combatSight.sampleTime&&now-combatSight.sampleTime<50)return;
    }
    CombatSightSnapshot result;
    result.sampleTime=now;result.activation=frame.activation;
    result.presentationEpoch=frame.controllers.presentationEpoch;
    result.playerOwner=frame.playerOwner;result.trackingSequence=frame.trackingSequence;
    const auto requested=combatMarker.load();
    result.requestedMarker.id=requested;
    const auto& weapon=frame.weaponRig;
    if(frame.controllers.presentationFocused&&!frame.controllers.vehicleControls
       &&frame.controllers.weaponReady&&frame.nativeFirearmActive
       &&frame.renderedPalmTracked[1]&&weapon.muzzleSolved){
        result.muzzle=compose(frame.renderedPalms[1],weapon.muzzleInGrip);
        result.muzzleTracked=valid(result.muzzle);
    }
    if(result.muzzleTracked){
        const auto records=read<uintptr_t>(markers+0x88);
        const auto actors=read<std::array<std::byte,0x3b7*0x30>>(records);
        std::vector<CombatSightTarget> candidates;
        for(size_t i=0;i<0x3b7;++i){
            const auto* record=actors.data()+i*0x30;
            const auto flags=std::to_integer<uint8_t>(record[0x1f]);
            const auto kind=std::to_integer<uint8_t>(record[0x1e]);
            CombatSightTarget target;
            std::memcpy(&target.id,record+0x1c,sizeof(target.id));
            std::memcpy(&target.marker,record+0x10,sizeof(target.marker));
            if(!target.id||target.id==0xffff||!valid(Pose{{},target.marker})||dot(target.marker,target.marker)<1)continue;
            if(target.id==requested)result.requestedMarker={target.id,target.marker,kind,flags,true};
            // Ordinary soldier observation is unchanged. An explicitly leased
            // non-person record uses its exact native marker point; no torso
            // offset or invented anatomical point is applied to a vehicle.
            const bool explicitMarker=target.id==requested&&requested!=0xffff;
            if((kind!=1&&!explicitMarker)||(flags&5)!=1)continue;
            // Aim inside the same native person-marker capsule used by recon;
            // this is a torso aim policy, not a recovered anatomical bone.
            target.aim=target.marker+(explicitMarker?Vec3{}:Vec3{0,-.65f,0});
            if(explicitMarker)target.aimKind=2;
            const auto delta=target.aim-result.muzzle.position;
            target.distance=std::sqrt(dot(delta,delta));
            if(target.distance>=combatSightMinimumRange&&target.distance<=180.f)candidates.push_back(target);
        }
        std::sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b){return a.distance<b.distance;});
        result.candidates=candidates.size();
        result.count=std::min(candidates.size(),result.targets.size());
        for(size_t i=0;i<result.count;++i){
            auto& target=candidates[i];
            const auto queryPoint=[&](CombatSightTarget& point){
                const auto delta=point.aim-result.muzzle.position;
                point.distance=std::sqrt(dot(delta,delta));
                const auto hit=combatSurface(result.muzzle.position,point);
                point.surfaceDistance=hit?std::sqrt(dot(Vec3{hit->x,hit->y,hit->z}-result.muzzle.position,
                    Vec3{hit->x,hit->y,hit->z}-result.muzzle.position)):point.distance;
                point.unoccluded=point.queryValid&&(!hit||point.surfaceDistance>=point.distance-.35f);
            };
            queryPoint(target);
            if(target.aimKind==2){
                // Read-only paired experiment on the same marker-update job,
                // fresh muzzle and exact target. Existing visibility remains
                // the sole admission result; never overwrite target with this.
                auto projectile=target;
                projectile.queryValid=false;projectile.rayResult=projectile.hitCount=0;
                projectile.hitIndex=-1;projectile.rawSurface={};
                projectile.rawAlong=projectile.rawLateralSquared=0;
                const auto hit=combatSurface(result.muzzle.position,projectile,true);
                std::ostringstream out;
                out<<"CombatProjectileComparison ms="<<now<<" marker="<<target.id
                    <<" muzzle="<<result.muzzle.position.x<<','<<result.muzzle.position.y<<','<<result.muzzle.position.z
                    <<" aim="<<target.aim.x<<','<<target.aim.y<<','<<target.aim.z
                    <<" distance="<<target.distance<<" visibility_valid="<<target.queryValid
                    <<" visibility_result="<<target.rayResult<<" visibility_along="<<target.rawAlong
                    <<" projectile_valid="<<projectile.queryValid<<" projectile_result="<<projectile.rayResult
                    <<" projectile_reason="<<static_cast<unsigned>(projectile.queryReason)<<" projectile_count="<<projectile.hitCount
                    <<" projectile_along="<<projectile.rawAlong
                    <<" projectile_hit="<<projectile.rawSurface.x<<','<<projectile.rawSurface.y<<','<<projectile.rawSurface.z
                    <<" projectile_clear="<<(projectile.queryValid&&(!hit||projectile.rawAlong>=target.distance-.35f));
                if(hit){
                    Vec3 normal{};
                    std::memcpy(&normal,reinterpret_cast<const std::byte*>(projectile.hitRecordWords.data())+0x20,sizeof(normal));
                    out<<" projectile_normal="<<normal.x<<','<<normal.y<<','<<normal.z
                        <<" projectile_matrix_read="<<projectile.hitTransformRead<<" projectile_record=";
                    for(size_t word=0;word<projectile.hitRecordWords.size();++word){
                        if(word)out<<',';
                        out<<projectile.hitRecordWords[word];
                    }
                    out<<" projectile_matrix=";
                    for(size_t element=0;element<projectile.hitTransform.size();++element){
                        if(element)out<<',';
                        out<<projectile.hitTransform[element];
                    }
                }
                log(out.str());
                auto ground=projectile;
                ground.aim=result.muzzle.position+Vec3{0,-3.f,0};
                ground.queryValid=false;ground.rayResult=ground.hitCount=0;
                ground.hitIndex=-1;ground.rawSurface={};
                ground.rawAlong=ground.rawLateralSquared=0;
                const auto groundHit=combatSurface(result.muzzle.position,ground,true);
                std::ostringstream control;
                control<<"CombatProjectileGroundControl ms="<<now
                    <<" valid="<<ground.queryValid<<" result="<<ground.rayResult
                    <<" reason="<<static_cast<unsigned>(ground.queryReason)<<" count="<<ground.hitCount
                    <<" along="<<ground.rawAlong<<" hit="<<static_cast<bool>(groundHit);
                log(control.str());
            }
            if(!target.unoccluded&&target.aimKind!=2){
                // Low cover can conceal the torso while the native recon
                // point is exposed. Query that actual point separately;
                // neither a guessed bone nor the first blocked ray admits it.
                auto exposed=target;exposed.aim=exposed.marker;exposed.aimKind=1;
                exposed.rawSurface={};exposed.rawAlong=exposed.rawLateralSquared=0;
                exposed.rayResult=exposed.hitCount=0;exposed.hitIndex=-1;
                queryPoint(exposed);
                if(exposed.unoccluded)target=exposed;
            }
            result.targets[i]=target;
        }
    }
    std::lock_guard lock(mutex);combatSight=result;
}

CoverProbeFrame coverFrame(const HeadCameraSample& frame,bool active){
    return {{frame.activation,frame.playerOwner,frame.controllers.presentationEpoch},
        frame.sampleTime,frame.trackingSequence,frame.nativePose.position,
        active&&frame.applied&&!frame.menuOpen&&frame.controllers.presentationFocused
        &&!frame.controllers.frontEnd&&!frame.controllers.loading&&!frame.controllers.scriptedDemo
        &&!frame.controllers.authoredCamera&&!frame.controllers.cabinPlay&&!frame.controllers.avatarEditor
        &&!frame.controllers.vehicleControls&&valid(frame.nativePose)};
}

void sampleCoverProbe(uintptr_t markers,const HeadCameraSample& frame,uint64_t now){
    CoverProbeRequest request;
    {std::lock_guard lock(mutex);
        if(!coverRequest.nonce||now>=coverRequest.expiresMs
           ||(coverResult.request.nonce==coverRequest.nonce&&coverResult.sampleMs))return;
        request=coverRequest;
    }
    const auto status=headCamera().status();
    CoverProbeSnapshot result;result.request=request;result.sampleMs=now;
    result.frame=coverFrame(frame,status.active&&!status.suspended&&status.activation==frame.activation);
    result.reason=coverProbeFrameReason(request,result.frame,now);
    if(result.reason==CoverProbeReason::none){
        // Native pose and supplied observed root are joined above before this
        // offset is used. No estimated standing/crouched eye height is added.
        result.eyeOffset=result.frame.nativeEye-request.observedPlayer;
        const auto actors=read<std::array<std::byte,0x3b7*0x30>>(read<uintptr_t>(markers+0x88));
        for(size_t i=0;i<0x3b7;++i){
            const auto* record=actors.data()+i*0x30;
            const auto flags=std::to_integer<uint8_t>(record[0x1f]);
            if(std::to_integer<uint8_t>(record[0x1e])!=1||(flags&5)!=1)continue;
            CoverProbePerson person;person.recordIndex=static_cast<uint16_t>(i);person.flags=flags;
            std::memcpy(&person.id,record+0x1c,sizeof(person.id));
            std::memcpy(&person.marker,record+0x10,sizeof(person.marker));
            if(!person.id||person.id==0xffff||!coverProbeFinite(person.marker)||dot(person.marker,person.marker)<1.f)continue;
            const auto distanceSquared=coverProbeDistanceSquared(person.marker,request.observedPlayer);
            if(distanceSquared<1.f||distanceSquared>180.f*180.f)continue;
            person.distance=std::sqrt(distanceSquared);
            bool duplicate{};
            for(size_t j=0;j<result.personCount;++j)if(result.persons[j].id==person.id)duplicate=true;
            if(duplicate)continue;
            if(result.personCount<result.persons.size())result.persons[result.personCount++]=person;
            else if(person.distance<result.persons.back().distance)result.persons.back()=person;
            else continue;
            std::sort(result.persons.begin(),result.persons.begin()+result.personCount,
                [](const auto& a,const auto& b){return a.distance<b.distance||(a.distance==b.distance&&a.id<b.id);});
        }
        if(!result.personCount)result.reason=CoverProbeReason::noActors;
        else {
            result.count=request.count;result.available=true;
            for(size_t i=0;i<result.count;++i){
                auto& candidate=result.candidates[i];
                candidate.node=request.nodes[i].node;candidate.point=request.nodes[i].point;
                candidate.eye=candidate.point+result.eyeOffset;
                candidate.count=result.personCount;candidate.allMarkerRaysObstructed=true;
                for(size_t j=0;j<result.personCount;++j){
                    const auto& person=result.persons[j];
                    CombatSightTarget ray;ray.id=person.id;ray.marker=person.marker;ray.aim=candidate.eye;
                    const auto delta=ray.aim-person.marker;ray.distance=std::sqrt(dot(delta,delta));
                    // Exactly the combat observer's verified ABI/filter, on
                    // this native job. The origin is the actual marker point,
                    // not a recovered enemy eye/bone or an AI vision request.
                    const auto hit=combatSurface(person.marker,ray);
                    ray.surfaceDistance=hit?std::sqrt(coverProbeDistanceSquared(ray.rawSurface,person.marker)):ray.distance;
                    const bool blocked=coverProbeTerrainObstruction(ray.queryValid,ray.queryReason,ray.rayResult,
                        ray.distance,ray.rawAlong,ray.rawLateralSquared);
                    candidate.rays[j]={person.id,person.marker,candidate.eye,ray.rawSurface,
                        ray.distance,ray.surfaceDistance,ray.rawAlong,ray.rawLateralSquared,
                        ray.queryValid,blocked,ray.queryReason,ray.rayResult,ray.hitCount,ray.hitIndex};
                    candidate.allMarkerRaysObstructed=candidate.allMarkerRaysObstructed&&blocked;
                }
            }
        }
    }
    std::lock_guard lock(mutex);
    if(coverRequest.nonce==request.nonce&&coverRequest.issuedMs==request.issuedMs
       &&coverRequest.expiresMs==request.expiresMs)coverResult=result;
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
        uint16_t actor{};std::memcpy(&actor,record+0x1c,sizeof(actor));
        if(valid(Pose{{},position})&&dot(position,position)>1)
            result.points[result.count++]={position,0,actor};
    }
    std::lock_guard lock(mutex);waypoints=result;
}

struct PersonTarget {uintptr_t record{};uint16_t id{};bool marked{};};
std::optional<PersonTarget> personTarget(uintptr_t markers,const HeadCameraSample& frame,const std::optional<Vector>& hit,bool acquire){
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
    if(!selected)return {};
    return PersonTarget{selected,id,(read<uint8_t>(selected+0x1f)&2)!=0};
}

bool markPerson(uintptr_t services,const PersonTarget& target,bool acquire){
    // Same GameObject command and payload as TppMarker.Enable/DisableMarker. The
    // owning native component publishes the tag and keeps its campaign state.
    struct Command {uint64_t name{0x73f1d2c50cda};uint8_t layers{3},flags{1};uint16_t pad{};uint32_t event{};float value{};uint32_t tail{};} command;
    command.flags=acquire?1:0;
    const auto objects=read<uintptr_t>(services+0x60);const auto table=read<uintptr_t>(objects);
    using Send=void*(*)(void*,int32_t*,uint32_t,Command*);
    const auto send=read<uintptr_t>(table+0x38);if(!send)return false;
    int32_t result{-1};reinterpret_cast<Send>(send)(reinterpret_cast<void*>(objects),&result,target.id,&command);
    const bool marked=(read<uint8_t>(target.record+0x1f)&2)!=0;
    std::ostringstream message;message<<"Physical optic native person "<<(acquire?"mark":"clear")<<" id="<<target.id<<" result="<<result<<" acquired="<<marked;
    log(message.str());
    return marked==acquire;
}

struct RadioTarget {uint16_t id{0xffff};float distance{};};
std::optional<RadioTarget> radioTarget(uintptr_t system,uintptr_t markers,
                                     const HeadCameraSample& frame,const std::optional<Vector>& hit){
    const auto sight=nativeTrackedPose(frame.nativePose,frame.headPose,frame.controllers.optic.pose.rightEyepiece);
    const auto forward=rotate(sight.orientation,{0,0,-1});
    const float visible=hit?dot(Vec3{hit->x,hit->y,hit->z}-sight.position,forward):1500.f;
    const auto groups=read<uintptr_t>(system+0x30),instances=read<uintptr_t>(system+0x48);
    std::optional<RadioTarget> best;
    for(uint32_t i=0;groups&&instances&&i<50;++i){
        const auto instance=instances+i*0x10;
        const auto group=read<uint8_t>(instance+8);
        if(group>=50||!(read<uint8_t>(instance+9)&2))continue;
        const auto parameter=groups+group*0x18;
        if((read<uint8_t>(parameter+0x14)&3)!=3)continue;
        const auto geometry=read<uintptr_t>(instance);
        if(!geometry)continue;
        const Pose volume{read<Quat>(geometry+0x20),read<Vec3>(geometry)};
        const auto distance=opticRadioBoxHit(sight,volume,read<Vec3>(geometry+0x10),
            read<float>(parameter+8),read<float>(parameter+0xc));
        if(!distance||*distance>visible+.35f||(best&&best->distance<=*distance))continue;
        best=RadioTarget{static_cast<uint16_t>(0x9000+group),*distance};
    }
    // The ordinary native person pool remains the source for actor intel.
    // Native radio availability decides whether this actor can be selected.
    if(const auto person=personTarget(markers,frame,hit,true)){
        const auto position=read<Vec3>(person->record+0x10);
        const auto distance=dot(position-sight.position,forward);
        using Available=bool(*)(void*,uint32_t);
        if((!best||distance<best->distance)&&reinterpret_cast<Available>(base+0xdc8610)(reinterpret_cast<void*>(system),person->id))
            best=RadioTarget{person->id,distance};
    }
    return best;
}

void commitRadio(void* object){
    const auto system=reinterpret_cast<uintptr_t>(object)+0x40;
    static uintptr_t ownedSystem{};
    static uint16_t ownedTarget{0xffff};
    static bool owned{};
    if(!enabled.load()||read<uintptr_t>(system)!=base+0x234c330){originalRadioCommit(object);return;}
    HeadCameraSample frame;bool requested{},useQueued{};
    {std::lock_guard lock(mutex);frame=latest;
        useQueued=queuedUse.sampleTime&&queuedUse.activation==frame.activation;
        requested=frame.controllers.opticIntelSequence&&frame.controllers.opticIntelSequence!=consumedIntel;
        if(requested)consumedIntel=frame.controllers.opticIntelSequence;
    }
    const auto now=steadyMilliseconds();const auto status=headCamera().status();
    const auto& optic=frame.controllers.optic;
    const bool usingOptic=frame.applied&&!frame.menuOpen&&status.active&&!status.suspended
        &&frame.activation==status.activation&&now>=frame.sampleTime&&now-frame.sampleTime<=150
        &&optic.active&&optic.held&&optic.pose.tracked&&optic.pose.ray.tracked&&optic.pose.kind==OpticKind::binocular;
    std::optional<RadioTarget> target;
    if(usingOptic&&useQueued){
        using Services=uintptr_t(*)();
        const auto services=reinterpret_cast<Services>(base+0xbff050)();
        const auto ui=read<uintptr_t>(services+0x98),radio=read<uintptr_t>(ui+0x90);
        const auto markers=read<uintptr_t>(ui+0x80);
        if(read<uintptr_t>(radio+0x20)==system&&read<uintptr_t>(markers)==base+0x21addf0){
            bool queryValid{};const auto hit=surface(frame,&queryValid);
            if(queryValid)target=radioTarget(system,markers,frame,hit);
        }
    }
    using Select=void(*)(void*,uint16_t);
    const auto select=reinterpret_cast<Select>(base+0xdc8ca0);
    const auto selectType=reinterpret_cast<Select>(base+0xdc8cb0);
    if(usingOptic){
        const uint16_t id=target?target->id:0xffff;
        if(!owned||ownedSystem!=system||ownedTarget!=id){
            log("Physical optic native intel candidate id="+std::to_string(id));
            if(target)rumbleMailbox().publish({0,.12f,now});
        }
        select(reinterpret_cast<void*>(system),id);
        selectType(reinterpret_cast<void*>(system),target?static_cast<uint16_t>(id>>9):0xffff);
        owned=true;ownedSystem=system;ownedTarget=id;
    }else if(owned){
        // Relinquish only the candidate this physical sight published. An
        // authored/native selection that has replaced it keeps ownership.
        if(ownedSystem==system&&read<uint16_t>(system+0x80)==ownedTarget){
            select(reinterpret_cast<void*>(system),0xffff);
            selectType(reinterpret_cast<void*>(system),0xffff);
        }
        owned=false;
    }
    // The original commit emits the game's queued Candidate event and owns
    // its selected target. Play uses its radio table, enable flags and busy
    // checks, including MessageOnly targets in authored lessons.
    originalRadioCommit(object);
    if(requested&&usingOptic&&target){
        using Play=bool(*)(void*);
        const bool played=reinterpret_cast<Play>(base+0xdc8740)(reinterpret_cast<void*>(system));
        log("Physical optic native intel request id="+std::to_string(target->id)+" accepted="+std::to_string(played));
        if(played)rumbleMailbox().publish({0,.4f,now});
    }
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
    if(resetReconClears.exchange(false))reconClears.reset();
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
       ||now<frame.sampleTime||now-frame.sampleTime>150){reconDwell.reset();opticUseEdge.reset();return;}
    using Services=uintptr_t(*)();
    const auto services=reinterpret_cast<Services>(base+0xbff050)();
    const auto ui=read<uintptr_t>(services+0x98);
    const auto markers=read<uintptr_t>(ui+0x80);
    if(read<uintptr_t>(markers)!=base+0x21addf0){reconDwell.reset();return;}
    reconClears.scene(frame.playerOwner,read<uintptr_t>(markers+0x88));
    snapshot(markers,now,status.activation);
    sampleCombatSight(markers,frame,now);
    sampleCoverProbe(markers,frame,now);
    const auto& optic=frame.controllers.optic;
    if(!optic.held||!optic.pose.tracked||!optic.pose.ray.tracked||optic.pose.kind!=OpticKind::binocular){reconDwell.reset();opticUseEdge.reset();return;}
    if(opticUseEdge.update(optic.active,frame.sampleTime,frame.activation))
        publishEvent({OpticEventKind::raised,{},frame.sampleTime,frame.activation});
    const bool automatic=optic.active&&frame.controllers.binocularAutoMark;
    if(!automatic)reconDwell.reset();
    if(!automatic&&!requested&&!clearRequested)return;
    if(clearRequested){
        const auto target=personTarget(markers,frame,{},false);
        const bool waypointCleared=clearWaypoint(markers,frame);
        const bool personCleared=!waypointCleared&&target&&markPerson(services,*target,false);
        if(waypointCleared||personCleared){
            if(personCleared){
                reconClears.clear(target->id);
                reconDwell.update(target->id,frame.sampleTime,frame.activation,frame.controllers.binocularMarkDwellMs);
                reconDwell.suppress();
            }
            snapshot(markers,now,status.activation);rumbleMailbox().publish({0,.25f,now});
        }
        return;
    }
    bool queryValid{};
    const auto hit=surface(frame,&queryValid);
    if(!queryValid){reconDwell.reset();return;}
    const auto target=personTarget(markers,frame,hit,true);
    bool acquiredByLook{};
    if(automatic){
        acquiredByLook=reconDwell.update(target?std::optional<uint16_t>{target->id}:std::nullopt,
            frame.sampleTime,frame.activation,frame.controllers.binocularMarkDwellMs);
    }
    if(target&&(requested||(acquiredByLook&&!target->marked&&reconClears.automaticAllowed(target->id)))){
        if(markPerson(services,*target,true)){
            if(requested)reconClears.explicitMark(target->id);
            reconDwell.suppress();
            snapshot(markers,now,status.activation);rumbleMailbox().publish({0,.45f,now});
            if(acquiredByLook)log("Physical optic automatically acquired visible person id="+std::to_string(target->id));
        }
        return;
    }
    // Looking at terrain must never fill the map with automatic A-Z pins.
    if(!requested)return;
    if(!hit){log("Physical optic mark: no visible surface on aim ray");return;}
    // TppMarker2System's native waypoint insertion method. Its own capacity,
    // replacement and marker-publication behavior remain authoritative.
    using Place=void(*)(void*,const Vector*);
    reinterpret_cast<Place>(base+0x5c53a0)(reinterpret_cast<void*>(markers),&*hit);
    snapshot(markers,now,status.activation);
    // Notify the ordinary player-message dispatcher only after the native
    // marker pool contains the point. Mission scripts retain their own
    // location and action rules; this adapter never selects a next sequence.
    const auto placed=opticWaypoints();
    const Vec3 position{hit->x,hit->y,hit->z};
    for(size_t i=0;i<placed.count;++i){
        const auto delta=placed.points[i].position-position;
        if(placed.points[i].letter&&dot(delta,delta)<.0001f){
            publishEvent({OpticEventKind::waypoint,position,frame.sampleTime,frame.activation});break;
        }
    }
    rumbleMailbox().publish({0,.35f,now});
    std::ostringstream message;message<<"Physical optic native waypoint sequence="<<consumed
        <<" position="<<hit->x<<','<<hit->y<<','<<hit->z
        <<" count="<<read<uint32_t>(markers+0xac);
    log(message.str());
}
}

namespace mgs5vr {
bool leaseCombatSight(uint64_t now,uint64_t milliseconds,uint16_t marker) noexcept {
    if(!milliseconds){combatSightExpiry.store(0);combatMarker.store(0xffff);std::lock_guard lock(mutex);combatSight={};return true;}
    if(!enabled.load()||milliseconds<100||milliseconds>5000)return false;
    const auto status=headCamera().status();
    if(!status.active||status.suspended)return false;
    combatMarker.store(marker);combatSightExpiry.store(now+milliseconds);return true;
}
CombatSightSnapshot combatSightSnapshot(){std::lock_guard lock(mutex);return combatSight;}
uint64_t combatSightLeaseExpiry() noexcept {return combatSightExpiry.load();}
bool leaseCoverProbe(CoverProbeRequest request,uint64_t now) noexcept {
    if(!enabled.load()||!validCoverProbeRequest(request)
       ||now>std::numeric_limits<uint64_t>::max()-request.leaseMs)return false;
    request.issuedMs=now;request.expiresMs=now+request.leaseMs;
    const auto status=headCamera().status();
    HeadCameraSample frame;
    {std::lock_guard lock(mutex);frame=latest;}
    if(coverProbeFrameReason(request,coverFrame(frame,status.active&&!status.suspended
       &&status.activation==frame.activation),now)!=CoverProbeReason::none)return false;
    std::lock_guard lock(mutex);
    if(coverRequest.nonce==request.nonce)return false; // A nonce cannot refresh an old result.
    coverRequest=request;coverResult={};coverResult.request=request;
    coverResult.frame=coverFrame(frame,true);return true;
}
void clearCoverProbe() noexcept {
    std::lock_guard lock(mutex);coverRequest={};coverResult={};
}
CoverProbeSnapshot coverProbeSnapshot(uint64_t now,uint64_t nonce){
    CoverProbeSnapshot result;HeadCameraSample current;
    {std::lock_guard lock(mutex);result=coverResult;current=latest;}
    if(!nonce||result.request.nonce!=nonce){
        result={};result.request.nonce=nonce;result.reason=CoverProbeReason::nonce;return result;
    }
    const auto status=headCamera().status();
    const auto reason=enabled.load()?coverProbeFrameReason(result.request,
        coverFrame(current,status.active&&!status.suspended&&status.activation==current.activation),now):CoverProbeReason::disabled;
    if(reason!=CoverProbeReason::none){result.available=false;result.reason=reason;}
    else if(result.available&&(!result.sampleMs||now<result.sampleMs||now-result.sampleMs>150)){
        result.available=false;result.reason=CoverProbeReason::staleFrame;
    }
    if(!result.available){result.count=0;result.personCount=0;result.candidates={};result.persons={};}
    return result;
}
void installOpticMarkers(uintptr_t moduleBase){
    constexpr std::array<unsigned char,9> updateEntry{0x48,0x85,0xd2,0x0f,0x84,0x99,0x02,0,0};
    constexpr std::array<unsigned char,10> placeEntry{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20};
    constexpr std::array<unsigned char,6> removeEntry{0x40,0x53,0x48,0x83,0xec,0x20};
    constexpr std::array<unsigned char,9> initEntry{0x48,0x89,0x4c,0x24,0x08,0x48,0x83,0xec,0x18};
    constexpr std::array<unsigned char,10> rayEntry{0x48,0x83,0xec,0x38,0xf3,0x0f,0x10,0x44,0x24,0x60};
    constexpr std::array<unsigned char,12> nativeRayEntry{0x40,0x53,0x55,0x56,0x57,0x48,0x81,0xec,0x98,0,0,0};
    constexpr std::array<unsigned char,10> positionEntry{0x4c,0x8b,0xdc,0x48,0x81,0xec,0x98,0,0,0};
    constexpr std::array<unsigned char,14> radioCommitEntry{0x40,0x57,0x48,0x83,0xec,0x20,0x0f,0xb7,0x81,0xc0,0,0,0,0x48};
    constexpr std::array<unsigned char,8> radioSelectEntry{0x66,0x89,0x91,0x80,0,0,0,0xc3};
    constexpr std::array<unsigned char,8> radioSelectTypeEntry{0x66,0x89,0x91,0x82,0,0,0,0xc3};
    if(!matches(moduleBase+0x930710,updateEntry)||!matches(moduleBase+0x5c53a0,placeEntry)
       ||!matches(moduleBase+0x5c6250,removeEntry)
       ||!matches(moduleBase+0xa0fb50,initEntry)||!matches(moduleBase+0x1b9b130,rayEntry)
       ||!matches(moduleBase+0x1b9a5d0,positionEntry)
       ||!matches(moduleBase+0x1b9b160,nativeRayEntry)
       ||read<uintptr_t>(moduleBase+0x21ade40)!=moduleBase+0x5c53a0
       ||read<uintptr_t>(moduleBase+0x21ade58)!=moduleBase+0x5c6250)
        throw std::runtime_error("Native optic marker signature mismatch");
    base=moduleBase;
    const auto nativeRayAddress=reinterpret_cast<void*>(base+0x1b9b160);
    const auto rayCreated=MH_CreateHook(nativeRayAddress,reinterpret_cast<void*>(&observeNativeRay),reinterpret_cast<void**>(&originalNativeRay));
    if(rayCreated!=MH_OK)throw std::runtime_error(MH_StatusToString(rayCreated));
    if(MH_EnableHook(nativeRayAddress)!=MH_OK)throw std::runtime_error("Cannot enable opt-in native ray observation");
    const auto address=reinterpret_cast<void*>(base+0x930710);
    const auto created=MH_CreateHook(address,reinterpret_cast<void*>(&update),reinterpret_cast<void**>(&original));
    if(created!=MH_OK)throw std::runtime_error(MH_StatusToString(created));
    if(MH_EnableHook(address)!=MH_OK)throw std::runtime_error("Cannot enable native optic markers");
    enabled.store(true);log("Physical optic trigger connected to native waypoint placement");
    if(!matches(moduleBase+0xdc8fb0,radioCommitEntry)
       ||!matches(moduleBase+0xdc8ca0,radioSelectEntry)||!matches(moduleBase+0xdc8cb0,radioSelectTypeEntry)
       ||read<uintptr_t>(moduleBase+0x234c340)!=moduleBase+0xdc8740
       ||read<uintptr_t>(moduleBase+0x234c3f8)!=moduleBase+0xdc8610){
        log("Physical optic intel unavailable: native signature mismatch");return;
    }
    const auto radioAddress=reinterpret_cast<void*>(base+0xdc8fb0);
    const auto radioCreated=MH_CreateHook(radioAddress,reinterpret_cast<void*>(&commitRadio),reinterpret_cast<void**>(&originalRadioCommit));
    if(radioCreated!=MH_OK)throw std::runtime_error(MH_StatusToString(radioCreated));
    if(MH_EnableHook(radioAddress)!=MH_OK)throw std::runtime_error("Cannot enable physical optic intel");
    log("Physical optic hand ray connected to native intel selection and radio");
}
void publishOpticMarkerFrame(const HeadCameraSample& frame){
    std::lock_guard lock(mutex);
    if(frame.controllers.loading||frame.controllers.frontEnd)resetReconClears.store(true);
    if(!frame.applied||frame.menuOpen||!frame.controllers.optic.active||frame.activation!=latest.activation)queuedUse={};
    latest=frame;
}
OpticWaypoints opticWaypoints(){std::lock_guard lock(mutex);return waypoints;}
std::optional<OpticEvent> takeOpticEvent(uint64_t now,uint64_t activation){
    std::lock_guard lock(mutex);
    return opticEvents.take(now,activation);
}
void acknowledgeOpticUse(const OpticEvent& event){
    if(event.kind!=OpticEventKind::raised)return;
    std::lock_guard lock(mutex);
    if(event.activation==latest.activation&&latest.controllers.optic.active)queuedUse=event;
}
void stopOpticMarkers() noexcept {enabled.store(false);combatSightExpiry.store(0);clearCoverProbe();}
}
