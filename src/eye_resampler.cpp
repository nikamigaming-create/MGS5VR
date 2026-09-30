#include "mgs5vr/eye_resampler.hpp"
#include "mgs5vr/mailbox.hpp"
#include <d3dcompiler.h>
#include <algorithm>
#include <stdexcept>

namespace mgs5vr {
namespace {
constexpr char shader[]=R"(
struct V {float4 p:SV_POSITION;float2 uv:TEXCOORD0;};
V vertex(uint id:SV_VertexID){
    V o;o.uv=float2((id<<1)&2,id&2);
    o.p=float4(o.uv*float2(2,-2)+float2(-1,1),0,1);return o;
}
Texture2D image:register(t0);SamplerState linearClamp:register(s0);
float4 pixelLinear(V v):SV_TARGET{return image.SampleLevel(linearClamp,v.uv,0);}
float4 pixelEncoded(V v):SV_TARGET{
    float4 c=image.SampleLevel(linearClamp,v.uv,0);
    c.rgb=c.rgb<=0.0031308?c.rgb*12.92:1.055*pow(max(c.rgb,0),1.0/2.4)-0.055;return c;
}
)";
bool bgra(DXGI_FORMAT format){return format==DXGI_FORMAT_B8G8R8A8_UNORM||format==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB||format==DXGI_FORMAT_B8G8R8A8_TYPELESS;}
bool rgba(DXGI_FORMAT format){return format==DXGI_FORMAT_R8G8B8A8_UNORM||format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB||format==DXGI_FORMAT_R8G8B8A8_TYPELESS;}
}
void EyeResampler::initialize(ID3D11Device* device){
    reset();device_=device;
    const auto compile=[](const char* entry,const char* profile){
        Ptr<ID3DBlob> code,errors;
        checkHr(D3DCompile(shader,sizeof(shader),nullptr,nullptr,nullptr,entry,profile,
            D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&errors),"Compile XR eye resampler");return code;
    };
    const auto vertex=compile("vertex","vs_4_0"),linear=compile("pixelLinear","ps_4_0"),encoded=compile("pixelEncoded","ps_4_0");
    checkHr(device->CreateVertexShader(vertex->GetBufferPointer(),vertex->GetBufferSize(),nullptr,&vertex_),"Create XR resampler vertex shader");
    checkHr(device->CreatePixelShader(linear->GetBufferPointer(),linear->GetBufferSize(),nullptr,&linear_),"Create XR linear resampler");
    checkHr(device->CreatePixelShader(encoded->GetBufferPointer(),encoded->GetBufferSize(),nullptr,&encoded_),"Create XR encoded resampler");
    D3D11_SAMPLER_DESC sampler{};sampler.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sampler.MaxLOD=D3D11_FLOAT32_MAX;
    checkHr(device->CreateSamplerState(&sampler,&sampler_),"Create XR resampler filter");
    D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;raster.CullMode=D3D11_CULL_NONE;raster.DepthClipEnable=TRUE;
    checkHr(device->CreateRasterizerState(&raster,&raster_),"Create XR resampler rasterizer");
    D3D11_DEPTH_STENCIL_DESC depth{};depth.DepthFunc=D3D11_COMPARISON_ALWAYS;
    checkHr(device->CreateDepthStencilState(&depth,&depth_),"Create XR resampler depth state");
}
void EyeResampler::copy(ID3D11DeviceContext* context,ID3D11Texture2D* source,unsigned slice,ID3D11Texture2D* destination){
    if(!context||!source||!destination||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)
        throw std::invalid_argument("XR resampling needs complete images and an immediate private context");
    Ptr<ID3D11Device> device,sourceDevice,targetDevice;context->GetDevice(&device);source->GetDevice(&sourceDevice);destination->GetDevice(&targetDevice);
    if(device.Get()!=sourceDevice.Get()||device.Get()!=targetDevice.Get())throw std::invalid_argument("XR resampling images belong to another device");
    D3D11_TEXTURE2D_DESC desc{},target{};source->GetDesc(&desc);destination->GetDesc(&target);
    if(slice>=desc.ArraySize||desc.SampleDesc.Count!=1||target.SampleDesc.Count!=1||target.ArraySize!=1
        ||!desc.Width||!desc.Height||!target.Width||!target.Height
        ||!(target.BindFlags&D3D11_BIND_RENDER_TARGET)||(!rgba(desc.Format)&&!bgra(desc.Format))
        ||(bgra(desc.Format)?!bgra(target.Format):!rgba(target.Format)))
        throw std::invalid_argument("Unsupported XR resampling image format or slice");
    if(device_.Get()!=device.Get())initialize(device.Get());
    if(!source_||desc.Width!=sourceDesc_.Width||desc.Height!=sourceDesc_.Height||desc.Format!=sourceDesc_.Format){
        auto copy=desc;copy.MipLevels=copy.ArraySize=1;copy.SampleDesc={1,0};copy.Usage=D3D11_USAGE_DEFAULT;
        copy.CPUAccessFlags=copy.MiscFlags=0;copy.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        copy.Format=bgra(desc.Format)?DXGI_FORMAT_B8G8R8A8_TYPELESS:DXGI_FORMAT_R8G8B8A8_TYPELESS;
        checkHr(device->CreateTexture2D(&copy,nullptr,source_.ReleaseAndGetAddressOf()),"Create XR resampling source");
        D3D11_SHADER_RESOURCE_VIEW_DESC view{};view.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;view.Texture2D.MipLevels=1;
        view.Format=bgra(desc.Format)?DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        checkHr(device->CreateShaderResourceView(source_.Get(),&view,image_.ReleaseAndGetAddressOf()),"Decode XR source pixels");sourceDesc_=desc;
    }
    auto found=std::find_if(targets_.begin(),targets_.end(),[&](const auto& t){return t.image.Get()==destination;});
    if(found==targets_.end()){
        if(targets_.size()>=16)targets_.clear();
        D3D11_RENDER_TARGET_VIEW_DESC view{};view.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2D;view.Format=target.Format;
        if(view.Format==DXGI_FORMAT_R8G8B8A8_TYPELESS)view.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        if(view.Format==DXGI_FORMAT_B8G8R8A8_TYPELESS)view.Format=DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        Target output;output.image=destination;output.encoded=view.Format==DXGI_FORMAT_R8G8B8A8_UNORM||view.Format==DXGI_FORMAT_B8G8R8A8_UNORM;
        checkHr(device->CreateRenderTargetView(destination,&view,&output.view),"Create XR resampling output");
        targets_.push_back(std::move(output));found=targets_.end()-1;
    }
    context->CopySubresourceRegion(source_.Get(),0,0,0,0,source,D3D11CalcSubresource(0,slice,desc.MipLevels),nullptr);
    auto* output=found->view.Get();context->OMSetRenderTargets(1,&output,nullptr);
    context->OMSetDepthStencilState(depth_.Get(),0);context->OMSetBlendState(nullptr,nullptr,0xffffffffu);
    const D3D11_VIEWPORT viewport{0,0,static_cast<float>(target.Width),static_cast<float>(target.Height),0,1};
    context->RSSetViewports(1,&viewport);context->RSSetState(raster_.Get());
    context->IASetInputLayout(nullptr);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vertex_.Get(),nullptr,0);context->GSSetShader(nullptr,nullptr,0);
    context->PSSetShader(found->encoded?encoded_.Get():linear_.Get(),nullptr,0);
    auto* image=image_.Get();auto* sampler=sampler_.Get();context->PSSetShaderResources(0,1,&image);context->PSSetSamplers(0,1,&sampler);
    context->Draw(3,0);
    image=nullptr;context->PSSetShaderResources(0,1,&image);context->OMSetRenderTargets(0,nullptr,nullptr);
}
}
