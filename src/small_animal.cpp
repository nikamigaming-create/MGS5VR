#include "mgs5vr/small_animal.hpp"
#include "mgs5vr/arm_ik.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/log.hpp"
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
#include <sstream>
#include <stdexcept>

namespace mgs5vr {
namespace {
uintptr_t base{};
std::atomic_bool enabled{};
struct Hands {HeadCameraSample frame;std::array<Pose,2> palms;HandContacts contacts;};
std::mutex handsMutex,animalsMutex;
Hands hands;
struct Animal {
    uintptr_t module{},model{},root{},metadata{};
    uint32_t logical{},slot{};
    Pose pose{};
    uint64_t seen{};
};
std::array<Animal,64> animals;
std::array<std::atomic_uintptr_t,64> activeModels;
std::atomic_uint activeCount{};
struct Carry {
    uintptr_t module{},model{};
    uint32_t logical{};
    unsigned hand{};
    Pose ground{},pose{};
    uint64_t began{},contact{},lowered{},activation{},cooldown{},lastUsable{};
    bool held{},lifted{};
} carry;
struct ReleasedContact {
    uintptr_t module{},model{};
    uint32_t logical{};
    Vec3 position{};
} releasedContact;
uint64_t skinUpdates{};
template<class T> T read(uintptr_t address){
    T out{};SIZE_T n{};
    if(address)ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),&out,sizeof(out),&n);
    return n==sizeof(out)?out:T{};
}
template<class T> void put(uintptr_t address,const T& value){std::memcpy(reinterpret_cast<void*>(address),&value,sizeof(value));}
float length(Vec3 p){return std::sqrt(dot(p,p));}
Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
Vec3 unit(Vec3 p){return p*(1.f/std::max(length(p),.001f));}
std::array<float,16> matrix(Pose p){
    const auto x=rotate(p.orientation,{1,0,0}),y=rotate(p.orientation,{0,1,0}),z=rotate(p.orientation,{0,0,1});
    return {x.x,x.y,x.z,0,y.x,y.y,y.z,0,z.x,z.y,z.z,0,p.position.x,p.position.y,p.position.z,1};
}
Hands latestHands(){std::lock_guard lock(handsMutex);return hands;}
bool contextAllowed(const Hands& sample){
    const auto& f=sample.frame;const auto s=headCamera().status();
    return enabled.load()&&!s.nativeMenuOpen&&!f.menuOpen&&!f.controllers.weaponReady
        &&!f.controllers.vehicleControls&&!f.controllers.optic.held&&!f.controllers.commandControls
        &&!f.controllers.equipmentCategory&&!f.controllers.frontEnd;
}
bool eligible(const Hands& sample,uint64_t now){
    const auto& f=sample.frame;const auto s=headCamera().status();
    return contextAllowed(sample)&&s.active&&f.applied&&f.activation==s.activation
        &&now>=f.sampleTime&&now-f.sampleTime<150;
}
Vec3 palmNormal(const Hands& h,unsigned side){return rotate(h.palms[side].orientation,{side?-1.f:1.f,0,0});}
Pose perch(const Hands& h,unsigned side){
    // The anatomical grip has -Y wrist-to-knuckles, -X out of the right palm
    // (+X on the left). The rat's native +Y is up and +Z faces its nose.
    const auto y=unit(palmNormal(h,side));
    const auto forward=rotate(h.palms[side].orientation,{0,-1,0});
    const auto z=unit(forward-y*dot(forward,y)),x=unit(cross(y,z));
    const auto p=h.palms[side].position+y*.022f+z*.035f;
    return nativeAffinePose({x.x,x.y,x.z,0,y.x,y.y,y.z,0,z.x,z.y,z.z,0,p.x,p.y,p.z,1}).value_or(Pose{{},p});
}
std::optional<Vec3> floorAt(Vec3 p,float ceiling){
    if(!valid(Pose{{},p})||!std::isfinite(ceiling)||!read<uintptr_t>(base+0x2c79710))return {};
    struct alignas(16) V {float x,y,z,w;};
    alignas(16) std::array<std::byte,0x250> query{};
    using Init=void*(*)(void*,uint32_t);
    using Ray=uint32_t(*)(void*,uint32_t,const V*,const V*,float);
    using Value=void*(*)(const void*,V*);
    reinterpret_cast<Init>(base+0xa0fb50)(query.data(),0);
    const uint64_t layers=0x800,filter=0x80000006;
    std::memcpy(query.data(),&layers,8);std::memcpy(query.data()+0x18,&filter,8);
    const V start{p.x,ceiling,p.z,0},end{p.x,ceiling-5,p.z,0};
    if(!reinterpret_cast<Ray>(base+0x1b9b130)(query.data(),0x04000700,&start,&end,0))return {};
    const auto q=reinterpret_cast<uintptr_t>(query.data());
    const auto count=read<uint32_t>(q+0x60);const auto index=read<int32_t>(q+0x64);
    if(!count||count>5||index<0||uint32_t(index)>=count)return {};
    V hit{},normal{};const auto record=query.data()+0x70+index*0x60;
    reinterpret_cast<Value>(base+0x1b9a5d0)(record,&hit);
    reinterpret_cast<Value>(base+0x1b99910)(record,&normal);
    if(normal.y<.65f||hit.y>ceiling+.01f||hit.y<end.y-.01f)return {};
    return Vec3{hit.x,hit.y,hit.z};
}
std::optional<Animal> identify(uintptr_t character,uint32_t index){
    if(read<uintptr_t>(character)!=base+0x22f36c0)return {};
    const auto module=read<uintptr_t>(character+0x60);
    if(read<uintptr_t>(module)!=base+0x23d9410)return {};
    const auto metadata=read<uintptr_t>(module+8),controller=read<uintptr_t>(character+8);
    if(read<uintptr_t>(controller)!=base+0x22e6070)return {};
    const auto count=read<uint32_t>(metadata+0x44),first=read<uint32_t>(controller+0xc);
    if(count>64||index>=count||index<first||index-first>=read<uint32_t>(controller+8))return {};
    const auto logical=read<uint16_t>(read<uintptr_t>(metadata+0x28)+index*2);
    const auto flags=read<uintptr_t>(module+0x10);
    if(logical>=read<uint32_t>(metadata+0x40)||logical>=read<uint32_t>(flags+0x14)
       ||(read<uint32_t>(read<uintptr_t>(flags+8)+logical*4)&0x6d0))return {};
    const auto record=read<uintptr_t>(controller+0x10)+(index-first)*0xc0ull;
    const auto root=read<uintptr_t>(record+0x60)+0x10,model=read<uintptr_t>(record+0x68);
    if(read<uintptr_t>(root)!=base+0x24c8ef0||read<uintptr_t>(model)!=base+0x20f4d90)return {};
    const auto pose=nativeAffinePose(read<std::array<float,16>>(model+0xa0));
    if(!pose)return {};
    return Animal{module,model,root,metadata,logical,index,*pose,steadyMilliseconds()};
}
void place(const Animal& a,Pose target){
    // This runs in this animal's skin transaction, after native movement.
    // Native queries and the skin share one root.
    const auto delta=target.position-read<Vec3>(a.root+0x10);
    put(a.root+0x20,read<Vec3>(a.root+0x20)+delta);
    put(a.root+0x10,target.position);
    put(a.root+0x60,target.orientation);put(a.root+0x80,target.orientation);
    put(a.root+0xe0,matrix(target));
    put(a.model+0x60,target.position);put(a.model+0xa0,matrix(target));
    put(read<uintptr_t>(a.metadata+0x20)+a.logical*16,target.position);
}
bool owns(const Animal& a){return carry.model==a.model&&carry.module==a.module&&carry.logical==a.logical;}
void release(const Animal& a,const char* reason,uint64_t now){
    place(a,carry.ground);
    releasedContact={a.module,a.model,a.logical,carry.ground.position};
    log("Small animal released id="+std::to_string(a.logical)+" reason="+reason);
    carry={};carry.cooldown=now+1800;
}
void observe(const Animal& a,const Hands& h){
    std::lock_guard lock(animalsMutex);
    const auto now=steadyMilliseconds();const bool usable=eligible(h,now);
    auto it=std::find_if(animals.begin(),animals.end(),[&](const auto& p){return p.model==a.model;});
    if(it==animals.end())it=std::min_element(animals.begin(),animals.end(),[](const auto& x,const auto& y){return x.seen<y.seen;});
    *it=a;
    if(carry.model&&now>carry.began+30000&&!carry.held)carry={};
    if(owns(a)){
        if(!contextAllowed(h)||carry.activation!=h.frame.activation){release(a,"interaction ended",now);return;}
        if(!usable||!h.frame.controllers.hands[carry.hand].gripTracked){
            // A missed publication is not a deliberate release. Preserve the
            // last complete hand/animal pose briefly, then return the animal
            // to its last safe floor if tracking does not recover.
            carry.contact=carry.lowered=0;
            if(now-carry.lastUsable>350){release(a,"hand unavailable",now);return;}
            place(a,carry.pose);return;
        }
        carry.lastUsable=now;
        const auto& input=h.frame.controllers.hands[carry.hand];
        const auto target=perch(h,carry.hand);
        const auto separation=length(target.position-carry.ground.position);
        if(!carry.held){
            if(separation>.45f||input.trigger>.2f||input.squeeze>.7f){release(a,"hand withdrew",now);return;}
            const bool close=separation<.19f&&palmNormal(h,carry.hand).y>.5f;
            if(!close)carry.contact=0;
            else if(!carry.contact)carry.contact=now;
            else if(now-carry.contact>=350){
                carry.held=true;carry.pose=target;carry.began=now;
                log("Small animal scooped id="+std::to_string(a.logical)+" hand="+std::to_string(carry.hand));
            }
            place(a,carry.held?target:carry.ground);return;
        }
        if(target.position.y-carry.ground.position.y>.22f)carry.lifted=true;
        if(const auto floor=floorAt(target.position,h.frame.nativePose.position.y+.2f)){
            carry.ground.position=*floor;
            const auto f=rotate(target.orientation,{0,0,1});
            const auto yaw=std::atan2(f.x,f.z);
            carry.ground.orientation={0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)};
            // The native arm surface solve keeps the glove above the floor.
            // Permit that clearance plus the animal's foot-to-palm offset.
            const bool lower=carry.lifted&&target.position.y-floor->y<.19f&&now-carry.began>900;
            if(!lower)carry.lowered=0;
            else if(!carry.lowered)carry.lowered=now;
            else if(now-carry.lowered>350){release(a,"lowered to ground",now);return;}
        }
        if(palmNormal(h,carry.hand).y<.15f){release(a,"palm turned over",now);return;}
        carry.pose=target;place(a,target);return;
    }
    if(carry.model||!usable||now<carry.cooldown)return;
    if(releasedContact.model==a.model&&releasedContact.module==a.module&&releasedContact.logical==a.logical){
        // Lowering the rat is a release, even when an open hand rests nearby.
        // Both hands must leave the contact area before offering it again.
        for(unsigned side=0;side<2;++side)
            if(!h.frame.controllers.hands[side].gripTracked
                ||length(perch(h,side).position-releasedContact.position)<.4f)return;
        releasedContact={};
    }
    for(unsigned side=0;side<2;++side){
        const auto& input=h.frame.controllers.hands[side];
        if(!input.gripTracked||input.trigger>.2f||input.squeeze>.7f||palmNormal(h,side).y<.5f)continue;
        const auto target=perch(h,side);
        if(length(target.position-a.pose.position)>.35f)continue;
        if(h.frame.nativePose.position.y-target.position.y<.35f)continue;
        const auto floor=floorAt(a.pose.position,h.frame.nativePose.position.y+.2f);
        if(!floor||std::abs(floor->y-a.pose.position.y)>.3f)continue;
        carry={};carry.module=a.module;carry.model=a.model;carry.logical=a.logical;carry.hand=side;
        carry.ground=a.pose;carry.ground.position=*floor;carry.pose=carry.ground;
        carry.began=carry.lastUsable=now;carry.activation=h.frame.activation;
        log("Small animal accepts open palm id="+std::to_string(a.logical));
        place(a,carry.ground);break;
    }
}
void refreshAnimals(){
    // Native active slots are independent of map-authored names. Follow the
    // current pool on every player publication, including block reloads.
    const auto registry=read<uintptr_t>(base+0x2c389a0);
    const auto pool=read<uintptr_t>(read<uintptr_t>(read<uintptr_t>(registry+0x30+33*8)+8)+0x20);
    const auto module=read<uintptr_t>(read<uintptr_t>(pool+0x48)+0x10)+0x20;
    if(read<uintptr_t>(module)!=base+0x23d9410){activeCount.store(0);return;}
    const auto metadata=read<uintptr_t>(module+8),characters=read<uintptr_t>(module+0x110);
    const auto count=read<uint32_t>(metadata+0x44);const auto batch=read<uint8_t>(module+0x119);
    if(count>64||!batch||batch>64){activeCount.store(0);return;}
    std::array<Animal,64> current{};unsigned n{};
    for(uint32_t slot=0;slot<count;++slot)
        if(auto a=identify(read<uintptr_t>(characters+(slot/batch)*8),slot))current[n++]=*a;
    {std::lock_guard lock(animalsMutex);
        animals=current;
        if(carry.model&&std::none_of(current.begin(),current.begin()+n,[](const auto& a){return owns(a);})){carry={};carry.cooldown=steadyMilliseconds()+1800;}
        if(releasedContact.model&&std::none_of(current.begin(),current.begin()+n,[](const auto& a){
            return a.model==releasedContact.model&&a.module==releasedContact.module&&a.logical==releasedContact.logical;
        }))releasedContact={};
    }
    for(unsigned i=0;i<n;++i)activeModels[i].store(current[i].model);
    activeCount.store(n);
}
}
void installSmallAnimalInteraction(uintptr_t imageBase){
    base=imageBase;
    const std::array<unsigned char,7> signature{0x4c,0x8b,0xc1,0xb9,0xff,0xff,0x00};
    if(read<std::array<unsigned char,7>>(base+0x12e2fe0)!=signature)throw std::runtime_error("Small animal active-slot ABI differs");
    enabled.store(true);log("Small animal palm interaction installed for every active native rat instance");
}
void stopSmallAnimalInteraction() noexcept{enabled.store(false);}
void publishSmallAnimalHands(const HeadCameraSample& f,const std::array<Pose,2>& p,const HandContacts& c){
    {std::lock_guard lock(handsMutex);hands={f,p,c};}
    if(enabled.load())refreshAnimals();
}
void applySmallAnimalSkin(uintptr_t binding){
    if(!enabled.load())return;
    bool match=false;
    for(unsigned i=0,n=activeCount.load();i<n;++i)if(activeModels[i].load()+0xa0==binding){match=true;break;}
    if(!match)return;
    Animal animal;
    {std::lock_guard lock(animalsMutex);
        const auto it=std::find_if(animals.begin(),animals.end(),[&](const auto& a){return a.model+0xa0==binding;});
        if(it==animals.end())return;animal=*it;
    }
    const auto h=latestHands();const auto now=steadyMilliseconds();
    if(const auto p=nativeAffinePose(read<std::array<float,16>>(binding)))animal.pose=*p;
    animal.seen=now;observe(animal,h);
    std::lock_guard lock(animalsMutex);
    if(!carry.model||binding!=carry.model+0xa0)return;
    const auto target=carry.pose;
    put(binding,matrix(target));put(carry.model+0x60,target.position);++skinUpdates;
}
std::string inspectSmallAnimals(){
    const auto h=latestHands();std::lock_guard lock(animalsMutex);
    std::ostringstream o;o<<"{\"held\":"<<carry.held<<",\"selected\":"<<(carry.model!=0)
        <<",\"logical\":"<<carry.logical<<",\"lifted\":"<<carry.lifted<<",\"skinUpdates\":"<<skinUpdates<<",\"palmUp\":["
        <<palmNormal(h,0).y<<','<<palmNormal(h,1).y<<"],\"rats\":[";
    bool first=true;const auto now=steadyMilliseconds();
    for(const auto& a:animals)if(a.model&&now-a.seen<2000){
        if(!first)o<<',';first=false;
        o<<"{\"id\":"<<a.logical<<",\"position\":["<<a.pose.position.x<<','<<a.pose.position.y<<','<<a.pose.position.z<<']';
        if(const auto f=floorAt(a.pose.position,h.frame.nativePose.position.y+.2f))o<<",\"floor\":"<<f->y;
        o<<'}';
    }
    o<<"]}";return o.str();
}
}
