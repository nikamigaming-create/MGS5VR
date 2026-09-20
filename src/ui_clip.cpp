#include "mgs5vr/ui_clip.hpp"
#include "mgs5vr/log.hpp"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <MinHook.h>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace mgs5vr {
std::optional<UiClipPlanes> uiClipPlanes(const std::array<float,16>& canvas) noexcept {
    for(float f:canvas)if(!std::isfinite(f))return {};
    constexpr float xy[4][2]={{-1,-1},{1,-1},{1,1},{-1,1}};
    std::array<std::array<double,3>,4> corners{};
    for(size_t i=0;i<4;++i){
        for(size_t n=0;n<3;++n){const auto k=n==2?3:n;
            corners[i][n]=xy[i][0]*double(canvas[k])+xy[i][1]*double(canvas[4+k])+canvas[12+k];}
        if(corners[i][2]<=.00001)return {};
    }
    UiClipPlanes out{};
    for(size_t i=0;i<4;++i){const auto& a=corners[i];const auto& b=corners[(i+1)%4];
        double x=a[1]*b[2]-a[2]*b[1],y=a[2]*b[0]-a[0]*b[2],w=a[0]*b[1]-a[1]*b[0];
        const double length=std::sqrt(x*x+y*y+w*w);if(length<1e-10)return {};
        const double sign=x*canvas[12]+y*canvas[13]+w*canvas[15];
        if(std::abs(sign)<1e-10)return {};
        const double scale=(sign>0?1:-1)/length;
        out[i]={float(x*scale),float(y*scale),0,float(w*scale)};
    }
    return out;
}
namespace {
using Microsoft::WRL::ComPtr;
thread_local std::optional<UiClipPlanes> active;
thread_local std::array<float,4> activeBounds;
constexpr GUID signatureKey{0x4391f5a1,0x8d14,0x427b,{0xb3,0xda,0x4c,0x58,0x85,0xba,0x02,0x11}};
constexpr GUID shaderKey{0x4391f5a2,0x8d14,0x427b,{0xb3,0xda,0x4c,0x58,0x85,0xba,0x02,0x11}};
constexpr GUID rasterKey{0x4391f5a3,0x8d14,0x427b,{0xb3,0xda,0x4c,0x58,0x85,0xba,0x02,0x11}};
using CreateVs=HRESULT(STDMETHODCALLTYPE*)(ID3D11Device*,const void*,SIZE_T,ID3D11ClassLinkage*,ID3D11VertexShader**);
CreateVs originalVs{};
using Draw=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT);
using DrawIndexed=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,INT);
using DrawInstanced=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,UINT,UINT);
using DrawIndexedInstanced=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,UINT,INT,UINT);
Draw originalDraw{};DrawIndexed originalIndexed{};
DrawInstanced originalInstanced{};DrawIndexedInstanced originalIndexedInstanced{};
std::atomic_uint64_t clipped{},unavailable{},shadersSeen{},signaturesSeen{},scissored{};
std::string geometrySource(const void* bytes,SIZE_T length){
    ComPtr<ID3D11ShaderReflection> reflection;
    if(FAILED(D3DReflect(bytes,length,IID_PPV_ARGS(&reflection))))return {};
    D3D11_SHADER_DESC desc{};if(FAILED(reflection->GetDesc(&desc))||desc.OutputParameters>28)return {};
    std::ostringstream members,copy;std::string position;
    for(UINT n=0;n<desc.OutputParameters;++n){
        D3D11_SIGNATURE_PARAMETER_DESC p{};if(FAILED(reflection->GetOutputParameterDesc(n,&p)))return {};
        if(p.Stream||!p.SemanticName||!p.Mask||p.Mask>15)return {};
        // Existing distance outputs and tessellation are not replaced.
        if(p.SystemValueType!=D3D_NAME_UNDEFINED&&p.SystemValueType!=D3D_NAME_POSITION)return {};
        const char* type=p.ComponentType==D3D_REGISTER_COMPONENT_FLOAT32?"float":
            p.ComponentType==D3D_REGISTER_COMPONENT_UINT32?"uint":p.ComponentType==D3D_REGISTER_COMPONENT_SINT32?"int":nullptr;
        if(!type)return {};
        unsigned count=0;for(unsigned bits=p.Mask;bits;bits>>=1)++count;
        // Packed non-leading components cannot be represented by this wrapper.
        if(p.Mask!=(1u<<count)-1)return {};
        const auto field="v"+std::to_string(n);
        members<<(p.ComponentType==D3D_REGISTER_COMPONENT_FLOAT32?"":"nointerpolation ")
            <<type<<count<<' '<<field<<':'<<p.SemanticName<<p.SemanticIndex<<";\n";
        copy<<"o."<<field<<"=v[i]."<<field<<";";
        if(p.SystemValueType==D3D_NAME_POSITION){if(count!=4)return {};position=field;}
    }
    if(position.empty())return {};
    return "cbuffer CanvasClip:register(b0){float4 planes[4];};\nstruct V{"+members.str()+"};\nstruct O{"+
        members.str()+"float4 distances:SV_ClipDistance0;};\n[maxvertexcount(3)] void main(triangle V v[3],inout TriangleStream<O> stream){"
        "[unroll]for(int i=0;i<3;i++){O o;"+copy.str()+"o.distances=float4(dot(v[i]."+position+
        ",planes[0]),dot(v[i]."+position+",planes[1]),dot(v[i]."+position+",planes[2]),dot(v[i]."+position+
        ",planes[3]));stream.Append(o);}}";
}
HRESULT STDMETHODCALLTYPE createVs(ID3D11Device* d,const void* bytes,SIZE_T size,ID3D11ClassLinkage* linkage,ID3D11VertexShader** out){
    const auto result=originalVs(d,bytes,size,linkage,out);
    if(SUCCEEDED(result)&&out&&*out)try{++shadersSeen;const auto source=geometrySource(bytes,size);
        if(!source.empty()){(*out)->SetPrivateData(signatureKey,static_cast<UINT>(source.size()),source.data());++signaturesSeen;}
    }catch(...){}
    return result;
}
struct ClipDraw {
    ID3D11DeviceContext* context{};
    ComPtr<ID3D11GeometryShader> prior;
    ComPtr<ID3D11Buffer> priorConstants;
    ComPtr<ID3D11RasterizerState> priorRaster;
    std::array<D3D11_RECT,16> priorRects{};
    UINT rectCount=16;
    bool scissorBound{};
    bool bound{};
    explicit ClipDraw(ID3D11DeviceContext* c):context(c){
        if(!active)return;
        // Native shaders can predate our device hook. The draw always gets a
        // projected canvas bound; a reflected shader additionally clips the
        // four exact edges when the handheld canvas is rotated.
        D3D11_VIEWPORT viewport{};UINT viewportCount=1;c->RSGetViewports(&viewportCount,&viewport);
        if(viewportCount==1&&viewport.Width>0&&viewport.Height>0){
            c->RSGetState(&priorRaster);c->RSGetScissorRects(&rectCount,priorRects.data());
            D3D11_RASTERIZER_DESC desc{};
            if(priorRaster)priorRaster->GetDesc(&desc);
            else {desc.FillMode=D3D11_FILL_SOLID;desc.CullMode=D3D11_CULL_BACK;desc.DepthClipEnable=TRUE;}
            ComPtr<ID3D11RasterizerState> raster;
            if(desc.ScissorEnable)raster=priorRaster;
            else {
                UINT size=sizeof(ID3D11RasterizerState*);
                if(!priorRaster||FAILED(priorRaster->GetPrivateData(rasterKey,&size,raster.GetAddressOf()))){
                    ComPtr<ID3D11Device> device;c->GetDevice(&device);desc.ScissorEnable=TRUE;
                    if(SUCCEEDED(device->CreateRasterizerState(&desc,&raster))&&priorRaster)
                        priorRaster->SetPrivateDataInterface(rasterKey,raster.Get());
                }
            }
            if(raster){
                const auto x=[&](float ndc){return viewport.TopLeftX+(ndc+1)*.5f*viewport.Width;};
                const auto y=[&](float ndc){return viewport.TopLeftY+(1-ndc)*.5f*viewport.Height;};
                D3D11_RECT rect{LONG(std::floor(x(activeBounds[0]))),LONG(std::floor(y(activeBounds[3]))),
                    LONG(std::ceil(x(activeBounds[2]))),LONG(std::ceil(y(activeBounds[1])))};
                // Preserve native clipping too; enabling our bound must never
                // reveal pixels that the native menu already clipped away.
                D3D11_RASTERIZER_DESC old{};if(priorRaster)priorRaster->GetDesc(&old);
                if(old.ScissorEnable&&rectCount){rect.left=std::max(rect.left,priorRects[0].left);
                    rect.top=std::max(rect.top,priorRects[0].top);rect.right=std::min(rect.right,priorRects[0].right);
                    rect.bottom=std::min(rect.bottom,priorRects[0].bottom);}
                rect.right=std::max(rect.left,rect.right);rect.bottom=std::max(rect.top,rect.bottom);
                c->RSSetState(raster.Get());c->RSSetScissorRects(1,&rect);scissorBound=true;
                if(!scissored.fetch_add(1))log("Native menu draws bounded to their projected handheld canvas");
            }
        }
        D3D11_PRIMITIVE_TOPOLOGY topology{};c->IAGetPrimitiveTopology(&topology);
        if(topology!=D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST&&topology!=D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP)return;
        c->GSGetShader(&prior,nullptr,nullptr);if(prior)return;
        ComPtr<ID3D11HullShader> hull;c->HSGetShader(&hull,nullptr,nullptr);if(hull)return;
        ComPtr<ID3D11VertexShader> vs;c->VSGetShader(&vs,nullptr,nullptr);if(!vs)return;
        ComPtr<ID3D11GeometryShader> gs;UINT size=sizeof(ID3D11GeometryShader*);
        if(FAILED(vs->GetPrivateData(shaderKey,&size,gs.GetAddressOf()))){
            size=0;if(FAILED(vs->GetPrivateData(signatureKey,&size,nullptr))||!size||size>32768){missing("signature");return;}
            std::string source(size,'\0');if(FAILED(vs->GetPrivateData(signatureKey,&size,source.data())))return;
            ComPtr<ID3DBlob> shader,errors;
            if(FAILED(D3DCompile(source.data(),source.size(),nullptr,nullptr,nullptr,"main","gs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&shader,&errors))){
                if(!unavailable.fetch_add(1))log("Native UI clip shader compilation failed: "+std::string(errors?static_cast<char*>(errors->GetBufferPointer()):"unknown"));return;}
            ComPtr<ID3D11Device> device;c->GetDevice(&device);
            if(FAILED(device->CreateGeometryShader(shader->GetBufferPointer(),shader->GetBufferSize(),nullptr,&gs))){missing("creation");return;}
            vs->SetPrivateDataInterface(shaderKey,gs.Get());
        }
        struct Local {ComPtr<ID3D11Device> device;ComPtr<ID3D11Buffer> constants;};
        thread_local Local local;
        ComPtr<ID3D11Device> device;c->GetDevice(&device);
        if(local.device.Get()!=device.Get()){local={};local.device=device;}
        if(!local.constants){D3D11_BUFFER_DESC desc{};desc.ByteWidth=sizeof(UiClipPlanes);desc.Usage=D3D11_USAGE_DYNAMIC;
            desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;desc.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
            if(FAILED(device->CreateBuffer(&desc,nullptr,&local.constants))){missing("constants");return;}}
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if(FAILED(c->Map(local.constants.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped))){missing("upload");return;}
        std::memcpy(mapped.pData,active->data(),sizeof(UiClipPlanes));c->Unmap(local.constants.Get(),0);
        c->GSGetConstantBuffers(0,1,&priorConstants);auto* buffer=local.constants.Get();
        c->GSSetConstantBuffers(0,1,&buffer);c->GSSetShader(gs.Get(),nullptr,0);bound=true;
        if(!clipped.fetch_add(1))log("Native menu geometry clipped to its tracked canvas on the native draw thread");
    }
    static void missing(const char* reason){if(!unavailable.fetch_add(1))log(std::string("Native UI exact edge clipping unavailable; projected bound retained: ")+reason
        +" shaders="+std::to_string(shadersSeen.load())+" signatures="+std::to_string(signaturesSeen.load()));}
    ~ClipDraw(){if(bound){context->GSSetShader(prior.Get(),nullptr,0);auto* b=priorConstants.Get();context->GSSetConstantBuffers(0,1,&b);}
        if(scissorBound){context->RSSetState(priorRaster.Get());context->RSSetScissorRects(rectCount,priorRects.data());}}
};
void STDMETHODCALLTYPE draw(ID3D11DeviceContext* c,UINT a,UINT b){ClipDraw clip(c);originalDraw(c,a,b);}
void STDMETHODCALLTYPE indexed(ID3D11DeviceContext* c,UINT a,UINT b,INT d){ClipDraw clip(c);originalIndexed(c,a,b,d);}
void STDMETHODCALLTYPE instanced(ID3D11DeviceContext* c,UINT a,UINT b,UINT d,UINT e){ClipDraw clip(c);originalInstanced(c,a,b,d,e);}
void STDMETHODCALLTYPE indexedInstanced(ID3D11DeviceContext* c,UINT a,UINT b,UINT d,INT e,UINT f){ClipDraw clip(c);originalIndexedInstanced(c,a,b,d,e,f);}
}
UiClipScope::UiClipScope(const std::array<float,16>& canvas) noexcept:saved_(active),savedBounds_(activeBounds){
    active=uiClipPlanes(canvas);if(!active)return;
    activeBounds={1,1,-1,-1};
    for(float x:{-1.f,1.f})for(float y:{-1.f,1.f}){
        const float w=x*canvas[3]+y*canvas[7]+canvas[15];
        const float px=std::clamp((x*canvas[0]+y*canvas[4]+canvas[12])/w,-1.f,1.f);
        const float py=std::clamp((x*canvas[1]+y*canvas[5]+canvas[13])/w,-1.f,1.f);
        activeBounds[0]=std::min(activeBounds[0],px);activeBounds[1]=std::min(activeBounds[1],py);
        activeBounds[2]=std::max(activeBounds[2],px);activeBounds[3]=std::max(activeBounds[3],py);
    }
}
UiClipScope::~UiClipScope(){active=saved_;activeBounds=savedBounds_;}
void installUiClip(ID3D11Device* device){
    ComPtr<ID3D11DeviceContext> context;if(FAILED(device->CreateDeferredContext(0,&context)))throw std::runtime_error("UI clip context unavailable");
    auto** d=*reinterpret_cast<void***>(device);auto** c=*reinterpret_cast<void***>(context.Get());
    const auto hook=[](void* address,void* replacement,void** original){
        if(MH_CreateHook(address,replacement,original)!=MH_OK||MH_EnableHook(address)!=MH_OK)throw std::runtime_error("UI clipping hook failed");};
    hook(d[12],reinterpret_cast<void*>(&createVs),reinterpret_cast<void**>(&originalVs));
    hook(c[12],reinterpret_cast<void*>(&indexed),reinterpret_cast<void**>(&originalIndexed));
    hook(c[13],reinterpret_cast<void*>(&draw),reinterpret_cast<void**>(&originalDraw));
    hook(c[20],reinterpret_cast<void*>(&indexedInstanced),reinterpret_cast<void**>(&originalIndexedInstanced));
    hook(c[21],reinterpret_cast<void*>(&instanced),reinterpret_cast<void**>(&originalInstanced));
    log("Native menu canvas clipping hooks installed");
}
}
