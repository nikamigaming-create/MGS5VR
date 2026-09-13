#include "mgs5vr/menu_surface.hpp"
#include "mgs5vr/scene_capture.hpp"
#include "mgs5vr/log.hpp"
#include <d3dcompiler.h>
#include <cstring>
#include <mutex>

namespace mgs5vr {
namespace {
struct Vertex {float position[4],uv[2];};
struct Surface {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11ShaderResourceView> image;
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
    ComPtr<ID3D11InputLayout> layout;
    ComPtr<ID3D11Buffer> vertices;
    ComPtr<ID3D11SamplerState> sampler;
    ComPtr<ID3D11RasterizerState> raster;
    ComPtr<ID3D11DepthStencilState> depth;
    ComPtr<ID3D11BlendState> blend;
    D3D11_TEXTURE2D_DESC description{};
    uint64_t source{},capturedAt{};
    bool reported{};
} surface,categorySurface;
EquipmentLabels categoryLabels{};
std::mutex surfaceMutex;
struct State {
    ID3D11DeviceContext* c;
    ComPtr<ID3D11RenderTargetView> target;
    ComPtr<ID3D11DepthStencilView> depthTarget;
    ComPtr<ID3D11InputLayout> layout;
    ComPtr<ID3D11Buffer> buffer;
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
    ComPtr<ID3D11GeometryShader> gs;
    ComPtr<ID3D11HullShader> hs;
    ComPtr<ID3D11DomainShader> ds;
    ComPtr<ID3D11ShaderResourceView> image;
    ComPtr<ID3D11SamplerState> sampler;
    ComPtr<ID3D11RasterizerState> raster;
    ComPtr<ID3D11DepthStencilState> depth;
    ComPtr<ID3D11BlendState> blend;
    UINT stride{},offset{},stencil{},mask{},count=16;
    FLOAT factor[4]{};
    D3D11_PRIMITIVE_TOPOLOGY topology{};
    D3D11_VIEWPORT viewports[16]{};
    explicit State(ID3D11DeviceContext* context):c(context){
        c->OMGetRenderTargets(1,&target,&depthTarget);c->IAGetInputLayout(&layout);
        c->IAGetVertexBuffers(0,1,&buffer,&stride,&offset);c->IAGetPrimitiveTopology(&topology);
        c->VSGetShader(&vs,nullptr,nullptr);c->PSGetShader(&ps,nullptr,nullptr);
        c->GSGetShader(&gs,nullptr,nullptr);c->HSGetShader(&hs,nullptr,nullptr);c->DSGetShader(&ds,nullptr,nullptr);
        c->PSGetShaderResources(0,1,&image);c->PSGetSamplers(0,1,&sampler);
        c->RSGetState(&raster);c->RSGetViewports(&count,viewports);
        c->OMGetDepthStencilState(&depth,&stencil);c->OMGetBlendState(&blend,factor,&mask);
    }
    ~State(){
        auto* rt=target.Get();c->OMSetRenderTargets(1,&rt,depthTarget.Get());
        auto* vb=buffer.Get();c->IASetVertexBuffers(0,1,&vb,&stride,&offset);c->IASetInputLayout(layout.Get());c->IASetPrimitiveTopology(topology);
        c->VSSetShader(vs.Get(),nullptr,0);c->PSSetShader(ps.Get(),nullptr,0);c->GSSetShader(gs.Get(),nullptr,0);
        c->HSSetShader(hs.Get(),nullptr,0);c->DSSetShader(ds.Get(),nullptr,0);
        auto* srv=image.Get();c->PSSetShaderResources(0,1,&srv);auto* sample=sampler.Get();c->PSSetSamplers(0,1,&sample);
        c->RSSetState(raster.Get());if(count)c->RSSetViewports(count,viewports);
        c->OMSetDepthStencilState(depth.Get(),stencil);c->OMSetBlendState(blend.Get(),factor,mask);
    }
};
bool initialize(ID3D11Device* device,Surface& targetSurface){
    if(targetSurface.device.Get()!=device){targetSurface={};targetSurface.device=device;}
    if(targetSurface.vs)return true;
    constexpr char shader[]=R"(
struct V { float4 p:SV_POSITION; float2 uv:TEXCOORD0; };
V vertex(float4 p:POSITION,float2 uv:TEXCOORD0) { V v;v.p=p;v.uv=uv;return v; }
Texture2D image:register(t0);SamplerState linearClamp:register(s0);
float4 pixel(V v):SV_TARGET { return float4(image.Sample(linearClamp,v.uv).rgb,1); }
)";
    ComPtr<ID3DBlob> vs,ps,errors;
    if(FAILED(D3DCompile(shader,sizeof(shader),nullptr,nullptr,nullptr,"vertex","vs_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&vs,&errors))
        ||FAILED(D3DCompile(shader,sizeof(shader),nullptr,nullptr,nullptr,"pixel","ps_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&ps,&errors)))return false;
    D3D11_INPUT_ELEMENT_DESC input[]={{"POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,16,D3D11_INPUT_PER_VERTEX_DATA,0}};
    D3D11_BUFFER_DESC buffer{};buffer.ByteWidth=6*sizeof(Vertex);buffer.Usage=D3D11_USAGE_DYNAMIC;
    buffer.BindFlags=D3D11_BIND_VERTEX_BUFFER;buffer.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
    D3D11_SAMPLER_DESC sampler{};sampler.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sampler.MaxLOD=D3D11_FLOAT32_MAX;
    D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;raster.CullMode=D3D11_CULL_NONE;raster.DepthClipEnable=TRUE;
    D3D11_DEPTH_STENCIL_DESC depth{};depth.DepthFunc=D3D11_COMPARISON_ALWAYS;
    D3D11_BLEND_DESC blend{};blend.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
    if(FAILED(device->CreateInputLayout(input,2,vs->GetBufferPointer(),vs->GetBufferSize(),&targetSurface.layout))
        ||FAILED(device->CreateBuffer(&buffer,nullptr,&targetSurface.vertices))
        ||FAILED(device->CreateSamplerState(&sampler,&targetSurface.sampler))
        ||FAILED(device->CreateRasterizerState(&raster,&targetSurface.raster))
        ||FAILED(device->CreateDepthStencilState(&depth,&targetSurface.depth))
        ||FAILED(device->CreateBlendState(&blend,&targetSurface.blend))
        ||FAILED(device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&targetSurface.ps))
        ||FAILED(device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&targetSurface.vs)))return false;
    return true;
}
}
bool captureNativeMenuSurface(ID3D11DeviceContext* context,uint64_t source) noexcept {try{
    if(!context||!source)return false;
    std::lock_guard lock(surfaceMutex);ComPtr<ID3D11Device> device;context->GetDevice(&device);
    if(!initialize(device.Get(),surface))return false;
    ComPtr<ID3D11Texture2D> native;if(!sceneSourceTexture(context,&native))return false;
    D3D11_TEXTURE2D_DESC desc{};native->GetDesc(&desc);
    if(desc.SampleDesc.Count!=1||desc.ArraySize!=1||desc.MipLevels!=1)return false;
    const auto& old=surface.description;
    if(!surface.texture||desc.Width!=old.Width||desc.Height!=old.Height||desc.Format!=old.Format){
        surface.image.Reset();surface.texture.Reset();desc.Usage=D3D11_USAGE_DEFAULT;desc.CPUAccessFlags=desc.MiscFlags=0;
        desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        if(FAILED(device->CreateTexture2D(&desc,nullptr,&surface.texture)))return false;
        D3D11_SHADER_RESOURCE_VIEW_DESC view{};view.Format=desc.Format;
        if(view.Format==DXGI_FORMAT_R8G8B8A8_TYPELESS)view.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        if(view.Format==DXGI_FORMAT_B8G8R8A8_TYPELESS)view.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
        view.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;view.Texture2D.MipLevels=1;
        if(FAILED(device->CreateShaderResourceView(surface.texture.Get(),&view,&surface.image)))return false;
        surface.description=desc;
    }
    State saved(context);ID3D11ShaderResourceView* empty=nullptr;context->PSSetShaderResources(0,1,&empty);
    context->OMSetRenderTargets(0,nullptr,nullptr);context->CopyResource(surface.texture.Get(),native.Get());
    surface.source=source;surface.capturedAt=GetTickCount64();return true;
}catch(...){return false;}}
bool drawSurface(ID3D11DeviceContext* context,Surface& targetSurface,const std::array<float,16>& view,EyeFov fov,Pose panel,float width,float height){
    constexpr std::array<float,16> identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    const auto transform=uiPanelProjection(identity,view,fov,panel,width,height);if(!transform)return false;
    constexpr float corners[6][2]={{-1,1},{1,1},{-1,-1},{-1,-1},{1,1},{1,-1}};
    Vertex vertices[6]{};
    for(size_t i=0;i<6;++i){const auto x=corners[i][0],y=corners[i][1];
        for(size_t n=0;n<4;++n)vertices[i].position[n]=x*(*transform)[n]+y*(*transform)[4+n]+(*transform)[12+n];
        vertices[i].uv[0]=(x+1)*.5f;vertices[i].uv[1]=(1-y)*.5f;
    }
    State saved(context);ComPtr<ID3D11Texture2D> native;ComPtr<ID3D11RenderTargetView> target;
    if(!sceneSourceTexture(context,&native)||FAILED(targetSurface.device->CreateRenderTargetView(native.Get(),nullptr,&target)))return false;
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if(FAILED(context->Map(targetSurface.vertices.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))return false;
    std::memcpy(mapped.pData,vertices,sizeof(vertices));context->Unmap(targetSurface.vertices.Get(),0);
    auto* rt=target.Get();context->OMSetRenderTargets(1,&rt,nullptr);
    auto* vb=targetSurface.vertices.Get();UINT stride=sizeof(Vertex),offset=0;context->IASetVertexBuffers(0,1,&vb,&stride,&offset);
    context->IASetInputLayout(targetSurface.layout.Get());context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(targetSurface.vs.Get(),nullptr,0);context->PSSetShader(targetSurface.ps.Get(),nullptr,0);
    context->GSSetShader(nullptr,nullptr,0);context->HSSetShader(nullptr,nullptr,0);context->DSSetShader(nullptr,nullptr,0);
    auto* image=targetSurface.image.Get();context->PSSetShaderResources(0,1,&image);auto* sampler=targetSurface.sampler.Get();context->PSSetSamplers(0,1,&sampler);
    context->RSSetState(targetSurface.raster.Get());context->OMSetDepthStencilState(targetSurface.depth.Get(),0);
    const FLOAT blend[4]{};context->OMSetBlendState(targetSurface.blend.Get(),blend,~0u);
    D3D11_TEXTURE2D_DESC destination{};native->GetDesc(&destination);
    D3D11_VIEWPORT viewport{0,0,float(destination.Width),float(destination.Height),0,1};context->RSSetViewports(1,&viewport);
    context->Draw(6,0);
    return true;
}
bool drawNativeMenuSurface(ID3D11DeviceContext* context,const std::array<float,16>& view,EyeFov fov,Pose panel) noexcept {try{
    if(!context)return false;std::lock_guard lock(surfaceMutex);
    if(!surface.image||!surface.source||GetTickCount64()-surface.capturedAt>250)return false;
    const bool drawn=drawSurface(context,surface,view,fov,panel,1.9f,1.9f*9.f/16.f);
    if(drawn&&!surface.reported){surface.reported=true;log("Complete native Title menu rendered on a spatial cabin panel");}
    return drawn;
}catch(...){return false;}}
bool drawWristCategorySelector(ID3D11DeviceContext* context,const std::array<float,16>& view,EyeFov fov,Pose panel,const EquipmentLabels& labels) noexcept {try{
    if(!context)return false;std::lock_guard lock(surfaceMutex);
    ComPtr<ID3D11Device> device;context->GetDevice(&device);
    if(!initialize(device.Get(),categorySurface))return false;
    if(!categorySurface.image||categoryLabels!=labels){
        const auto pixels=makeWristSelectorImage(labels);if(pixels.bgra.empty())return false;
        D3D11_TEXTURE2D_DESC desc{};desc.Width=pixels.width;desc.Height=pixels.height;
        desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA data{pixels.bgra.data(),pixels.width*4,0};
        categorySurface.image.Reset();categorySurface.texture.Reset();
        if(FAILED(device->CreateTexture2D(&desc,&data,&categorySurface.texture))
            ||FAILED(device->CreateShaderResourceView(categorySurface.texture.Get(),nullptr,&categorySurface.image)))return false;
        categorySurface.description=desc;categoryLabels=labels;
    }
    const bool drawn=drawSurface(context,categorySurface,view,fov,panel,.48f,.48f*320.f/1440.f);
    if(drawn&&!categorySurface.reported){categorySurface.reported=true;log("Four-category wrist selector rendered with configured input labels");}
    return drawn;
}catch(...){return false;}}
void stopNativeMenuSurface() noexcept {std::lock_guard lock(surfaceMutex);surface={};categorySurface={};categoryLabels={};}
}
