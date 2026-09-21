#pragma once
#include <cstdint>
namespace mgs5vr {
// Exact-build graphics-option adapter. Does not change simulation delta time.
bool enableNativeFrameRate(uintptr_t moduleBase) noexcept;
bool nativeFrameRateEnabled() noexcept;
// Bounded producer cadence from the runtime's actual display period. Zero or
// invalid values restore the 120 Hz fallback; this never changes game time.
int64_t nativeProducerIntervalNs(int64_t displayPeriodNs) noexcept;
void reportConsumerDisplayPeriod(int64_t displayPeriodNs) noexcept;
void paceNativePresent() noexcept;
void recordNativePresent(double captureMs,double pacingMs,double presentMs) noexcept;
void stopNativePerformance() noexcept;
}
