#include "mgs5vr/player_visibility.hpp"
#include "mgs5vr/log.hpp"
#include "mgs5vr/head_camera.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/ui_renderer.hpp"
#include <windows.h>
#include <MinHook.h>
#include <intrin.h>
#include <array>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <sstream>

namespace {
uintptr_t base{};
using GroupFn=void(*)(void*,uint32_t);
GroupFn hideVisibleGroup{},showVisibleGroup{};
using ResolveWeaponPart=uintptr_t(*)(void*,uint32_t,uint32_t,uint32_t*);
ResolveWeaponPart resolveWeaponPart{};
using VisualUpdate=void(*)(void*,uint32_t);
VisualUpdate originalVisualUpdate{};
struct alignas(16) FadePoint {float x{},y{},z{},w{};};
using ActorCameraFade=float(*)(void*,uint32_t,void*,const void*,const FadePoint*,float,float);
ActorCameraFade originalActorCameraFade{};
std::atomic_uintptr_t fadeOwner{};
std::mutex hiddenMutex;
std::atomic_uint64_t lastFirstPersonUpdateMs{};
std::atomic_bool fullBodyRequested{};
struct FullBodyReady {
    uintptr_t owner{},model{};
    uint64_t time{},activation{},referenceEpoch{},presentationEpoch{};
    uint64_t trackingSequence{},playerSequence{},rigSequence{},sampleTime{},menuGeneration{};
} fullBodyReady;
constexpr uint32_t headName=0xa9e88501,bodyName=0x1a166b34,armName=0x4e74fd8c;
// Owned EquipIdTable's normal and rain-cover NVG models. These are renderer
// resource codes, independent of the native night-vision filter/equip state.
constexpr std::array<uint64_t,2> nvgModels{0x84a1ee5ee0254be2ull,0x84a12e91e8362fe1ull};
// Owned hospital /Assets/tpp/item/msk/Scenes/msk0_main0_cov.fmdl.
// Its bandage is a modular player attachment with independently visible
// children and no "head" group, so head-name classification cannot find it.
constexpr uint64_t hospitalFaceCoverModel=0x84a06ebcd692bb87ull;
std::atomic_bool visibilityTraceEnabled{},visibilityTraceArmed{},visibilityTraceWindow{},visibilityTraceNeedsOwner{true};
std::atomic_uintptr_t visibilitySourceOwner{};
std::atomic_uintptr_t visibilityPublisherPool{};
std::atomic_uint32_t visibilityPublisherId{};
struct VisibilitySnapshot {
    mgs5vr::NativeVisibilityIdentity identity{};
    uint64_t publisherQpc{};
    uint8_t opacity{},armFlags{},nativeFlags{};
};
std::mutex visibilityTraceMutex;
VisibilitySnapshot visibilityPublisher;
mgs5vr::VisibilityBoundaryBudget visibilityBudget;
mgs5vr::VisibilityInspectionBudget visibilityInspectionBudget;
template<class T> bool read(uintptr_t address,T& value){
    SIZE_T copied{};
    return address&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),&value,sizeof(value),&copied)&&copied==sizeof(value);
}
template<class T> T get(uintptr_t address){T value{};read(address,value);return value;}
uint64_t visibilityQpc() noexcept {
    LARGE_INTEGER value{};QueryPerformanceCounter(&value);return static_cast<uint64_t>(value.QuadPart);
}
bool inspectVisibility(uintptr_t owner,uintptr_t pool,uint32_t visualId,VisibilitySnapshot& value){
    auto& id=value.identity;id.owner=owner;id.pool=pool;id.visualId=visualId;
    if(get<uintptr_t>(owner)!=base+0x23b8218||get<uintptr_t>(pool)!=base+0x23adef0)return false;
    id.character=get<uintptr_t>(owner+0x370);
    if(get<uintptr_t>(id.character)!=base+0x2295210||get<uintptr_t>(pool+0x38)!=id.character)return false;
    id.bodyInterface=get<uintptr_t>(id.character+0x10);
    if(get<uintptr_t>(id.bodyInterface)!=base+0x22e6070)return false;
    uint32_t index{},first{},count{};
    uintptr_t holders{};
    if(!read(owner+0x3a0,index)||!read(id.bodyInterface+0xc,first)||!read(id.bodyInterface+8,count)
        ||!read(id.bodyInterface+0x10,holders)||!holders||!count||count>16||index<first||index-first>=count)return false;
    id.holder=holders+static_cast<uintptr_t>(index-first)*0xc0;
    id.model=get<uintptr_t>(id.holder+0x68);
    if(get<uintptr_t>(id.model)!=base+0x20f4d90||!read(pool+8,id.records)
        ||!read(pool+0x10,id.count)||!read(pool+0x14,id.first))return false;
    const auto record=mgs5vr::nativeVisibilityRecordAddress(id.records,id.count,id.first,visualId);
    if(!record||get<uintptr_t>(*record)!=id.model)return false;
    id.record=*record;
    uint16_t groups{};uintptr_t names{},flags{};
    if(!read(id.model+0x1e8,groups)||!groups||groups>128||!read(id.model+0x180,names)||!names
        ||!read(id.model+0x170,flags)||!flags)return false;
    std::array<uint32_t,128> groupNames{};
    SIZE_T copied{};
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(names),groupNames.data(),groups*4u,&copied)
        ||copied!=groups*4u)return false;
    uint16_t armIndex=groups;
    for(uint16_t i=0;i<groups;++i)if(groupNames[i]==armName){if(armIndex!=groups)return false;armIndex=i;}
    if(armIndex==groups||!read(flags+armIndex,value.armFlags)||!read(id.record+0x6e,value.opacity)
        ||!read(id.record+0x79,value.nativeFlags))return false;
    // Every input is revalidated against the same current typed owner. Reads
    // are evidence only; no native callback or visibility setter is invoked.
    return get<uintptr_t>(owner+0x370)==id.character&&get<uintptr_t>(id.character+0x10)==id.bodyInterface
        &&get<uintptr_t>(id.bodyInterface+0x10)==holders&&get<uintptr_t>(id.holder+0x68)==id.model
        &&get<uintptr_t>(pool+0x38)==id.character&&get<uintptr_t>(pool+8)==id.records
        &&get<uint32_t>(pool+0x10)==id.count&&get<uint32_t>(pool+0x14)==id.first
        &&get<uintptr_t>(id.record)==id.model&&get<uintptr_t>(id.model+0x180)==names
        &&get<uintptr_t>(id.model+0x170)==flags&&get<uint16_t>(id.model+0x1e8)==groups;
}
bool inspectVisibilityBounded(uintptr_t owner,uintptr_t pool,uint32_t visualId,VisibilitySnapshot& value){
    {
        std::lock_guard lock(visibilityTraceMutex);
        if(!visibilityTraceEnabled.load()||!visibilityInspectionBudget.reserve(GetTickCount64())){
            visibilityTraceEnabled.store(false);visibilityTraceWindow.store(false);return false;
        }
    }
    return inspectVisibility(owner,pool,visualId,value);
}
void restoreStaleVisibilityIfAllowed() noexcept;
float actorCameraFade(void* owner,uint32_t index,void* character,const void* transform,
        const FadePoint* camera,float inner,float outer){
    // The native buddy publisher (c5e860/c5ea15) owns the model's opacity.
    // Its camera-nearness query still receives the stock ACC camera even
    // though our cabin renderer uses the tracked VR view. iDroid temporarily
    // bypasses that query, which made the same dog appear only with it open.
    // Join the existing native calculation to the view we actually render.
    if(reinterpret_cast<uintptr_t>(_ReturnAddress())!=base+0xc5e99a||!camera
        ||!mgs5vr::nativeCabinPlay())
        return originalActorCameraFade(owner,index,character,transform,camera,inner,outer);
    const auto registry=get<uintptr_t>(base+0x2c389a0);
    const auto registration=get<uintptr_t>(registry+0x30+19*8);
    const auto definition=get<uintptr_t>(registration+8);
    const auto interface=get<uintptr_t>(definition+0x20);
    if(get<uintptr_t>(interface)!=base+0x22bdf30)
        return originalActorCameraFade(owner,index,character,transform,camera,inner,outer);
    const auto module=get<uintptr_t>(get<uintptr_t>(interface+0x80)+0x10);
    const auto system=module+0x20;
    if(get<uintptr_t>(module)!=base+0x2315d80||get<uintptr_t>(system)!=base+0x2315db0
        ||get<uint32_t>(get<uintptr_t>(system+0x10)+0x44)!=1||get<uint32_t>(system+0x108)!=1)
        return originalActorCameraFade(owner,index,character,transform,camera,inner,outer);
    const auto body=get<uintptr_t>(get<uintptr_t>(system+0x110));
    const auto pool=reinterpret_cast<uintptr_t>(owner);
    // The write breakpoint at c5ea15 joined this exact current dog owner:
    // its Character+8 is the publisher's embedded +0x38 interface. R8 is
    // the publisher's native character service, not the Character itself.
    if(get<uintptr_t>(body)!=base+0x22f0830||get<uintptr_t>(body+8)!=pool+0x38
        ||get<uintptr_t>(pool+0xa0)!=reinterpret_cast<uintptr_t>(character))
        return originalActorCameraFade(owner,index,character,transform,camera,inner,outer);
    const auto frame=mgs5vr::headCamera().publishedRigFrame(mgs5vr::steadyMilliseconds());
    if(frame&&frame->applied&&frame->controllers.cabinPlay){
        const auto component=get<uintptr_t>(body+0x10);
        const auto model=get<uintptr_t>(get<uintptr_t>(component+0x10)+0x68);
        if(get<uintptr_t>(component)==base+0x22e6070&&get<uintptr_t>(model)==base+0x20f4d90
            &&get<uint64_t>(model+0x120)==0x84a0e671b02ebb56ull){
            const auto position=frame->nativePose.position;
            const FadePoint tracked{position.x,position.y,position.z,camera->w};
            const auto alpha=originalActorCameraFade(owner,index,character,transform,&tracked,inner,outer);
            static std::atomic_bool reported{};
            if(!reported.exchange(true)){
                std::ostringstream out;out<<"Cabin D-Dog camera fade uses tracked view: native="
                    <<camera->x<<','<<camera->y<<','<<camera->z<<" tracked="
                    <<tracked.x<<','<<tracked.y<<','<<tracked.z<<" preceding_alpha="
                    <<get<float>(model+0x1b8)
                    <<" tracked_alpha="<<alpha;
                mgs5vr::log(out.str());
            }
            return alpha;
        }
    }
    return originalActorCameraFade(owner,index,character,transform,camera,inner,outer);
}
void suppressOwnedArmFade(void* object,uint32_t visualId){
    restoreStaleVisibilityIfAllowed();
    if(!mgs5vr::headCamera().active())return;
    const auto pool=reinterpret_cast<uintptr_t>(object);
    if(get<uintptr_t>(pool)!=base+0x23adef0)return;
    const auto owner=fadeOwner.load();
    if(get<uintptr_t>(owner)!=base+0x23b8218)return;
    const auto character=get<uintptr_t>(owner+0x370);
    if(get<uintptr_t>(character)!=base+0x2295210)return;
    const auto component=get<uintptr_t>(character+0x10),parts=get<uintptr_t>(component+0x10);
    const auto model=get<uintptr_t>(parts+0x68);
    if(get<uintptr_t>(model)!=base+0x20f4d90)return;
    if(get<uintptr_t>(pool+0x38)!=character)return;
    const auto records=get<uintptr_t>(pool+8);const auto count=get<uint32_t>(pool+0x10);
    const auto first=get<uint32_t>(pool+0x14);
    if(!records||!count||count>32||visualId<first||visualId-first>=count)return;
    const auto record=records+static_cast<uintptr_t>(visualId-first)*0x80;
    if(get<uintptr_t>(record)!=model)return;
    uint8_t nativeFlags{};
    // The publisher returns without consuming a command when neither its
    // active nor dirty bit is set. Leave inactive native records untouched.
    if(!read(record+0x79,nativeFlags)||(nativeFlags&3u)==0)return;
    // RVA ffe3e0 computes obstruction opacity and immediately calls this
    // publisher (fe0d00). It consumes +0x20, smooths the alpha through its
    // +0x18 pointer and publishes
    // renderer alpha before resetting +0x20 to 255. A post-fade override
    // therefore cannot affect the prepared image. Set the owned arm body's
    // opaque command BEFORE that consumption; native +0x79 bit 0x20 bypasses
    // smoothing (fe1069/fe10a1), and is consumed/cleared at fe115a. Do not
    // change group visibility, shadow state, other models or animation.
    const auto command=mgs5vr::firstPersonOpaquePublication(nativeFlags);
    *reinterpret_cast<uint8_t*>(record+0x20)=command.opacity;
    *reinterpret_cast<uint8_t*>(record+0x79)=command.flags;
    static std::atomic_bool reported{};
    if(!reported.exchange(true))mgs5vr::log("Native first-person arm opacity preserved before visual publication");
}
void visualUpdate(void* object,uint32_t visualId){
    // A shot may have no player-camera publisher. Restore our concealment at
    // the native visual boundary before its first actor draw, not 250 ms later.
    if(mgs5vr::nativeScriptedDemoActive())mgs5vr::updateAuthoredPlayerVisibility();
    suppressOwnedArmFade(object,visualId);
    VisibilitySnapshot before;
    bool trace=false;
    if(visibilityTraceEnabled.load()&&visibilityTraceArmed.load()
        &&(visibilityTraceWindow.load()||visibilityTraceNeedsOwner.load())
        &&(visibilityTraceNeedsOwner.load()||(visibilityPublisherPool.load()==reinterpret_cast<uintptr_t>(object)
            &&visibilityPublisherId.load()==visualId)))try{
        trace=inspectVisibilityBounded(visibilitySourceOwner.load(),reinterpret_cast<uintptr_t>(object),visualId,before)
            &&(before.nativeFlags&3u)!=0;
    }catch(...){}
    originalVisualUpdate(object,visualId);
    if(trace)try{
        VisibilitySnapshot after;
        if(inspectVisibilityBounded(before.identity.owner,before.identity.pool,visualId,after)
            &&mgs5vr::sameNativeVisibilityIdentity(before.identity,after.identity)){
            after.publisherQpc=visibilityQpc();
            std::lock_guard lock(visibilityTraceMutex);
            visibilityPublisher=after;visibilityPublisherPool.store(after.identity.pool);
            visibilityPublisherId.store(after.identity.visualId);visibilityTraceNeedsOwner.store(false);
        }
    }catch(...){}
}
struct GroupChange {uint32_t name{};uint8_t previousFlags{};};
struct Binding {
    uintptr_t owner{},character{},parts{},record{},renderer{},model{};
    bool body{},hideArms{},headOnly{};
    bool equipment{};
    uintptr_t service{},itemInterface{};
    uint32_t itemIndex{};
    bool backWeapon{};
    uintptr_t custom{};
    uint64_t resourceCode{};
    uint32_t assembly{},part{};
    std::array<GroupChange,128> groups{};
    uint16_t changed{};
};
// Up to 32 modular player parts plus two twelve-part back assemblies and NVG.
std::array<Binding,64> hidden{};
std::mutex authoredHeadMutex;
mgs5vr::AuthoredHeadVisibility authoredHead;
bool names(uintptr_t model,std::array<uint32_t,128>& result,uint16_t& count){
    if(get<uintptr_t>(model)!=base+0x20f4d90)return false;
    if(!read(model+0x1e8,count)||!count||count>result.size())return false;
    const auto data=get<uintptr_t>(model+0x180);SIZE_T copied{};
    return data&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(data),result.data(),count*4,&copied)&&copied==count*4;
}
bool owned(const Binding& b){
    if(get<uintptr_t>(b.owner)!=base+0x23b8218||get<uintptr_t>(b.owner+0x370)!=b.character
        ||get<uintptr_t>(b.character)!=base+0x2295210)return false;
    if(b.equipment||b.backWeapon){
        const auto component=get<uintptr_t>(b.character+0x80);
        if(get<uintptr_t>(component)!=base+0x23b3e80||get<uintptr_t>(component+8)!=b.character
            ||get<uintptr_t>(component+0x60)!=b.service||get<uintptr_t>(b.service)!=base+0x234b840
            ||get<uintptr_t>(b.itemInterface)!=base+0x22e2970)return false;
        if(b.backWeapon){
            if(!resolveWeaponPart||get<uintptr_t>(b.service+0xf0)!=b.custom
                ||get<uintptr_t>(b.custom)!=base+0x233ad90
                ||get<uintptr_t>(base+0x233ad90+0x10)!=base+0xd45930
                ||(b.assembly!=1&&b.assembly!=4)||b.part>11)return false;
            uint32_t index{};
            if(resolveWeaponPart(reinterpret_cast<void*>(b.custom),b.assembly,b.part,&index)!=b.itemInterface
                ||index!=b.itemIndex)return false;
        }else if(get<uintptr_t>(b.service+0x190)!=b.itemInterface)return false;
        const auto count=get<uint32_t>(b.itemInterface+0x50);
        const auto models=get<uintptr_t>(b.itemInterface+0x58);
        if(!models||!count||count>256||b.itemIndex>=count
            ||get<uintptr_t>(models+static_cast<uintptr_t>(b.itemIndex)*8)!=b.model
            ||get<uintptr_t>(b.model)!=base+0x20f4d90)return false;
        const auto code=get<uint64_t>(b.model+0x120);
        if(b.backWeapon)return code==b.resourceCode;
        return std::find(nvgModels.begin(),nvgModels.end(),code)!=nvgModels.end();
    }
    if(b.body){
        const auto component=get<uintptr_t>(b.character+0x10);
        return component&&get<uintptr_t>(component+0x10)==b.parts&&get<uintptr_t>(b.parts+0x68)==b.model;
    }
    if(get<uintptr_t>(b.character+0x610)!=b.parts||get<uintptr_t>(b.parts)!=base+0x22e56c0
        ||get<uintptr_t>(b.parts+0x38)!=b.character)return false;
    const auto list=get<uintptr_t>(b.parts+0x48);
    if(!list||get<uintptr_t>(list)!=base+0x2215c78)return false;
    const auto count=get<uint32_t>(list+0x10);const auto data=get<uintptr_t>(list+8);
    if(!count||count>32||!data)return false;
    bool member=false;
    for(uint32_t i=0;i<count;++i)if(get<uintptr_t>(data+i*8)==b.record){member=true;break;}
    return member&&get<uintptr_t>(b.record)==b.renderer&&get<uintptr_t>(b.renderer)==base+0x20f9460
        &&get<uintptr_t>(b.renderer+0x40)==b.model;
}
int groupIndex(const Binding& b,uint32_t name){
    std::array<uint32_t,128> groups{};uint16_t count{};
    if(!names(b.model,groups,count))return -1;
    const auto end=groups.begin()+count,found=std::find(groups.begin(),end,name);
    return found==end?-1:static_cast<int>(found-groups.begin());
}
uint8_t groupFlags(const Binding& b,uint32_t name){
    const auto index=groupIndex(b,name);const auto data=get<uintptr_t>(b.model+0x170);
    return index>=0&&data?get<uint8_t>(data+static_cast<uintptr_t>(index)):0;
}
bool bodyBranches(uintptr_t model,const std::array<uint32_t,128>& groups,uint16_t count,std::array<bool,128>& conceal){
    // The native group inheritance walk (1ce980) reads signed parents at +4
    // in eight-byte records. Preserve the arm subtree and its ancestry; other
    // branches contain body/garment attachments that must keep casting shadows.
    const auto asset=get<uintptr_t>(model+0x188),records=get<uintptr_t>(asset+8);
    if(!asset||!records)return false;
    std::array<int16_t,128> parents{};std::array<bool,128> keep{};
    int arm=-1;
    for(uint16_t i=0;i<count;++i){
        if(!read(records+i*8+4,parents[i])||parents[i]<-1||parents[i]>=i)return false;
        if(groups[i]==armName){if(arm>=0)return false;arm=i;}
    }
    if(arm<0||parents[arm]<0)return false;
    // Reject a body/arm overlap instead of hiding an ancestor of visible arms.
    for(int i=arm;i>=0;i=parents[i]){if(groups[i]==bodyName)return false;keep[i]=true;}
    for(uint16_t i=0;i<count;++i){
        auto ancestor=parents[i];
        while(ancestor>=0&&ancestor!=arm)ancestor=parents[ancestor];
        if(ancestor==arm)keep[i]=true;
    }
    for(uint16_t i=0;i<count;++i)
        conceal[i]=!keep[i]&&(parents[i]<0||keep[parents[i]]);
    return true;
}
bool headBranches(uintptr_t model,const std::array<uint32_t,128>& groups,uint16_t count,std::array<bool,128>& conceal){
    const auto asset=get<uintptr_t>(model+0x188),records=get<uintptr_t>(asset+8);
    if(!asset||!records)return false;
    bool found=false;
    for(uint16_t i=0;i<count;++i){
        int16_t parent{};
        if(!read(records+i*8+4,parent)||parent<-1||parent>=i)return false;
        conceal[i]=groups[i]==headName||(parent>=0&&conceal[static_cast<uint16_t>(parent)]);
        found=found||groups[i]==headName;
    }
    return found;
}
void restore(Binding& b){
    if(b.model&&owned(b)&&showVisibleGroup){
        for(uint16_t i=0;i<b.changed;++i){const auto& group=b.groups[i];
            // Restore only our normal-draw bit while the rest of the named
            // group's state still matches. Never enable shadows here.
            if(groupFlags(b,group.name)==static_cast<uint8_t>(group.previousFlags&~4u))
                showVisibleGroup(reinterpret_cast<void*>(b.model),group.name);
        }
    }
    b={};
}
void restoreStaleVisibilityIfAllowed() noexcept {
    if(mgs5vr::nativeSortiePreviewActive()){
        std::unique_lock lock(hiddenMutex,std::try_to_lock);
        if(!lock.owns_lock())return;
        for(auto& binding:hidden)if(binding.model)restore(binding);
        fullBodyReady={};
        return;
    }
    const auto now=GetTickCount64();
    const auto observedLast=lastFirstPersonUpdateMs.load();
    if(!observedLast||now<observedLast||now-observedLast<250)return;
    std::unique_lock lock(hiddenMutex,std::try_to_lock);
    if(!lock.owns_lock())return;
    const auto last=lastFirstPersonUpdateMs.load();
    if(!last||now<last||now-last<250)return;
    const auto elapsed=now-last;
    const auto status=mgs5vr::headCamera().status();
    const bool immersive=status.active||status.pending;
    const bool stereoMenuOpen=immersive&&(status.nativeMenuOpen||status.nativeIdroidOpen);
    if(!mgs5vr::shouldRestoreStalePlayerVisibility(elapsed,immersive,
                                                   mgs5vr::nativeScriptedDemoActive(),stereoMenuOpen))return;
    bool restored=false;
    for(auto& binding:hidden)if(binding.model){restore(binding);restored=true;}
    if(restored)mgs5vr::log("Restored owned first-person visibility after stale publication and confirmed presentation transition");
}
void conceal(Binding candidate){
    if(!hideVisibleGroup||!owned(candidate))return;
    auto slot=std::find_if(hidden.begin(),hidden.end(),[&](const auto& b){return b.model==candidate.model&&b.owner==candidate.owner;});
    if(slot==hidden.end()){
        slot=std::find_if(hidden.begin(),hidden.end(),[](const auto& b){return !b.model;});
        if(slot==hidden.end())return;
        *slot=candidate;
        mgs5vr::log(candidate.backWeapon?"First-person stowed back weapon concealed; native draw/equip and shadow retained":candidate.equipment?"First-person NVG goggles hidden before draw preparation; native vision filter and shadow retained":candidate.headOnly?"Integrated player head subtree hidden from main view; native body and shadow retained":candidate.hideArms?"Front-end player arms hidden before draw preparation; native shadow retained":candidate.body?"First-person player body hidden from main view; native shadow retained":"First-person player head hidden from main view; native shadow retained");
    }
    std::array<uint32_t,128> groups{};uint16_t count{};
    if(!names(candidate.model,groups,count))return;
    std::array<bool,128> bodyConceal{};
    if(candidate.headOnly){
        if(!headBranches(candidate.model,groups,count,bodyConceal))return;
    }else if(candidate.body&&!candidate.hideArms&&!bodyBranches(candidate.model,groups,count,bodyConceal))return;
    const auto flags=get<uintptr_t>(candidate.model+0x170);if(!flags)return;
    for(uint16_t i=0;i<count;++i){
        if((candidate.headOnly||(candidate.body&&!candidate.hideArms))&&!bodyConceal[i])continue;
        uint8_t previous{};if(!read(flags+i,previous)||!(previous&4))continue;
        auto change=std::find_if(slot->groups.begin(),slot->groups.begin()+slot->changed,
            [&](const auto& g){return g.name==groups[i];});
        if(change==slot->groups.begin()+slot->changed){
            if(slot->changed==slot->groups.size())continue;
            change=slot->groups.begin()+slot->changed++;
        }
        *change={groups[i],previous};
        hideVisibleGroup(reinterpret_cast<void*>(candidate.model),groups[i]);
    }
}
}
namespace mgs5vr {
void requestFullBodyVisibility(bool enabled) noexcept{
    fullBodyRequested.store(enabled);
    if(!enabled){std::lock_guard lock(hiddenMutex);fullBodyReady={};}
}
void publishFullBodyVisibility(uintptr_t owner,uintptr_t model,bool ready) noexcept{
    // Read the admitted skin transaction before taking the visibility lock,
    // keeping camera/visibility lock order independent of native callbacks.
    const auto frame=ready&&fullBodyRequested.load()&&!nativeSortiePreviewActive()
        ?headCamera().publishedRigFrame(steadyMilliseconds()):std::nullopt;
    FullBodyReady candidate{};
    if(frame&&frame->applied&&frame->body.valid&&frame->controllers.fullBody
       &&owner&&model&&frame->playerOwner==owner&&frame->activation
       &&frame->controllers.referenceEpoch&&frame->controllers.presentationEpoch
       &&frame->trackingSequence&&frame->playerSequence&&frame->rigSequence){
        candidate={owner,model,GetTickCount64(),frame->activation,
            frame->controllers.referenceEpoch,frame->controllers.presentationEpoch,
            frame->trackingSequence,frame->playerSequence,frame->rigSequence,
            frame->sampleTime,frame->menuGeneration};
    }
    std::lock_guard lock(hiddenMutex);
    fullBodyReady=fullBodyRequested.load()?candidate:FullBodyReady{};
    // A new native camera publication can retire the preceding skin before
    // this solve arrives. Reveal this exact admitted body at its own skin
    // publication, before draw preparation, rather than waiting for the next
    // camera callback. Modular head/NVG and shadow ownership stay untouched.
    if(fullBodyReady.owner)for(auto& b:hidden)
        if(b.model&&b.body&&!b.hideArms&&!b.headOnly&&b.owner==fullBodyReady.owner
           &&b.model==fullBodyReady.model&&owned(b))restore(b);
    // Restoring the body's normal draw bits also restores an integrated head.
    // Reapply its exclusion at this same admitted skin publication; the next
    // camera callback has already retired this solve and cannot do this for it.
    if(fullBodyReady.owner&&get<uintptr_t>(owner)==base+0x23b8218){
        const auto character=get<uintptr_t>(owner+0x370);
        const auto component=get<uintptr_t>(character+0x10);
        const auto bodyParts=get<uintptr_t>(component+0x10);
        Binding head{owner,character,bodyParts,0,0,model,true};
        head.headOnly=true;
        if(owned(head)&&groupIndex(head,headName)>=0)conceal(head);
    }
}
void observePlayerVisibilityForSource(const HeadCameraSample& frame,uint64_t sourceSequence) noexcept {
    if(!visibilityTraceEnabled.load())return;
    if(!frame.applied||!frame.stereoTracked||!frame.rigSequence||!frame.playerOwner
        ||frame.controllers.frontEnd||frame.controllers.loading||frame.controllers.avatarEditor
        ||!frame.controllers.handheldMenus)return;
    try{
        VisibilitySnapshot publisher;uint32_t boundary{},sample{};
        {
            std::lock_guard lock(visibilityTraceMutex);
            visibilityTraceArmed.store(true);
            visibilitySourceOwner.store(frame.playerOwner);
            visibilityInspectionBudget.arm(GetTickCount64());
            if(visibilityPublisher.identity.owner!=frame.playerOwner)visibilityTraceNeedsOwner.store(true);
            const bool admitted=visibilityBudget.admit(frame.playerOwner,frame.activation,
                frame.menuGeneration,frame.menuIdroid,sourceSequence);
            visibilityTraceWindow.store(admitted);
            if(!admitted)return;
            boundary=visibilityBudget.boundary();sample=visibilityBudget.sample();publisher=visibilityPublisher;
        }
        const auto eventQpc=visibilityQpc();VisibilitySnapshot current,verified;
        const bool accepted=publisher.identity.owner==frame.playerOwner
            &&inspectVisibilityBounded(frame.playerOwner,publisher.identity.pool,publisher.identity.visualId,current)
            &&sameNativeVisibilityIdentity(publisher.identity,current.identity)
            &&inspectVisibilityBounded(frame.playerOwner,publisher.identity.pool,publisher.identity.visualId,verified)
            &&sameNativeVisibilityIdentity(current.identity,verified.identity)
            &&current.opacity==verified.opacity&&current.armFlags==verified.armFlags&&current.nativeFlags==verified.nativeFlags;
        if(!accepted)visibilityTraceNeedsOwner.store(true);
        std::ostringstream out;
        out<<"Player visibility source observe source="<<sourceSequence<<" rig="<<frame.rigSequence
            <<" tracking="<<frame.trackingSequence<<" activation="<<frame.activation<<" menu="<<frame.menuGeneration
            <<" reference="<<frame.controllers.referenceEpoch<<" presentation="<<frame.controllers.presentationEpoch
            <<" predicted="<<frame.controllers.predictedXrTime<<" focused="<<frame.controllers.presentationFocused
            <<" owner="<<frame.playerOwner<<" sample_ms="<<frame.sampleTime<<" qpc="<<eventQpc
            <<" boundary="<<boundary<<" boundary_sample="<<sample<<" idroid="<<frame.menuIdroid
            <<" accepted="<<accepted<<" fade_owner="<<fadeOwner.load()<<" pool="<<publisher.identity.pool
            <<" record="<<publisher.identity.record<<" model="<<publisher.identity.model
            <<" character="<<publisher.identity.character<<" body_interface="<<publisher.identity.bodyInterface
            <<" holder="<<publisher.identity.holder<<" records="<<publisher.identity.records
            <<" visual_id="<<publisher.identity.visualId<<" first="<<publisher.identity.first<<" count="<<publisher.identity.count
            <<" publisher_available="<<(publisher.publisherQpc!=0)<<" publisher_qpc="<<publisher.publisherQpc
            <<" publisher_opacity="<<static_cast<unsigned>(publisher.opacity)
            <<" publisher_arm_flags="<<static_cast<unsigned>(publisher.armFlags)
            <<" publisher_native_flags="<<static_cast<unsigned>(publisher.nativeFlags)
            <<" current_opacity="<<(accepted?static_cast<int>(current.opacity):-1)
            <<" current_arm_flags="<<(accepted?static_cast<int>(current.armFlags):-1)
            <<" current_native_flags="<<(accepted?static_cast<int>(current.nativeFlags):-1)
            <<" normal_arm_draw="<<(accepted&&(current.armFlags&4u)!=0)
            <<" before_both_eye_replays=1 publisher_rig_joined=0 atomic_with_pixels=0 observation_only=1";
        log(out.str());
        if(boundary==4&&sample==12){visibilityTraceEnabled.store(false);visibilityTraceWindow.store(false);}
    }catch(...){}
}
MenuCapturePlayerExclusion::MenuCapturePlayerExclusion(uintptr_t owner,bool preserveArms) noexcept
    :preserveArms_(preserveArms){
    if(!base||!hideVisibleGroup||!showVisibleGroup||get<uintptr_t>(owner)!=base+0x23b8218)return;
    std::lock_guard lock(hiddenMutex);
    const auto character=get<uintptr_t>(owner+0x370);
    if(get<uintptr_t>(character)!=base+0x2295210)return;
    const auto component=get<uintptr_t>(character+0x10),parts=get<uintptr_t>(component+0x10);
    const auto model=get<uintptr_t>(parts+0x68);
    Binding candidate{owner,character,parts,0,0,model,true};
    if(!owned(candidate)||groupIndex(candidate,bodyName)<0||groupIndex(candidate,armName)<0
        ||!names(model,groups_,count_)){count_=0;return;}
    owner_=owner;character_=character;parts_=parts;model_=model;
    std::array<bool,128> bodyConceal{};
    if(preserveArms_&&!bodyBranches(model,groups_,count_,bodyConceal)){count_=0;return;}
    for(uint16_t i=0;i<count_;++i){
        flags_[i]=groupFlags(candidate,groups_[i]);
        if((flags_[i]&4u)&&(!preserveArms_||bodyConceal[i]))
            hideVisibleGroup(reinterpret_cast<void*>(model),groups_[i]);
    }
}
MenuCapturePlayerExclusion::~MenuCapturePlayerExclusion(){
    std::lock_guard lock(hiddenMutex);
    const Binding candidate{owner_,character_,parts_,0,0,model_,true};
    if(!model_||!showVisibleGroup||!owned(candidate))return;
    std::array<bool,128> bodyConceal{};
    if(preserveArms_&&!bodyBranches(model_,groups_,count_,bodyConceal))return;
    for(uint16_t i=0;i<count_;++i)
        if((flags_[i]&4u)&&(!preserveArms_||bodyConceal[i])
           &&groupFlags(candidate,groups_[i])==static_cast<uint8_t>(flags_[i]&~4u))
            showVisibleGroup(reinterpret_cast<void*>(model_),groups_[i]);
}
RocketHandPresentation::RocketHandPresentation(const HeadCameraSample& frame,bool headPass) noexcept {
    if(!headPass||!frame.rocketArmGuidance||!frame.body.valid||!frame.idroidDeviceTracked
       ||!frame.applied||!frame.rigSequence||!showVisibleGroup||!hideVisibleGroup)return;
    constexpr uint64_t blastHandModel=0x84a2a348c3d25162ull;
    constexpr uint32_t handGroup=0xf948d635u;
    const auto owner=frame.playerOwner,character=get<uintptr_t>(owner+0x370);
    if(get<uintptr_t>(owner)!=base+0x23b8218||get<uintptr_t>(character)!=base+0x2295210)return;
    const auto parts=get<uintptr_t>(character+0x610);
    if(get<uintptr_t>(parts)!=base+0x22e56c0||get<uintptr_t>(parts+0x38)!=character)return;
    const auto list=get<uintptr_t>(parts+0x48);
    if(get<uintptr_t>(list)!=base+0x2215c78)return;
    const auto count=get<uint32_t>(list+0x10);const auto records=get<uintptr_t>(list+8);
    if(!records||!count||count>32)return;
    Binding selected{};
    for(uint32_t i=0;i<count;++i){
        const auto record=get<uintptr_t>(records+i*8),renderer=get<uintptr_t>(record);
        const auto model=get<uintptr_t>(renderer+0x40);
        Binding candidate{owner,character,parts,record,renderer,model};
        if(!owned(candidate)||get<uintptr_t>(model)!=base+0x20f4d90
           ||get<uint64_t>(model+0x120)!=blastHandModel||groupIndex(candidate,handGroup)<0)continue;
        if(selected.model)return; // Never choose between ambiguous parts.
        selected=candidate;
    }
    if(!selected.model||(groupFlags(selected,handGroup)&4u))return;
    showVisibleGroup(reinterpret_cast<void*>(selected.model),handGroup);
    owner_=owner;character_=character;parts_=parts;record_=selected.record;
    renderer_=selected.renderer;model_=selected.model;
}
RocketHandPresentation::~RocketHandPresentation(){
    const Binding binding{owner_,character_,parts_,record_,renderer_,model_};
    if(model_&&hideVisibleGroup&&owned(binding)&&get<uint64_t>(model_+0x120)==0x84a2a348c3d25162ull)
        hideVisibleGroup(reinterpret_cast<void*>(model_),0xf948d635u);
}
void initializePlayerVisibility(uintptr_t moduleBase) noexcept {
    base=moduleBase;hideVisibleGroup=showVisibleGroup=nullptr;
    resolveWeaponPart=nullptr;
    constexpr std::array<unsigned char,10> partSignature{0x41,0x83,0xf8,0x0b,0x0f,0x87,0xb9,0,0,0};
    std::array<unsigned char,10> partBytes{};
    if(read(base+0xd45930,partBytes)&&partBytes==partSignature
        &&get<uintptr_t>(base+0x233ad90+0x10)==base+0xd45930)
        resolveWeaponPart=reinterpret_cast<ResolveWeaponPart>(base+0xd45930);
    wchar_t trace[2]{};
    visibilityTraceEnabled.store(GetEnvironmentVariableW(L"MGS5VR_IDROID_UI_BOUNDARY_TRACE",trace,2)==1&&trace[0]==L'1');
    if(visibilityTraceEnabled.load())log("Player visibility source observation enabled max_boundaries=4 max_sources_per_boundary=12 max_ms=45000 writes=0");
    // StaticModel SHADOW_ONLY=1 calls the first-list hide helper. Its separate
    // DISABLE_SHADOW=2 branch hides the second list. Both callers and helper
    // bytes were verified in this exact profile; no guessed visibility masks.
    constexpr std::array<unsigned char,22> hideSignature{0x40,0x53,0x48,0x83,0xec,0x20,0x44,0x0f,0xb7,0x89,0xe8,0x01,0,0,0x33,0xc0,0x48,0x8b,0xd9,0x45,0x85,0xc9};
    constexpr std::array<unsigned char,23> showSignature{0x48,0x83,0xec,0x28,0x44,0x0f,0xb7,0x91,0xe8,0x01,0,0,0x33,0xc0,0x4c,0x8b,0xd9,0x44,0x8b,0xc0,0x45,0x85,0xd2};
    std::array<unsigned char,22> hideBytes{};std::array<unsigned char,23> showBytes{};
    if(read(base+0x1ccc30,hideBytes)&&hideBytes==hideSignature&&read(base+0x1cee30,showBytes)&&showBytes==showSignature){
        hideVisibleGroup=reinterpret_cast<GroupFn>(base+0x1ccc30);showVisibleGroup=reinterpret_cast<GroupFn>(base+0x1cee30);
    }
    constexpr std::array<unsigned char,20> visualSignature{0x89,0x54,0x24,0x10,0x55,0x53,0x56,0x57,
        0x48,0x8d,0x6c,0x24,0xa8,0x48,0x81,0xec,0x58,0x01,0,0};
    std::array<unsigned char,20> visualBytes{};
    auto* target=reinterpret_cast<void*>(base+0xfe0d00);
    if(read(base+0xfe0d00,visualBytes)&&visualBytes==visualSignature
       &&get<uintptr_t>(base+0x23adef0)==base+0xfe0d00
       &&MH_CreateHook(target,reinterpret_cast<void*>(&visualUpdate),reinterpret_cast<void**>(&originalVisualUpdate))==MH_OK){
        if(MH_EnableHook(target)==MH_OK)log("Native first-person arm opacity suppression connected before visual publication");
        else MH_RemoveHook(target);
    }
    constexpr std::array<unsigned char,12> fadeSignature{
        0x48,0x8b,0xc4,0x48,0x89,0x58,8,0x48,0x89,0x70,0x20,0x57};
    std::array<unsigned char,12> fadeBytes{};
    auto* fadeTarget=reinterpret_cast<void*>(base+0xc8c300);
    if(read(base+0xc8c300,fadeBytes)&&fadeBytes==fadeSignature
        &&MH_CreateHook(fadeTarget,reinterpret_cast<void*>(&actorCameraFade),
            reinterpret_cast<void**>(&originalActorCameraFade))==MH_OK){
        if(MH_EnableHook(fadeTarget)==MH_OK)log("Native buddy camera-nearness query connected to tracked cabin view");
        else MH_RemoveHook(fadeTarget);
    }
}
void publishAuthoredPlayerHead(uintptr_t owner,uintptr_t model,Pose head,uint64_t activation) noexcept {
    if(!owner||!model||!activation||!valid(head))return;
    std::lock_guard lock(authoredHeadMutex);
    authoredHead={owner,model,head,{},steadyMilliseconds(),activation,false};
}
AuthoredHeadVisibility authoredHeadVisibility() noexcept {
    std::lock_guard lock(authoredHeadMutex);return authoredHead;
}
void updateAuthoredPlayerVisibility() noexcept {
    const auto frame=headCamera().publishedView();
    const auto now=steadyMilliseconds();
    auto head=authoredHeadVisibility();
    const bool joined=frame&&frame->applied&&frame->controllers.scriptedDemo&&frame->activation==head.activation
        &&head.time&&now>=head.time&&now-head.time<=150&&now>=frame->sampleTime&&now-frame->sampleTime<=150;
    const auto offset=joined?frame->nativePose.position-head.head.position:Vec3{1,1,1};
    const bool headNearby=joined&&dot(offset,offset)<.09f;
    {
        std::lock_guard lock(authoredHeadMutex);
        if(authoredHead.owner==head.owner&&authoredHead.time==head.time){
            authoredHead.viewer=joined?frame->nativePose:Pose{};authoredHead.concealed=headNearby;
        }
    }
    // A first-person authored shot keeps native torso/limbs and their shadow.
    // Only the local player's own modular head/NVG can obstruct these eyes.
    // Outside that head volume the complete authored actor is restored.
    updatePlayerVisibility(headNearby?head.owner:0,headNearby,false,true);
}
void updatePlayerVisibility(uintptr_t owner,bool firstPerson,bool hideArms,bool authoredBody) noexcept {
    // The sortie panel renders the complete native character. Its actor can
    // reuse the player models concealed in the cabin, but is not a view from
    // Snake's eyes. Restore our visibility edits while this exact preview owns
    // presentation; ordinary menus and first-person play retain their policy.
    if(nativeSortiePreviewActive())firstPerson=false;
    if(!base)return;
    fadeOwner.store(firstPerson&&!hideArms&&!authoredBody?owner:0);
    // A recent solve from this model alone is insufficient after a tracking,
    // activation or presentation boundary. Use the current admitted rig and
    // never acquire the camera lock while holding hiddenMutex.
    const auto frame=firstPerson&&!authoredBody
        ?headCamera().publishedRigFrame(steadyMilliseconds()):std::nullopt;
    std::lock_guard lock(hiddenMutex);
    lastFirstPersonUpdateMs.store(firstPerson?GetTickCount64():0);
    const auto now=GetTickCount64();
    const bool fieldGear=frame&&frame->applied&&frame->playerOwner==owner
        &&frame->weaponRig.componentAccepted&&frame->weaponRig.instanceIndexMatched&&frame->weaponRig.resourceAccepted
        &&!frame->controllers.cabinPlay&&!frame->controllers.cabinTransit
        &&!frame->controllers.frontEnd&&!frame->controllers.authoredCamera&&!frame->body.mounted;
    const uint32_t equipment=fieldGear?(frame->weaponRig.resourceHandle>>5)&0x7ffu:0;
    const bool drawn=fieldGear&&(frame->controllers.weaponReady||(frame->weaponRig.mode&0x04000000u)!=0);
    // Moving publishes a new native camera before the matching skin solve.
    // That temporarily retires publishedRigFrame; it is not a field exit.
    // Retain owned concealment through that gap. The accepted skin below
    // updates draw/stow policy; explicit presentation/owner exits still restore.
    if(frame||!firstPerson||authoredBody)
        for(auto& b:hidden)if(b.backWeapon&&!concealStowedBackAssembly(b.assembly,equipment,fieldGear,drawn))restore(b);
    if(!firstPerson||!fullBodyRequested.load())fullBodyReady={};
    const bool ready=frame&&frame->applied&&frame->body.valid&&frame->controllers.fullBody
        &&fullBodyRequested.load()&&fullBodyReady.owner==owner&&frame->playerOwner==owner
        &&now>=fullBodyReady.time&&now-fullBodyReady.time<=250
        &&frame->activation==fullBodyReady.activation
        &&frame->controllers.referenceEpoch==fullBodyReady.referenceEpoch
        &&frame->controllers.presentationEpoch==fullBodyReady.presentationEpoch
        &&frame->trackingSequence==fullBodyReady.trackingSequence
        &&frame->playerSequence==fullBodyReady.playerSequence
        &&frame->rigSequence==fullBodyReady.rigSequence
        &&frame->sampleTime==fullBodyReady.sampleTime
        &&frame->menuGeneration==fullBodyReady.menuGeneration;
    for(auto& b:hidden)if(b.model&&(!firstPerson||b.owner!=owner||!owned(b)||(b.body&&(
        b.headOnly?!(authoredBody||(ready&&!hideArms&&b.model==fullBodyReady.model)):
        (authoredBody||b.hideArms!=hideArms||(ready&&!hideArms&&b.model==fullBodyReady.model))))))restore(b);
    if(!firstPerson||get<uintptr_t>(owner)!=base+0x23b8218){setForearmPlayer(0,0);return;}
    const auto character=get<uintptr_t>(owner+0x370);
    if(!character||get<uintptr_t>(character)!=base+0x2295210)return;
    const auto component=get<uintptr_t>(character+0x10),bodyParts=get<uintptr_t>(component+0x10);
    const auto bodyModel=get<uintptr_t>(bodyParts+0x68);
    setForearmPlayer(owner,!hideArms&&!authoredBody&&!(ready&&bodyModel==fullBodyReady.model)?bodyModel:0);
    Binding body{owner,character,bodyParts,0,0,bodyModel,true};
    // Do this at player publication, before FOX builds the visible draw list.
    // A scene-only hide is too late for already-prepared arm geometry.
    body.hideArms=hideArms;
    // Hospital Snake keeps his head in the body model. Its head group is
    // already hidden natively, but independently visible child groups still
    // enter the near eyes. Conceal only that verified subtree; retain torso,
    // arms and every group's shadow bit.
    if((authoredBody||(ready&&!hideArms&&bodyModel==fullBodyReady.model))&&groupIndex(body,headName)>=0){
        body.headOnly=true;body.hideArms=false;conceal(body);
    }
    if(!authoredBody&&bodyModel&&groupIndex(body,bodyName)>=0&&groupIndex(body,armName)>=0
       &&!(ready&&!hideArms&&bodyModel==fullBodyReady.model))conceal(body);
    // NVG is native equipment, outside the modular head list. Conceal only its
    // exact already-loaded model in this character's typed item interface.
    // Keep equip/on/off and the green vision effect under normal game control.
    const auto weapon=get<uintptr_t>(character+0x80);
    if(get<uintptr_t>(weapon)==base+0x23b3e80&&get<uintptr_t>(weapon+8)==character){
        const auto service=get<uintptr_t>(weapon+0x60);
        if(get<uintptr_t>(service)==base+0x234b840){
            const auto custom=get<uintptr_t>(service+0xf0);
            if(fieldGear&&resolveWeaponPart&&get<uintptr_t>(custom)==base+0x233ad90){
                for(const uint32_t assembly:{1u,4u}){
                    if(!concealStowedBackAssembly(assembly,equipment,true,drawn))continue;
                    for(uint32_t part=0;part<12;++part){
                        uint32_t index{};
                        const auto partInterface=resolveWeaponPart(reinterpret_cast<void*>(custom),assembly,part,&index);
                        if(get<uintptr_t>(partInterface)!=base+0x22e2970)continue;
                        const auto count=get<uint32_t>(partInterface+0x50);
                        const auto models=get<uintptr_t>(partInterface+0x58);
                        if(!models||!count||count>256||index>=count)continue;
                        const auto model=get<uintptr_t>(models+static_cast<uintptr_t>(index)*8);
                        if(get<uintptr_t>(model)!=base+0x20f4d90)continue;
                        Binding stowed{owner,character,0,0,0,model};
                        stowed.backWeapon=true;stowed.service=service;stowed.custom=custom;
                        stowed.itemInterface=partInterface;stowed.itemIndex=index;
                        stowed.resourceCode=get<uint64_t>(model+0x120);stowed.assembly=assembly;stowed.part=part;
                        conceal(stowed);
                    }
                }
            }
            const auto interface=get<uintptr_t>(service+0x190);
            if(get<uintptr_t>(interface)==base+0x22e2970){
                const auto models=get<uintptr_t>(interface+0x58);
                const auto count=get<uint32_t>(interface+0x50);
                if(models&&count&&count<=256)for(uint32_t i=0;i<count;++i){
                    const auto model=get<uintptr_t>(models+static_cast<uintptr_t>(i)*8);
                    if(get<uintptr_t>(model)!=base+0x20f4d90)continue;
                    const auto code=get<uint64_t>(model+0x120);
                    if(std::find(nvgModels.begin(),nvgModels.end(),code)==nvgModels.end())continue;
                    Binding goggles{owner,character,0,0,0,model};
                    goggles.equipment=true;goggles.service=service;
                    goggles.itemInterface=interface;goggles.itemIndex=i;
                    conceal(goggles);
                }
            }
        }
    }
    const auto parts=get<uintptr_t>(character+0x610);
    if(!parts||get<uintptr_t>(parts)!=base+0x22e56c0||get<uintptr_t>(parts+0x38)!=character)return;
    const auto list=get<uintptr_t>(parts+0x48);
    if(!list||get<uintptr_t>(list)!=base+0x2215c78)return;
    const auto count=get<uint32_t>(list+0x10);const auto data=get<uintptr_t>(list+8);
    if(!count||count>32||!data)return;
    for(uint32_t i=0;i<count;++i){
        const auto record=get<uintptr_t>(data+i*8),renderer=get<uintptr_t>(record);
        if(!renderer||get<uintptr_t>(renderer)!=base+0x20f9460)continue;
        const auto model=get<uintptr_t>(renderer+0x40);
        Binding head{owner,character,parts,record,renderer,model};
        if(model&&groupIndex(head,bodyName)<0&&groupIndex(head,armName)<0
           &&(groupIndex(head,headName)>=0||get<uint64_t>(model+0x120)==hospitalFaceCoverModel))conceal(head);
    }
}
}
