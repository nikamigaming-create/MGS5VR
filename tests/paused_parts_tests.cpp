#include "mgs5vr/paused_parts.hpp"
#include <array>
#include <cmath>
#include <stdexcept>

int main(){
    using namespace mgs5vr;
    const auto require=[](bool ok){if(!ok)throw std::runtime_error("paused modular arm contract failed");};
    const std::array<uint32_t,2> names{10,20};
    const std::array<uint32_t,4> part{10,20,30,40};
    const std::array<int32_t,4> parents{-1,0,1,2};
    const std::array<Pose,4> frozen{{{{0,0,0,1},{0,0,0}},{{0,0,0,1},{1,0,0}},
                                    {{0,0,0,1},{1,1,0}},{{0,0,0,1},{1,2,0}}}};
    // A translated forearm and a 90-degree wrist turn carry mechanical
    // descendants rigidly; they must not remain at the old wrist or compound.
    const float s=std::sqrt(.5f);
    const std::array<Pose,2> source{{{{0,0,0,1},{2,0,0}},{{0,0,s,s},{3,0,0}}}};
    std::array<Pose,4> output{};
    require(retargetPausedPart(names,source,part,frozen,parents,output));
    require(std::abs(output[2].position.x-2)<.0001f&&std::abs(output[3].position.x-1)<.0001f);
    require(std::abs(output[3].position.y)<.0001f);
    const auto previous=output;
    for(int i=0;i<1000;++i)require(retargetPausedPart(names,source,part,frozen,parents,output));
    require(output[3].position.x==previous[3].position.x);
    auto invalid=parents;invalid[2]=2;
    require(!retargetPausedPart(names,source,part,frozen,invalid,output));
    invalid=parents;invalid[2]=-1;
    require(!retargetPausedPart(names,source,part,frozen,invalid,output));
    auto duplicate=part;duplicate[2]=20;
    require(!retargetPausedPart(names,source,duplicate,frozen,parents,output));
    const std::array<uint32_t,2> duplicateSource{10,10};
    require(!retargetPausedPart(duplicateSource,source,part,frozen,parents,output));
    const std::array<uint32_t,3> withHiddenMount{10,20,99};
    const Pose collapsed{{0,0,0,0},{}};
    const std::array<Pose,3> withUnusedCollapse{source[0],source[1],collapsed};
    require(retargetPausedPart(withHiddenMount,withUnusedCollapse,part,frozen,parents,output));
    const std::array<Pose,3> withSharedCollapse{source[0],collapsed,source[1]};
    require(!retargetPausedPart(withHiddenMount,withSharedCollapse,part,frozen,parents,output));
}
