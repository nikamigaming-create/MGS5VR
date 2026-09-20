#pragma once
#include "stereo.hpp"
struct ID3D11Device;
namespace mgs5vr {
using UiClipPlanes=std::array<std::array<float,4>,4>;
// Clip-space half planes of the actual tracked canvas; no head-fit rectangle.
std::optional<UiClipPlanes> uiClipPlanes(const std::array<float,16>& canvas) noexcept;
void installUiClip(ID3D11Device* device);
class UiClipScope {
public:
    explicit UiClipScope(const std::array<float,16>& canvas) noexcept;
    ~UiClipScope();
    UiClipScope(const UiClipScope&)=delete;
    UiClipScope& operator=(const UiClipScope&)=delete;
private:
    std::optional<UiClipPlanes> saved_;
    std::array<float,4> savedBounds_{};
};
}
