#pragma once
#include <d3d11.h>
namespace mgs5vr {
void observeCabinCoverBuffer(ID3D11Device*,const D3D11_BUFFER_DESC*,const D3D11_SUBRESOURCE_DATA*,ID3D11Buffer*) noexcept;
void setCabinCoverOpening(float) noexcept;
ID3D11Buffer* cabinCoverBuffer(ID3D11Buffer*) noexcept;
}
