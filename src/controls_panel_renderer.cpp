#include "mgs5vr/controls_panel_renderer.hpp"
#include "mgs5vr/native_text_input.hpp"
#include "mgs5vr/log.hpp"
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace mgs5vr {
using Microsoft::WRL::ComPtr;
namespace {
constexpr UINT panelWidth=2048,panelHeight=1280;
struct ModelVertex {float position[3],normal[3],uv[2];};
struct ModelPart {ComPtr<ID3D11Buffer> vertices,indices;UINT count{};uint32_t inputs{};};
struct Model {std::vector<ModelPart> parts;ComPtr<ID3D11ShaderResourceView> texture;};
bool textureFile(ID3D11Device* device,const std::filesystem::path& path,ComPtr<ID3D11ShaderResourceView>& output){
    const auto initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    struct Apartment {HRESULT result;~Apartment(){if(SUCCEEDED(result))CoUninitialize();}} apartment{initialized};
    ComPtr<IWICImagingFactory> factory;ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> converter;
    if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))
        ||FAILED(factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder))
        ||FAILED(decoder->GetFrame(0,&frame))||FAILED(factory->CreateFormatConverter(&converter))
        ||FAILED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))return false;
    UINT width{},height{};if(FAILED(frame->GetSize(&width,&height))||!width||!height||width>4096||height>4096)return false;
    std::vector<uint32_t> pixels(size_t(width)*height);
    if(FAILED(converter->CopyPixels(nullptr,width*4,static_cast<UINT>(pixels.size()*4),reinterpret_cast<BYTE*>(pixels.data()))))return false;
    D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=height;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{pixels.data(),width*4,0};ComPtr<ID3D11Texture2D> texture;
    return SUCCEEDED(device->CreateTexture2D(&desc,&data,&texture))&&SUCCEEDED(device->CreateShaderResourceView(texture.Get(),nullptr,&output));
}
bool modelFile(ID3D11Device* device,const std::filesystem::path& path,Model& model){
    std::ifstream file(path,std::ios::binary);char magic[4]{};uint32_t version{},count{};
    file.read(magic,4);file.read(reinterpret_cast<char*>(&version),4);file.read(reinterpret_cast<char*>(&count),4);
    if(!file||std::string_view(magic,4)!="M5CT"||version!=1||count>16)return false;
    for(uint32_t p=0;p<count;++p){
        ModelPart part;uint32_t vertices{},indices{};
        file.read(reinterpret_cast<char*>(&part.inputs),4);file.read(reinterpret_cast<char*>(&vertices),4);file.read(reinterpret_cast<char*>(&indices),4);
        if(!file||!vertices||vertices>10000||!indices||indices>50000)return false;
        std::vector<ModelVertex> vertex(vertices);std::vector<uint32_t> index(indices);
        file.read(reinterpret_cast<char*>(vertex.data()),static_cast<std::streamsize>(vertex.size()*sizeof(ModelVertex)));
        file.read(reinterpret_cast<char*>(index.data()),static_cast<std::streamsize>(index.size()*sizeof(uint32_t)));
        if(!file||std::any_of(index.begin(),index.end(),[&](uint32_t n){return n>=vertices;}))return false;
        D3D11_BUFFER_DESC desc{};desc.ByteWidth=static_cast<UINT>(vertex.size()*sizeof(ModelVertex));desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA data{vertex.data(),0,0};if(FAILED(device->CreateBuffer(&desc,&data,&part.vertices)))return false;
        desc.ByteWidth=indices*4;desc.BindFlags=D3D11_BIND_INDEX_BUFFER;data.pSysMem=index.data();
        if(FAILED(device->CreateBuffer(&desc,&data,&part.indices)))return false;
        part.count=indices;model.parts.push_back(std::move(part));
    }
    auto texture=path;texture.replace_extension(L".png");return textureFile(device,texture,model.texture);
}
std::vector<uint32_t> panelPixels(const ControlsEditorFrame& f){
    BITMAPINFO bitmap{};bitmap.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bitmap.bmiHeader.biWidth=panelWidth;
    bitmap.bmiHeader.biHeight=-static_cast<LONG>(panelHeight);bitmap.bmiHeader.biPlanes=1;bitmap.bmiHeader.biBitCount=32;
    HDC dc=CreateCompatibleDC(nullptr);void* raw{};HBITMAP surface=CreateDIBSection(dc,&bitmap,DIB_RGB_COLORS,&raw,nullptr,0);
    if(!dc||!surface||!raw){if(surface)DeleteObject(surface);if(dc)DeleteDC(dc);return {};}
    const auto old=SelectObject(dc,surface);auto* bits=static_cast<uint32_t*>(raw);
    std::fill(bits,bits+size_t(panelWidth)*panelHeight,0xff111a20);
    SetBkMode(dc,TRANSPARENT);
    const auto fill=[&](RECT area,COLORREF color){HBRUSH brush=CreateSolidBrush(color);FillRect(dc,&area,brush);DeleteObject(brush);};
    const auto text=[&](std::string_view label,RECT area,int size,COLORREF color=RGB(229,236,232),UINT flags=DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS){
        HFONT font=CreateFontW(-size,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Bahnschrift");const auto prior=SelectObject(dc,font);SetTextColor(dc,color);
        const std::wstring wide(label.begin(),label.end());DrawTextW(dc,wide.c_str(),static_cast<int>(wide.size()),&area,flags);
        SelectObject(dc,prior);DeleteObject(font);};
    fill({0,0,2048,105},RGB(23,36,44));text(f.settings?"VR SETTINGS":"CONTROLS",{42,18,790,94},56);
    text("LIVE MODE: "+std::string(controlsModeName(f.liveMode)),{980,26,1840,82},32,RGB(98,211,179));
    text(f.settings?"APPLIES LIVE":std::string(controlsModeName(f.mode)),{1120,123,1980,201},38);
    text(f.settings?(f.settingsGroup?"RELATED SWITCHES AND TUNING":"SWITCHES FIRST / ARROWS CHOOSE GROUP"):f.automatic?"FOLLOWING GAME":"INSPECTING MODE",{32,215,880,241},24,RGB(145,165,169));
    if(!f.settings){
        text("LEFT",{995,212,1455,269},34,RGB(229,236,232),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        text("RIGHT",{1500,212,1980,269},34,RGB(229,236,232),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    }
    for(const auto& hit:controlsEditorLayout(f)){
        RECT area{static_cast<LONG>(hit.x*panelWidth),static_cast<LONG>(hit.y*panelHeight),
            static_cast<LONG>((hit.x+hit.width)*panelWidth),static_cast<LONG>((hit.y+hit.height)*panelHeight)};
        const bool chosen=(hit.id<11&&f.page*11+static_cast<size_t>(hit.id)==f.selected)||(hit.id==116&&!f.settings)||(hit.id==117&&f.settings);
        fill(area,chosen?RGB(38,76,85):hit.id==f.hover?RGB(54,75,84):RGB(29,43,51));
        if(hit.id<11){const auto& row=f.rows[f.page*11+static_cast<size_t>(hit.id)];
            text(row.title,{area.left+18,area.top+2,area.right-16,area.top+39},31);
            text(row.label,{area.left+18,area.top+37,area.right-16,area.bottom-1},25,RGB(159,206,205));
        }else if(hit.id==106||hit.id==113||hit.id==100||hit.id==112||hit.id==116||hit.id==117||hit.id==120){
            HPEN pen=CreatePen(PS_SOLID,5,RGB(226,235,232));const auto prior=SelectObject(dc,pen);
            const LONG cx=(area.left+area.right)/2,cy=(area.top+area.bottom)/2;
            if(hit.id==106){HBRUSH brush=CreateSolidBrush(RGB(225,82,66));const auto b=SelectObject(dc,brush);
                Ellipse(dc,cx-14,cy-14,cx+14,cy+14);SelectObject(dc,b);DeleteObject(brush);}
            else if(hit.id==113){const POINT points[]{{cx-16,cy},{cx-3,cy+14},{cx+22,cy-17}};Polyline(dc,points,3);}
            else if(hit.id==117){const auto b=SelectObject(dc,GetStockObject(NULL_BRUSH));Ellipse(dc,cx-14,cy-14,cx+14,cy+14);Ellipse(dc,cx-5,cy-5,cx+5,cy+5);
                for(int n=0;n<8;++n){const float angle=n*3.14159265f/4;MoveToEx(dc,cx+static_cast<int>(14*std::cos(angle)),cy+static_cast<int>(14*std::sin(angle)),nullptr);
                    LineTo(dc,cx+static_cast<int>(22*std::cos(angle)),cy+static_cast<int>(22*std::sin(angle)));}SelectObject(dc,b);}
            else if(hit.id==116){const auto b=SelectObject(dc,GetStockObject(NULL_BRUSH));RoundRect(dc,cx-20,cy-12,cx+20,cy+17,13,13);
                MoveToEx(dc,cx-13,cy-1,nullptr);LineTo(dc,cx-3,cy-1);MoveToEx(dc,cx-8,cy-6,nullptr);LineTo(dc,cx-8,cy+4);Ellipse(dc,cx+7,cy-5,cx+13,cy+1);SelectObject(dc,b);}
            else if(hit.id==120){HBRUSH brush=CreateSolidBrush(f.settingValue>0?RGB(91,224,177):RGB(110,123,129));const auto b=SelectObject(dc,brush);
                RoundRect(dc,cx-30,cy-14,cx+30,cy+14,26,26);SelectObject(dc,b);DeleteObject(brush);
                const LONG knob=cx+(f.settingValue>0?16:-16);Ellipse(dc,knob-10,cy-10,knob+10,cy+10);}
            else {MoveToEx(dc,cx-14,cy-14,nullptr);LineTo(dc,cx+14,cy+14);MoveToEx(dc,cx-14,cy+14,nullptr);LineTo(dc,cx+14,cy-14);}
            SelectObject(dc,prior);DeleteObject(pen);
        }else {std::string label(hit.text);
            if(hit.id==107)label=f.gesture;
            if(hit.id==108)label=f.append?"+ ALT ON":"+ ALT";
            if(hit.id==109)label=f.touches?"TOUCH ON":"TOUCH";
            text(label,area,30,RGB(229,236,232),DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
    }
    text(std::to_string(f.page+1)+" / "+std::to_string(std::max<size_t>(1,(f.rows.size()+10)/11)),{180,1150,710,1221},32,RGB(154,176,180),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    if(!f.action.empty()){
        text(f.settings?f.rows[f.selected].title:f.action,{1004,830,1980,878},36);
        text(f.proposed.empty()?f.binding:f.proposed,{1004,882,1980,955},30,RGB(240,202,117),DT_LEFT|DT_WORDBREAK|DT_NOPREFIX);
        if(f.gesture=="tap"||f.gesture=="hold")text(std::to_string(f.milliseconds)+" ms",{1682,914,1850,977},26,RGB(154,176,180),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        text(f.status,{1004,1081,1980,1138},27,f.matched?RGB(91,224,177):RGB(203,215,213),DT_LEFT|DT_WORDBREAK|DT_NOPREFIX);
    }
    if(f.settings){
        text(f.rows[f.selected].title,{1004,255,1980,390},48,RGB(229,236,232),DT_LEFT|DT_WORDBREAK|DT_NOPREFIX);
        std::string value=f.proposed.empty()?f.binding:f.proposed;
        if(f.minimum==0&&f.maximum==1)value=f.settingValue>0?"ON":"OFF";
        std::replace(value.begin(),value.end(),'_',' ');
        text(value,{1004,420,1980,584},70,RGB(240,202,117),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        std::ostringstream range;range<<"Range "<<f.minimum<<" to "<<f.maximum<<"    Step "<<f.step;
        text(range.str(),{1004,755,1980,806},28,RGB(154,176,180));
    }else text("GOLD = ASSIGNED     GREEN = PRESSED     BLUE = TOUCH",{1000,747,1990,796},25,RGB(154,176,180));
    text("Point with either controller. Trigger selects. Release before recording.",{41,1240,1995,1275},26,RGB(154,176,180));
    if(f.cursorValid){HPEN pen=CreatePen(PS_SOLID,3,RGB(255,238,160));const auto p=SelectObject(dc,pen),b=SelectObject(dc,GetStockObject(NULL_BRUSH));
        const int x=static_cast<int>(f.cursor[0]*panelWidth),y=static_cast<int>(f.cursor[1]*panelHeight);Ellipse(dc,x-9,y-9,x+9,y+9);
        SelectObject(dc,b);SelectObject(dc,p);DeleteObject(pen);}
    GdiFlush();std::vector<uint32_t> pixels(bits,bits+size_t(panelWidth)*panelHeight);for(auto& p:pixels)p|=0xff000000;
    SelectObject(dc,old);DeleteObject(surface);DeleteDC(dc);return pixels;
}
constexpr char shader[]=R"(
Texture2D image:register(t0);SamplerState linearClamp:register(s0);
cbuffer Params:register(b0){float4 pose;float4 tint;};
struct Vertex{float3 p:POSITION;float3 n:NORMAL;float2 uv:TEXCOORD;};
struct Pixel{float4 p:SV_Position;float3 n:NORMAL;float2 uv:TEXCOORD;};
Pixel modelVs(Vertex v){Pixel o;float3 p=v.p,n=v.n;
 float3 rotated=float3(p.x*pose.x+p.z*pose.y,p.y,-p.x*pose.y+p.z*pose.x);
 rotated=float3(rotated.x,rotated.y*pose.z-rotated.z*pose.w,rotated.y*pose.w+rotated.z*pose.z);
 o.p=float4(rotated.x*11.2658,rotated.y*12,.5-rotated.z*3,1);
 float3 normal=float3(n.x*pose.x+n.z*pose.y,n.y,-n.x*pose.y+n.z*pose.x);
 o.n=float3(normal.x,normal.y*pose.z-normal.z*pose.w,normal.y*pose.w+normal.z*pose.z);o.uv=v.uv;return o;}
float4 modelPs(Pixel v):SV_Target{float3 base=image.Sample(linearClamp,v.uv).rgb;
 float light=.60+.40*saturate(dot(normalize(v.n),normalize(float3(-.3,.6,1))));
 return float4(lerp(base*light,tint.rgb,tint.a),1);}
Pixel panelVs(uint id:SV_VertexID){Pixel o;o.uv=float2((id<<1)&2,id&2);o.p=float4(o.uv*float2(2,-2)+float2(-1,1),0,1);o.n=0;return o;}
float4 panelPs(Pixel v):SV_Target{return image.Sample(linearClamp,v.uv);}
)";
}
struct ControlsPanelRenderer::Impl {
    ComPtr<ID3D11Texture2D> color,output,depth,background;
    ComPtr<ID3D11RenderTargetView> target;
    ComPtr<ID3D11DepthStencilView> depthView;
    ComPtr<ID3D11ShaderResourceView> backgroundView;
    ComPtr<ID3D11VertexShader> modelVs,panelVs;
    ComPtr<ID3D11PixelShader> modelPs,panelPs;
    ComPtr<ID3D11InputLayout> layout;
    ComPtr<ID3D11Buffer> constants;
    ComPtr<ID3D11SamplerState> sampler;
    ComPtr<ID3D11DepthStencilState> depthOn,depthOff;
    ComPtr<ID3D11RasterizerState> raster;
    std::array<Model,2> models;
    uint64_t revision{~uint64_t{0}};
    bool ready{},failed{};
    UINT samples{1};
    std::array<float,2> yaw{},pitch{};
    bool initialize(ID3D11Device* device,const std::filesystem::path& game){
        const auto compile=[&](const char* entry,const char* profile,ComPtr<ID3DBlob>& result){ComPtr<ID3DBlob> errors;
            return SUCCEEDED(D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,entry,profile,D3DCOMPILE_ENABLE_STRICTNESS,0,&result,&errors));};
        ComPtr<ID3DBlob> code;
        if(!compile("modelVs","vs_5_0",code)||FAILED(device->CreateVertexShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&modelVs)))return false;
        const D3D11_INPUT_ELEMENT_DESC input[]{{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
            {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0}};
        if(FAILED(device->CreateInputLayout(input,3,code->GetBufferPointer(),code->GetBufferSize(),&layout)))return false;
        if(!compile("modelPs","ps_5_0",code)||FAILED(device->CreatePixelShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&modelPs))
            ||!compile("panelVs","vs_5_0",code)||FAILED(device->CreateVertexShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&panelVs))
            ||!compile("panelPs","ps_5_0",code)||FAILED(device->CreatePixelShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&panelPs)))return false;
        D3D11_TEXTURE2D_DESC texture{};texture.Width=panelWidth;texture.Height=panelHeight;texture.MipLevels=texture.ArraySize=texture.SampleDesc.Count=1;
        texture.Format=DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;texture.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        if(FAILED(device->CreateTexture2D(&texture,nullptr,&background))||FAILED(device->CreateShaderResourceView(background.Get(),nullptr,&backgroundView))
            ||FAILED(device->CreateTexture2D(&texture,nullptr,&output)))return false;
        UINT quality{};if(SUCCEEDED(device->CheckMultisampleQualityLevels(texture.Format,4,&quality))&&quality)samples=4;
        texture.SampleDesc.Count=samples;texture.BindFlags=D3D11_BIND_RENDER_TARGET;
        if(FAILED(device->CreateTexture2D(&texture,nullptr,&color))||FAILED(device->CreateRenderTargetView(color.Get(),nullptr,&target)))return false;
        texture.Format=DXGI_FORMAT_D32_FLOAT;texture.BindFlags=D3D11_BIND_DEPTH_STENCIL;
        if(FAILED(device->CreateTexture2D(&texture,nullptr,&depth))||FAILED(device->CreateDepthStencilView(depth.Get(),nullptr,&depthView)))return false;
        D3D11_BUFFER_DESC buffer{};buffer.ByteWidth=32;buffer.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        if(FAILED(device->CreateBuffer(&buffer,nullptr,&constants)))return false;
        D3D11_SAMPLER_DESC sample{};sample.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;sample.AddressU=sample.AddressV=sample.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sample.MaxLOD=D3D11_FLOAT32_MAX;
        if(FAILED(device->CreateSamplerState(&sample,&sampler)))return false;
        D3D11_DEPTH_STENCIL_DESC z{};z.DepthEnable=TRUE;z.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;z.DepthFunc=D3D11_COMPARISON_LESS;
        if(FAILED(device->CreateDepthStencilState(&z,&depthOn)))return false;z.DepthEnable=FALSE;
        if(FAILED(device->CreateDepthStencilState(&z,&depthOff)))return false;
        D3D11_RASTERIZER_DESC rs{};rs.FillMode=D3D11_FILL_SOLID;rs.CullMode=D3D11_CULL_NONE;rs.DepthClipEnable=TRUE;rs.MultisampleEnable=TRUE;
        if(FAILED(device->CreateRasterizerState(&rs,&raster)))return false;
        for(size_t hand=0;hand<2;++hand)if(!modelFile(device,game/L"assets/controls"/(hand?L"right.mesh":L"left.mesh"),models[hand]))return false;
        ready=true;return true;
    }
};
ControlsPanelRenderer::ControlsPanelRenderer():impl_(std::make_unique<Impl>()){}
ControlsPanelRenderer::~ControlsPanelRenderer()=default;
ID3D11Texture2D* ControlsPanelRenderer::draw(ID3D11Device* device,ID3D11DeviceContext* context,
    const ControlsEditorFrame& f,const std::filesystem::path& game){
    auto& p=*impl_;if(!f.visible||p.failed)return nullptr;
    if(!p.ready&&!p.initialize(device,game)){p.failed=true;log("VR controls panel resources unavailable");return nullptr;}
    if(p.revision!=f.revision){const auto pixels=panelPixels(f);if(pixels.empty())return nullptr;
        context->UpdateSubresource(p.background.Get(),0,nullptr,pixels.data(),panelWidth*4,0);p.revision=f.revision;}
    context->ClearDepthStencilView(p.depthView.Get(),D3D11_CLEAR_DEPTH,1,0);
    auto* target=p.target.Get();context->OMSetRenderTargets(1,&target,p.depthView.Get());context->OMSetBlendState(nullptr,nullptr,~0u);
    context->RSSetState(p.raster.Get());D3D11_VIEWPORT viewport{0,0,static_cast<float>(panelWidth),static_cast<float>(panelHeight),0,1};context->RSSetViewports(1,&viewport);
    context->OMSetDepthStencilState(p.depthOff.Get(),0);context->IASetInputLayout(nullptr);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(p.panelVs.Get(),nullptr,0);context->PSSetShader(p.panelPs.Get(),nullptr,0);
    auto* sampler=p.sampler.Get();context->PSSetSamplers(0,1,&sampler);auto* image=p.backgroundView.Get();context->PSSetShaderResources(0,1,&image);context->Draw(3,0);
    context->OMSetDepthStencilState(p.depthOn.Get(),0);context->IASetInputLayout(p.layout.Get());context->VSSetShader(p.modelVs.Get(),nullptr,0);context->PSSetShader(p.modelPs.Get(),nullptr,0);
    auto* constants=p.constants.Get();context->VSSetConstantBuffers(0,1,&constants);context->PSSetConstantBuffers(0,1,&constants);
    const auto active=physicalControlMask(f.physical),assigned=f.proposed.empty()?f.selectedInputs:f.capturedInputs;
    for(size_t hand=0;!f.settings&&hand<2;++hand){
        viewport={hand?1502.f:997.f,279,474,445,0,1};context->RSSetViewports(1,&viewport);
        const auto shownInputs=assigned|active;
        // Squeeze normals face inward (+X left, -X right). Turn that actual
        // contact surface toward the player rather than behind the housing.
        const float targetYaw=(shownInputs&(1u<<(7+hand)))?(hand?.95f:-.95f):hand?-.20f:.20f;
        const float targetPitch=(shownInputs&(1u<<(9+hand)))?.75f:0.f;
        p.yaw[hand]+=(targetYaw-p.yaw[hand])*.15f;
        p.pitch[hand]+=(targetPitch-p.pitch[hand])*.15f;
        image=p.models[hand].texture.Get();context->PSSetShaderResources(0,1,&image);
        for(const auto& part:p.models[hand].parts){
            const bool pressed=(part.inputs&active&((1u<<19)-1))!=0,touched=(part.inputs&active&~((1u<<19)-1))!=0,chosen=(part.inputs&assigned)!=0;
            if(part.inputs&&(part.inputs&(1u<<4|1u<<19|1u<<20))&&!pressed&&!touched&&!chosen)continue;
            std::array<float,8> params{std::cos(p.yaw[hand]),std::sin(p.yaw[hand]),std::cos(p.pitch[hand]),std::sin(p.pitch[hand]),1,1,1,0};
            if(pressed){params[4]=.12f;params[5]=.85f;params[6]=.55f;params[7]=.72f;}
            else if(touched){params[4]=.15f;params[5]=.65f;params[6]=1;params[7]=.55f;}
            else if(chosen){params[4]=1;params[5]=.64f;params[6]=.20f;params[7]=.65f;}
            context->UpdateSubresource(constants,0,nullptr,params.data(),0,0);
            const UINT stride=sizeof(ModelVertex),offset=0;auto* vertices=part.vertices.Get();context->IASetVertexBuffers(0,1,&vertices,&stride,&offset);
            context->IASetIndexBuffer(part.indices.Get(),DXGI_FORMAT_R32_UINT,0);context->DrawIndexed(part.count,0,0);
        }
    }
    ID3D11ShaderResourceView* empty{};context->PSSetShaderResources(0,1,&empty);context->OMSetRenderTargets(0,nullptr,nullptr);
    if(p.samples>1)context->ResolveSubresource(p.output.Get(),0,p.color.Get(),0,DXGI_FORMAT_B8G8R8A8_UNORM_SRGB);
    else context->CopyResource(p.output.Get(),p.color.Get());
    return p.output.Get();
}
struct NativeTextPanelRenderer::Impl {
    ComPtr<ID3D11Texture2D> texture;
    int hover{-2};
};
NativeTextPanelRenderer::NativeTextPanelRenderer():impl_(std::make_unique<Impl>()){}
NativeTextPanelRenderer::~NativeTextPanelRenderer()=default;
ID3D11Texture2D* NativeTextPanelRenderer::draw(ID3D11Device* device,ID3D11DeviceContext* context,const CabinStudioFrame& frame){
    if(!frame.available||!frame.keyboardOnly||!device||!context)return nullptr;
    auto& p=*impl_;if(p.texture&&p.hover==frame.hoverKey)return p.texture.Get();
    constexpr LONG cell=160,width=cell*11,height=cell*4;
    BITMAPINFO bitmap{};bitmap.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bitmap.bmiHeader.biWidth=width;
    bitmap.bmiHeader.biHeight=-height;bitmap.bmiHeader.biPlanes=1;bitmap.bmiHeader.biBitCount=32;
    HDC dc=CreateCompatibleDC(nullptr);void* raw{};HBITMAP surface=CreateDIBSection(dc,&bitmap,DIB_RGB_COLORS,&raw,nullptr,0);
    if(!dc||!surface||!raw){if(surface)DeleteObject(surface);if(dc)DeleteDC(dc);return nullptr;}
    const auto old=SelectObject(dc,surface);auto* bits=static_cast<uint32_t*>(raw);
    std::fill(bits,bits+size_t(width)*height,0xff10181e);SetBkMode(dc,TRANSPARENT);
    HFONT font=CreateFontW(-80,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Bahnschrift");
    const auto oldFont=SelectObject(dc,font);SetTextColor(dc,RGB(238,245,245));
    for(size_t key=0;key<44;++key){
        if(!nativeTextKeyEnabled(key))continue;
        const LONG x=static_cast<LONG>(key%11)*cell,y=static_cast<LONG>(key/11)*cell;
        RECT tile{x+4,y+4,x+cell-4,y+cell-4};
        HBRUSH brush=CreateSolidBrush(frame.hoverKey==static_cast<int>(key)?RGB(40,111,119):RGB(34,48,57));
        FillRect(dc,&tile,brush);DeleteObject(brush);
        if(key!=33&&key!=34&&key!=36){
            const auto label=studioKey(key);const std::wstring wide(label.begin(),label.end());
            DrawTextW(dc,wide.c_str(),static_cast<int>(wide.size()),&tile,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        }else{
            const LONG cx=x+cell/2,cy=y+cell/2;
            HPEN pen=CreatePen(PS_SOLID,6,RGB(238,245,245));const auto oldPen=SelectObject(dc,pen);
            if(key==33){const POINT points[]{{cx-35,cy-12},{cx-35,cy+12},{cx+35,cy+12},{cx+35,cy-12}};Polyline(dc,points,4);}
            else if(key==34){const POINT points[]{{cx-40,cy},{cx-18,cy-25},{cx+38,cy-25},{cx+38,cy+25},{cx-18,cy+25},{cx-40,cy}};Polyline(dc,points,6);
                MoveToEx(dc,cx-5,cy-12,nullptr);LineTo(dc,cx+18,cy+12);MoveToEx(dc,cx-5,cy+12,nullptr);LineTo(dc,cx+18,cy-12);}
            else {const POINT points[]{{cx-31,cy},{cx-8,cy+24},{cx+34,cy-28}};Polyline(dc,points,3);}
            SelectObject(dc,oldPen);DeleteObject(pen);
        }
    }
    GdiFlush();std::vector<uint32_t> pixels(bits,bits+size_t(width)*height);for(auto& pixel:pixels)pixel|=0xff000000;
    SelectObject(dc,oldFont);DeleteObject(font);SelectObject(dc,old);DeleteObject(surface);DeleteDC(dc);
    if(!p.texture){D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=height;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;
        desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        if(FAILED(device->CreateTexture2D(&desc,nullptr,&p.texture)))return nullptr;}
    context->UpdateSubresource(p.texture.Get(),0,nullptr,pixels.data(),width*4,0);p.hover=frame.hoverKey;
    return p.texture.Get();
}
struct NativeCanvasRenderer::Impl {ComPtr<ID3D11Texture2D> texture;D3D11_TEXTURE2D_DESC description{};};
NativeCanvasRenderer::NativeCanvasRenderer():impl_(std::make_unique<Impl>()){}
NativeCanvasRenderer::~NativeCanvasRenderer()=default;
ID3D11Texture2D* NativeCanvasRenderer::crop(ID3D11Device* device,ID3D11DeviceContext* context,ID3D11Texture2D* source){
    if(!device||!context||!source)return nullptr;
    D3D11_TEXTURE2D_DESC desc{};source->GetDesc(&desc);
    // FOX centers its authored 16:9 menu canvas in a tall per-eye buffer.
    // Keep its original pixels and aspect rather than stretching a portrait
    // capture into the appearance panel.
    if(desc.ArraySize!=1||desc.SampleDesc.Count!=1||desc.Width*9>=desc.Height*16)return source;
    auto& p=*impl_;const UINT height=desc.Width*9/16;if(!height)return nullptr;
    const UINT top=(desc.Height-height)/2;auto output=desc;output.Height=height;
    output.MipLevels=output.ArraySize=1;output.Usage=D3D11_USAGE_DEFAULT;output.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    output.CPUAccessFlags=output.MiscFlags=0;
    if(!p.texture||p.description.Width!=output.Width||p.description.Height!=output.Height||p.description.Format!=output.Format){
        p.texture.Reset();if(FAILED(device->CreateTexture2D(&output,nullptr,&p.texture)))return nullptr;p.description=output;}
    const D3D11_BOX region{0,top,0,desc.Width,top+height,1};
    context->CopySubresourceRegion(p.texture.Get(),0,0,0,0,source,0,&region);return p.texture.Get();
}
}
