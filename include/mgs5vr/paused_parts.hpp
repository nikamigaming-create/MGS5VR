#pragma once
#include "core.hpp"
#include <span>
#include <algorithm>

namespace mgs5vr {
inline bool pausedArmPartMatches(uint64_t resource,std::span<const uint32_t> names,
                                std::span<const int32_t> parents) noexcept {
    if(names.empty()||names.size()!=parents.size())return false;
    constexpr uint32_t forearm=0x31e2de16,wrist=0x3e2d77e5;
    const auto has=[&](uint32_t name){return std::find(names.begin(),names.end(),name)!=names.end();};
    if(has(forearm)&&has(wrist))return true;
    // The owned prosthetic also has a separate rigid shell. It has only
    // LFOREARM, so requiring LHAND leaves it at its pre-Help pose while the
    // articulated hand moves. Admit this observed resource/rig exactly.
    return resource==0x84a051ef13b1e379ull&&names.size()==1&&names[0]==forearm&&parents[0]==-1;
}
// Native modular arm models share named joints with the body and add their
// own mechanical joints. Re-publish shared joints and retain the frozen local
// transforms of those extra joints. All poses are in the same model space.
inline bool retargetPausedPart(std::span<const uint32_t> sourceNames,
    std::span<const Pose> source,std::span<const uint32_t> partNames,
    std::span<const Pose> frozen,std::span<const int32_t> parents,
    std::span<Pose> output) noexcept {
    const auto count=partNames.size();
    if(source.empty()||source.size()!=sourceNames.size()||!count||count>512
       ||frozen.size()!=count||parents.size()!=count||output.size()!=count)return false;
    for(size_t i=0;i<sourceNames.size();++i)
        if(!sourceNames[i]
           ||std::find(sourceNames.begin(),sourceNames.begin()+i,sourceNames[i])!=sourceNames.begin()+i)return false;
    for(size_t i=0;i<count;++i){
        if(!partNames[i]||!valid(frozen[i])||parents[i]<-1||parents[i]>=static_cast<int32_t>(i)
           ||std::find(partNames.begin(),partNames.begin()+i,partNames[i])!=partNames.begin()+i)return false;
        const auto found=std::find(sourceNames.begin(),sourceNames.end(),partNames[i]);
        if(found!=sourceNames.end()){
            // Hidden weapon mounts can deliberately have collapsed matrices.
            // Only joints referenced by this part need a rigid source pose.
            const auto pose=source[static_cast<size_t>(found-sourceNames.begin())];
            if(!valid(pose))return false;
            output[i]=pose;
        }
        else {
            if(parents[i]<0)return false;
            const auto parent=static_cast<size_t>(parents[i]);
            output[i]=compose(output[parent],compose(inverse(frozen[parent]),frozen[i]));
        }
        if(!valid(output[i]))return false;
    }
    return true;
}
}
