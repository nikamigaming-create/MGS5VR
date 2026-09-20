#include "mgs5vr/ui_clip.hpp"
#include "mgs5vr/mailbox.hpp"
#include <d3dcompiler.h>
#include <MinHook.h>
#include <iostream>
#include <string>
#include <stdexcept>
using namespace mgs5vr;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> immediate,context;
    D3D_FEATURE_LEVEL level=D3D_FEATURE_LEVEL_11_0;
    checkHr(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,&level,1,D3D11_SDK_VERSION,
        &device,nullptr,&immediate),"Create clip test device");
    require(MH_Initialize()==MH_OK,"Initialize hooks");
    checkHr(device->CreateDeferredContext(0,&context),"Create clip test context");
    constexpr char source[]=R"(
struct V{float4 position:SV_POSITION;float2 uv:TEXCOORD0;float4 color:COLOR0;};
V vs(uint id:SV_VertexID){V o;float2 p=id==0?float2(-1,-1):id==1?float2(-1,3):float2(3,-1);
o.position=float4(p,0.5,1);o.uv=p;o.color=float4(1,0,0,1);return o;}
float4 ps(V i):SV_TARGET{return i.color+float4(i.uv*0,0,0);}
)";
    ComPtr<ID3DBlob> vertex,pixel,error;
    checkHr(D3DCompile(source,sizeof(source),nullptr,nullptr,nullptr,"vs","vs_5_0",0,0,&vertex,&error),"Compile test VS");
    checkHr(D3DCompile(source,sizeof(source),nullptr,nullptr,nullptr,"ps","ps_5_0",0,0,&pixel,&error),"Compile test PS");
    ComPtr<ID3D11VertexShader> vs;ComPtr<ID3D11PixelShader> ps;
    checkHr(device->CreateVertexShader(vertex->GetBufferPointer(),vertex->GetBufferSize(),nullptr,&vs),"Create test VS");
    checkHr(device->CreatePixelShader(pixel->GetBufferPointer(),pixel->GetBufferSize(),nullptr,&ps),"Create test PS");
    std::string earlySource=source;
    earlySource.replace(earlySource.find("float4(1,0,0,1)"),15,"float4(.9,0,0,1)");
    ComPtr<ID3DBlob> earlyBytecode;
    checkHr(D3DCompile(earlySource.data(),earlySource.size(),nullptr,nullptr,nullptr,"vs","vs_5_0",0,0,&earlyBytecode,&error),"Compile early VS");
    ComPtr<ID3D11VertexShader> earlyVs;
    checkHr(device->CreateVertexShader(earlyBytecode->GetBufferPointer(),earlyBytecode->GetBufferSize(),nullptr,&earlyVs),"Create early VS");
    installUiClip(device.Get());
    checkHr(device->CreateVertexShader(vertex->GetBufferPointer(),vertex->GetBufferSize(),nullptr,vs.ReleaseAndGetAddressOf()),"Create reflected test VS");
    D3D11_TEXTURE2D_DESC desc{};desc.Width=desc.Height=64;desc.ArraySize=desc.MipLevels=1;
    desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> target,readback;ComPtr<ID3D11RenderTargetView> rtv;
    checkHr(device->CreateTexture2D(&desc,nullptr,&target),"Create test target");
    checkHr(device->CreateRenderTargetView(target.Get(),nullptr,&rtv),"Create test RTV");
    desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    checkHr(device->CreateTexture2D(&desc,nullptr,&readback),"Create test staging");
    const auto render=[&](std::array<float,16> plane,bool scoped){
        const float black[4]{};context->ClearRenderTargetView(rtv.Get(),black);
        auto* rt=rtv.Get();context->OMSetRenderTargets(1,&rt,nullptr);
        D3D11_VIEWPORT viewport{0,0,64,64,0,1};context->RSSetViewports(1,&viewport);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(vs.Get(),nullptr,0);context->PSSetShader(ps.Get(),nullptr,0);
        if(scoped){UiClipScope clip(plane);context->Draw(3,0);}else context->Draw(3,0);
        ComPtr<ID3D11GeometryShader> gs;context->GSGetShader(&gs,nullptr,nullptr);
        require(!gs,"Native geometry shader state restored");
        ComPtr<ID3D11RasterizerState> raster;context->RSGetState(&raster);
        require(!raster,"Native rasterizer state restored");
        ComPtr<ID3D11CommandList> commands;checkHr(context->FinishCommandList(FALSE,&commands),"Finish clip test");
        immediate->ExecuteCommandList(commands.Get(),FALSE);immediate->CopyResource(readback.Get(),target.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};checkHr(immediate->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped),"Read clip pixels");
        std::array<unsigned char,4096> pixels{};
        for(size_t y=0;y<64;++y)for(size_t x=0;x<64;++x)pixels[y*64+x]=static_cast<unsigned char*>(mapped.pData)[y*mapped.RowPitch+x*4];
        immediate->Unmap(readback.Get(),0);return pixels;
    };
    const std::array<float,16> plane{.5f,0,0,0,0,.25f,0,0,0,0,0,0,0,0,0,1};
    const auto pixels=render(plane,true);
    require(pixels[32*64+32]>200,"Canvas center retains native pixels");
    require(pixels[32*64+4]==0&&pixels[4*64+32]==0,"Oversized native geometry clipped on both axes");
    const std::array<float,16> rotated{.3f,.3f,0,0,-.3f,.3f,0,0,0,0,0,0,0,0,0,1};
    const auto diamond=render(rotated,true);
    require(diamond[32*64+32]>200&&diamond[18*64+18]==0,"Rotated canvas clips its corners, not only bounding box");
    const auto outside=render(plane,false);
    require(outside[32*64+4]>200,"Non-menu native draws retain their original coverage");
    vs=earlyVs;
    const auto fallback=render(plane,true);
    require(fallback[32*64+32]>200&&fallback[32*64+4]==0&&fallback[4*64+32]==0,
        "Shaders created before hooks retain canvas bounds");
    auto behind=plane;behind[15]=-1;require(!uiClipPlanes(behind),"Behind-eye canvas rejected");
    MH_Uninitialize();std::cout<<"Native canvas clip GPU pixels and state restoration passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
