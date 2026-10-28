#pragma once
#include "gz_player.hpp"
namespace mgs5vr {
struct GzInputState { bool enabled{},player{};std::optional<GzMenuState> menu;std::optional<bool> basicAction; };
GzInputState gzInputState() noexcept;
void installGzRig(uintptr_t base);
void observeGzRigCamera(uintptr_t identity,const std::optional<GzPlayerHead>& player);
void stopGzRig() noexcept;
}
