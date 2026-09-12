#pragma once
#include <cstdint>

namespace mgs5vr {
struct HeadCameraSample;
void initializePlayerVisibility(uintptr_t moduleBase) noexcept;
void publishHandFade(const HeadCameraSample& frame) noexcept;
// Called on the native player camera publication thread, after the player has
// updated its appearance. Only verified models owned by this player are changed.
void updatePlayerVisibility(uintptr_t cameraOwner,bool firstPerson) noexcept;
}
