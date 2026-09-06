#include "mgs5vr/player_visibility.hpp"
#include "mgs5vr/log.hpp"
#include <windows.h>
#include <array>
#include <algorithm>

namespace {
uintptr_t base{};
using GroupFn=void(*)(void*,uint32_t);
GroupFn hideVisibleGroup{},showVisibleGroup{};
constexpr uint32_t headName=0xa9e88501,bodyName=0x1a166b34,armName=0x4e74fd8c;
template<class T> bool read(uintptr_t address,T& value){
    SIZE_T copied{};
    return address&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),&value,sizeof(value),&copied)&&copied==sizeof(value);
}
template<class T> T get(uintptr_t address){T value{};read(address,value);return value;}
struct GroupChange {uint32_t name{};uint8_t previousFlags{};};
struct Binding {
    uintptr_t owner{},character{},parts{},record{},renderer{},model{};
    bool body{};
    std::array<GroupChange,128> groups{};
    uint16_t changed{};
};
std::array<Binding,32> hidden{};
bool names(uintptr_t model,std::array<uint32_t,128>& result,uint16_t& count){
    if(get<uintptr_t>(model)!=base+0x20f4d90)return false;
    if(!read(model+0x1e8,count)||!count||count>result.size())return false;
    const auto data=get<uintptr_t>(model+0x180);SIZE_T copied{};
    return data&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(data),result.data(),count*4,&copied)&&copied==count*4;
}
bool owned(const Binding& b){
    if(get<uintptr_t>(b.owner)!=base+0x23b8218||get<uintptr_t>(b.owner+0x370)!=b.character
        ||get<uintptr_t>(b.character)!=base+0x2295210)return false;
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
void conceal(Binding candidate){
    if(!hideVisibleGroup||!owned(candidate))return;
    auto slot=std::find_if(hidden.begin(),hidden.end(),[&](const auto& b){return b.model==candidate.model&&b.owner==candidate.owner;});
    if(slot==hidden.end()){
        slot=std::find_if(hidden.begin(),hidden.end(),[](const auto& b){return !b.model;});
        if(slot==hidden.end())return;
        *slot=candidate;
        mgs5vr::log(candidate.body?"First-person player body hidden from main view; native shadow retained":"First-person player head hidden from main view; native shadow retained");
    }
    std::array<uint32_t,128> groups{};uint16_t count{};
    if(!names(candidate.model,groups,count))return;
    const auto flags=get<uintptr_t>(candidate.model+0x170);if(!flags)return;
    for(uint16_t i=0;i<count;++i){
        if(candidate.body&&groups[i]!=bodyName)continue;
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
void initializePlayerVisibility(uintptr_t moduleBase) noexcept {
    base=moduleBase;hideVisibleGroup=showVisibleGroup=nullptr;
    // StaticModel SHADOW_ONLY=1 calls the first-list hide helper. Its separate
    // DISABLE_SHADOW=2 branch hides the second list. Both callers and helper
    // bytes were verified in this exact profile; no guessed visibility masks.
    constexpr std::array<unsigned char,22> hideSignature{0x40,0x53,0x48,0x83,0xec,0x20,0x44,0x0f,0xb7,0x89,0xe8,0x01,0,0,0x33,0xc0,0x48,0x8b,0xd9,0x45,0x85,0xc9};
    constexpr std::array<unsigned char,23> showSignature{0x48,0x83,0xec,0x28,0x44,0x0f,0xb7,0x91,0xe8,0x01,0,0,0x33,0xc0,0x4c,0x8b,0xd9,0x44,0x8b,0xc0,0x45,0x85,0xd2};
    std::array<unsigned char,22> hideBytes{};std::array<unsigned char,23> showBytes{};
    if(read(base+0x1ccc30,hideBytes)&&hideBytes==hideSignature&&read(base+0x1cee30,showBytes)&&showBytes==showSignature){
        hideVisibleGroup=reinterpret_cast<GroupFn>(base+0x1ccc30);showVisibleGroup=reinterpret_cast<GroupFn>(base+0x1cee30);
    }
}
void updatePlayerVisibility(uintptr_t owner,bool firstPerson) noexcept {
    if(!base)return;
    for(auto& b:hidden)if(b.model&&(!firstPerson||b.owner!=owner||!owned(b)))restore(b);
    if(!firstPerson||get<uintptr_t>(owner)!=base+0x23b8218)return;
    const auto character=get<uintptr_t>(owner+0x370);
    if(!character||get<uintptr_t>(character)!=base+0x2295210)return;
    const auto component=get<uintptr_t>(character+0x10),bodyParts=get<uintptr_t>(component+0x10);
    const auto bodyModel=get<uintptr_t>(bodyParts+0x68);
    Binding body{owner,character,bodyParts,0,0,bodyModel,true};
    if(bodyModel&&groupIndex(body,bodyName)>=0&&groupIndex(body,armName)>=0)conceal(body);
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
        if(model&&groupIndex(head,headName)>=0&&groupIndex(head,bodyName)<0&&groupIndex(head,armName)<0)conceal(head);
    }
}
}
