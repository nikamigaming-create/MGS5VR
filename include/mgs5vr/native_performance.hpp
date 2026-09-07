#pragma once
#include <cstdint>
namespace mgs5vr {
// Exact-build graphics-option adapter. Does not change simulation delta time.
bool enableNativeFrameRate(uintptr_t moduleBase) noexcept;
bool nativeFrameRateEnabled() noexcept;
void paceNativePresent() noexcept;
void recordNativePresent(double captureMs,double pacingMs,double presentMs) noexcept;
void stopNativePerformance() noexcept;
}
