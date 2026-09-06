#pragma once
#include <cstdint>
#include "input_bridge.hpp"
namespace mgs5vr {
void installControllerRig(uintptr_t imageBase);
void observeControllerRigOwner(uintptr_t owner) noexcept;
void stopControllerRig() noexcept;
bool controllerRigEnabled() noexcept;
TravelMode nativeTravelMode() noexcept;
}
