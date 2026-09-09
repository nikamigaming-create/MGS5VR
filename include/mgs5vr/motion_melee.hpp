#pragma once
#include "core.hpp"
#include <cstdint>
namespace mgs5vr {
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
