#pragma once
#include <cstdint>

namespace mgs5vr {
void installNativeCover(uintptr_t imageBase);
void stopNativeCover() noexcept;
}
