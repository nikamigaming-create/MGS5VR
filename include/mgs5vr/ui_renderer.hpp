#pragma once
#include "stereo.hpp"
#include <array>
#include <cstdint>
#include <iosfwd>

namespace mgs5vr {
void installUiRenderer(uintptr_t moduleBase);
// Producer scope is the native scene invocation; the UI may execute on a worker.
void setUiRenderSource(const EyeFrame& eye,uintptr_t camera,const std::array<float,16>& view);
void clearUiRenderSource() noexcept;
// Called only at the verified native UI perspective-builder return address.
bool applyUiEyeProjection(float* output) noexcept;
void reportUiRenderer(std::ostream& output);
void stopUiRenderer() noexcept;
}
