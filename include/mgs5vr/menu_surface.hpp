#pragma once
#include "stereo.hpp"
struct ID3D11DeviceContext;
namespace mgs5vr {
bool captureNativeMenuSurface(ID3D11DeviceContext* context,uint64_t source) noexcept;
bool drawNativeMenuSurface(ID3D11DeviceContext* context,
    const std::array<float,16>& eyeView,EyeFov fov,Pose panel) noexcept;
void stopNativeMenuSurface() noexcept;
}
