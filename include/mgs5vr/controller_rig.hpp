#pragma once
#include <cstdint>
#include "head_camera.hpp"
#include "input_bridge.hpp"
namespace mgs5vr {
bool controllerThrowReady() noexcept;
void installControllerRig(uintptr_t imageBase);
void observeControllerRigOwner(uintptr_t owner) noexcept;
void stopControllerRig() noexcept;
bool controllerRigEnabled() noexcept;
TravelMode nativeTravelMode() noexcept;

// Retain the sampled actor envelope; move the cabin camera against native
// swept clearance and floor queries. No player or actor transform is written.
void publishNativeCabinBounds(HeadCameraSample& frame) noexcept;
}
