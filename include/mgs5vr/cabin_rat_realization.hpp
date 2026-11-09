#pragma once
#include "room_bounds.hpp"

namespace mgs5vr {
// Native logical positions describe the simulation center, while the skin
// root is at the feet. Preserve their current offset when carrying/releasing.
inline Vec3 translatedRatLogicalPosition(Vec3 logical,Vec3 root,Vec3 targetRoot) noexcept {
    return logical+(targetRoot-root);
}
// The six center rays sample a floor/ceiling context, not the complete cabin
// footprint. Native walking rats can legitimately pass an X/Z sample edge;
// using those edges as actor visibility/carry limits strands them there.
// Their native AI and collision retain horizontal ownership. Acquisition and
// lowering independently require the native floorAt query at the actor/hand.
inline bool cabinRatContactContains(const NativeRoomBounds& bounds,Vec3 world) noexcept {
    if(!validNativeRoomBounds(bounds)||!valid(Pose{{},world}))return false;
    const auto local=nativeRoomLocal(bounds,world);
    constexpr float ceilingMargin=.035f,floorContactTolerance=.01f;
    return local.y>=bounds.min.y-floorContactTolerance&&local.y<=bounds.max.y-ceilingMargin;
}
struct CabinRatCandidate {
    uint32_t locator{},route{};
    Vec3 position{};
    float nativeDistanceSquared{};
    bool enabled{},nativeEligible{},outsideNativeGrid{};
};
struct CabinRatSelectionContext {
    bool freshFrame{},nativeCabin{},nativeTitle{},frontEnd{},cabinPlay{};
    bool openingBackend{},openingSelector{},openingNativeMenu{},loading{},avatarEditor{};
    bool authoredCamera{},scriptedDemo{},nativeGamepad{},nativeStopped{},ownedSelector{};
    uint32_t nativeCount{},logicalCount{},capacity{};
    NativeRoomBounds bounds{};
    Vec3 head{};
};
struct CabinRatSelection {std::array<uint16_t,2> indices{};uint32_t count{};};
// The helicopter has collision geometry but no field streaming cells. Admit
// only our two authored locators when that grid excluded them. Native enable,
// NPC eligibility and nearest-first ranking still belong to the game.
inline std::optional<CabinRatSelection> cabinRatSelection(
        const CabinRatSelectionContext& c,const std::array<CabinRatCandidate,2>& rats) noexcept {
    const bool title=c.nativeTitle&&c.frontEnd&&c.openingBackend&&c.openingSelector&&!c.openingNativeMenu;
    const bool cabin=c.nativeCabin&&c.cabinPlay&&!c.frontEnd;
    if(!c.freshFrame||!c.ownedSelector||(!title&&!cabin)||c.loading||c.avatarEditor
        ||c.authoredCamera||c.scriptedDemo||c.nativeGamepad||c.nativeStopped
        ||c.nativeCount||c.logicalCount!=2||c.capacity!=2||!validNativeRoomBounds(c.bounds)
        ||!valid(Pose{{},c.head}))return {};
    // Native StrCode32 of mgs5vr_cabin_rat_00/01 and their rt_ routes.
    constexpr std::array<uint32_t,2> names{0x4917765c,0xcef22f3b};
    constexpr std::array<uint32_t,2> routes{0xfb33a487,0x86358b80};
    CabinRatSelection result;
    for(size_t i=0;i<rats.size();++i){
        const auto& rat=rats[i];
        if(rat.locator!=names[i]||rat.route!=routes[i])return {};
        if(!rat.enabled||!rat.nativeEligible||!rat.outsideNativeGrid
            ||!valid(Pose{{},rat.position})||!std::isfinite(rat.nativeDistanceSquared)||rat.nativeDistanceSquared<0)continue;
        const auto local=nativeRoomLocal(c.bounds,rat.position);
        if(local.y<c.bounds.min.y||local.y>c.bounds.max.y)continue;
        const auto distance=rat.position-c.head;
        // Additional cabin-only bound, not an inferred native distance cutoff.
        if(dot(distance,distance)>36.f)continue;
        result.indices[result.count++]=static_cast<uint16_t>(i);
    }
    if(!result.count)return {};
    if(result.count==2&&rats[1].nativeDistanceSquared<rats[0].nativeDistanceSquared)result.indices={1,0};
    return result;
}
}
