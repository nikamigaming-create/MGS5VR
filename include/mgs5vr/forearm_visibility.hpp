#pragma once
#include "head_camera.hpp"
#include <d3d11.h>
#include <wrl/client.h>

namespace mgs5vr {
// Only the player's first-person scene can select the cropped native indices.
// Native skinning, materials, hand sockets and shadow geometry stay native.
void resetForearmBindings(ID3D11DeviceContext*) noexcept;
void requestEyepatchVisibility(bool visible) noexcept;
void setForearmPlayer(uintptr_t owner,uintptr_t model) noexcept;
void setForearmPlayer(uintptr_t owner,uintptr_t model,bool groundZeroes) noexcept;
void bindForearmIndex(ID3D11DeviceContext*,ID3D11Buffer*,DXGI_FORMAT,UINT) noexcept;
void bindForearmVertices(ID3D11DeviceContext*,UINT,UINT,ID3D11Buffer* const*,const UINT*,const UINT*) noexcept;
void bindForearmConstants(ID3D11DeviceContext*,UINT,UINT,ID3D11Buffer* const*) noexcept;
void bindForearmTargets(ID3D11DeviceContext*,UINT,ID3D11RenderTargetView* const*) noexcept;
void observeForearmBuffer(ID3D11Device*,const D3D11_BUFFER_DESC*,const D3D11_SUBRESOURCE_DATA*,ID3D11Buffer*) noexcept;
void mapForearmConstants(ID3D11DeviceContext*,ID3D11Resource*,UINT,D3D11_MAP,const D3D11_MAPPED_SUBRESOURCE*) noexcept;
void unmapForearmConstants(ID3D11DeviceContext*,ID3D11Resource*,UINT) noexcept;
void updateForearmConstants(ID3D11DeviceContext*,ID3D11Resource*,UINT,const D3D11_BOX*,const void*) noexcept;
class ForearmDraw {
public:
    explicit ForearmDraw(ID3D11DeviceContext*) noexcept;
    ~ForearmDraw();
private:
    ID3D11DeviceContext* context_{};
    Microsoft::WRL::ComPtr<ID3D11Buffer> original_;
    struct VertexBinding {
        Microsoft::WRL::ComPtr<ID3D11Buffer> original;
        UINT slot{},stride{},offset{};
    };
    std::array<VertexBinding,32> vertices_;
    size_t vertexCount_{};
    DXGI_FORMAT format_{};
    UINT offset_{};
};
}
