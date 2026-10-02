#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <limits>
#include <optional>
#include <ostream>

namespace mgs5vr {
// Independently sampled diagnostic data, not a rendered-frame transaction.
// Current IDs require a proven active native popup owner. The last result is
// a completed response and must never be used as the live selected choice.
struct NativePopupSnapshot {
    bool readerVerified{},ownerVerified{},choiceOwnerVerified{};
    std::optional<bool> active;
    std::optional<uint32_t> numericId;
    std::optional<uint64_t> stringId;
    std::optional<int32_t> lastResult;
    std::optional<uint32_t> currentChoiceIndex;
    std::optional<bool> currentChoiceNegative;
    std::optional<uint64_t> choiceSampleMs;
};
// Leases come only from the verified native popup's own update callback. They
// are bounded hints: every current read revalidates type, activity and identity.
struct NativePopupChoiceLease {uintptr_t owner{};uint64_t sampleMs{};};
using NativePopupChoiceLeases=std::array<NativePopupChoiceLease,4>;

// Share the wire contract between the fresh bot RPC and the periodic renderer
// journal. Unknown readiness/identity stays null; last_result is never focus.
inline void writeNativePopupSnapshotJson(std::ostream& out,const NativePopupSnapshot& popup,uint64_t sampleMs){
    const auto flags=out.flags();
    out<<std::dec<<"{\"reader_verified\":"<<(popup.readerVerified?"true":"false")
       <<",\"owner_verified\":"<<(popup.ownerVerified?"true":"false")
       <<",\"sample_ms\":"<<sampleMs<<",\"coherent_frame\":false,\"active\":";
    if(popup.active)out<<(*popup.active?"true":"false");else out<<"null";
    out<<",\"numeric_id\":";if(popup.numericId)out<<*popup.numericId;else out<<"null";
    out<<",\"string_id\":";
    if(popup.stringId)out<<"\"0x"<<std::hex<<*popup.stringId<<std::dec<<'"';else out<<"null";
    out<<",\"last_result\":";if(popup.lastResult)out<<*popup.lastResult;else out<<"null";
    out<<",\"choice_owner_verified\":"<<(popup.choiceOwnerVerified?"true":"false")
       <<",\"choice_sample_ms\":";if(popup.choiceSampleMs)out<<*popup.choiceSampleMs;else out<<"null";
    out<<",\"current_choice_index\":";if(popup.currentChoiceIndex)out<<*popup.currentChoiceIndex;else out<<"null";
    out<<",\"current_choice_negative\":";
    if(popup.currentChoiceNegative)out<<(*popup.currentChoiceNegative?"true":"false");else out<<"null";
    out<<'}';out.flags(flags);
}

namespace native_popup_detail {
using ChildBytes=std::array<unsigned char,0x23>;
template<class T,size_t N> T value(const std::array<unsigned char,N>& bytes,size_t offset) noexcept {
    T result{};std::memcpy(&result,bytes.data()+offset,sizeof(result));return result;
}
inline std::optional<bool> activeChild(uintptr_t owner,const ChildBytes& bytes) noexcept {
    if(!owner)return false;
    if(!value<uintptr_t>(bytes,0))return {};
    const auto open=bytes[0x20],ready=bytes[0x22];
    if(open>1||ready>1)return {};
    // The retail readiness getter returns false without a layout. Calling it
    // can initialize +0x22, so this observer reads it and never calls it.
    if(!value<uintptr_t>(bytes,0x10)||!open)return false;
    return ready?std::optional<bool>{true}:std::nullopt;
}
inline std::optional<bool> combined(std::optional<bool> first,std::optional<bool> second) noexcept {
    if((first&&*first)||(second&&*second))return true;
    if(first&&second)return false;
    return {};
}
}

// Reader must return true only after copying the complete requested byte range.
// ABI verification belongs to the supported retail adapter. The typed UiSystem
// owns the two popup terminal slots used by IsShowPopup's native implementation.
template<class Reader>
NativePopupSnapshot readNativePopupSnapshot(uintptr_t imageBase,bool abiVerified,Reader&& read,
    const NativePopupChoiceLeases& leases={},uint64_t sampleMs=0) noexcept {
    NativePopupSnapshot result;result.readerVerified=abiVerified;
    if(!abiVerified||!imageBase)return result;
    const auto at=[&](uintptr_t owner,size_t offset,void* output,size_t size){
        if(!owner||offset>std::numeric_limits<uintptr_t>::max()-owner
            ||size>std::numeric_limits<uintptr_t>::max()-owner-offset)return false;
        return read(owner+offset,output,size);
    };
    uintptr_t system{},type{},again{},againType{};
    std::array<uintptr_t,2> children{},againChildren{};
    std::array<native_popup_detail::ChildBytes,2> childBytes{},againChildBytes{};
    std::array<unsigned char,0x28> parameters{},againParameters{};
    if(!at(imageBase,0x2bf1940,&system,sizeof(system))||!system
        ||!at(system,0,&type,sizeof(type))||type!=imageBase+0x22447e8
        ||!at(system,0xe80,children.data(),sizeof(children))
        ||!at(system,0x1510,parameters.data(),parameters.size()))return result;
    for(size_t index=0;index<children.size();++index)
        if(children[index]&&!at(children[index],0,childBytes[index].data(),childBytes[index].size()))return result;
    // Replaced owners or a changing dialog cannot borrow a preceding ID/read.
    if(!at(imageBase,0x2bf1940,&again,sizeof(again))||again!=system
        ||!at(system,0,&againType,sizeof(againType))||againType!=type
        ||!at(system,0xe80,againChildren.data(),sizeof(againChildren))||againChildren!=children
        ||!at(system,0x1510,againParameters.data(),againParameters.size())||againParameters!=parameters)return result;
    for(size_t index=0;index<children.size();++index)
        if(children[index]&&(!at(children[index],0,againChildBytes[index].data(),againChildBytes[index].size())
            ||againChildBytes[index]!=childBytes[index]))return result;
    result.ownerVerified=true;
    result.active=native_popup_detail::combined(native_popup_detail::activeChild(children[0],childBytes[0]),
                                                native_popup_detail::activeChild(children[1],childBytes[1]));
    if(!result.active)return result;
    result.lastResult=native_popup_detail::value<int32_t>(parameters,8);
    if(*result.active){
        result.numericId=native_popup_detail::value<uint32_t>(parameters,0);
        result.stringId=native_popup_detail::value<uint64_t>(parameters,0x20);
        // Native 898500 publishes this exact component; 897C80 binds +E8 to
        // UiSystem. Its state 2 is the input state. 897600 consumes +CC and
        // the selected button's negative flag, independently of last_result.
        unsigned accepted{};
        for(const auto& lease:leases){
            if(!lease.owner||!lease.sampleMs||sampleMs<lease.sampleMs||sampleMs-lease.sampleMs>150)continue;
            uintptr_t choiceType{},againChoiceType{};uint64_t name{},againName{};
            std::array<unsigned char,0x30> choice{},againChoice{};
            if(!at(lease.owner,0,&choiceType,sizeof(choiceType))||choiceType!=imageBase+0x224ba40
                ||!at(lease.owner,0xc0,choice.data(),choice.size())
                ||!at(lease.owner,0x390,&name,sizeof(name))
                ||choice[0]!=1||native_popup_detail::value<uint32_t>(choice,4)!=2
                ||native_popup_detail::value<uintptr_t>(choice,0x28)!=system
                ||native_popup_detail::value<uint32_t>(choice,8)!=*result.numericId||name!=*result.stringId)continue;
            const auto index=native_popup_detail::value<int32_t>(choice,0xc);
            const auto count=native_popup_detail::value<uint32_t>(choice,0x10);
            // A one-button dialog displays physical slot 1, so count is not
            // an upper bound for its index. Two-button dialogs use slots 0/1.
            if(index<0||index>1||!count||count>2||(count==1&&index!=1))continue;
            unsigned char negative{},againNegative{};
            const auto flagOffset=0x3b8+static_cast<size_t>(index)*0x28;
            if(!at(lease.owner,flagOffset,&negative,sizeof(negative))||negative>1
                ||!at(lease.owner,0,&againChoiceType,sizeof(againChoiceType))||againChoiceType!=choiceType
                ||!at(lease.owner,0xc0,againChoice.data(),againChoice.size())||againChoice!=choice
                ||!at(lease.owner,0x390,&againName,sizeof(againName))||againName!=name
                ||!at(lease.owner,flagOffset,&againNegative,sizeof(againNegative))||againNegative!=negative)continue;
            ++accepted;result.currentChoiceIndex=static_cast<uint32_t>(index);
            result.currentChoiceNegative=negative!=0;result.choiceSampleMs=lease.sampleMs;
        }
        // Two simultaneous matching owners are ambiguous; neither may borrow
        // the other's choice. Final parent reads also reject a replaced dialog.
        if(!at(imageBase,0x2bf1940,&again,sizeof(again))||again!=system
            ||!at(system,0,&againType,sizeof(againType))||againType!=type
            ||!at(system,0xe80,againChildren.data(),sizeof(againChildren))||againChildren!=children
            ||!at(system,0x1510,againParameters.data(),againParameters.size())||againParameters!=parameters){
            NativePopupSnapshot unknown;unknown.readerVerified=abiVerified;return unknown;
        }
        for(size_t index=0;index<children.size();++index)
            if(children[index]&&(!at(children[index],0,againChildBytes[index].data(),againChildBytes[index].size())
                ||againChildBytes[index]!=childBytes[index])){
                NativePopupSnapshot unknown;unknown.readerVerified=abiVerified;return unknown;
            }
        result.choiceOwnerVerified=accepted==1;
        if(!result.choiceOwnerVerified){result.currentChoiceIndex.reset();result.currentChoiceNegative.reset();result.choiceSampleMs.reset();}
    }
    return result;
}
}
