#include "mgs5vr/player_visibility.hpp"
#include "mgs5vr/log.hpp"
#include <windows.h>
#include <array>
#include <algorithm>

namespace {
uintptr_t base{};
using GroupFn=void(*)(void*,const uint64_t*);
GroupFn hideGroup{},showGroup{};
constexpr uint32_t headName=0xa9e88501,bodyName=0x1a166b34,armName=0x4e74fd8c;
constexpr uint64_t bodyHash=0xff131a166b34;
template<class T> bool read(uintptr_t address,T& value){
    SIZE_T copied{};
    return address&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),&value,sizeof(value),&copied)&&copied==sizeof(value);
}
template<class T> T get(uintptr_t address){T value{};read(address,value);return value;}
bool mask(uintptr_t model,uint32_t value){
    SIZE_T copied{};return WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(model+0x1a4),&value,sizeof(value),&copied)&&copied==sizeof(value);
}
struct Binding {
    uintptr_t owner{},character{},parts{},record{},renderer{},model{};
    uint32_t previousMask{};
    bool body{};
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
uint8_t bodyFlags(const Binding& b){
    const auto index=groupIndex(b,bodyName);const auto data=get<uintptr_t>(b.model+0x170);
    return index>=0&&data?get<uint8_t>(data+static_cast<uintptr_t>(index)):0;
}
void restore(Binding& b){
    if(b.model&&owned(b)){
        if(b.body){
            if(showGroup&&bodyFlags(b)==3)showGroup(reinterpret_cast<void*>(b.model),&bodyHash);
        }else if(get<uintptr_t>(b.model)==base+0x20f4d90&&get<uint32_t>(b.model+0x1a4)==0xffffffff){
            mask(b.model,b.previousMask);
        }
    }
    b={};
}
void conceal(Binding candidate){
    if(!owned(candidate))return;
    auto slot=std::find_if(hidden.begin(),hidden.end(),[&](const auto& b){return b.model==candidate.model&&b.owner==candidate.owner;});
    if(slot==hidden.end()){
        slot=std::find_if(hidden.begin(),hidden.end(),[](const auto& b){return !b.model;});
        if(slot==hidden.end())return;
        if(candidate.body){if(!hideGroup||bodyFlags(candidate)!=15)return;}
        else if(!read(candidate.model+0x1a4,candidate.previousMask))return;
        *slot=candidate;
        mgs5vr::log(candidate.body?"First-person player body group hidden; arm groups retained":"First-person player head model hidden");
    }
    if(candidate.body){
        if(bodyFlags(candidate)==15)hideGroup(reinterpret_cast<void*>(candidate.model),&bodyHash);
    }else{
        uint32_t current{};if(!read(candidate.model+0x1a4,current))return;
        if(current!=0xffffffff)slot->previousMask=current;
        mask(candidate.model,0xffffffff);
    }
}
}
namespace mgs5vr {
void initializePlayerVisibility(uintptr_t moduleBase) noexcept {
    base=moduleBase;
    constexpr std::array<unsigned char,22> hideSignature{0x40,0x53,0x48,0x83,0xec,0x20,0x4c,0x8b,0x02,0x48,0x8b,0xd9,0x0f,0xb7,0x89,0xe8,0x01,0,0,0x33,0xc0,0x85};
    constexpr std::array<unsigned char,22> showSignature{0x48,0x83,0xec,0x28,0x4c,0x8b,0x0a,0x33,0xd2,0x4c,0x8b,0xd1,0x0f,0xb7,0x89,0xe8,0x01,0,0,0x8b,0xc2,0x85};
    std::array<unsigned char,22> bytes{};
    if(read(base+0x1cca90,bytes)&&bytes==hideSignature&&read(base+0x1ceb80,bytes)&&bytes==showSignature){
        hideGroup=reinterpret_cast<GroupFn>(base+0x1cca90);showGroup=reinterpret_cast<GroupFn>(base+0x1ceb80);
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
    Binding body{owner,character,bodyParts,0,0,bodyModel,0,true};
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
