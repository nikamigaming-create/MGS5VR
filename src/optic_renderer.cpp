#include "mgs5vr/optic_renderer.hpp"
#include "mgs5vr/core.hpp"
#include "mgs5vr/optic_rig.hpp"
#include "mgs5vr/optic_markers.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/log.hpp"
#include "mgs5vr/scene_capture.hpp"
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace {
using Microsoft::WRL::ComPtr;

// Vertex payload decoded directly from the retail Fox Engine FMDL.  The
// positions and triangle indices are not authored or simplified. FMDL
// Studio applies the same X reflection when importing FOX assets.
struct Vertex {
    float position[3]{};
    float normal[3]{};
    float uv[2]{};
};

struct Constants {
    float mvp[16]{};
    float world[16]{};
    float baseColor[4]{};
};

struct LensVertex {
    float position[3]{};
    float aperture[2]{};
};

struct LensConstants {
    float mvp[16]{};
    float sampleCenter[4]{}; // source UV center, magnification, unused
    float viewport[4]{}; // target width, height, unused, unused
};

struct MarkerVertex {float x{},y{};float r{},g{},b{},a{1};};

// Center of the large, front-facing ocular recess on the imported retail FMDL.
// The runtime reflects FMDL X on import; this is the exact post-reflection
// socket measured from the +Z rim, with a small forward relief bias.
constexpr auto ocularCenter=mgs5vr::binocularOcularCenter;
constexpr float ocularRadius=mgs5vr::binocularOcularRadius;

struct Section0Info { uint16_t type{}, count{}; uint32_t offset{}; };
struct Section1Info { uint32_t type{}, offset{}, length{}; };
struct MeshFormat { uint8_t bufferOffset{}, vertexFormatCount{}, length{}, type{}; uint32_t offset{}; };
struct VertexFormat { uint8_t type{}, dataType{}; uint16_t offset{}; };

struct RetailModel {
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    std::filesystem::path path;
    float min[3]{std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),std::numeric_limits<float>::max()};
    float max[3]{std::numeric_limits<float>::lowest(),std::numeric_limits<float>::lowest(),std::numeric_limits<float>::lowest()};
};

struct RetailTexture {
    std::filesystem::path path;
    DXGI_FORMAT format{DXGI_FORMAT_UNKNOWN};
    uint32_t width{},height{};
    std::vector<std::vector<uint8_t>> mipData;
};

template<class T>
bool readAt(const std::vector<uint8_t>& bytes,size_t offset,T& value){
    static_assert(std::is_trivially_copyable_v<T>);
    if(offset>bytes.size()||sizeof(T)>bytes.size()-offset)return false;
    std::memcpy(&value,bytes.data()+offset,sizeof(T));
    return true;
}

float halfToFloat(uint16_t value){
    const uint32_t sign=(static_cast<uint32_t>(value&0x8000u))<<16;
    const uint32_t exponent=(value>>10)&0x1fu;
    uint32_t mantissa=value&0x3ffu;
    uint32_t bits{};
    if(exponent==0){
        if(mantissa==0)bits=sign;
        else{
            int shift=0;
            while((mantissa&0x400u)==0){mantissa<<=1;++shift;}
            mantissa&=0x3ffu;
            bits=sign|static_cast<uint32_t>(127-15-shift)<<23|(mantissa<<13);
        }
    }else if(exponent==0x1fu)bits=sign|0x7f800000u|(mantissa<<13);
    else bits=sign|((exponent+112u)<<23)|(mantissa<<13);
    return std::bit_cast<float>(bits);
}

bool finiteVertex(const Vertex& vertex){
    for(float value:vertex.position)if(!std::isfinite(value))return false;
    for(float value:vertex.normal)if(!std::isfinite(value))return false;
    for(float value:vertex.uv)if(!std::isfinite(value))return false;
    return true;
}

std::filesystem::path gameRoot(){
    std::array<wchar_t,32768> path{};
    const auto count=GetModuleFileNameW(nullptr,path.data(),static_cast<DWORD>(path.size()));
    if(!count||count>=path.size())return {};
    return std::filesystem::path(path.data()).parent_path();
}

std::filesystem::path retailBinocularPath(){
    const auto root=gameRoot();
    if(root.empty())return {};
    std::array<wchar_t,32768> configured{};
    const auto ini=root/L"mgs5vr.ini";
    const auto length=GetPrivateProfileStringW(L"optics",L"binocular_fmdl",
        L"retail-assets\\Assets\\tpp\\item\\tel\\Scenes\\tel0_main0_def.fmdl",
        configured.data(),static_cast<DWORD>(configured.size()),ini.c_str());
    if(!length||length>=configured.size())return {};
    std::filesystem::path path(configured.data());
    if(path.is_relative())path=root/path;
    return path;
}

std::filesystem::path retailBinocularDiffusePath(){
    const auto root=gameRoot();
    if(root.empty())return {};
    std::array<wchar_t,32768> configured{};
    const auto ini=root/L"mgs5vr.ini";
    const auto length=GetPrivateProfileStringW(L"optics",L"binocular_diffuse_dds",
        L"retail-assets\\Assets\\tpp\\item\\tel\\Pictures\\tel0_main0_def_c00_bsm.dds",
        configured.data(),static_cast<DWORD>(configured.size()),ini.c_str());
    if(!length||length>=configured.size())return {};
    std::filesystem::path path(configured.data());
    if(path.is_relative())path=root/path;
    return path;
}

std::optional<RetailModel> readRetailFmdl(const std::filesystem::path& path,std::string& failure){
    if(path.empty()){failure="game module path unavailable";return {};}
    std::ifstream input(path,std::ios::binary|std::ios::ate);
    if(!input){failure="file not found: "+path.string();return {};}
    const auto size=input.tellg();
    if(size<=0||size>static_cast<std::streamoff>(64*1024*1024)){
        failure="invalid file size: "+path.string();return {};
    }
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    input.seekg(0);
    if(!input.read(reinterpret_cast<char*>(bytes.data()),size)){
        failure="file read failed: "+path.string();return {};
    }

    uint32_t signature{},section0Offset{},section1Offset{},section0Count{},section1Count{};
    float version{};uint64_t sectionInfoOffset{};
    if(!readAt(bytes,0,signature)||!readAt(bytes,4,version)||!readAt(bytes,8,sectionInfoOffset)
       ||!readAt(bytes,0x20,section0Count)||!readAt(bytes,0x24,section1Count)
       ||!readAt(bytes,0x28,section0Offset)||!readAt(bytes,0x30,section1Offset)
       ||signature!=0x4c444d46u||!std::isfinite(version)||std::abs(version-2.04f)>.02f){
        failure="not a supported retail FMDL: "+path.string();return {};
    }
    if(section0Count>64||section1Count>16||sectionInfoOffset>bytes.size()
       ||section0Count>(bytes.size()-static_cast<size_t>(sectionInfoOffset))/8u
       ||section1Count>(bytes.size()-static_cast<size_t>(sectionInfoOffset)-section0Count*8u)/12u){
        failure="invalid FMDL section table: "+path.string();return {};
    }
    std::array<std::optional<Section0Info>,32> sections{};
    for(uint32_t i=0;i<section0Count;++i){
        Section0Info info{};
        if(!readAt(bytes,static_cast<size_t>(sectionInfoOffset)+i*8,info)){
            failure="truncated FMDL section table: "+path.string();return {};
        }
        if(info.type<sections.size())sections[info.type]=info;
    }
    std::optional<Section1Info> bufferSection;
    const auto section1Table=static_cast<size_t>(sectionInfoOffset)+section0Count*8u;
    for(uint32_t i=0;i<section1Count;++i){
        Section1Info info{};
        if(!readAt(bytes,section1Table+i*12,info)){
            failure="truncated FMDL section table: "+path.string();return {};
        }
        if(info.type==2)bufferSection=info;
    }
    const auto need=[&](uint16_t type)->std::optional<Section0Info>{
        if(type>=sections.size()||!sections[type])return {};
        return sections[type];
    };
    const auto meshInfo=need(3),meshFormatInfo=need(9),meshFormats=need(10),vertexFormats=need(11),bufferOffsets=need(14);
    if(!meshInfo||!meshFormatInfo||!meshFormats||!vertexFormats||!bufferOffsets||!bufferSection){
        failure="retail FMDL is missing mesh buffer sections: "+path.string();return {};
    }
    const auto sectionBase=[&](uint32_t base,uint32_t offset,size_t length)->bool{
        return base<=bytes.size()&&offset<=bytes.size()-base&&length<=bytes.size()-base-offset;
    };
    if(meshInfo->count==0||meshInfo->count>64
       ||!sectionBase(section0Offset,meshInfo->offset,static_cast<size_t>(meshInfo->count)*0x30u)
       ||!sectionBase(section0Offset,meshFormatInfo->offset,static_cast<size_t>(meshInfo->count)*8u)
       ||!sectionBase(section0Offset,meshFormats->offset,static_cast<size_t>(meshFormats->count)*8u)
       ||!sectionBase(section0Offset,vertexFormats->offset,static_cast<size_t>(vertexFormats->count)*4u)
       ||!sectionBase(section0Offset,bufferOffsets->offset,static_cast<size_t>(bufferOffsets->count)*16u)
       ||!sectionBase(section1Offset,bufferSection->offset,bufferSection->length)){
        failure="retail FMDL section bounds failed: "+path.string();return {};
    }
    std::vector<std::array<uint32_t,3>> buffers;
    for(uint16_t i=0;i<bufferOffsets->count;++i){
        uint32_t unknown{},length{},offset{};
        const auto at=static_cast<size_t>(section0Offset)+bufferOffsets->offset+i*16u;
        if(!readAt(bytes,at,unknown)||!readAt(bytes,at+4,length)||!readAt(bytes,at+8,offset)
           ||!sectionBase(section1Offset+bufferSection->offset,offset,length)){
            failure="retail FMDL buffer bounds failed: "+path.string();return {};
        }
        buffers.push_back({unknown,length,offset});
    }
    const auto bufferBase=static_cast<size_t>(section1Offset)+bufferSection->offset;
    RetailModel model;model.path=path;
    for(uint16_t meshIndex=0;meshIndex<meshInfo->count;++meshIndex){
        const auto meshAt=static_cast<size_t>(section0Offset)+meshInfo->offset+meshIndex*0x30u;
        uint16_t vertexCount{};uint32_t firstFaceVertexIndex{},faceVertexCount{};
        if(!readAt(bytes,meshAt+0x0a,vertexCount)||!readAt(bytes,meshAt+0x10,firstFaceVertexIndex)
           ||!readAt(bytes,meshAt+0x14,faceVertexCount)||vertexCount==0||faceVertexCount==0||faceVertexCount%3!=0){
            failure="retail FMDL mesh counts invalid: "+path.string();return {};
        }
        uint8_t meshFormatCount{},vertexFormatCount{};uint16_t firstMeshFormat{},firstVertexFormat{};
        const auto formatAt=static_cast<size_t>(section0Offset)+meshFormatInfo->offset+meshIndex*8u;
        if(!readAt(bytes,formatAt,meshFormatCount)||!readAt(bytes,formatAt+1,vertexFormatCount)
           ||!readAt(bytes,formatAt+4,firstMeshFormat)||!readAt(bytes,formatAt+6,firstVertexFormat)
           ||meshFormatCount<2||firstMeshFormat>meshFormats->count-meshFormatCount
           ||firstVertexFormat>vertexFormats->count-vertexFormatCount){
            failure="retail FMDL vertex formats invalid: "+path.string();return {};
        }
        std::vector<MeshFormat> formats(meshFormatCount);std::vector<VertexFormat> attributes(vertexFormatCount);
        for(uint8_t i=0;i<meshFormatCount;++i){
            const auto at=static_cast<size_t>(section0Offset)+meshFormats->offset+(firstMeshFormat+i)*8u;
            if(!readAt(bytes,at,formats[i].bufferOffset)||!readAt(bytes,at+1,formats[i].vertexFormatCount)
               ||!readAt(bytes,at+2,formats[i].length)||!readAt(bytes,at+3,formats[i].type)
               ||!readAt(bytes,at+4,formats[i].offset)||formats[i].bufferOffset>=buffers.size()
               ||formats[i].offset>buffers[formats[i].bufferOffset][1]
               ||formats[i].length>buffers[formats[i].bufferOffset][1]-formats[i].offset){
                failure="retail FMDL mesh format bounds failed: "+path.string();return {};
            }
        }
        for(uint8_t i=0;i<vertexFormatCount;++i){
            const auto at=static_cast<size_t>(section0Offset)+vertexFormats->offset+(firstVertexFormat+i)*4u;
            if(!readAt(bytes,at,attributes[i].type)||!readAt(bytes,at+1,attributes[i].dataType)
               ||!readAt(bytes,at+2,attributes[i].offset)){
                failure="retail FMDL attribute format read failed: "+path.string();return {};
            }
        }
        const auto& positionFormat=formats[0];
        const auto& attributeFormat=formats[1];
        if(positionFormat.length<12||attributeFormat.length==0||positionFormat.bufferOffset>=buffers.size()
           ||attributeFormat.bufferOffset>=buffers.size()){
            failure="retail FMDL position/attribute streams invalid: "+path.string();return {};
        }
        const auto positionBase=bufferBase+buffers[positionFormat.bufferOffset][2]+positionFormat.offset;
        const auto attributeBase=bufferBase+buffers[attributeFormat.bufferOffset][2]+attributeFormat.offset;
        if(positionFormat.length>std::numeric_limits<size_t>::max()/vertexCount
           ||attributeFormat.length>std::numeric_limits<size_t>::max()/vertexCount
           ||positionBase>bytes.size()-static_cast<size_t>(positionFormat.length)*vertexCount
           ||attributeBase>bytes.size()-static_cast<size_t>(attributeFormat.length)*vertexCount){
            failure="retail FMDL vertex stream bounds failed: "+path.string();return {};
        }
        const auto firstVertex=static_cast<uint32_t>(model.vertices.size());
        if(model.vertices.size()>std::numeric_limits<uint16_t>::max()-vertexCount){
            failure="retail FMDL is too large for its index format: "+path.string();return {};
        }
        for(uint16_t vertexIndex=0;vertexIndex<vertexCount;++vertexIndex){
            Vertex vertex{};const auto positionAt=positionBase+static_cast<size_t>(vertexIndex)*positionFormat.length;
            float x{},y{},z{};
            if(!readAt(bytes,positionAt,x)||!readAt(bytes,positionAt+4,y)||!readAt(bytes,positionAt+8,z)){
                failure="retail FMDL position read failed: "+path.string();return {};
            }
            vertex.position[0]=-x;vertex.position[1]=y;vertex.position[2]=z;
            const auto attributeAt=attributeBase+static_cast<size_t>(vertexIndex)*attributeFormat.length;
            for(const auto& attribute:attributes){
                const auto at=attributeAt+attribute.offset;
                if(attribute.type==2){
                    uint16_t h0{},h1{},h2{};
                    if(static_cast<size_t>(attribute.offset)+6>attributeFormat.length||!readAt(bytes,at,h0)||!readAt(bytes,at+2,h1)||!readAt(bytes,at+4,h2)){
                        failure="retail FMDL normal read failed: "+path.string();return {};
                    }
                    vertex.normal[0]=-halfToFloat(h0);vertex.normal[1]=halfToFloat(h1);vertex.normal[2]=halfToFloat(h2);
                }else if(attribute.type==8){
                    uint16_t h0{},h1{};
                    if(static_cast<size_t>(attribute.offset)+4>attributeFormat.length||!readAt(bytes,at,h0)||!readAt(bytes,at+2,h1)){
                        failure="retail FMDL UV read failed: "+path.string();return {};
                    }
            // FMDL and DDS already use D3D's texture origin. Blender's import
            // conversion flips V for its own UV convention; doing that here
            // maps the decals, lens and side panels onto unrelated faces.
            vertex.uv[0]=halfToFloat(h0);vertex.uv[1]=halfToFloat(h1);
                }
            }
            if(!finiteVertex(vertex)){failure="retail FMDL contains non-finite vertex data: "+path.string();return {};}
            for(int axis=0;axis<3;++axis){
                model.min[axis]=std::min(model.min[axis],vertex.position[axis]);
                model.max[axis]=std::max(model.max[axis],vertex.position[axis]);
            }
            model.vertices.push_back(vertex);
        }
        const auto indexBuffer=static_cast<uint16_t>(buffers.size()-1);
        const auto indexBase=bufferBase+buffers[indexBuffer][2];
        const auto indexAt=indexBase+static_cast<size_t>(firstFaceVertexIndex)*2u;
        if(indexAt>bytes.size()-static_cast<size_t>(faceVertexCount)*2u){
            failure="retail FMDL index stream bounds failed: "+path.string();return {};
        }
        for(uint32_t index=0;index<faceVertexCount;++index){
            uint16_t value{};
            if(!readAt(bytes,indexAt+static_cast<size_t>(index)*2u,value)||value>=vertexCount){
                failure="retail FMDL index value invalid: "+path.string();return {};
            }
            model.indices.push_back(static_cast<uint16_t>(firstVertex+value));
        }
    }
    if(model.vertices.empty()||model.indices.empty()||model.indices.size()%3!=0){
        failure="retail FMDL produced no drawable triangles: "+path.string();return {};
    }
    return model;
}

std::optional<RetailTexture> readRetailDds(const std::filesystem::path& path,std::string& failure){
    if(path.empty()){failure="texture path unavailable";return {};}
    std::ifstream input(path,std::ios::binary|std::ios::ate);
    if(!input){failure="file not found: "+path.string();return {};}
    const auto size=input.tellg();
    if(size<128||size>static_cast<std::streamoff>(128*1024*1024)){
        failure="invalid DDS file size: "+path.string();return {};
    }
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    input.seekg(0);
    if(!input.read(reinterpret_cast<char*>(bytes.data()),size)){
        failure="DDS file read failed: "+path.string();return {};
    }
    uint32_t magic{},headerSize{},height{},width{},mipCount{},pixelFormatSize{},pixelFormatFlags{},fourcc{};
    if(!readAt(bytes,0,magic)||!readAt(bytes,4,headerSize)||!readAt(bytes,12,height)||!readAt(bytes,16,width)
       ||!readAt(bytes,28,mipCount)||!readAt(bytes,76,pixelFormatSize)||!readAt(bytes,80,pixelFormatFlags)
       ||!readAt(bytes,84,fourcc)||magic!=0x20534444u||headerSize!=124||pixelFormatSize!=32||pixelFormatFlags!=4u
       ||!width||!height||mipCount==0||mipCount>16){
        failure="not a supported DDS texture: "+path.string();return {};
    }
    DXGI_FORMAT format=DXGI_FORMAT_UNKNOWN;uint32_t blockBytes{};
    if(fourcc==0x31545844u){format=DXGI_FORMAT_BC1_UNORM;blockBytes=8;}
    else if(fourcc==0x35545844u){format=DXGI_FORMAT_BC3_UNORM;blockBytes=16;}
    else {failure="retail binocular texture is not DXT1/DXT5: "+path.string();return {};}
    RetailTexture texture;texture.path=path;texture.format=format;texture.width=width;texture.height=height;
    size_t dataAt=128;uint32_t levelWidth=width,levelHeight=height;
    for(uint32_t level=0;level<mipCount;++level){
        const auto blocksX=std::max<uint32_t>(1,(levelWidth+3)/4),blocksY=std::max<uint32_t>(1,(levelHeight+3)/4);
        const uint64_t levelBytes=static_cast<uint64_t>(blocksX)*blocksY*blockBytes;
        if(levelBytes>std::numeric_limits<size_t>::max()||dataAt>bytes.size()||levelBytes>bytes.size()-dataAt){
            failure="retail binocular DDS mip bounds failed: "+path.string();return {};
        }
        texture.mipData.emplace_back(bytes.begin()+static_cast<std::ptrdiff_t>(dataAt),
            bytes.begin()+static_cast<std::ptrdiff_t>(dataAt+static_cast<size_t>(levelBytes)));
        dataAt+=static_cast<size_t>(levelBytes);levelWidth=std::max<uint32_t>(1,levelWidth/2);levelHeight=std::max<uint32_t>(1,levelHeight/2);
    }
    if(dataAt!=bytes.size()){
        failure="retail binocular DDS has trailing or missing mip data: "+path.string();return {};
    }
    return texture;
}

const char* vertexShaderSource=R"HLSL(
cbuffer ViewModel : register(b0) {
    row_major float4x4 mvp;
    row_major float4x4 world;
    float4 baseColor;
};
struct VSIn { float3 position : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD0; };
struct VSOut { float4 position : SV_POSITION; float3 normal : NORMAL0; float2 uv : TEXCOORD0; };
VSOut main(VSIn input) {
    VSOut output;
    output.position = mul(float4(input.position, 1.0), mvp);
    output.normal = mul(float4(input.normal, 0.0), world).xyz;
    output.uv = input.uv;
    return output;
}
)HLSL";

const char* pixelShaderSource=R"HLSL(
cbuffer ViewModel : register(b0) {
    row_major float4x4 mvp;
    row_major float4x4 world;
    float4 baseColor;
};
Texture2D retailDiffuse : register(t0);
SamplerState retailSampler : register(s0);
struct PSIn { float4 position : SV_POSITION; float3 normal : NORMAL0; float2 uv : TEXCOORD0; };
float4 main(PSIn input) : SV_TARGET {
    float3 normal = normalize(input.normal);
    float3 light = normalize(float3(-0.45, 0.75, -0.55));
    // The retail BSM is authored as an sRGB color texture.  The SRV below
    // performs the decode; keep the material response restrained so the
    // decals and wear remain the source asset's colors instead of becoming a
    // pale untextured block under the simulator's near-field lighting.
    float diffuse = 0.22 + 0.78 * saturate(dot(normal, light));
    float3 albedo = retailDiffuse.Sample(retailSampler,input.uv).rgb;
    return float4(albedo * diffuse, baseColor.a);
}
)HLSL";

const char* lensVertexShaderSource=R"HLSL(
cbuffer LensView : register(b0) {
    row_major float4x4 mvp;
    float4 sampleCenter;
    float4 viewport;
};
struct VSIn { float3 position : POSITION; float2 aperture : APERTURE; };
struct VSOut { float4 position : SV_POSITION; float2 aperture : APERTURE; };
VSOut main(VSIn input) {
    VSOut output;
    output.position = mul(float4(input.position, 1.0), mvp);
    output.aperture = input.aperture;
    // At the exit pupil, open the field of view smoothly without moving the
    // physical housing or either tracked eye through the face.
    output.position.xy=lerp(output.position.xy,
        float2(-input.aperture.x,input.aperture.y)*0.96*output.position.w,sampleCenter.w);
    return output;
}
)HLSL";

const char* lensPixelShaderSource=R"HLSL(
cbuffer LensView : register(b0) {
    row_major float4x4 mvp;
    float4 sampleCenter;
    float4 viewport;
};
Texture2D nativeScene : register(t0);
SamplerState sceneSampler : register(s0);
struct PSIn { float4 position : SV_POSITION; float2 aperture : APERTURE; };
float4 main(PSIn input) : SV_TARGET {
    if (dot(input.aperture,input.aperture)>1.0) discard;
    // Perspective-correct coordinates on the physical ocular, independent
    // of its size or position in either HMD eye. Zoom is rendered by the
    // device camera, never manufactured by cropping the HMD image.
    float2 sourceUv=float2(0.5-0.5*input.aperture.x,0.5-0.5*input.aperture.y);
    float4 color=nativeScene.SampleLevel(sceneSampler,saturate(sourceUv),0.0);
    float edge=1.0-smoothstep(0.92,1.0,length(input.aperture));
    color.rgb*=lerp(1.0,edge,sampleCenter.w);
    return color;
}
)HLSL";

struct Resources {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11Buffer> vertices;
    ComPtr<ID3D11Buffer> indices;
    ComPtr<ID3D11Buffer> constants;
    ComPtr<ID3D11Buffer> lensVertices;
    ComPtr<ID3D11Buffer> lensConstants;
    ComPtr<ID3D11VertexShader> vertexShader;
    ComPtr<ID3D11PixelShader> pixelShader;
    ComPtr<ID3D11VertexShader> lensVertexShader;
    ComPtr<ID3D11PixelShader> lensPixelShader;
    ComPtr<ID3D11ShaderResourceView> diffuse;
    ComPtr<ID3D11Texture2D> sceneCopy;
    ComPtr<ID3D11ShaderResourceView> scene;
    ComPtr<ID3D11RenderTargetView> sceneOverlayTarget;
    ComPtr<ID3D11Buffer> markerVertices;
    ComPtr<ID3D11VertexShader> markerVertexShader;
    ComPtr<ID3D11PixelShader> markerPixelShader;
    ComPtr<ID3D11InputLayout> markerInputLayout;
    ComPtr<ID3D11RasterizerState> markerRasterizer;
    ComPtr<ID3D11DepthStencilState> markerDepth;
    ComPtr<ID3D11SamplerState> sampler;
    ComPtr<ID3D11SamplerState> lensSampler;
    ComPtr<ID3D11InputLayout> inputLayout;
    ComPtr<ID3D11InputLayout> lensInputLayout;
    ComPtr<ID3D11RasterizerState> rasterizer;
    ComPtr<ID3D11RasterizerState> lensRasterizer;
    ComPtr<ID3D11DepthStencilState> depthStencil;
    ComPtr<ID3D11DepthStencilState> reversedDepthStencil;
    ComPtr<ID3D11DepthStencilState> lensDepthStencil;
    ComPtr<ID3D11DepthStencilState> reversedLensDepthStencil;
    ComPtr<ID3D11BlendState> blend;
    ComPtr<ID3D11Texture2D> housingDepth;
    ComPtr<ID3D11DepthStencilView> housingDepthView;
    UINT depthWidth{},depthHeight{},depthSamples{};
    D3D11_TEXTURE2D_DESC sceneDescription{};
    UINT indexCount{};
    bool attempted{};
    bool ready{};
    bool reported{};
    bool lensReported{};
    bool lensFailureReported{};
    bool sourceReported{};
    bool depthReported{};
    bool markerReported{};
    uint64_t lensTraceCalls{};
};

std::mutex rendererMutex;
Resources resources;

void logHresult(const char* what,HRESULT result,ID3DBlob* errors){
    std::string message=what;message+=" hr=0x";char hex[16]{};
    std::snprintf(hex,sizeof(hex),"%08lx",static_cast<unsigned long>(result));message+=hex;
    if(errors&&errors->GetBufferPointer()&&errors->GetBufferSize()){
        message+=" ";message.append(static_cast<const char*>(errors->GetBufferPointer()),errors->GetBufferSize());
    }
    mgs5vr::log(message);
}

bool compile(ID3D11Device* device,const char* source,const char* profile,ID3DBlob** bytecode){
    (void)device;ComPtr<ID3DBlob> errors;
    const auto result=D3DCompile(source,std::strlen(source),"mgs5vr-retail-optic",nullptr,nullptr,
        "main",profile,D3DCOMPILE_OPTIMIZATION_LEVEL3,0,bytecode,errors.GetAddressOf());
    if(FAILED(result)){logHresult("Retail optic shader compile failed",result,errors.Get());return false;}
    return device&&*bytecode;
}

bool createResources(ID3D11Device* device){
    resources.attempted=true;resources.ready=false;resources.reported=false;
    if(!device)return false;
    std::string failure;const auto model=readRetailFmdl(retailBinocularPath(),failure);
    if(!model){mgs5vr::log("Retail binocular FMDL load failed; binocular rendering remains failed closed: "+failure);return false;}
    const auto diffuse=readRetailDds(retailBinocularDiffusePath(),failure);
    if(!diffuse){mgs5vr::log("Retail binocular diffuse texture load failed; binocular rendering remains failed closed: "+failure);return false;}
    if(model->vertices.size()>std::numeric_limits<UINT>::max()/sizeof(Vertex)
       ||model->indices.size()>std::numeric_limits<UINT>::max()/sizeof(uint16_t))return false;
    D3D11_BUFFER_DESC vertexDesc{};vertexDesc.ByteWidth=static_cast<UINT>(model->vertices.size()*sizeof(Vertex));
    vertexDesc.Usage=D3D11_USAGE_IMMUTABLE;vertexDesc.BindFlags=D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vertexData{};vertexData.pSysMem=model->vertices.data();
    if(FAILED(device->CreateBuffer(&vertexDesc,&vertexData,resources.vertices.GetAddressOf())))return false;
    D3D11_BUFFER_DESC indexDesc{};indexDesc.ByteWidth=static_cast<UINT>(model->indices.size()*sizeof(uint16_t));
    indexDesc.Usage=D3D11_USAGE_IMMUTABLE;indexDesc.BindFlags=D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA indexData{};indexData.pSysMem=model->indices.data();
    if(FAILED(device->CreateBuffer(&indexDesc,&indexData,resources.indices.GetAddressOf())))return false;
    D3D11_TEXTURE2D_DESC diffuseDesc{};diffuseDesc.Width=diffuse->width;diffuseDesc.Height=diffuse->height;
    diffuseDesc.MipLevels=static_cast<UINT>(diffuse->mipData.size());diffuseDesc.ArraySize=1;
    const auto colorFormat=diffuse->format==DXGI_FORMAT_BC1_UNORM?DXGI_FORMAT_BC1_UNORM_SRGB:DXGI_FORMAT_BC3_UNORM_SRGB;
    diffuseDesc.Format=colorFormat;
    diffuseDesc.SampleDesc.Count=1;diffuseDesc.Usage=D3D11_USAGE_IMMUTABLE;diffuseDesc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    std::vector<D3D11_SUBRESOURCE_DATA> diffuseData;diffuseData.reserve(diffuse->mipData.size());
    uint32_t levelWidth=diffuse->width;
    for(const auto& mip:diffuse->mipData){
        D3D11_SUBRESOURCE_DATA data{};data.pSysMem=mip.data();data.SysMemPitch=std::max<uint32_t>(1,(levelWidth+3)/4)*16u; // BC1 is corrected below.
        if(diffuse->format==DXGI_FORMAT_BC1_UNORM)data.SysMemPitch=std::max<uint32_t>(1,(levelWidth+3)/4)*8u;
        diffuseData.push_back(data);levelWidth=std::max<uint32_t>(1,levelWidth/2);
    }
    ComPtr<ID3D11Texture2D> diffuseTexture;
    if(FAILED(device->CreateTexture2D(&diffuseDesc,diffuseData.data(),diffuseTexture.GetAddressOf())))return false;
    D3D11_SHADER_RESOURCE_VIEW_DESC diffuseView{};diffuseView.Format=colorFormat;diffuseView.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;
    diffuseView.Texture2D.MostDetailedMip=0;diffuseView.Texture2D.MipLevels=static_cast<UINT>(diffuse->mipData.size());
    if(FAILED(device->CreateShaderResourceView(diffuseTexture.Get(),&diffuseView,resources.diffuse.GetAddressOf())))return false;
    D3D11_SAMPLER_DESC sampler{};sampler.Filter=D3D11_FILTER_ANISOTROPIC;sampler.AddressU=D3D11_TEXTURE_ADDRESS_WRAP;
    sampler.AddressV=D3D11_TEXTURE_ADDRESS_WRAP;sampler.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;sampler.MaxAnisotropy=8;sampler.ComparisonFunc=D3D11_COMPARISON_NEVER;
    sampler.MinLOD=0;sampler.MaxLOD=D3D11_FLOAT32_MAX;
    if(FAILED(device->CreateSamplerState(&sampler,resources.sampler.GetAddressOf())))return false;
    D3D11_BUFFER_DESC constantDesc{};constantDesc.ByteWidth=sizeof(Constants);constantDesc.Usage=D3D11_USAGE_DEFAULT;constantDesc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    if(FAILED(device->CreateBuffer(&constantDesc,nullptr,resources.constants.GetAddressOf())))return false;
    const std::array<LensVertex,4> lensVertices{{
        {{ocularCenter.x-ocularRadius,ocularCenter.y-ocularRadius,ocularCenter.z},{-1.f,-1.f}},
        {{ocularCenter.x-ocularRadius,ocularCenter.y+ocularRadius,ocularCenter.z},{-1.f,1.f}},
        {{ocularCenter.x+ocularRadius,ocularCenter.y-ocularRadius,ocularCenter.z},{1.f,-1.f}},
        {{ocularCenter.x+ocularRadius,ocularCenter.y+ocularRadius,ocularCenter.z},{1.f,1.f}}
    }};
    D3D11_BUFFER_DESC lensVertexDesc{};lensVertexDesc.ByteWidth=static_cast<UINT>(sizeof(lensVertices));
    lensVertexDesc.Usage=D3D11_USAGE_IMMUTABLE;lensVertexDesc.BindFlags=D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA lensVertexData{};lensVertexData.pSysMem=lensVertices.data();
    if(FAILED(device->CreateBuffer(&lensVertexDesc,&lensVertexData,resources.lensVertices.GetAddressOf())))return false;
    D3D11_BUFFER_DESC lensConstantDesc{};lensConstantDesc.ByteWidth=sizeof(LensConstants);
    lensConstantDesc.Usage=D3D11_USAGE_DEFAULT;lensConstantDesc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    if(FAILED(device->CreateBuffer(&lensConstantDesc,nullptr,resources.lensConstants.GetAddressOf())))return false;
    ComPtr<ID3DBlob> vs,ps;
    ComPtr<ID3DBlob> lensVs,lensPs;
    if(!compile(device,vertexShaderSource,"vs_4_0",vs.GetAddressOf())||!compile(device,pixelShaderSource,"ps_4_0",ps.GetAddressOf())
       ||!compile(device,lensVertexShaderSource,"vs_4_0",lensVs.GetAddressOf())
       ||!compile(device,lensPixelShaderSource,"ps_4_0",lensPs.GetAddressOf()))return false;
    if(FAILED(device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,resources.vertexShader.GetAddressOf()))
       ||FAILED(device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,resources.pixelShader.GetAddressOf()))
       ||FAILED(device->CreateVertexShader(lensVs->GetBufferPointer(),lensVs->GetBufferSize(),nullptr,resources.lensVertexShader.GetAddressOf()))
       ||FAILED(device->CreatePixelShader(lensPs->GetBufferPointer(),lensPs->GetBufferSize(),nullptr,resources.lensPixelShader.GetAddressOf())))return false;
    const std::array<D3D11_INPUT_ELEMENT_DESC,3> elements{{
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0}}};
    if(FAILED(device->CreateInputLayout(elements.data(),static_cast<UINT>(elements.size()),vs->GetBufferPointer(),vs->GetBufferSize(),resources.inputLayout.GetAddressOf())))return false;
    const std::array<D3D11_INPUT_ELEMENT_DESC,2> lensElements{{
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"APERTURE",0,DXGI_FORMAT_R32G32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0}
    }};
    if(FAILED(device->CreateInputLayout(lensElements.data(),static_cast<UINT>(lensElements.size()),lensVs->GetBufferPointer(),lensVs->GetBufferSize(),resources.lensInputLayout.GetAddressOf())))return false;
    D3D11_RASTERIZER_DESC rasterizer{};rasterizer.FillMode=D3D11_FILL_SOLID;rasterizer.CullMode=D3D11_CULL_NONE;
    // Use the real surface depth, including where a hand cups the housing.
    // FOX's 10 cm world near plane must not slice the eyecup at normal eye
    // relief. The grip solver keeps it outside the face; D3D depth clamping
    // preserves its world-scale X/Y projection during the last few cm.
    rasterizer.DepthClipEnable=FALSE;
    if(FAILED(device->CreateRasterizerState(&rasterizer,resources.rasterizer.GetAddressOf())))return false;
    D3D11_RASTERIZER_DESC lensRasterizer=rasterizer;lensRasterizer.DepthBias=0;lensRasterizer.SlopeScaledDepthBias=0;
    if(FAILED(device->CreateRasterizerState(&lensRasterizer,resources.lensRasterizer.GetAddressOf())))return false;
    D3D11_DEPTH_STENCIL_DESC depth{};depth.DepthEnable=TRUE;depth.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;depth.DepthFunc=D3D11_COMPARISON_LESS_EQUAL;
    if(FAILED(device->CreateDepthStencilState(&depth,resources.depthStencil.GetAddressOf())))return false;
    depth.DepthFunc=D3D11_COMPARISON_GREATER_EQUAL;
    if(FAILED(device->CreateDepthStencilState(&depth,resources.reversedDepthStencil.GetAddressOf())))return false;
    // The exit pupil sits in front of its recess. Hands and world geometry
    // must still occlude it. FOX can publish reversed depth; match that mapping.
    D3D11_DEPTH_STENCIL_DESC lensDepth=depth;lensDepth.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;
    if(FAILED(device->CreateDepthStencilState(&lensDepth,resources.reversedLensDepthStencil.GetAddressOf())))return false;
    lensDepth.DepthFunc=D3D11_COMPARISON_LESS_EQUAL;
    if(FAILED(device->CreateDepthStencilState(&lensDepth,resources.lensDepthStencil.GetAddressOf())))return false;
    D3D11_BLEND_DESC blend{};blend.RenderTarget[0].BlendEnable=FALSE;blend.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
    if(FAILED(device->CreateBlendState(&blend,resources.blend.GetAddressOf())))return false;
    D3D11_SAMPLER_DESC lensSampler{};lensSampler.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    lensSampler.AddressU=D3D11_TEXTURE_ADDRESS_CLAMP;lensSampler.AddressV=D3D11_TEXTURE_ADDRESS_CLAMP;lensSampler.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;
    lensSampler.MaxAnisotropy=1;lensSampler.ComparisonFunc=D3D11_COMPARISON_NEVER;lensSampler.MinLOD=0;lensSampler.MaxLOD=D3D11_FLOAT32_MAX;
    if(FAILED(device->CreateSamplerState(&lensSampler,resources.lensSampler.GetAddressOf())))return false;
    resources.indexCount=static_cast<UINT>(model->indices.size());resources.ready=true;
    std::ostringstream message;message<<"Retail binocular FMDL loaded path="<<model->path.string()
        <<" bytes="<<std::filesystem::file_size(model->path)<<" vertices="<<model->vertices.size()
        <<" triangles="<<model->indices.size()/3<<" bounds="<<model->min[0]<<","<<model->min[1]<<","<<model->min[2]<<".."
        <<model->max[0]<<","<<model->max[1]<<","<<model->max[2];mgs5vr::log(message.str());
    std::ostringstream textureMessage;textureMessage<<"Retail binocular diffuse loaded path="<<diffuse->path.string()
        <<" size="<<diffuse->width<<"x"<<diffuse->height<<" mips="<<diffuse->mipData.size()
        <<" format="<<(diffuse->format==DXGI_FORMAT_BC1_UNORM?"BC1_SRGB":"BC3_SRGB");mgs5vr::log(textureMessage.str());
    return true;
}

struct SavedState {
    ComPtr<ID3D11RenderTargetView> renderTarget;ComPtr<ID3D11DepthStencilView> depthTarget;ComPtr<ID3D11InputLayout> inputLayout;
    ComPtr<ID3D11Buffer> vertexBuffer;ComPtr<ID3D11Buffer> indexBuffer;ComPtr<ID3D11Buffer> vertexConstants;ComPtr<ID3D11Buffer> pixelConstants;
    ComPtr<ID3D11VertexShader> vertexShader;ComPtr<ID3D11PixelShader> pixelShader;ComPtr<ID3D11ShaderResourceView> diffuse;ComPtr<ID3D11SamplerState> sampler;ComPtr<ID3D11RasterizerState> rasterizer;
    ComPtr<ID3D11DepthStencilState> depthStencil;ComPtr<ID3D11BlendState> blend;UINT stride{},offset{},indexOffset{},sampleMask{};
    DXGI_FORMAT indexFormat{DXGI_FORMAT_UNKNOWN};D3D11_PRIMITIVE_TOPOLOGY topology{D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED};
    D3D11_VIEWPORT viewport{};UINT viewportCount{};FLOAT blendFactor[4]{};UINT stencilRef{};
};

void save(ID3D11DeviceContext* context,SavedState& state){
    context->OMGetRenderTargets(1,state.renderTarget.GetAddressOf(),state.depthTarget.GetAddressOf());context->IAGetInputLayout(state.inputLayout.GetAddressOf());
    context->IAGetVertexBuffers(0,1,state.vertexBuffer.GetAddressOf(),&state.stride,&state.offset);context->IAGetIndexBuffer(state.indexBuffer.GetAddressOf(),&state.indexFormat,&state.indexOffset);
    context->IAGetPrimitiveTopology(&state.topology);context->VSGetShader(state.vertexShader.GetAddressOf(),nullptr,nullptr);context->PSGetShader(state.pixelShader.GetAddressOf(),nullptr,nullptr);
    context->VSGetConstantBuffers(0,1,state.vertexConstants.GetAddressOf());context->PSGetConstantBuffers(0,1,state.pixelConstants.GetAddressOf());context->PSGetShaderResources(0,1,state.diffuse.GetAddressOf());context->PSGetSamplers(0,1,state.sampler.GetAddressOf());context->RSGetState(state.rasterizer.GetAddressOf());context->OMGetDepthStencilState(state.depthStencil.GetAddressOf(),&state.stencilRef);
    context->OMGetBlendState(state.blend.GetAddressOf(),state.blendFactor,&state.sampleMask);state.viewportCount=1;context->RSGetViewports(&state.viewportCount,&state.viewport);
}

void restore(ID3D11DeviceContext* context,const SavedState& state){
    ID3D11RenderTargetView* renderTarget=state.renderTarget.Get();ID3D11DepthStencilView* depthTarget=state.depthTarget.Get();context->OMSetRenderTargets(1,&renderTarget,depthTarget);
    ID3D11Buffer* vertexBuffer=state.vertexBuffer.Get();context->IASetInputLayout(state.inputLayout.Get());context->IASetVertexBuffers(0,1,&vertexBuffer,&state.stride,&state.offset);
    context->IASetIndexBuffer(state.indexBuffer.Get(),state.indexFormat,state.indexOffset);context->IASetPrimitiveTopology(state.topology);context->VSSetShader(state.vertexShader.Get(),nullptr,0);context->PSSetShader(state.pixelShader.Get(),nullptr,0);
    ID3D11Buffer* constants=state.vertexConstants.Get();context->VSSetConstantBuffers(0,1,&constants);ID3D11Buffer* pixelConstants=state.pixelConstants.Get();context->PSSetConstantBuffers(0,1,&pixelConstants);ID3D11ShaderResourceView* diffuse=state.diffuse.Get();context->PSSetShaderResources(0,1,&diffuse);ID3D11SamplerState* sampler=state.sampler.Get();context->PSSetSamplers(0,1,&sampler);context->RSSetState(state.rasterizer.Get());context->OMSetDepthStencilState(state.depthStencil.Get(),state.stencilRef);
    context->OMSetBlendState(state.blend.Get(),state.blendFactor,state.sampleMask);if(state.viewportCount)context->RSSetViewports(1,&state.viewport);
}

bool bindHousingDepth(ID3D11DeviceContext* context,ID3D11Device* device,
    const SavedState& state,bool reversed,ComPtr<ID3D11DepthStencilView>& target){
    const char* source="native writable depth";
    if(state.depthTarget){
        D3D11_DEPTH_STENCIL_VIEW_DESC view{};state.depthTarget->GetDesc(&view);
        if(view.Flags&D3D11_DSV_READ_ONLY_DEPTH){
            ComPtr<ID3D11Resource> texture;state.depthTarget->GetResource(texture.GetAddressOf());
            view.Flags&=~D3D11_DSV_READ_ONLY_DEPTH;
            if(FAILED(device->CreateDepthStencilView(texture.Get(),&view,target.GetAddressOf())))return false;
            source="native depth with writable housing view";
        }else target=state.depthTarget;
    }else{
        // FOX's final color pass can leave no depth target bound. A depth
        // state alone then does nothing: rear caps paint through front faces
        // and look like a broken UV atlas. Give the housing and lens one real
        // depth surface for their mutual occlusion in this eye.
        ComPtr<ID3D11Resource> resource;state.renderTarget->GetResource(resource.GetAddressOf());
        ComPtr<ID3D11Texture2D> color;if(FAILED(resource.As(&color)))return false;
        D3D11_TEXTURE2D_DESC desc{};color->GetDesc(&desc);
        if(!resources.housingDepthView||resources.depthWidth!=desc.Width
            ||resources.depthHeight!=desc.Height||resources.depthSamples!=desc.SampleDesc.Count){
            resources.housingDepthView.Reset();resources.housingDepth.Reset();
            desc.Format=DXGI_FORMAT_D32_FLOAT;desc.MipLevels=1;desc.ArraySize=1;
            desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_DEPTH_STENCIL;
            desc.CPUAccessFlags=0;desc.MiscFlags=0;
            if(FAILED(device->CreateTexture2D(&desc,nullptr,resources.housingDepth.GetAddressOf()))
                ||FAILED(device->CreateDepthStencilView(resources.housingDepth.Get(),nullptr,resources.housingDepthView.GetAddressOf())))return false;
            resources.depthWidth=desc.Width;resources.depthHeight=desc.Height;resources.depthSamples=desc.SampleDesc.Count;
        }
        target=resources.housingDepthView;
        context->ClearDepthStencilView(target.Get(),D3D11_CLEAR_DEPTH,reversed?0.f:1.f,0);
        source="dedicated housing depth; native final pass has no DSV";
    }
    ID3D11RenderTargetView* color=state.renderTarget.Get();context->OMSetRenderTargets(1,&color,target.Get());
    if(!resources.depthReported){resources.depthReported=true;mgs5vr::log(std::string("Retail optic depth: ")+source);}
    return true;
}

struct ClipPoint {float x{},y{},z{},w{};};

ClipPoint transformPoint(const std::array<float,16>& matrix,mgs5vr::Vec3 point){
    return {
        point.x*matrix[0]+point.y*matrix[4]+point.z*matrix[8]+matrix[12],
        point.x*matrix[1]+point.y*matrix[5]+point.z*matrix[9]+matrix[13],
        point.x*matrix[2]+point.y*matrix[6]+point.z*matrix[10]+matrix[14],
        point.x*matrix[3]+point.y*matrix[7]+point.z*matrix[11]+matrix[15]
    };
}

bool sameSceneDescription(const D3D11_TEXTURE2D_DESC& a,const D3D11_TEXTURE2D_DESC& b){
    return a.Width==b.Width&&a.Height==b.Height&&a.MipLevels==b.MipLevels&&a.ArraySize==b.ArraySize
        &&a.Format==b.Format&&a.SampleDesc.Count==b.SampleDesc.Count&&a.SampleDesc.Quality==b.SampleDesc.Quality;
}

DXGI_FORMAT shaderResourceFormat(DXGI_FORMAT format){
    switch(format){
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:return DXGI_FORMAT_B8G8R8A8_UNORM;
    case DXGI_FORMAT_B8G8R8X8_TYPELESS:return DXGI_FORMAT_B8G8R8X8_UNORM;
    default:return format;
    }
}

bool prepareSceneCopy(ID3D11DeviceContext* context,ID3D11Device* device,const SavedState& state,const char*& failure){
    failure=nullptr;
    if(!context||!device){failure="missing D3D11 context or device";return false;}
    ComPtr<ID3D11Texture2D> source;
    if(!mgs5vr::sceneSourceTexture(context,source.GetAddressOf())){
        failure="native optic scene output unavailable";return false;
    }
    D3D11_TEXTURE2D_DESC sourceDescription{};source->GetDesc(&sourceDescription);
    if(!sourceDescription.Width||!sourceDescription.Height||sourceDescription.MipLevels!=1||sourceDescription.ArraySize!=1
       ||sourceDescription.SampleDesc.Count!=1||shaderResourceFormat(sourceDescription.Format)==DXGI_FORMAT_UNKNOWN){failure="native scene source has unsupported texture description";return false;}
    if(!resources.sourceReported){
        resources.sourceReported=true;
        ComPtr<ID3D11Texture2D> swapSource;mgs5vr::sceneSourceTexture(context,swapSource.GetAddressOf());
        std::ostringstream message;message<<"Retail optic scene target="<<reinterpret_cast<uintptr_t>(source.Get())
            <<" swapchain="<<reinterpret_cast<uintptr_t>(swapSource.Get())
            <<" size="<<sourceDescription.Width<<"x"<<sourceDescription.Height
            <<" format=0x"<<std::hex<<static_cast<unsigned>(sourceDescription.Format)<<std::dec;
        mgs5vr::log(message.str());
    }
    if(!resources.sceneCopy||!sameSceneDescription(resources.sceneDescription,sourceDescription)){
        resources.scene.Reset();resources.sceneOverlayTarget.Reset();resources.sceneCopy.Reset();resources.sceneDescription={};
        auto destination=sourceDescription;destination.Usage=D3D11_USAGE_DEFAULT;destination.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
        destination.CPUAccessFlags=0;destination.MiscFlags=0;
        if(FAILED(device->CreateTexture2D(&destination,nullptr,resources.sceneCopy.GetAddressOf()))){failure="scene copy texture creation failed";return false;}
        D3D11_SHADER_RESOURCE_VIEW_DESC view{};view.Format=shaderResourceFormat(destination.Format);view.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;
        view.Texture2D.MostDetailedMip=0;view.Texture2D.MipLevels=1;
        if(FAILED(device->CreateShaderResourceView(resources.sceneCopy.Get(),&view,resources.scene.GetAddressOf()))){resources.sceneCopy.Reset();failure="scene copy shader-resource creation failed";return false;}
        D3D11_RENDER_TARGET_VIEW_DESC target{};target.Format=view.Format;target.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2D;
        if(FAILED(device->CreateRenderTargetView(resources.sceneCopy.Get(),&target,resources.sceneOverlayTarget.GetAddressOf())))return false;
        resources.sceneDescription=sourceDescription;
    }
    ID3D11ShaderResourceView* none=nullptr;context->PSSetShaderResources(0,1,&none);
    // D3D11 rejects or drops a copy whose source is still bound as an output
    // on some deferred contexts.  The native scene remains intact; restore
    // the exact eye target immediately after the copy for the optic passes.
    context->OMSetRenderTargets(0,nullptr,nullptr);
    context->CopyResource(resources.sceneCopy.Get(),source.Get());
    ID3D11RenderTargetView* renderTarget=state.renderTarget.Get();
    context->OMSetRenderTargets(1,&renderTarget,state.depthTarget.Get());
    if(state.viewportCount)context->RSSetViewports(1,&state.viewport);
    return true;
}

std::array<float,16> matrixProduct(const std::array<float,16>& a,const std::array<float,16>& b){
    std::array<float,16> out{};
    for(size_t row=0;row<4;++row)for(size_t column=0;column<4;++column){double value=0;
        for(size_t k=0;k<4;++k)value+=static_cast<double>(a[row*4+k])*b[k*4+column];out[row*4+column]=static_cast<float>(value);
    }
    return out;
}

bool createMarkerResources(ID3D11Device* device){
    if(resources.markerVertices)return true;
    constexpr auto vs=R"(
struct V {float2 position:POSITION;float4 color:COLOR;};
struct O {float4 position:SV_POSITION;float4 color:COLOR;};
O main(V v){O o;o.position=float4(v.position,0,1);o.color=v.color;return o;})";
    constexpr auto ps=R"(
float4 main(float4 position:SV_POSITION,float4 color:COLOR):SV_TARGET{return color;})";
    ComPtr<ID3DBlob> vertex,pixel;
    if(!compile(device,vs,"vs_5_0",&vertex)||!compile(device,ps,"ps_5_0",&pixel))return false;
    if(FAILED(device->CreateVertexShader(vertex->GetBufferPointer(),vertex->GetBufferSize(),nullptr,&resources.markerVertexShader))
        ||FAILED(device->CreatePixelShader(pixel->GetBufferPointer(),pixel->GetBufferSize(),nullptr,&resources.markerPixelShader)))return false;
    const D3D11_INPUT_ELEMENT_DESC elements[]{
        {"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,8,D3D11_INPUT_PER_VERTEX_DATA,0}};
    if(FAILED(device->CreateInputLayout(elements,2,vertex->GetBufferPointer(),vertex->GetBufferSize(),&resources.markerInputLayout)))return false;
    D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;raster.CullMode=D3D11_CULL_NONE;raster.DepthClipEnable=TRUE;
    D3D11_DEPTH_STENCIL_DESC depth{};
    if(FAILED(device->CreateRasterizerState(&raster,&resources.markerRasterizer))
        ||FAILED(device->CreateDepthStencilState(&depth,&resources.markerDepth)))return false;
    D3D11_BUFFER_DESC buffer{};buffer.ByteWidth=16384*sizeof(MarkerVertex);buffer.Usage=D3D11_USAGE_DYNAMIC;
    buffer.BindFlags=D3D11_BIND_VERTEX_BUFFER;buffer.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
    return SUCCEEDED(device->CreateBuffer(&buffer,nullptr,&resources.markerVertices));
}

// Small atlas-free glyphs remain sharp in the independently rendered lens.
// These are native waypoint letters and measured distances, never video labels.
std::array<uint8_t,7> markerGlyph(char c){
    static constexpr std::array<std::array<uint8_t,7>,36> glyphs{{
        {{14,17,19,21,25,17,14}},{{4,12,4,4,4,4,14}},{{14,17,1,2,4,8,31}},
        {{30,1,1,14,1,1,30}},{{2,6,10,18,31,2,2}},{{31,16,16,30,1,1,30}},
        {{14,16,16,30,17,17,14}},{{31,1,2,4,8,8,8}},{{14,17,17,14,17,17,14}},
        {{14,17,17,15,1,1,14}},{{14,17,17,31,17,17,17}},{{30,17,17,30,17,17,30}},
        {{14,17,16,16,16,17,14}},{{30,17,17,17,17,17,30}},{{31,16,16,30,16,16,31}},
        {{31,16,16,30,16,16,16}},{{14,17,16,23,17,17,15}},{{17,17,17,31,17,17,17}},
        {{14,4,4,4,4,4,14}},{{7,2,2,2,2,18,12}},{{17,18,20,24,20,18,17}},
        {{16,16,16,16,16,16,31}},{{17,27,21,21,17,17,17}},{{17,25,21,19,17,17,17}},
        {{14,17,17,17,17,17,14}},{{30,17,17,30,16,16,16}},{{14,17,17,17,21,18,13}},
        {{30,17,17,30,20,18,17}},{{15,16,16,14,1,1,30}},{{31,4,4,4,4,4,4}},
        {{17,17,17,17,17,17,14}},{{17,17,17,17,17,10,4}},{{17,17,17,21,21,21,10}},
        {{17,17,10,4,10,17,17}},{{17,17,10,4,4,4,4}},{{31,1,2,4,8,16,31}}
    }};
    if(c>='0'&&c<='9')return glyphs[c-'0'];
    if(c>='A'&&c<='Z')return glyphs[10+c-'A'];
    return {};
}

void drawOpticWaypoints(ID3D11DeviceContext* context,ID3D11Device* device,
    const std::array<float,16>& view,const std::array<float,16>& projection,mgs5vr::Vec3 camera){
    const auto points=mgs5vr::opticWaypoints();const auto status=mgs5vr::headCamera().status();
    const auto now=mgs5vr::steadyMilliseconds();
    if(!resources.sceneOverlayTarget||!status.active||points.activation!=status.activation
        ||now<points.sampleTime||now-points.sampleTime>150||!createMarkerResources(device))return;
    std::vector<MarkerVertex> vertices;vertices.reserve(8192);
    using Color=std::array<float,3>;
    constexpr Color amber{1.f,.72f,.18f},red{1.f,.18f,.10f},black{.015f,.02f,.025f},white{.8f,.9f,.85f};
    const auto quad=[&](float x0,float y0,float x1,float y1,Color color){
        const auto v=[&](float x,float y){return MarkerVertex{x,y,color[0],color[1],color[2],1};};
        for(const auto vertex:{v(x0,y0),v(x1,y0),v(x0,y1),v(x0,y1),v(x1,y0),v(x1,y1)})vertices.push_back(vertex);
    };
    const auto line=[&](float x0,float y0,float x1,float y1,float thickness,Color color){
        const float dx=x1-x0,dy=y1-y0,length=std::sqrt(dx*dx+dy*dy);
        if(length<.00001f)return;
        const float px=-dy*thickness/length,py=dx*thickness/length;
        const auto v=[&](float x,float y){return MarkerVertex{x,y,color[0],color[1],color[2],1};};
        const auto a=v(x0+px,y0+py),b=v(x1+px,y1+py),c=v(x0-px,y0-py),d=v(x1-px,y1-py);
        for(const auto vertex:{a,b,c,c,b,d})vertices.push_back(vertex);
    };
    // Four short ticks leave the exact native marking ray unobstructed.
    for(const auto color:{black,white}){
        const float width=color==black?.006f:.0025f;
        line(-.05f,0,-.022f,0,width,color);line(.022f,0,.05f,0,width,color);
        line(0,-.05f,0,-.022f,width,color);line(0,.022f,0,.05f,width,color);
    }
    const auto vp=matrixProduct(view,projection);unsigned visible{};
    for(size_t i=0;i<points.count;++i){
        const auto& point=points.points[i];const auto clip=transformPoint(vp,point.position);
        if(!std::isfinite(clip.w)||clip.w<=.01f)continue;
        const float x=clip.x/clip.w,y=clip.y/clip.w;
        if(!std::isfinite(x)||!std::isfinite(y)||x*x+y*y>.90f*.90f)continue;
        const auto delta=point.position-camera;const float distance=std::sqrt(mgs5vr::dot(delta,delta));
        if(!std::isfinite(distance)||distance>99999.f)continue;
        ++visible;
        const auto markerColor=point.letter?amber:red;
        for(const auto color:{black,markerColor}){
            const float width=color==black?.005f:.002f;
            line(x,y+.032f,x+.032f,y,width,color);line(x+.032f,y,x,y-.032f,width,color);
            line(x,y-.032f,x-.032f,y,width,color);line(x-.032f,y,x,y+.032f,width,color);
        }
        char label[24]{};
        if(point.letter)std::snprintf(label,sizeof(label),"%c %uM",'A'+point.letter-1,static_cast<unsigned>(std::lround(distance)));
        else std::snprintf(label,sizeof(label),"%uM",static_cast<unsigned>(std::lround(distance)));
        constexpr float pixel=.0075f;const float length=static_cast<float>(std::strlen(label));
        // Keep nearby waypoint and person labels on opposite sides of the
        // marker, leaving the person and the exact aiming point visible.
        const float left=x-(length*6-1)*pixel*.5f,top=y+(point.letter?-.052f:.112f);
        quad(left-.008f,top+.008f,left+(length*6-1)*pixel+.008f,top-7*pixel-.008f,black);
        for(size_t n=0;label[n];++n){const auto glyph=markerGlyph(label[n]);
            for(size_t row=0;row<7;++row)for(size_t col=0;col<5;++col)if(glyph[row]&(1u<<(4-col))){
                const float gx=left+static_cast<float>(n*6+col)*pixel,gy=top-static_cast<float>(row)*pixel;
                quad(gx,gy,gx+pixel,gy-pixel,markerColor);
            }
        }
    }
    if(vertices.size()>16384)return;
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if(FAILED(context->Map(resources.markerVertices.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))return;
    std::memcpy(mapped.pData,vertices.data(),vertices.size()*sizeof(MarkerVertex));context->Unmap(resources.markerVertices.Get(),0);
    ID3D11ShaderResourceView* none=nullptr;context->PSSetShaderResources(0,1,&none);
    ID3D11RenderTargetView* target=resources.sceneOverlayTarget.Get();context->OMSetRenderTargets(1,&target,nullptr);
    const D3D11_VIEWPORT viewport{0,0,static_cast<float>(resources.sceneDescription.Width),static_cast<float>(resources.sceneDescription.Height),0,1};
    context->RSSetViewports(1,&viewport);context->RSSetState(resources.markerRasterizer.Get());
    context->OMSetDepthStencilState(resources.markerDepth.Get(),0);context->OMSetBlendState(nullptr,nullptr,0xffffffffu);
    ID3D11Buffer* buffer=resources.markerVertices.Get();const UINT stride=sizeof(MarkerVertex),offset=0;
    context->IASetVertexBuffers(0,1,&buffer,&stride,&offset);context->IASetInputLayout(resources.markerInputLayout.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(resources.markerVertexShader.Get(),nullptr,0);context->PSSetShader(resources.markerPixelShader.Get(),nullptr,0);
    context->Draw(static_cast<UINT>(vertices.size()),0);
    if(visible&&!resources.markerReported){resources.markerReported=true;mgs5vr::log("Native waypoint letters and distances rendered only in the binocular scene");}
}

bool drawLensPortal(ID3D11DeviceContext* context,const std::array<float,16>& world,
    const std::array<float,16>& view,const std::array<float,16>& projection,float magnification,
    const SavedState& state,ID3D11DepthStencilView* depthTarget,ID3D11ShaderResourceView* sceneSource){
    if(magnification<=1.0001f)return false;
    const bool trace=(++resources.lensTraceCalls%120u)==0u;
    const char* failure=nullptr;
    if(!sceneSource||!state.renderTarget||!state.viewport.Width||!state.viewport.Height){
        failure=sceneSource?"lens target state unavailable":"native per-eye scene copy unavailable";
        if(trace)mgs5vr::log(std::string("Retail optic lens trace unavailable: ")+failure);
        if(!resources.lensFailureReported){resources.lensFailureReported=true;mgs5vr::log(std::string("Retail optic lens portal failed: ")+failure);}
        return false;
    }
    const auto mvp=matrixProduct(matrixProduct(world,view),projection);const auto clip=transformPoint(mvp,ocularCenter);
    if(!std::isfinite(clip.x)||!std::isfinite(clip.y)||!std::isfinite(clip.w)||clip.w<=.0001f){
        if(trace){std::ostringstream message;message<<"Retail optic lens trace clip="<<clip.x<<","<<clip.y<<","<<clip.z<<","<<clip.w;mgs5vr::log(message.str());}
        if(!resources.lensFailureReported){resources.lensFailureReported=true;std::ostringstream message;message<<"Retail optic lens portal failed: ocular clip="<<clip.x<<","<<clip.y<<","<<clip.z<<","<<clip.w;mgs5vr::log(message.str());}
        return false;
    }
    const float centerX=.5f*(clip.x/clip.w+1.f),centerY=.5f*(1.f-clip.y/clip.w);
    if(!std::isfinite(centerX)||!std::isfinite(centerY)){
        if(trace){std::ostringstream message;message<<"Retail optic lens trace center="<<centerX<<","<<centerY<<" clip="<<clip.x<<","<<clip.y<<","<<clip.z<<","<<clip.w;mgs5vr::log(message.str());}
        if(!resources.lensFailureReported){resources.lensFailureReported=true;std::ostringstream message;message<<"Retail optic lens portal failed: ocular center projects outside eye image center="<<centerX<<","<<centerY<<" clip="<<clip.x<<","<<clip.y<<","<<clip.z<<","<<clip.w;mgs5vr::log(message.str());}
        return false;
    }
    LensConstants constants{};std::copy(mvp.begin(),mvp.end(),std::begin(constants.mvp));
    constants.sampleCenter[0]=centerX;constants.sampleCenter[1]=centerY;constants.sampleCenter[2]=magnification;
    const auto worldView=matrixProduct(world,view);const auto ocular=transformPoint(worldView,ocularCenter);
    const float distance=std::sqrt(ocular.x*ocular.x+ocular.y*ocular.y+ocular.z*ocular.z);
    const float facing=-(ocular.x*worldView[8]+ocular.y*worldView[9]+ocular.z*worldView[10])/std::max(distance,.001f);
    const float centering=std::sqrt(ocular.x*ocular.x+ocular.y*ocular.y)/std::max(distance,.001f);
    float focus=facing>.80f&&centering<.35f?std::clamp((.115f-distance)/.055f,0.f,1.f):0.f;
    focus=focus*focus*(3-2*focus);constants.sampleCenter[3]=focus;
    constants.viewport[0]=state.viewport.Width;constants.viewport[1]=state.viewport.Height;
    context->UpdateSubresource(resources.lensConstants.Get(),0,nullptr,&constants,0,0);
    ID3D11RenderTargetView* renderTarget=state.renderTarget.Get();
    context->OMSetRenderTargets(1,&renderTarget,depthTarget);
    if(state.viewportCount)context->RSSetViewports(1,&state.viewport);
    const UINT stride=sizeof(LensVertex),offset=0;ID3D11Buffer* vertexBuffer=resources.lensVertices.Get();
    context->IASetInputLayout(resources.lensInputLayout.Get());context->IASetVertexBuffers(0,1,&vertexBuffer,&stride,&offset);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    context->VSSetShader(resources.lensVertexShader.Get(),nullptr,0);ID3D11Buffer* constantBuffer=resources.lensConstants.Get();context->VSSetConstantBuffers(0,1,&constantBuffer);context->PSSetConstantBuffers(0,1,&constantBuffer);
    context->PSSetShader(resources.lensPixelShader.Get(),nullptr,0);ID3D11ShaderResourceView* scene=sceneSource;context->PSSetShaderResources(0,1,&scene);
    ID3D11SamplerState* sampler=resources.lensSampler.Get();context->PSSetSamplers(0,1,&sampler);
    context->RSSetState(resources.lensRasterizer.Get());context->OMSetDepthStencilState(focus>0&&resources.markerDepth?resources.markerDepth.Get():
        projection[14]>0?resources.reversedLensDepthStencil.Get():resources.lensDepthStencil.Get(),0);
    const FLOAT blendFactor[4]{0,0,0,0};context->OMSetBlendState(resources.blend.Get(),blendFactor,0xffffffffu);
    context->Draw(4,0);
    if(trace){std::ostringstream message;message<<"Retail optic lens trace rendered center="<<centerX<<","<<centerY<<" magnification="<<magnification;mgs5vr::log(message.str());}
    return true;
}
}

namespace mgs5vr {
bool capturePhysicalOpticScene(ID3D11DeviceContext* context,const std::array<float,16>& view,
    const std::array<float,16>& projection,Vec3 cameraPosition,ID3D11Texture2D** output) noexcept{
    if(output)*output=nullptr;
    if(!context||!output)return false;
    try{
        std::lock_guard lock(rendererMutex);
        ComPtr<ID3D11Device> device;context->GetDevice(device.GetAddressOf());
        if(!device)return false;
        if(resources.device.Get()!=device.Get()){resources=Resources{};resources.device=device;}
        SavedState state;save(context,state);
        const char* failure=nullptr;
        const bool copied=prepareSceneCopy(context,device.Get(),state,failure);
        if(copied)drawOpticWaypoints(context,device.Get(),view,projection,cameraPosition);
        restore(context,state);
        if(!copied)return false;
        *output=resources.sceneCopy.Get();(*output)->AddRef();
        return true;
    }catch(...){return false;}
}
bool drawPhysicalBinoculars(ID3D11DeviceContext* context,const std::array<float,16>& world,const std::array<float,16>& view,
    const std::array<float,16>& projection,float magnification,ID3D11Texture2D* sceneSource,bool) noexcept{
    if(!context)return false;
    try{
        std::lock_guard lock(rendererMutex);ComPtr<ID3D11Device> device;context->GetDevice(device.GetAddressOf());if(!device)return false;
        if(resources.device.Get()!=device.Get())resources=Resources{};
        if(!resources.attempted){resources.device=device;if(!createResources(device.Get()))return false;}
        if(!resources.ready||!std::isfinite(magnification)||magnification<1||magnification>4)return false;
        SavedState state;save(context,state);if(!state.renderTarget){restore(context,state);return false;}
        // The native eye remains the user's full stereo view. Optical
        // magnification is composited through the actual retail ocular; the
        // housing itself is always projected at world scale.
        const auto mvp=matrixProduct(matrixProduct(world,view),projection);for(float value:mvp)if(!std::isfinite(value)){restore(context,state);return false;}
        // The owned retail asset is a single monocular/telescope. Keep its
        // physical housing in both native stereo eyes, but show the optical
        // portal only through the eye that is actually behind its one ocular.
        // Copy the native scene before drawing the housing; the portal is
        // composited after the housing so its pixels survive the housing's
        // depth pass and remain confined to the real aperture.
        const bool needsLens=magnification>1.0001f&&sceneSource;
        ComPtr<ID3D11ShaderResourceView> sceneSourceView;
        if(needsLens){
            const char* failure=nullptr;
            if(!sceneSource){failure="native per-eye scene copy unavailable";}
            else {
                ComPtr<ID3D11Device> sourceDevice;sceneSource->GetDevice(&sourceDevice);
                D3D11_TEXTURE2D_DESC sourceDescription{};sceneSource->GetDesc(&sourceDescription);
                if(!sourceDevice||sourceDevice.Get()!=device.Get()||!sourceDescription.Width||!sourceDescription.Height
                   ||sourceDescription.MipLevels!=1||sourceDescription.ArraySize!=1||sourceDescription.SampleDesc.Count!=1
                   ||shaderResourceFormat(sourceDescription.Format)==DXGI_FORMAT_UNKNOWN){
                    failure="native per-eye scene copy has an incompatible texture";
                }else {
                    D3D11_SHADER_RESOURCE_VIEW_DESC viewDescription{};viewDescription.Format=shaderResourceFormat(sourceDescription.Format);
                    viewDescription.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;viewDescription.Texture2D.MostDetailedMip=0;viewDescription.Texture2D.MipLevels=1;
                    if(FAILED(device->CreateShaderResourceView(sceneSource,&viewDescription,sceneSourceView.GetAddressOf())))failure="native per-eye scene view creation failed";
                }
            }
            if(!sceneSourceView){
                restore(context,state);
                if(!resources.lensFailureReported){resources.lensFailureReported=true;
                    mgs5vr::log(std::string("Retail optic lens portal failed: ")+(failure?failure:"scene copy unavailable"));}
                return false;
            }
        }
        Constants constants{};std::copy(mvp.begin(),mvp.end(),std::begin(constants.mvp));std::copy(world.begin(),world.end(),std::begin(constants.world));
        ComPtr<ID3D11DepthStencilView> housingDepth;
        if(!bindHousingDepth(context,device.Get(),state,projection[14]>0,housingDepth)){restore(context,state);return false;}
        constants.baseColor[0]=1.f;constants.baseColor[1]=1.f;constants.baseColor[2]=1.f;constants.baseColor[3]=1.f;
        context->UpdateSubresource(resources.constants.Get(),0,nullptr,&constants,0,0);const UINT stride=sizeof(Vertex),offset=0;ID3D11Buffer* vertexBuffer=resources.vertices.Get();
        context->IASetInputLayout(resources.inputLayout.Get());context->IASetVertexBuffers(0,1,&vertexBuffer,&stride,&offset);context->IASetIndexBuffer(resources.indices.Get(),DXGI_FORMAT_R16_UINT,0);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context->VSSetShader(resources.vertexShader.Get(),nullptr,0);ID3D11Buffer* constantsBuffer=resources.constants.Get();context->VSSetConstantBuffers(0,1,&constantsBuffer);
        context->PSSetShader(resources.pixelShader.Get(),nullptr,0);ID3D11ShaderResourceView* diffuse=resources.diffuse.Get();context->PSSetShaderResources(0,1,&diffuse);ID3D11SamplerState* sampler=resources.sampler.Get();context->PSSetSamplers(0,1,&sampler);context->RSSetState(resources.rasterizer.Get());context->OMSetDepthStencilState(projection[14]>0?resources.reversedDepthStencil.Get():resources.depthStencil.Get(),0);const FLOAT blendFactor[4]{0,0,0,0};context->OMSetBlendState(resources.blend.Get(),blendFactor,0xffffffffu);
        context->DrawIndexed(resources.indexCount,0,0);
        if(needsLens&&!drawLensPortal(context,world,view,projection,magnification,state,housingDepth.Get(),sceneSourceView.Get())){
            restore(context,state);if(!resources.lensReported){resources.lensReported=true;mgs5vr::log("Retail optic lens portal unavailable; optical rendering remains failed closed");}return false;
        }
        restore(context,state);if(!resources.reported){resources.reported=true;mgs5vr::log("Retail binocular FMDL rendered in both native stereo eyes");}return true;
    }catch(...){return false;}
}
void stopPhysicalOpticRenderer() noexcept{std::lock_guard lock(rendererMutex);resources=Resources{};}
}
