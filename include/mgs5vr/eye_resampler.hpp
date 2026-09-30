#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <vector>

namespace mgs5vr {
// Used only on the private XR device/context. Source pixels are display-encoded;
// filter in linear light and retain each source eye's complete angular canvas.
class EyeResampler {
public:
    void copy(ID3D11DeviceContext* context,ID3D11Texture2D* source,unsigned slice,ID3D11Texture2D* destination);
    void reset(){*this=EyeResampler{};}
private:
    template<class T>using Ptr=Microsoft::WRL::ComPtr<T>;
    Ptr<ID3D11Device> device_;
    Ptr<ID3D11VertexShader> vertex_;
    Ptr<ID3D11PixelShader> linear_,encoded_;
    Ptr<ID3D11SamplerState> sampler_;
    Ptr<ID3D11RasterizerState> raster_;
    Ptr<ID3D11DepthStencilState> depth_;
    Ptr<ID3D11Texture2D> source_;
    Ptr<ID3D11ShaderResourceView> image_;
    D3D11_TEXTURE2D_DESC sourceDesc_{};
    struct Target {Ptr<ID3D11Texture2D> image;Ptr<ID3D11RenderTargetView> view;bool encoded{};};
    std::vector<Target> targets_;
    void initialize(ID3D11Device* device);
};
}
