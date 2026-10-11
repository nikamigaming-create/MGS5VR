#pragma once
#include <cstdint>

namespace mgs5vr {
enum class WeaponObstructionMode { native, trackedBarrel, ignoreProximity };
struct WeaponObstructionWitness {
    uint64_t sampleTime{},frameTime{},activation{},player{};
    int32_t nativeHits{},appliedHits{};
    WeaponObstructionMode mode{WeaponObstructionMode::native};
};
WeaponObstructionWitness weaponObstructionWitness();

struct WeaponObstructionOwner {
    uintptr_t player{},component{},state{},physics{};
    uint32_t playerIndex{},resource{};
    bool operator==(const WeaponObstructionOwner&) const = default;
};

struct WeaponObstructionSample {
    WeaponObstructionOwner published{},current{};
    uint64_t frameTime{},weaponTime{};
    bool exactCaller{},enabled{},verified{},ready{},tracked{},nativeControls{};
    bool vehicleAllowed{true};
};

// The native adapter verifies object types and the character/model chain before
// publishing this identity. Neither an old rig nor another physics query may
// inherit the local player's cover preference.
inline WeaponObstructionMode weaponObstructionMode(const WeaponObstructionSample& sample,
        uint64_t now,bool blockFiringNearCover,bool trackedBarrel) noexcept {
    const auto& owner=sample.published;
    if(!sample.exactCaller||!sample.enabled||!sample.verified||!sample.ready||!sample.tracked
       ||sample.nativeControls||!sample.vehicleAllowed||!owner.player||!owner.component
       ||!owner.state||!owner.physics||!owner.resource||(owner.resource>>16)==0xffff
       ||owner!=sample.current||!sample.frameTime||!sample.weaponTime
       ||now<sample.frameTime||now-sample.frameTime>100
       ||now<sample.weaponTime||now-sample.weaponTime>100)
        return WeaponObstructionMode::native;
    if(!blockFiringNearCover)return WeaponObstructionMode::ignoreProximity;
    return trackedBarrel?WeaponObstructionMode::trackedBarrel:WeaponObstructionMode::native;
}

// Called only after the native consumer has waited for/released its query job.
// Zero and negative native results retain their exact meaning.
inline int32_t weaponObstructionHitCount(int32_t nativeCount,WeaponObstructionMode mode) noexcept {
    return nativeCount>0&&mode==WeaponObstructionMode::ignoreProximity?0:nativeCount;
}
}
