#pragma once
#include "core.hpp"
#include <cstdint>
namespace mgs5vr {
// FOX actor tags identify a GameObject (kind 1), with the type in ID bits 9..15.
// These are the owned 1.0.15.4 TppGameObject registration indices.
constexpr bool protectedMotionMeleeTarget(uint64_t tag){
    if(!(tag&(uint64_t{1}<<63))||((tag>>59)&15)!=1)return false;
    const auto type=static_cast<uint16_t>(tag)>>9;
    // Companion actors and non-hostile wildlife, including the rat/critter pool.
    // This changes only automatic VR swings; explicit native actions retain
    // their game rules.
    return type==13||type==18||type==19||type==20||(type>=28&&type<=34)
        ||type==39||type==40||type==41||type==42;
}
struct MeleeSweep {
    uintptr_t owner{},character{};
    Vec3 start{},end{};
    uint64_t time{},activation{};
    unsigned hand{};
    bool started{};
};
void installMotionMelee(uintptr_t imageBase);
void publishMeleeSweep(const MeleeSweep& sweep);
void consumeMeleeSweep();
void stopMotionMelee() noexcept;
}
