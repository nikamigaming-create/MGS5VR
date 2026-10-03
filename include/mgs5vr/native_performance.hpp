#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
namespace mgs5vr {
// Fixed storage for bounded diagnostics. Percentiles are histogram upper
// bounds, never reconstructed global percentiles from multiple log windows.
struct FrameTimingHistogram {
    static constexpr double binMilliseconds=.25;
    std::array<uint64_t,1025> bins{};
    uint64_t samples{},overBudget{};
    double totalMs{},maximumMs{};
    bool add(int64_t durationNs,int64_t displayPeriodNs) noexcept;
    double percentileUpperBound(double fraction) const noexcept;
};
struct XrSubmissionTiming {
    FrameTimingHistogram layerPrepare,endFrame;
    uint64_t samples{},budgetSamples{},noLayers{},projectionOnly{},quadOnly{},
        projectionAndQuad{},otherLayers{},retainedProjection{};
    int64_t minimumBudgetNs{},maximumBudgetNs{};
    // Layer counts describe the actual XrFrameEndInfo, not a desired mode.
    bool record(int64_t layerPrepareNs,int64_t endFrameNs,int64_t displayPeriodNs,
        uint32_t layerCount,bool projection,bool quad,bool retained) noexcept;
    std::string summary() const;
};
// Exact-build graphics-option adapter. Does not change simulation delta time.
bool enableNativeFrameRate(uintptr_t moduleBase) noexcept;
bool nativeFrameRateEnabled() noexcept;
// Bounded producer cadence from the runtime's actual display period. Zero or
// invalid values restore the 120 Hz fallback; this never changes game time.
enum class NativeProducerPacing { adaptiveMargin, displayPeriod };
int64_t nativeProducerIntervalNs(int64_t displayPeriodNs,
    NativeProducerPacing mode=NativeProducerPacing::adaptiveMargin) noexcept;
// Test-only per-process lease. Invalid/unowned/unbounded requests return zero;
// the normal producer margin is restored at expiry and on shutdown.
uint64_t nativeProducerDiagnosticExpiryMs(std::wstring_view mode,std::wstring_view seconds,
    bool ownedRuntime,uint64_t nowMs) noexcept;
bool nativeProducerDiagnosticActive(uint64_t expiryMs,uint64_t nowMs) noexcept;
void reportConsumerDisplayPeriod(int64_t displayPeriodNs) noexcept;
void paceNativePresent() noexcept;
void recordNativePresent(double captureMs,double pacingMs,double presentMs) noexcept;
void stopNativePerformance() noexcept;
}
