#include "mgs5vr/animal_interaction.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/log.hpp"
#include "mgs5vr/opening_selector.hpp"
#include <windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
#include <sstream>

namespace mgs5vr {
namespace {
struct Hands {HeadCameraSample frame;std::array<Pose,2> palms;HandContacts contacts;};
std::mutex publicationMutex,interactionMutex;
Hands latest;
struct Stroke {
    Vec3 relative{},local{},tracked{};
    uint64_t sampledAt{},startedAt{},activation{};
    float travel{};
    bool latched{};
};
std::array<Stroke,2> strokes;
uint64_t consumed{},lastReport{},nextResponseAt{},handActivation{};
uint64_t lastRuntimeReport{};
std::atomic_uint64_t physicalResponses{},physicalResponseAt{};
struct ContactTrace {
    uint64_t samples{},resets{},elapsed{},touchingFor{};
    float distance{},speed{},travel{},curl{};
    const char* state{"unobserved"};
};
std::array<ContactTrace,2> contactTrace;
uint32_t eligibilityMask{};
void resetContact(Stroke& stroke){
    stroke.sampledAt=stroke.startedAt=0;stroke.travel=0;
}
constexpr uint32_t dogId=19u<<9;
template<class T> T read(uintptr_t address){
    T value{};SIZE_T size{};
    if(address)ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),&value,sizeof(value),&size);
    return size==sizeof(value)?value:T{};
}
float length(Vec3 p){return std::sqrt(dot(p,p));}
bool cabinActor(const HeadCameraSample& frame,Vec3 point){
    if(!frame.controllers.openingSelector&&!frame.controllers.cabinPlay)return true;
    const auto& bounds=frame.controllers.cabinBounds;
    return nativeRoomContains(bounds,nativeRoomLocal(bounds,point),.10f);
}
struct alignas(16) DogBones {
    uint64_t command{0xdb3413073cef},pad{};
    std::array<float,4> first{},second{};
    uint8_t valid{};
    std::array<uint8_t,15> tail{};
};
bool send(void* command){
    const auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    const auto services=read<uintptr_t>(read<uintptr_t>(base+0x2c3a8a0)+8);
    const auto objects=read<uintptr_t>(services+0x60);
    const auto address=read<uintptr_t>(read<uintptr_t>(objects)+0x38);
    if(address<base+0x1000||address>=base+0x2090000)return false;
    using Send=void*(*)(void*,int32_t*,uint32_t,void*);
    int32_t result{-1};
    reinterpret_cast<Send>(address)(reinterpret_cast<void*>(objects),&result,dogId,command);
    return result>=0;
}
std::optional<std::array<Vec3,2>> dogBones(){
    DogBones bones;
    if(!send(&bones)||!bones.valid)return {};
    const Vec3 a{bones.first[0],bones.first[1],bones.first[2]},b{bones.second[0],bones.second[1],bones.second[2]};
    if(!valid(Pose{{},a})||!valid(Pose{{},b})||length(a-b)>2||dot(a,a)<1)return {};
    return std::array<Vec3,2>{a,b};
}
bool respond(){
    // Eight-byte native command observed from the player's pet interaction.
    // The buddy owns its response, animation and eligibility. The tracked
    // player never enters the stock leaning/petting animation.
    uint64_t command{0x2ceb936a9b67};
    const bool accepted=send(&command);
    log("Physical D-Dog pet response native_result="+std::to_string(accepted));
    return accepted;
}
Hands hands(){std::lock_guard lock(publicationMutex);return latest;}
}
void publishAnimalHands(const HeadCameraSample& frame,const std::array<Pose,2>& palms,const HandContacts& contacts){
    std::lock_guard lock(publicationMutex);latest={frame,palms,contacts};
}
void consumeAnimalTouch(){
    std::lock_guard lock(interactionMutex);
    const auto sample=hands();const auto& frame=sample.frame;
    const auto now=steadyMilliseconds();const auto status=headCamera().status();
    bool freshActive{};
    gamepadMailbox().read(now,&freshActive);
    eligibilityMask=(!frame.controllers.allowAnimalTouch?1u:0u)|(!status.active?2u:0u)
        |(status.nativeMenuOpen?4u:0u)|(!frame.applied?8u:0u)|(frame.activation!=status.activation?16u:0u)
        |(now<frame.sampleTime||now-frame.sampleTime>100?32u:0u)|(frame.controllers.weaponReady?64u:0u)
        |(frame.controllers.vehicleControls?128u:0u)|(frame.controllers.optic.held?256u:0u)
        |(frame.controllers.commandControls?512u:0u)|(frame.controllers.equipmentOpen?1024u:0u)
        |(frame.controllers.avatarEditor?2048u:0u)
        |(frame.controllers.frontEnd&&!frame.controllers.cabinPlay&&!frame.controllers.openingSelector?4096u:0u)
        |(!freshActive||nativeGamepadActive()?8192u:0u);
    const auto resetAll=[&](const char* reason){
        for(size_t side=0;side<2;++side){resetContact(strokes[side]);contactTrace[side].state=reason;++contactTrace[side].resets;}
    };
    if(eligibilityMask){
        // A missed tracking frame may interrupt contact, but cannot re-arm
        // a hand that has already petted the buddy. It must withdraw first.
        resetAll("ineligible");
        return;
    }
    if(frame.sampleTime<=consumed)return;
    consumed=frame.sampleTime;
    if(!frame.controllers.hands[0].gripTracked&&!frame.controllers.hands[1].gripTracked)return;
    const auto bones=dogBones();
    if(frame.controllers.cabinPlay&&now-lastRuntimeReport>=2000){
        lastRuntimeReport=now;
        log(std::string("Cabin native D-Dog runtime=")+(bones?"present":"absent")
            +" hand_touch_enabled="+std::to_string(frame.controllers.allowAnimalTouch));
    }
    if(handActivation!=frame.activation){strokes={};handActivation=frame.activation;}
    if(!bones){resetAll("no_native_bones");return;}
    if(!cabinActor(frame,(*bones)[0])){
        // Gate the contact region, not the whole animal against six axial
        // samples. The authored dog's hindquarters can extend past a bench
        // ray while its head is visibly inside the reachable cabin.
        resetAll("outside_cabin");return;
    }
    const Vec3 skull=(*bones)[0],end=(*bones)[1];
    // Bone 5 is at the skull, behind the visible muzzle. Extend the contact
    // centerline along the animal's own head-to-body direction, so touching
    // the nose does not require pushing the fingers inside the skull.
    const auto forward=skull-end;
    const Vec3 start=skull+forward*(.20f/std::max(length(forward),.001f)),axis=end-start;
    for(size_t side=0;side<2;++side){
        auto& stroke=strokes[side];const auto& hand=frame.controllers.hands[side];
        auto& trace=contactTrace[side];++trace.samples;trace.curl=frame.controllers.strikeCurl[side];
        const auto palm=(sample.contacts[side][2]+sample.contacts[side][3]+sample.contacts[side][4])*(1.f/3.f);
        const auto tracked=hand.grip.position;
        const auto local=tracked-frame.headPose.position;
        float distance=100;
        for(const auto point:sample.contacts[side]){
            const float along=std::clamp(dot(point-start,axis)/std::max(dot(axis,axis),.0001f),0.f,1.f);
            distance=std::min(distance,length(point-(start+axis*along)));
        }
        trace.distance=distance;trace.travel=stroke.travel;
        if(!hand.gripTracked||hand.squeeze>.4f||hand.trigger>.2f||frame.controllers.strikeCurl[side]>.25f||distance>.24f){
            trace.state=!hand.gripTracked?"untracked":hand.squeeze>.4f?"grip":hand.trigger>.2f?"trigger":frame.controllers.strikeCurl[side]>.25f?"strike":"distant";
            ++trace.resets;
            if(distance>.42f||!hand.gripTracked)stroke.latched=false;
            resetContact(stroke);continue;
        }
        if(stroke.latched||now<nextResponseAt){trace.state="latched_or_cooldown";continue;}
        const auto elapsed=stroke.sampledAt&&frame.sampleTime>stroke.sampledAt?frame.sampleTime-stroke.sampledAt:0;
        trace.elapsed=elapsed;
        if(!elapsed||elapsed>100||stroke.activation!=frame.activation){
            trace.state=!elapsed?"begin":elapsed>100?"sample_gap":"activation";++trace.resets;
            stroke.relative=palm-start;stroke.local=local;stroke.tracked=tracked;stroke.sampledAt=stroke.startedAt=frame.sampleTime;
            stroke.activation=frame.activation;stroke.travel=0;continue;
        }
        // Input can arrive between two native polls only a millisecond apart.
        // Measure the stroke across a short tracking interval; those polls
        // must not turn an ordinary wrist movement into a rejected fast hit.
        if(elapsed<40){trace.state="short_interval";continue;}
        const float dt=static_cast<float>(elapsed)*.001f;
        const auto movement=animalStrokeDelta(palm-start-stroke.relative,tracked-stroke.tracked,local-stroke.local,dt);
        const float speed=movement.speed;
        trace.speed=speed;trace.state="contact";
        stroke.relative=palm-start;stroke.local=local;stroke.tracked=tracked;stroke.sampledAt=frame.sampleTime;
        if(speed>.9f){trace.state="too_fast";++trace.resets;stroke.startedAt=frame.sampleTime;stroke.travel=0;continue;}
        stroke.travel+=movement.travel;
        const auto touchingFor=frame.sampleTime-stroke.startedAt;
        trace.touchingFor=touchingFor;trace.travel=stroke.travel;
        if(touchingFor>=500&&stroke.travel>=.025f){
            if(respond()){
                // Buddy responses are shared across the two hands. Preserve
                // the cooldown even if tracking or the native pose pauses.
                nextResponseAt=now+3500;
                for(auto& other:strokes)other.latched=true;
                ++physicalResponses;physicalResponseAt.store(now);
                rumbleMailbox().publish({side?0.f:.35f,side?.35f:0.f,now});
                log("Physical D-Dog stroke hand="+std::to_string(side)+" travel="+std::to_string(stroke.travel));
            }
            stroke.startedAt=frame.sampleTime;stroke.travel=0;
        }
        if(now-lastReport>3000){lastReport=now;log("Physical animal contact distance="+std::to_string(distance)+" travel="+std::to_string(stroke.travel));}
    }
}
std::string inspectAnimalTouch(){
    std::lock_guard lock(interactionMutex);
    const auto sample=hands();const auto bones=dogBones();
    std::ostringstream out;out<<"{\"dog\":"<<(bones?"true":"false");
    const auto vector=[&](const char* key,Vec3 v){out<<",\""<<key<<"\":["<<v.x<<','<<v.y<<','<<v.z<<']';};
    if(bones){vector("bone5",(*bones)[0]);vector("bone26",(*bones)[1]);}
    vector("head",sample.frame.nativePose.position);
    if(const auto anchor=headCamera().openingTrackingOrigin(steadyMilliseconds())){
        vector("openingPosition",anchor->position);
        const auto q=anchor->orientation;
        out<<",\"openingOrientation\":["<<q.x<<','<<q.y<<','<<q.z<<','<<q.w<<']';
    }
    vector("left",sample.palms[0].position);vector("right",sample.palms[1].position);
    vector("rightIndex",sample.contacts[1][2]);vector("rightMiddle",sample.contacts[1][3]);
    const auto orientation=sample.frame.nativePose.orientation;
    out<<",\"headOrientation\":["<<orientation.x<<','<<orientation.y<<','<<orientation.z<<','<<orientation.w<<']';
    const auto status=headCamera().status();
    out<<",\"sampleTime\":"<<sample.frame.sampleTime<<",\"now\":"<<steadyMilliseconds()
       <<",\"active\":"<<status.active<<",\"menu\":"<<status.nativeMenuOpen
       <<",\"applied\":"<<sample.frame.applied<<",\"activation\":"<<sample.frame.activation
       <<",\"currentActivation\":"<<status.activation<<",\"weapon\":"<<sample.frame.controllers.weaponReady
       <<",\"optic\":"<<sample.frame.controllers.optic.held<<",\"frontEnd\":"<<sample.frame.controllers.frontEnd
       <<",\"squeeze\":"<<sample.frame.controllers.hands[1].squeeze
       <<",\"physicalPetResponses\":"<<physicalResponses.load()
       <<",\"lastPhysicalPetAt\":"<<physicalResponseAt.load()
       <<",\"eligibilityMask\":"<<eligibilityMask<<",\"contact\":[";
    for(size_t side=0;side<2;++side){
        const auto& trace=contactTrace[side];if(side)out<<',';
        out<<"{\"state\":\""<<trace.state<<"\",\"samples\":"<<trace.samples<<",\"resets\":"<<trace.resets
           <<",\"elapsed\":"<<trace.elapsed<<",\"touchingFor\":"<<trace.touchingFor
           <<",\"distance\":"<<trace.distance<<",\"speed\":"<<trace.speed<<",\"travel\":"<<trace.travel
           <<",\"strikeCurl\":"<<trace.curl<<'}';
    }
    out<<"]}";return out.str();
}
std::string requestNativeDogResponse(){
    if(!dogBones())return "No active D-Dog";
    return respond()?"Native D-Dog response accepted":"Native D-Dog response unavailable";
}
}
