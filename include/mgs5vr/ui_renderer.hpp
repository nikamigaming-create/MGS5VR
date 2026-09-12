#pragma once
#include "stereo.hpp"
#include "head_camera.hpp"
#include <array>
#include <cstdint>
#include <iosfwd>

namespace mgs5vr {
void installUiRenderer(uintptr_t moduleBase);
// Exact native IsMbDvcTerminalOpened reader; no UI state is written.
std::optional<bool> nativeMenuOpen() noexcept;
bool nativeTitleMenuOpen() noexcept;
bool nativeLoadingTipsOpen() noexcept;
// Source time of the latest expanded native equipment-description draw.
uint64_t nativeEquipmentPickerDrawTime() noexcept;
uint64_t nativeCommandsDrawTime() noexcept;
// Producer scope is the native scene invocation; the UI may execute on a worker.
void setUiRenderSource(const EyeFrame& eye,uintptr_t camera,const std::array<float,16>& view,
                       const HeadCameraSample& rig,const std::array<float,16>& authoredView,
                       const std::array<float,16>& authoredProjection);
void clearUiRenderSource() noexcept;
// Called only at the verified native UI perspective-builder return address.
bool applyUiEyeProjection(float* output) noexcept;
void reportUiRenderer(std::ostream& output);
void stopUiRenderer() noexcept;
}
