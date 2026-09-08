#pragma once
#include <cstdint>
#include "input_bridge.hpp"
namespace mgs5vr {
bool controllerThrowReady() noexcept;
void installControllerRig(uintptr_t imageBase);
void observeControllerRigOwner(uintptr_t owner) noexcept;
void stopControllerRig() noexcept;
bool controllerRigEnabled() noexcept;
TravelMode nativeTravelMode() noexcept;
}
