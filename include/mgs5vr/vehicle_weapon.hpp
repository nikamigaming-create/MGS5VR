#pragma once
#include <cstdint>

namespace mgs5vr {
// Native acknowledgement, not the requested grip state. The native input masks,
// weapon availability, action state, ammunition and cadence remain authoritative.
struct VehicleWeaponState {
    uint64_t time{},activation{};
    uintptr_t player{};
    bool requested{},ready{},nativeEligible{},inputMasked{};
    uint32_t semanticHeld{},inputMask{},weaponFlags{},renderFlags{},renderPoseFlags{},renderActionFlags{};
    uint16_t nativeActionId{};
    uint8_t nativePosture{},nativeAction{},equippedSlot{},requestedSlot{};
};
void installVehicleWeaponInput(uintptr_t imageBase);
void stopVehicleWeaponInput() noexcept;
bool vehicleWeaponRoutingAvailable() noexcept;
// Called for every XR input publication, including false on lost ownership.
// Fire is the physical request before readiness filtering, so a trigger held
// while acquiring the gun cannot silently become a new attack.
void publishVehicleWeaponInput(bool requested,bool fire,bool reload,uint64_t time,uint64_t activation);
VehicleWeaponState vehicleWeaponState();
}
