#pragma once
#include <cstdint>

namespace mgs5vr {
void initializePlayerVisibility(uintptr_t moduleBase) noexcept;
// Called on the native player camera publication thread, after the player has
// updated its appearance. Only verified models owned by this player are changed.
void updatePlayerVisibility(uintptr_t cameraOwner,bool firstPerson) noexcept;
}
