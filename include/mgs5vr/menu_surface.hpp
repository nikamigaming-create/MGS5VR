#pragma once
#include "stereo.hpp"
struct ID3D11DeviceContext;
namespace mgs5vr {
bool captureNativeMenuSurface(ID3D11DeviceContext* context,uint64_t source) noexcept;
bool drawNativeMenuSurface(ID3D11DeviceContext* context,
    const std::array<float,16>& eyeView,EyeFov fov,Pose panel,
    bool retainForLoading=false) noexcept;
void stopNativeMenuSurface() noexcept;
enum class MenuSurface { none, device, spatial, fallback };
// An open menu may never borrow an absent device mount. This policy does not
// promote ordinary HUD or a device's outgoing close animation into a menu.
constexpr MenuSurface menuSurface(bool open,bool spatial,bool deviceReady) noexcept {
    if(!open)return MenuSurface::none;
    if(spatial)return MenuSurface::spatial;
    return deviceReady?MenuSurface::device:MenuSurface::fallback;
}
}
