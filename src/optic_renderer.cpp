#include "mgs5vr/optic_renderer.hpp"
#include "mgs5vr/core.hpp"
#include "mgs5vr/eyepatch.hpp"
#include "mgs5vr/optic_rig.hpp"
#include "mgs5vr/optic_markers.hpp"
#include "mgs5vr/optic_native_lighting_probe.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/log.hpp"
#include "mgs5vr/scene_capture.hpp"
#include "mgs5vr/opening_selector.hpp"
#include "mgs5vr/wrist_selector.hpp"
#include "mgs5vr/cabin_studio_store.hpp"
#include "mgs5vr/native_text_input.hpp"
#include <windows.h>
#include <bcrypt.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <tuple>
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
    float tangent[4]{};
};

struct Constants {
    float mvp[16]{};
    float world[16]{};
    float baseColor[4]{};
};
struct NativeMaterialConstants {
    float mvp[16]{};
    float worldView[16]{};
};

struct IdroidVertex {
    float position[3]{};
    float normal[3]{};
    float color[4]{};
};

struct IdroidConstants {
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
    float sampleCenter[4]{}; // source UV center, magnification, weapon reticle
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
    bool authoredTangents{true};
    std::string sha256;
    bool nativeBinocularMaterial{};
    std::array<float,4> nativeMaterialIndices{};
    float min[3]{std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),std::numeric_limits<float>::max()};
    float max[3]{std::numeric_limits<float>::lowest(),std::numeric_limits<float>::lowest(),std::numeric_limits<float>::lowest()};
};

struct RetailTexture {
    std::filesystem::path path;
    std::string sha256;
    DXGI_FORMAT format{DXGI_FORMAT_UNKNOWN};
    uint32_t width{},height{};
    std::vector<std::vector<uint8_t>> mipData;
};

struct RetailGpuAsset {
    ComPtr<ID3D11Buffer> vertices;
    ComPtr<ID3D11Buffer> indices;
    ComPtr<ID3D11ShaderResourceView> diffuse;
    std::filesystem::path modelPath;
    std::filesystem::path diffusePath;
    UINT indexCount{};
    bool ready{};
    std::vector<Vertex> bindVertices;
    ComPtr<ID3D11Buffer> doorVertices,doorIndices;
    UINT doorIndexCount{};
    mgs5vr::Vec3 hinge{};
    float lid{-1};
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
    for(float value:vertex.tangent)if(!std::isfinite(value))return false;
    return true;
}

std::string ownedAssetDigest(const std::vector<uint8_t>& bytes){
    struct Provider {BCRYPT_ALG_HANDLE value{};~Provider(){if(value)BCryptCloseAlgorithmProvider(value,0);}} provider;
    struct Hash {BCRYPT_HASH_HANDLE value{};~Hash(){if(value)BCryptDestroyHash(value);}} hash;
    if(bytes.empty()||bytes.size()>std::numeric_limits<ULONG>::max()
        ||BCryptOpenAlgorithmProvider(&provider.value,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0
        ||BCryptCreateHash(provider.value,&hash.value,nullptr,0,nullptr,0,0)<0
        ||BCryptHashData(hash.value,const_cast<PUCHAR>(bytes.data()),static_cast<ULONG>(bytes.size()),0)<0)return {};
    std::array<UCHAR,32> result{};
    if(BCryptFinishHash(hash.value,result.data(),static_cast<ULONG>(result.size()),0)<0)return {};
    std::ostringstream text;text<<std::hex<<std::setfill('0');
    for(const auto value:result)text<<std::setw(2)<<static_cast<unsigned>(value);
    return text.str();
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

std::filesystem::path retailBinocularMaterialPath(const wchar_t* key,const wchar_t* filename){
    const auto root=gameRoot();if(root.empty())return {};
    const auto fallback=retailBinocularDiffusePath().parent_path()/filename;
    std::array<wchar_t,32768> configured{};
    const auto length=GetPrivateProfileStringW(L"optics",key,fallback.c_str(),configured.data(),
        static_cast<DWORD>(configured.size()),(root/L"mgs5vr.ini").c_str());
    if(!length||length>=configured.size())return {};
    std::filesystem::path path(configured.data());return path.is_relative()?root/path:path;
}

std::filesystem::path openingAssetPath(const wchar_t* key,const wchar_t* fallback){
    const auto root=gameRoot();
    if(root.empty())return {};
    std::array<wchar_t,32768> configured{};
    const auto ini=root/L"mgs5vr.ini";
    const auto length=GetPrivateProfileStringW(L"opening",key,fallback,configured.data(),
        static_cast<DWORD>(configured.size()),ini.c_str());
    if(!length||length>=configured.size())return {};
    std::filesystem::path path(configured.data());
    if(path.is_relative())path=root/path;
    return path;
}

std::optional<RetailModel> readRetailFmdl(const std::filesystem::path& path,std::string& failure,int selectedMesh=-1){
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
    std::optional<Section1Info> bufferSection,parameterSection;
    const auto section1Table=static_cast<size_t>(sectionInfoOffset)+section0Count*8u;
    for(uint32_t i=0;i<section1Count;++i){
        Section1Info info{};
        if(!readAt(bytes,section1Table+i*12,info)){
            failure="truncated FMDL section table: "+path.string();return {};
        }
        if(info.type==2)bufferSection=info;
        if(info.type==0)parameterSection=info;
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
    RetailModel model;model.path=path;model.sha256=ownedAssetDigest(bytes);
    for(uint16_t meshIndex=0;meshIndex<meshInfo->count;++meshIndex){
        if(selectedMesh>=0&&meshIndex!=selectedMesh)continue;
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
            bool tangentPresent=false;
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
                }else if(attribute.type==14){
                    uint16_t h[4]{};
                    // Other existing model paths do not use this attribute.
                    // An unknown tangent encoding only disables native material
                    // eligibility; it does not replace their legacy loader.
                    if(attribute.dataType!=6)continue;
                    if(static_cast<size_t>(attribute.offset)+sizeof(h)>attributeFormat.length
                        ||!readAt(bytes,at,h)){
                        failure="retail FMDL tangent read failed: "+path.string();return {};
                    }
                    // X reflection reverses the tangent-frame determinant.
                    // Flip handedness so cross(N,T)*w remains the reflected
                    // authored binormal used by the native 4MT shader.
                    vertex.tangent[0]=-halfToFloat(h[0]);vertex.tangent[1]=halfToFloat(h[1]);
                    vertex.tangent[2]=halfToFloat(h[2]);vertex.tangent[3]=-halfToFloat(h[3]);
                    tangentPresent=true;
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
            model.authoredTangents=model.authoredTangents&&tangentPresent;
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
    // Bind the native material only to the exact supported owned FMDL.
    // Read its typed hashes/references and vectors; no filename inference.
    if(model.sha256=="935739377e6e0b14eb7186e778e265e65c8e2f66d97b8909d74725800eb21011"
        &&model.authoredTangents&&parameterSection){
        const auto instances=need(4),textures=need(6),parameters=need(7),materials=need(8),paths=need(21),names=need(22);
        constexpr std::array<uint64_t,4> parameterNames{0xcab8e38487ffull,0x7bbd89662077ull,0xe715065f2f0eull,0xc9e199a419a6ull};
        bool valid=instances&&textures&&parameters&&materials&&paths&&names&&instances->count==1&&materials->count==1
            &&textures->count==4&&parameters->count==8&&paths->count==4;
        uint16_t materialIndex{},typeIndex{},firstTexture{},firstParameter{};uint8_t textureCount{},parameterCount{};
        const auto hashName=[&](uint16_t index,uint64_t expected){uint64_t actual{};
            return names&&index<names->count&&readAt(bytes,static_cast<size_t>(section0Offset)+names->offset+index*8u,actual)
                &&(actual&0xffffffffffffull)==expected;};
        if(valid){const auto at=static_cast<size_t>(section0Offset)+instances->offset;
            valid=readAt(bytes,at+4,materialIndex)&&materialIndex==0&&readAt(bytes,at+6,textureCount)&&textureCount==4
                &&readAt(bytes,at+7,parameterCount)&&parameterCount==4&&readAt(bytes,at+8,firstTexture)&&firstTexture==0
                &&readAt(bytes,at+10,firstParameter)&&firstParameter==4&&readAt(bytes,static_cast<size_t>(section0Offset)+materials->offset+2,typeIndex)
                &&hashName(typeIndex,0x7f9271882430ull);}
        for(size_t i=0;valid&&i<4;++i){
            uint16_t nameIndex{},referenceIndex{},textureTypeIndex{},typeBindingIndex{};
            mgs5vr::OpticBinocularTextureRecord textureRecord;
            const auto textureIndex=static_cast<size_t>(firstTexture)+i;
            valid=textureIndex<textures->count
                &&readAt(bytes,static_cast<size_t>(section0Offset)+textures->offset+textureIndex*4,nameIndex)
                &&readAt(bytes,static_cast<size_t>(section0Offset)+textures->offset+textureIndex*4+2,referenceIndex)
                &&nameIndex<names->count&&referenceIndex<paths->count
                &&readAt(bytes,static_cast<size_t>(section0Offset)+names->offset+nameIndex*8u,textureRecord.bindingName)
                &&readAt(bytes,static_cast<size_t>(section0Offset)+paths->offset+referenceIndex*8u,textureRecord.path)
                &&readAt(bytes,static_cast<size_t>(section0Offset)+parameters->offset+i*4,textureTypeIndex)
                &&readAt(bytes,static_cast<size_t>(section0Offset)+parameters->offset+i*4+2,typeBindingIndex)
                &&textureTypeIndex<names->count
                &&readAt(bytes,static_cast<size_t>(section0Offset)+names->offset+textureTypeIndex*8u,textureRecord.typeName);
            textureRecord.bindingName&=0xffffffffffffull;textureRecord.typeName&=0xffffffffffffull;
            textureRecord.typeBindingIndex=typeBindingIndex;textureRecord.pathIndex=referenceIndex;
            valid=valid&&mgs5vr::opticBinocularTextureRecordEligible(i,textureRecord);
            const auto parameterIndex=static_cast<size_t>(firstParameter)+i;
            std::array<float,4> vector{};
            valid=valid&&parameterIndex<parameters->count
                &&readAt(bytes,static_cast<size_t>(section0Offset)+parameters->offset+parameterIndex*4,nameIndex)
                &&readAt(bytes,static_cast<size_t>(section0Offset)+parameters->offset+parameterIndex*4+2,referenceIndex)
                &&hashName(nameIndex,parameterNames[i])&&referenceIndex<parameterSection->length/16u
                &&readAt(bytes,static_cast<size_t>(section1Offset)+parameterSection->offset+referenceIndex*16u,vector)
                &&std::all_of(vector.begin(),vector.end(),[](float value){return std::isfinite(value);});
            if(valid)model.nativeMaterialIndices[i]=mgs5vr::opticNativeMaterialIndex(vector[0]);
        }
        model.nativeBinocularMaterial=valid;
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
    RetailTexture texture;texture.path=path;texture.sha256=ownedAssetDigest(bytes);
    texture.format=format;texture.width=width;texture.height=height;
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

bool createRetailDiffuseView(ID3D11Device* device,const RetailTexture& diffuse,
    ComPtr<ID3D11ShaderResourceView>& output,bool srgb=true){
    if(!device||!diffuse.width||!diffuse.height||diffuse.mipData.empty())return false;
    D3D11_TEXTURE2D_DESC description{};description.Width=diffuse.width;description.Height=diffuse.height;
    description.MipLevels=static_cast<UINT>(diffuse.mipData.size());description.ArraySize=1;
    description.Format=srgb?(diffuse.format==DXGI_FORMAT_BC1_UNORM?DXGI_FORMAT_BC1_UNORM_SRGB:DXGI_FORMAT_BC3_UNORM_SRGB)
        :diffuse.format;
    description.SampleDesc.Count=1;description.Usage=D3D11_USAGE_IMMUTABLE;description.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    std::vector<D3D11_SUBRESOURCE_DATA> data;data.reserve(diffuse.mipData.size());
    uint32_t width=diffuse.width;
    for(const auto& mip:diffuse.mipData){
        D3D11_SUBRESOURCE_DATA level{};level.pSysMem=mip.data();
        const uint32_t blockBytes=diffuse.format==DXGI_FORMAT_BC1_UNORM?8u:16u;
        level.SysMemPitch=std::max<uint32_t>(1,(width+3)/4)*blockBytes;
        data.push_back(level);width=std::max<uint32_t>(1,width/2);
    }
    ComPtr<ID3D11Texture2D> texture;
    if(FAILED(device->CreateTexture2D(&description,data.data(),texture.GetAddressOf())))return false;
    D3D11_SHADER_RESOURCE_VIEW_DESC view{};view.Format=description.Format;
    view.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;view.Texture2D.MostDetailedMip=0;
    view.Texture2D.MipLevels=static_cast<UINT>(diffuse.mipData.size());
    return SUCCEEDED(device->CreateShaderResourceView(texture.Get(),&view,output.ReleaseAndGetAddressOf()));
}

bool createRetailGpuAsset(ID3D11Device* device,RetailGpuAsset& asset,
    const std::filesystem::path& modelPath,const std::filesystem::path& diffusePath,const char* label){
    if(!device||modelPath.empty()||diffusePath.empty())return false;
    std::string failure;const auto model=readRetailFmdl(modelPath,failure);
    if(!model){mgs5vr::log(std::string("Opening ")+label+" FMDL load failed: "+failure);return false;}
    const auto diffuse=readRetailDds(diffusePath,failure);
    if(!diffuse){mgs5vr::log(std::string("Opening ")+label+" diffuse load failed: "+failure);return false;}
    if(model->vertices.empty()||model->indices.empty()
       ||model->vertices.size()>std::numeric_limits<UINT>::max()/sizeof(Vertex)
       ||model->indices.size()>std::numeric_limits<UINT>::max()/sizeof(uint16_t))return false;
    D3D11_BUFFER_DESC vertexDescription{};vertexDescription.ByteWidth=static_cast<UINT>(model->vertices.size()*sizeof(Vertex));
    vertexDescription.Usage=D3D11_USAGE_IMMUTABLE;
    vertexDescription.BindFlags=D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vertexData{};vertexData.pSysMem=model->vertices.data();
    if(FAILED(device->CreateBuffer(&vertexDescription,&vertexData,asset.vertices.ReleaseAndGetAddressOf())))return false;
    auto bodyIndices=model->indices;std::vector<uint16_t> doorIndices;
    if(model->sha256=="26fa26a4814aa3271e06392a1bdf96bd2120b597bd89de79b7ff588b8d8dbea7"){
        bodyIndices.clear();
        // The owned player has a static front cover. Its control bone is a
        // small lever, not a door. Separate complete cover triangles and add
        // a VR hinge at the authored upper cover edge; retain its native UVs.
        for(size_t face=0;face<model->indices.size();face+=3){
            bool front=true;float normal{};
            for(size_t i=0;i<3;++i){const auto& vertex=model->vertices[model->indices[face+i]];
                front=front&&vertex.position[2]>=.010f;normal+=vertex.normal[2];}
            auto& target=front&&normal>1.8f?doorIndices:bodyIndices;
            target.insert(target.end(),model->indices.begin()+face,model->indices.begin()+face+3);
        }
        if(!doorIndices.empty()){
            asset.bindVertices=model->vertices;asset.hinge={0,.040f,model->max[2]};
            auto doorDescription=vertexDescription;doorDescription.Usage=D3D11_USAGE_DYNAMIC;
            doorDescription.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
            if(FAILED(device->CreateBuffer(&doorDescription,&vertexData,&asset.doorVertices)))return false;
            D3D11_BUFFER_DESC indices{};indices.ByteWidth=static_cast<UINT>(doorIndices.size()*sizeof(uint16_t));
            indices.Usage=D3D11_USAGE_IMMUTABLE;indices.BindFlags=D3D11_BIND_INDEX_BUFFER;
            D3D11_SUBRESOURCE_DATA data{};data.pSysMem=doorIndices.data();
            if(FAILED(device->CreateBuffer(&indices,&data,&asset.doorIndices)))return false;
            asset.doorIndexCount=static_cast<UINT>(doorIndices.size());
        }
    }
    D3D11_BUFFER_DESC indexDescription{};indexDescription.ByteWidth=static_cast<UINT>(bodyIndices.size()*sizeof(uint16_t));
    indexDescription.Usage=D3D11_USAGE_IMMUTABLE;indexDescription.BindFlags=D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA indexData{};indexData.pSysMem=bodyIndices.data();
    if(FAILED(device->CreateBuffer(&indexDescription,&indexData,asset.indices.ReleaseAndGetAddressOf()))
       ||!createRetailDiffuseView(device,*diffuse,asset.diffuse))return false;
    asset.modelPath=modelPath;asset.diffusePath=diffusePath;asset.indexCount=static_cast<UINT>(bodyIndices.size());asset.ready=true;
    std::ostringstream message;message<<"Opening "<<label<<" FMDL loaded path="<<modelPath.string()
        <<" bytes="<<std::filesystem::file_size(modelPath)<<" vertices="<<model->vertices.size()
        <<" triangles="<<model->indices.size()/3<<" bounds="<<model->min[0]<<","<<model->min[1]<<","<<model->min[2]<<".."
        <<model->max[0]<<","<<model->max[1]<<","<<model->max[2];mgs5vr::log(message.str());
    return true;
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
    if (baseColor.x < 0) return float4(albedo, 1);
    return float4(albedo * diffuse, baseColor.a);
}
)HLSL";

// This emits the exact native 4MT PS input signature. Its material/normal
// response remains in the verified native pixel shader, not a custom light.
const char* nativeMaterialVertexSource=R"HLSL(
cbuffer NativeOpticView : register(b0) {
    row_major float4x4 mvp;
    row_major float4x4 worldView;
};
struct VSIn {float3 position:POSITION;float3 normal:NORMAL;float4 tangent:TANGENT;float2 uv:TEXCOORD0;};
struct VSOut {
    float4 position:SV_POSITION;
    float4 color:COLOR0;
    float2 uv:TEXCOORD0;
    float3 tangent:TEXCOORD5;
    float3 binormal:TEXCOORD6;
    float3 normal:TEXCOORD7;
};
VSOut main(VSIn input) {
    VSOut output;
    output.position=mul(float4(input.position,1),mvp);
    output.normal=normalize(mul(float4(input.normal,0),worldView).xyz);
    output.tangent=normalize(mul(float4(input.tangent.xyz,0),worldView).xyz);
    output.binormal=normalize(cross(output.normal,output.tangent))*input.tangent.w;
    output.color=float4(1,1,1,1);
    output.uv=input.uv;
    return output;
}
)HLSL";

const char* idroidVertexShaderSource=R"HLSL(
cbuffer IdroidView : register(b0) {
    row_major float4x4 mvp;
    row_major float4x4 world;
    float4 baseColor;
};
struct VSIn { float3 position : POSITION; float3 normal : NORMAL; float4 color : COLOR; };
struct VSOut { float4 position : SV_POSITION; float3 normal : NORMAL0; float4 color : COLOR0; };
VSOut main(VSIn input) {
    VSOut output;
    output.position=mul(float4(input.position,1.0),mvp);
    output.normal=mul(float4(input.normal,0.0),world).xyz;
    output.color=input.color;
    return output;
}
)HLSL";

const char* idroidPixelShaderSource=R"HLSL(
cbuffer IdroidView : register(b0) {
    row_major float4x4 mvp;
    row_major float4x4 world;
    float4 baseColor;
};
struct PSIn { float4 position : SV_POSITION; float3 normal : NORMAL0; float4 color : COLOR0; };
float4 main(PSIn input) : SV_TARGET {
    float3 n=normalize(input.normal);
    float light=.35+.65*saturate(dot(n,normalize(float3(-.35,.8,-.5))));
    return float4(input.color.rgb*light*baseColor.rgb,input.color.a*baseColor.a);
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
    // The lens stays on the physical ocular at every distance. Expanding it
    // into screen space covered the normal world and reversed its X direction
    // on approach, creating a changing zoom/mirror transition.
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
    if(sampleCenter.w>2.5) {
        // A 1x collimator retains the native transparent glass and stereo
        // scene. Only its sight point emits light; no copied lens image.
        float edgeWidth=max(length(fwidth(input.aperture)),0.0001);
        float dotMask=1.0-smoothstep(0.018-edgeWidth,0.018+edgeWidth,length(input.aperture));
        if(dotMask<=0.001) discard;
        return float4(1.0,0.035,0.015,dotMask);
    }
    // Perspective-correct coordinates on the physical ocular, independent
    // of its size or position in either HMD eye. Zoom is rendered by the
    // device camera, never manufactured by cropping the HMD image.
    float2 sourceUv=float2(0.5+0.5*input.aperture.x,0.5-0.5*input.aperture.y);
    float4 color=nativeScene.SampleLevel(sceneSampler,saturate(sourceUv),0.0);
    if(sampleCenter.w>1.5) {
        float edgeWidth=max(length(fwidth(input.aperture)),0.0001);
        float dotMask=1.0-smoothstep(0.018-edgeWidth,0.018+edgeWidth,length(input.aperture));
        color.rgb=lerp(color.rgb,float3(1.0,0.035,0.015),dotMask);
    } else if(sampleCenter.w>0.5) {
        // A world-scale weapon reticle: never a head-locked aiming overlay.
        float2 pixelWidth=max(fwidth(input.aperture),float2(0.0001,0.0001));
        float2 stroke=1.0-smoothstep(pixelWidth*0.65,pixelWidth*1.35,abs(input.aperture));
        float cross=max(stroke.x,stroke.y);
        color.rgb=lerp(color.rgb,float3(0.01,0.01,0.01),cross);
    }
    float edge=1.0-smoothstep(0.92,1.0,length(input.aperture));
    color.rgb*=edge;
    return color;
}
)HLSL";

const char* wristHintPixelShaderSource=R"HLSL(
Texture2D hint : register(t0);
SamplerState hintSampler : register(s0);
struct PSIn { float4 position : SV_POSITION; float2 aperture : APERTURE; };
float4 main(PSIn input) : SV_TARGET { return hint.Sample(hintSampler,input.aperture); }
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
    ComPtr<ID3D11ShaderResourceView> nativeNormal,nativeSpecular,nativeMaterialMap;
    ComPtr<ID3D11VertexShader> nativeMaterialVertexShader;
    ComPtr<ID3D11PixelShader> nativeMaterialPixelShader;
    ComPtr<ID3D11InputLayout> nativeMaterialInputLayout;
    ComPtr<ID3D11Buffer> nativeMaterialView,nativeMaterialParameters;
    std::array<float,4> nativeMaterialIndices{};
    bool nativeMaterialRecognized{},nativeMaterialAttempted{},nativeMaterialReady{},nativeMaterialReported{};
    ComPtr<ID3D11Texture2D> sceneCopy;
    ComPtr<ID3D11ShaderResourceView> scene;
    ComPtr<ID3D11RenderTargetView> sceneOverlayTarget;
    ComPtr<ID3D11Buffer> markerVertices;
    ComPtr<ID3D11VertexShader> markerVertexShader;
    ComPtr<ID3D11PixelShader> markerPixelShader;
    ComPtr<ID3D11InputLayout> markerInputLayout;
    ComPtr<ID3D11RasterizerState> markerRasterizer;
    ComPtr<ID3D11DepthStencilState> markerDepth;
    ComPtr<ID3D11BlendState> eyepatchBlend;
    RetailGpuAsset eyepatch;
    ComPtr<ID3D11VertexShader> eyepatchVs;
    ComPtr<ID3D11PixelShader> eyepatchPs;
    ComPtr<ID3D11InputLayout> eyepatchLayout;
    ComPtr<ID3D11SamplerState> eyepatchSampler;
    bool eyepatchAttempted{};
    ComPtr<ID3D11DepthStencilState> markerMaskWrite,markerMaskRead;
    ComPtr<ID3D11Texture2D> markerMaskTexture;
    ComPtr<ID3D11DepthStencilView> markerMask;
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
    ComPtr<ID3D11BlendState> nativeHousingDepthOnlyBlend;
    std::array<mgs5vr::OpticNativeLightingSource,2> nativeInsertedSources;
    std::array<bool,2> nativeInsertedValid{};
    std::array<uint64_t,2> nativeInsertedGeneration{};
    ComPtr<ID3D11Buffer> idroidVertices;
    ComPtr<ID3D11Buffer> idroidCursorVertices;
    ComPtr<ID3D11Buffer> idroidConstants;
    ComPtr<ID3D11VertexShader> idroidVertexShader;
    ComPtr<ID3D11PixelShader> idroidPixelShader;
    ComPtr<ID3D11InputLayout> idroidInputLayout;
    ComPtr<ID3D11RasterizerState> idroidRasterizer;
    ComPtr<ID3D11DepthStencilState> idroidDepthStencil;
    ComPtr<ID3D11DepthStencilState> idroidReversedDepthStencil;
    ComPtr<ID3D11BlendState> idroidBlend;
    ComPtr<ID3D11Texture2D> housingDepth;
    ComPtr<ID3D11DepthStencilView> housingDepthView;
    std::array<RetailGpuAsset,11> studioCamera;
    RetailGpuAsset studioMarker,studioMarkerTip,studioNotes,studioStickyBack,studioPhotoBack,studioViewfinder,studioFocus;
    RetailGpuAsset idroidGuidance;
    float idroidGuidanceWidth{},idroidGuidanceAspect{};
    std::array<RetailGpuAsset,44> studioKeys;
    std::array<RetailGpuAsset,6> studioButtons,studioButtonFaces;
    RetailGpuAsset studioButtonHover,studioResume,studioConfirmControl,studioCancelControl,studioCameraStatus,studioGallery,studioPaperConfirmation,
        studioCancel,studioDelete;
    uint64_t studioGalleryPhoto{};
    std::string studioStatusText;
    struct PaperGpu {RetailGpuAsset face;uint64_t revision{};};
    std::map<uint32_t,PaperGpu> studioPapers;
    bool studioReady{};
    RetailGpuAsset openingCassette;
    std::array<RetailGpuAsset,6> openingLabels;
    RetailGpuAsset openingEjectLabel;
    ComPtr<ID3D11Buffer> wristHintVertices;
    ComPtr<ID3D11PixelShader> wristHintPixelShader;
    ComPtr<ID3D11ShaderResourceView> wristHintTexture;
    std::string wristHintBinding;
    bool wristHintReported{};
    UINT depthWidth{},depthHeight{},depthSamples{};
    D3D11_TEXTURE2D_DESC sceneDescription{};
    UINT indexCount{};
    UINT idroidVertexCount{};
    UINT idroidCursorVertexCount{};
    bool attempted{};
    bool ready{};
    bool lensReady{};
    bool lensAttempted{};
    bool reported{};
    bool lensReported{};
    bool lensFailureReported{};
    bool idroidReady{};
    bool idroidAttempted{};
    bool idroidReported{};
    bool sourceReported{};
    bool depthReported{};
    bool markerReported{};
    bool openingAttempted{};
    bool openingReady{};
    bool openingReported{};
    uint64_t lensTraceCalls{};
};

std::mutex rendererMutex;
Resources resources;

void logHresult(const char* what,HRESULT result,ID3DBlob* errors){
    std::string message=what;message+=" hr=0x";char hex[16]{};
    std::snprintf(hex,sizeof(hex),"%08lx",static_cast<unsigned long>(result));message+=hex;
    if(errors&&errors->GetBufferPointer()&&errors->GetBufferSize()){
        const auto* errorText=static_cast<const char*>(errors->GetBufferPointer());
        auto errorBytes=errors->GetBufferSize();while(errorBytes&&errorText[errorBytes-1]=='\0')--errorBytes;
        message+=" ";message.append(errorText,errorBytes);
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

bool createLensResources(ID3D11Device* device){
    if(resources.lensReady)return true;
    if(!device||resources.lensAttempted)return false;
    resources.lensAttempted=true;
    const std::array<LensVertex,4> vertices{{
        {{ocularCenter.x-ocularRadius,ocularCenter.y-ocularRadius,ocularCenter.z},{-1,-1}},
        {{ocularCenter.x-ocularRadius,ocularCenter.y+ocularRadius,ocularCenter.z},{-1,1}},
        {{ocularCenter.x+ocularRadius,ocularCenter.y-ocularRadius,ocularCenter.z},{1,-1}},
        {{ocularCenter.x+ocularRadius,ocularCenter.y+ocularRadius,ocularCenter.z},{1,1}}
    }};
    D3D11_BUFFER_DESC vb{};vb.ByteWidth=sizeof(vertices);vb.Usage=D3D11_USAGE_IMMUTABLE;vb.BindFlags=D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA data{};data.pSysMem=vertices.data();
    D3D11_BUFFER_DESC cb{};cb.ByteWidth=sizeof(LensConstants);cb.Usage=D3D11_USAGE_DEFAULT;cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    ComPtr<ID3DBlob> vs,ps;
    if(!compile(device,lensVertexShaderSource,"vs_4_0",vs.GetAddressOf())
       ||!compile(device,lensPixelShaderSource,"ps_4_0",ps.GetAddressOf()))return false;
    const std::array<D3D11_INPUT_ELEMENT_DESC,2> elements{{
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"APERTURE",0,DXGI_FORMAT_R32G32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0}}};
    D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;raster.CullMode=D3D11_CULL_NONE;raster.DepthClipEnable=TRUE;
    D3D11_DEPTH_STENCIL_DESC depth{};depth.DepthEnable=TRUE;depth.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;depth.DepthFunc=D3D11_COMPARISON_LESS_EQUAL;
    D3D11_BLEND_DESC blend{};blend.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
    D3D11_SAMPLER_DESC sampler{};sampler.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.ComparisonFunc=D3D11_COMPARISON_NEVER;sampler.MaxLOD=D3D11_FLOAT32_MAX;
    if(FAILED(device->CreateBuffer(&vb,&data,resources.lensVertices.ReleaseAndGetAddressOf()))
       ||FAILED(device->CreateBuffer(&cb,nullptr,resources.lensConstants.ReleaseAndGetAddressOf()))
       ||FAILED(device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,resources.lensVertexShader.ReleaseAndGetAddressOf()))
       ||FAILED(device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,resources.lensPixelShader.ReleaseAndGetAddressOf()))
       ||FAILED(device->CreateInputLayout(elements.data(),static_cast<UINT>(elements.size()),vs->GetBufferPointer(),vs->GetBufferSize(),resources.lensInputLayout.ReleaseAndGetAddressOf()))
       ||FAILED(device->CreateRasterizerState(&raster,resources.lensRasterizer.ReleaseAndGetAddressOf()))
       ||FAILED(device->CreateDepthStencilState(&depth,resources.lensDepthStencil.ReleaseAndGetAddressOf()))
       ||FAILED(device->CreateBlendState(&blend,resources.blend.ReleaseAndGetAddressOf()))
       ||FAILED(device->CreateSamplerState(&sampler,resources.lensSampler.ReleaseAndGetAddressOf())))return false;
    D3D11_BLEND_DESC depthOnly{};
    if(FAILED(device->CreateBlendState(&depthOnly,resources.nativeHousingDepthOnlyBlend.ReleaseAndGetAddressOf())))return false;
    depth.DepthFunc=D3D11_COMPARISON_GREATER_EQUAL;
    if(FAILED(device->CreateDepthStencilState(&depth,resources.reversedLensDepthStencil.ReleaseAndGetAddressOf())))return false;
    resources.lensReady=true;return true;
}

bool createAccWristHintResources(ID3D11Device* device,std::string_view binding){
    if(!createLensResources(device))return false;
    if(!resources.wristHintVertices){
        // X runs along the forearm toward the palm. Keep this help toward the
        // elbow, behind the compact status readout, with the authored 4:1
        // pixel aspect unchanged: 14 cm by 3.5 cm. The old far edge was
        // 30 cm behind the wrist (including its mount setback), past the
        // native forearm and into the sleeve when the elbow bent.
        const std::array<LensVertex,4> vertices{{
            {{-.225f,-.0175f,.001f},{0,1}},{{-.225f,.0175f,.001f},{0,0}},
            {{-.085f,-.0175f,.001f},{1,1}},{{-.085f,.0175f,.001f},{1,0}}}};
        D3D11_BUFFER_DESC vb{};vb.ByteWidth=sizeof(vertices);vb.Usage=D3D11_USAGE_IMMUTABLE;vb.BindFlags=D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA data{};data.pSysMem=vertices.data();ComPtr<ID3DBlob> ps;
        if(!compile(device,wristHintPixelShaderSource,"ps_4_0",ps.GetAddressOf())
            ||FAILED(device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,resources.wristHintPixelShader.ReleaseAndGetAddressOf()))
            ||FAILED(device->CreateBuffer(&vb,&data,resources.wristHintVertices.ReleaseAndGetAddressOf())))return false;
    }
    if(resources.wristHintTexture&&resources.wristHintBinding==binding)return true;
    const auto image=mgs5vr::makeAccWristHintImage(binding);
    if(!image.width||!image.height||image.bgra.size()!=size_t(image.width)*image.height)return false;
    D3D11_TEXTURE2D_DESC texture{};texture.Width=image.width;texture.Height=image.height;
    texture.MipLevels=texture.ArraySize=texture.SampleDesc.Count=1;
    texture.Format=DXGI_FORMAT_B8G8R8A8_UNORM;texture.Usage=D3D11_USAGE_IMMUTABLE;texture.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA pixels{};pixels.pSysMem=image.bgra.data();pixels.SysMemPitch=image.width*4;
    ComPtr<ID3D11Texture2D> surface;ComPtr<ID3D11ShaderResourceView> view;
    if(FAILED(device->CreateTexture2D(&texture,&pixels,surface.GetAddressOf()))
        ||FAILED(device->CreateShaderResourceView(surface.Get(),nullptr,view.GetAddressOf())))return false;
    resources.wristHintTexture=std::move(view);resources.wristHintBinding=binding;return true;
}

void appendIdroidFace(std::vector<IdroidVertex>& vertices,mgs5vr::Vec3 a,mgs5vr::Vec3 b,
    mgs5vr::Vec3 c,mgs5vr::Vec3 d,mgs5vr::Vec3 normal,const std::array<float,4>& color){
    const auto make=[&](mgs5vr::Vec3 p){return IdroidVertex{{p.x,p.y,p.z},{normal.x,normal.y,normal.z},
        {color[0],color[1],color[2],color[3]}};};
    const auto v0=make(a),v1=make(b),v2=make(c),v3=make(d);
    for(const auto& v:{v0,v1,v2,v2,v1,v3})vertices.push_back(v);
}

void appendIdroidBox(std::vector<IdroidVertex>& vertices,mgs5vr::Vec3 minimum,mgs5vr::Vec3 maximum,
    const std::array<float,4>& color){
    const auto& a=minimum;const auto& b=maximum;
    appendIdroidFace(vertices,{a.x,a.y,b.z},{b.x,a.y,b.z},{a.x,b.y,b.z},{b.x,b.y,b.z},{0,0,1},color);
    appendIdroidFace(vertices,{b.x,a.y,a.z},{a.x,a.y,a.z},{b.x,b.y,a.z},{a.x,b.y,a.z},{0,0,-1},color);
    appendIdroidFace(vertices,{b.x,a.y,b.z},{b.x,a.y,a.z},{b.x,b.y,b.z},{b.x,b.y,a.z},{1,0,0},color);
    appendIdroidFace(vertices,{a.x,a.y,a.z},{a.x,a.y,b.z},{a.x,b.y,a.z},{a.x,b.y,b.z},{-1,0,0},color);
    appendIdroidFace(vertices,{a.x,b.y,b.z},{b.x,b.y,b.z},{a.x,b.y,a.z},{b.x,b.y,a.z},{0,1,0},color);
    appendIdroidFace(vertices,{a.x,a.y,a.z},{b.x,a.y,a.z},{a.x,a.y,b.z},{b.x,a.y,b.z},{0,-1,0},color);
}

bool createIdroidResources(ID3D11Device* device){
    if(resources.idroidReady)return true;
    if(!device||resources.idroidAttempted)return false;
    resources.idroidAttempted=true;
    // The native display is the iDroid's emitted front. Do not add a bezel or
    // a backplate here: even a thin opaque frame masks the arm during the
    // first menu frame. The real device/body remains native; this pass only
    // supplies the tracked projection cursor below.
    std::vector<IdroidVertex> vertices(1);
    const std::array<float,4> pointer{.18f,.95f,.76f,1.f};
    std::vector<IdroidVertex> cursor;cursor.reserve(72);
    // Keep the hit marker legible without swallowing the native map details.
    constexpr float cursorArm=.007f,cursorThickness=.0007f,cursorDepth=.0015f;
    appendIdroidBox(cursor,{-cursorArm,-cursorThickness,0},{cursorArm,cursorThickness,cursorDepth},pointer);
    appendIdroidBox(cursor,{-cursorThickness,-cursorArm,0},{cursorThickness,cursorArm,cursorDepth},pointer);
    if(vertices.empty()||cursor.empty()
       ||vertices.size()>std::numeric_limits<UINT>::max()/sizeof(IdroidVertex)
       ||cursor.size()>std::numeric_limits<UINT>::max()/sizeof(IdroidVertex))return false;
    D3D11_BUFFER_DESC vertexDesc{};vertexDesc.ByteWidth=static_cast<UINT>(vertices.size()*sizeof(IdroidVertex));
    vertexDesc.Usage=D3D11_USAGE_IMMUTABLE;vertexDesc.BindFlags=D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vertexData{};vertexData.pSysMem=vertices.data();
    D3D11_BUFFER_DESC cursorDesc{};cursorDesc.ByteWidth=static_cast<UINT>(cursor.size()*sizeof(IdroidVertex));
    cursorDesc.Usage=D3D11_USAGE_IMMUTABLE;cursorDesc.BindFlags=D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA cursorData{};cursorData.pSysMem=cursor.data();
    D3D11_BUFFER_DESC constantsDesc{};constantsDesc.ByteWidth=sizeof(IdroidConstants);
    constantsDesc.Usage=D3D11_USAGE_DEFAULT;constantsDesc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    ComPtr<ID3DBlob> vs,ps;
    if(!compile(device,idroidVertexShaderSource,"vs_4_0",vs.GetAddressOf())
       ||!compile(device,idroidPixelShaderSource,"ps_4_0",ps.GetAddressOf()))return false;
    const std::array<D3D11_INPUT_ELEMENT_DESC,3> elements{{
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0}}};
    D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;raster.CullMode=D3D11_CULL_NONE;raster.DepthClipEnable=FALSE;
    D3D11_DEPTH_STENCIL_DESC depth{};depth.DepthEnable=TRUE;depth.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;depth.DepthFunc=D3D11_COMPARISON_LESS_EQUAL;
    D3D11_BLEND_DESC blend{};blend.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
    if(FAILED(device->CreateBuffer(&vertexDesc,&vertexData,resources.idroidVertices.ReleaseAndGetAddressOf()))
       ||FAILED(device->CreateBuffer(&cursorDesc,&cursorData,resources.idroidCursorVertices.ReleaseAndGetAddressOf()))
       ||FAILED(device->CreateBuffer(&constantsDesc,nullptr,resources.idroidConstants.ReleaseAndGetAddressOf()))
       ||FAILED(device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,resources.idroidVertexShader.ReleaseAndGetAddressOf()))
       ||FAILED(device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,resources.idroidPixelShader.ReleaseAndGetAddressOf()))
       ||FAILED(device->CreateInputLayout(elements.data(),static_cast<UINT>(elements.size()),vs->GetBufferPointer(),vs->GetBufferSize(),resources.idroidInputLayout.ReleaseAndGetAddressOf()))
       ||FAILED(device->CreateRasterizerState(&raster,resources.idroidRasterizer.ReleaseAndGetAddressOf()))
       ||FAILED(device->CreateDepthStencilState(&depth,resources.idroidDepthStencil.ReleaseAndGetAddressOf()))
       ||FAILED(device->CreateBlendState(&blend,resources.idroidBlend.ReleaseAndGetAddressOf())))return false;
    depth.DepthFunc=D3D11_COMPARISON_GREATER_EQUAL;
    if(FAILED(device->CreateDepthStencilState(&depth,resources.idroidReversedDepthStencil.ReleaseAndGetAddressOf())))return false;
    // No overlay bezel is drawn. Keep the buffer allocated for ABI-stable
    // renderer resources while the live native pixels remain unobstructed.
    resources.idroidVertexCount=0;
    resources.idroidCursorVertexCount=static_cast<UINT>(cursor.size());
    resources.idroidReady=true;return true;
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
    if(!createLensResources(device))return false;
    ComPtr<ID3DBlob> vs,ps;
    if(!compile(device,vertexShaderSource,"vs_4_0",vs.GetAddressOf())||!compile(device,pixelShaderSource,"ps_4_0",ps.GetAddressOf()))return false;
    if(FAILED(device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,resources.vertexShader.GetAddressOf()))
       ||FAILED(device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,resources.pixelShader.GetAddressOf())))return false;
    const std::array<D3D11_INPUT_ELEMENT_DESC,3> elements{{
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0}}};
    if(FAILED(device->CreateInputLayout(elements.data(),static_cast<UINT>(elements.size()),vs->GetBufferPointer(),vs->GetBufferSize(),resources.inputLayout.GetAddressOf())))return false;
    D3D11_RASTERIZER_DESC rasterizer{};rasterizer.FillMode=D3D11_FILL_SOLID;rasterizer.CullMode=D3D11_CULL_NONE;
    // The tracked scene already uses its close near plane. Clip housing
    // triangles that cross the eye rather than projecting a near/behind-eye
    // face across the surrounding world.
    rasterizer.DepthClipEnable=TRUE;
    if(FAILED(device->CreateRasterizerState(&rasterizer,resources.rasterizer.GetAddressOf())))return false;
    D3D11_DEPTH_STENCIL_DESC depth{};depth.DepthEnable=TRUE;depth.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;depth.DepthFunc=D3D11_COMPARISON_LESS_EQUAL;
    if(FAILED(device->CreateDepthStencilState(&depth,resources.depthStencil.GetAddressOf())))return false;
    depth.DepthFunc=D3D11_COMPARISON_GREATER_EQUAL;
    if(FAILED(device->CreateDepthStencilState(&depth,resources.reversedDepthStencil.GetAddressOf())))return false;
    resources.indexCount=static_cast<UINT>(model->indices.size());resources.ready=true;
    resources.nativeMaterialRecognized=model->nativeBinocularMaterial
        &&diffuse->sha256=="7cb40d536f37faa66d8153ef04afe6a23331d5bce45908dc7aa3f63558f566b3";
    resources.nativeMaterialIndices=model->nativeMaterialIndices;
    std::ostringstream message;message<<"Retail binocular FMDL loaded path="<<model->path.string()
        <<" bytes="<<std::filesystem::file_size(model->path)<<" vertices="<<model->vertices.size()
        <<" triangles="<<model->indices.size()/3<<" bounds="<<model->min[0]<<","<<model->min[1]<<","<<model->min[2]<<".."
        <<model->max[0]<<","<<model->max[1]<<","<<model->max[2]
        <<" authored_tangents="<<model->authoredTangents<<" native_material_recognized="<<resources.nativeMaterialRecognized;mgs5vr::log(message.str());
    std::ostringstream textureMessage;textureMessage<<"Retail binocular diffuse loaded path="<<diffuse->path.string()
        <<" size="<<diffuse->width<<"x"<<diffuse->height<<" mips="<<diffuse->mipData.size()
        <<" format="<<(diffuse->format==DXGI_FORMAT_BC1_UNORM?"BC1_SRGB":"BC3_SRGB");mgs5vr::log(textureMessage.str());
    return true;
}

bool createCabinSurface(ID3D11Device* device,RetailGpuAsset& asset,
    const std::vector<Vertex>& vertices,const std::vector<uint16_t>& indices,
    const std::vector<uint32_t>& pixels,UINT width,UINT height){
    if(vertices.empty()||indices.empty()||pixels.size()!=size_t(width)*height)return false;
    D3D11_BUFFER_DESC vb{};vb.ByteWidth=static_cast<UINT>(vertices.size()*sizeof(Vertex));
    vb.Usage=D3D11_USAGE_IMMUTABLE;vb.BindFlags=D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA data{};data.pSysMem=vertices.data();
    if(FAILED(device->CreateBuffer(&vb,&data,&asset.vertices)))return false;
    vb.ByteWidth=static_cast<UINT>(indices.size()*sizeof(uint16_t));vb.BindFlags=D3D11_BIND_INDEX_BUFFER;data.pSysMem=indices.data();
    if(FAILED(device->CreateBuffer(&vb,&data,&asset.indices)))return false;
    // Full mip chain keeps the printed label sharp at hand distance without
    // turning small text into shimmer when it rests farther away on the bench.
    std::vector<std::vector<uint32_t>> levels{pixels};UINT w=width,h=height;
    while(w>1||h>1){const UINT nw=std::max(1u,w/2),nh=std::max(1u,h/2);std::vector<uint32_t> next(size_t(nw)*nh);
        for(UINT y=0;y<nh;++y)for(UINT x=0;x<nw;++x){uint32_t value=0xff000000;
            for(unsigned c=0;c<3;++c){unsigned sum{};for(UINT dy=0;dy<2;++dy)for(UINT dx=0;dx<2;++dx)
                sum+=(levels.back()[size_t(std::min(y*2+dy,h-1))*w+std::min(x*2+dx,w-1)]>>(c*8))&255;
                value|=(sum/4)<<(c*8);}next[size_t(y)*nw+x]=value;}
        levels.push_back(std::move(next));w=nw;h=nh;
    }
    std::vector<D3D11_SUBRESOURCE_DATA> mips;w=width;
    for(const auto& level:levels){D3D11_SUBRESOURCE_DATA mip{};mip.pSysMem=level.data();mip.SysMemPitch=w*4;
        mips.push_back(mip);w=std::max(1u,w/2);}
    D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=height;desc.MipLevels=static_cast<UINT>(levels.size());
    desc.ArraySize=desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> texture;
    if(FAILED(device->CreateTexture2D(&desc,mips.data(),&texture))
       ||FAILED(device->CreateShaderResourceView(texture.Get(),nullptr,&asset.diffuse)))return false;
    asset.indexCount=static_cast<UINT>(indices.size());asset.ready=true;return true;
}
bool studioBox(ID3D11Device* device,RetailGpuAsset& asset,mgs5vr::Vec3 half,uint32_t color,
    mgs5vr::Vec3 center={}){
    std::vector<IdroidVertex> geometry;appendIdroidBox(geometry,center-half,center+half,{1,1,1,1});
    std::vector<Vertex> vertices;std::vector<uint16_t> indices;
    for(const auto& source:geometry){Vertex vertex{};std::copy(std::begin(source.position),std::end(source.position),vertex.position);
        std::copy(std::begin(source.normal),std::end(source.normal),vertex.normal);
        indices.push_back(static_cast<uint16_t>(vertices.size()));vertices.push_back(vertex);}
    return createCabinSurface(device,asset,vertices,indices,{color},1,1);
}
bool studioBevel(ID3D11Device* device,RetailGpuAsset& asset,mgs5vr::Vec3 half,float radius,
    uint32_t color,mgs5vr::Vec3 center={}){
    radius=std::min({radius,half.x*.4f,half.y*.4f,half.z*.4f});
    std::vector<Vertex> vertices;std::vector<uint16_t> indices;
    const auto face=[&](mgs5vr::Vec3 a,mgs5vr::Vec3 b,mgs5vr::Vec3 c){
        const auto u=b-a,edgeV=c-a;
        const mgs5vr::Vec3 cross{u.y*edgeV.z-u.z*edgeV.y,u.z*edgeV.x-u.x*edgeV.z,u.x*edgeV.y-u.y*edgeV.x};
        const auto n=cross*(1.f/std::max(.000001f,std::sqrt(mgs5vr::dot(cross,cross))));
        for(auto p:{a,b,c}){p=p+center;Vertex v{};v.position[0]=p.x;v.position[1]=p.y;v.position[2]=p.z;
            v.normal[0]=n.x;v.normal[1]=n.y;v.normal[2]=n.z;
            indices.push_back(static_cast<uint16_t>(vertices.size()));vertices.push_back(v);}
    };
    std::array<std::array<mgs5vr::Vec3,8>,4> rings{};
    for(size_t ring=0;ring<4;++ring){
        const bool cap=ring==0||ring==3;const float h=cap?radius*.35f:0;
        const float x=half.x-h,y=half.y-h,r=radius-h,z=ring==0?-half.z:ring==1?-half.z+radius:ring==2?half.z-radius:half.z;
        rings[ring]={mgs5vr::Vec3{-x+r,-y,z},{x-r,-y,z},{x,-y+r,z},{x,y-r,z},
            {x-r,y,z},{-x+r,y,z},{-x,y-r,z},{-x,-y+r,z}};
    }
    for(size_t ring=0;ring<3;++ring)for(size_t i=0;i<8;++i){const size_t j=(i+1)%8;
        face(rings[ring][i],rings[ring][j],rings[ring+1][i]);
        face(rings[ring+1][i],rings[ring][j],rings[ring+1][j]);}
    for(size_t i=0;i<8;++i){const size_t j=(i+1)%8;
        face({0,0,-half.z},rings[0][j],rings[0][i]);face({0,0,half.z},rings[3][i],rings[3][j]);}
    return createCabinSurface(device,asset,vertices,indices,{color},1,1);
}
bool studioQuad(ID3D11Device* device,RetailGpuAsset& asset,mgs5vr::Vec3 half,
    const std::vector<uint32_t>& pixels,UINT width,UINT height,float z=.001f){
    std::vector<Vertex> vertices;const std::vector<uint16_t> indices{0,1,2,2,1,3};
    for(const auto p:std::array<std::array<float,4>,4>{{{-half.x,-half.y,0,1},{half.x,-half.y,1,1},
        {-half.x,half.y,0,0},{half.x,half.y,1,0}}}){
        Vertex v{};v.position[0]=p[0];v.position[1]=p[1];v.position[2]=z;v.normal[2]=1;
        v.uv[0]=p[2];v.uv[1]=p[3];vertices.push_back(v);}
    return createCabinSurface(device,asset,vertices,indices,pixels,width,height);
}
bool studioCylinder(ID3D11Device* device,RetailGpuAsset& asset,float radius,float from,float to,
    uint32_t color,mgs5vr::Vec3 center={}){
    std::vector<Vertex> vertices;std::vector<uint16_t> indices;
    const auto vertex=[&](float x,float y,float z,float nx,float ny,float nz){Vertex v{};
        v.position[0]=x+center.x;v.position[1]=y+center.y;v.position[2]=z+center.z;
        v.normal[0]=nx;v.normal[1]=ny;v.normal[2]=nz;indices.push_back(static_cast<uint16_t>(vertices.size()));vertices.push_back(v);};
    for(int segment=0;segment<32;++segment){
        const float a=segment*6.2831853f/32,b=(segment+1)*6.2831853f/32;
        const float x=std::cos(a),y=std::sin(a),xx=std::cos(b),yy=std::sin(b);
        vertex(x*radius,y*radius,from,x,y,0);vertex(xx*radius,yy*radius,from,xx,yy,0);vertex(x*radius,y*radius,to,x,y,0);
        vertex(x*radius,y*radius,to,x,y,0);vertex(xx*radius,yy*radius,from,xx,yy,0);vertex(xx*radius,yy*radius,to,xx,yy,0);
        vertex(0,0,from,0,0,-1);vertex(xx*radius,yy*radius,from,0,0,-1);vertex(x*radius,y*radius,from,0,0,-1);
        vertex(0,0,to,0,0,1);vertex(x*radius,y*radius,to,0,0,1);vertex(xx*radius,yy*radius,to,0,0,1);
    }
    return createCabinSurface(device,asset,vertices,indices,{color},1,1);
}
std::vector<uint32_t> studioPaperPixels(const mgs5vr::StudioPaper& paper,UINT size=1024){
    static const bool fontLoaded=[](){const auto path=openingAssetPath(L"studio_font",
        L"assets\\fonts\\permanent-marker\\PermanentMarker-Regular.ttf");return AddFontResourceExW(path.c_str(),FR_PRIVATE,nullptr)>0;}();
    BITMAPINFO bitmap{};bitmap.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bitmap.bmiHeader.biWidth=size;
    bitmap.bmiHeader.biHeight=-static_cast<LONG>(size);bitmap.bmiHeader.biPlanes=1;bitmap.bmiHeader.biBitCount=32;
    HDC dc=CreateCompatibleDC(nullptr);void* raw{};HBITMAP surface=CreateDIBSection(dc,&bitmap,DIB_RGB_COLORS,&raw,nullptr,0);
    if(!dc||!surface||!raw){if(surface)DeleteObject(surface);if(dc)DeleteDC(dc);return {};}
    const auto old=SelectObject(dc,surface);auto* bits=static_cast<uint32_t*>(raw);
    const bool photo=paper.photo!=0;std::fill(bits,bits+size_t(size)*size,photo?0xfff4f0e7:0xfff8e885);
    if(photo)if(const auto image=mgs5vr::studioPhotoPixels(paper.photo)){
        const UINT inset=size/17,photoSize=size-2*inset;
        for(UINT y=0;y<photoSize;++y)for(UINT x=0;x<photoSize;++x)
            bits[size_t(y+inset)*size+x+inset]=image->bgra[size_t(y)*image->height/photoSize*image->width+size_t(x)*image->width/photoSize];
        // A real Polaroid border remains available for the caption.
        for(UINT y=size*81/100;y<size;++y)std::fill(bits+size_t(y)*size,bits+size_t(y+1)*size,0xfff4f0e7);
    }
    const auto& ink=*paper.ink;
    const auto letters=ink.text.size();
    const float lettering=photo?(letters<=16?.09f:.047f)
        :letters<=8?.25f:letters<=24?.18f:letters<=48?.135f:.10f;
    HFONT font=CreateFontW(-static_cast<int>(size*lettering),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
        DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,
        fontLoaded?L"Permanent Marker":L"Segoe Print");
    const auto oldFont=SelectObject(dc,font);SetTextColor(dc,RGB(22,23,24));SetBkMode(dc,TRANSPARENT);
    RECT rect{static_cast<LONG>(size/18),static_cast<LONG>(photo?size*83/100:size/15),
        static_cast<LONG>(size-size/18),static_cast<LONG>(size-size/24)};
    const std::wstring caption(ink.text.begin(),ink.text.end());
    DrawTextW(dc,caption.c_str(),static_cast<int>(caption.size()),&rect,DT_WORDBREAK|DT_NOPREFIX);
    HPEN pen=CreatePen(PS_SOLID,std::max(2,static_cast<int>(size/130)),RGB(18,18,20));const auto oldPen=SelectObject(dc,pen);
    for(const auto& stroke:ink.strokes)if(!stroke.points.empty()){
        MoveToEx(dc,static_cast<int>(stroke.points.front().u*size),static_cast<int>(stroke.points.front().v*size),nullptr);
        for(const auto point:stroke.points)LineTo(dc,static_cast<int>(point.u*size),static_cast<int>(point.v*size));}
    SelectObject(dc,oldPen);DeleteObject(pen);SelectObject(dc,oldFont);DeleteObject(font);
    std::vector<uint32_t> pixels(bits,bits+size_t(size)*size);for(auto& p:pixels)p|=0xff000000;
    SelectObject(dc,old);DeleteObject(surface);DeleteDC(dc);return pixels;
}
std::vector<uint32_t> studioTextPixels(std::string_view text,UINT width=512,UINT height=160,
    uint32_t background=0xff182023,COLORREF color=RGB(240,240,230)){
    BITMAPINFO bitmap{};bitmap.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bitmap.bmiHeader.biWidth=width;
    bitmap.bmiHeader.biHeight=-static_cast<LONG>(height);bitmap.bmiHeader.biPlanes=1;bitmap.bmiHeader.biBitCount=32;
    HDC dc=CreateCompatibleDC(nullptr);void* raw{};HBITMAP surface=CreateDIBSection(dc,&bitmap,DIB_RGB_COLORS,&raw,nullptr,0);
    if(!dc||!surface||!raw){if(surface)DeleteObject(surface);if(dc)DeleteDC(dc);return {};}
    const auto old=SelectObject(dc,surface);auto* bits=static_cast<uint32_t*>(raw);
    std::fill(bits,bits+size_t(width)*height,background);
    const bool multiline=text.find('\n')!=std::string_view::npos;
    const auto makeFont=[](int pixels){return CreateFontW(-pixels,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,
        DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Bahnschrift");};
    const int fontHeight=static_cast<int>(height*(multiline?.35f:.65f));
    HFONT font=makeFont(fontHeight);const auto oldFont=SelectObject(dc,font);
    const std::wstring label(text.begin(),text.end());LONG widest{};size_t start{};
    while(start<label.size()){
        const auto end=label.find(L'\n',start);const auto length=(end==std::wstring::npos?label.size():end)-start;
        SIZE size{};GetTextExtentPoint32W(dc,label.data()+start,static_cast<int>(length),&size);widest=std::max(widest,size.cx);
        if(end==std::wstring::npos)break;start=end+1;
    }
    if(widest>static_cast<LONG>(width)-12){
        SelectObject(dc,oldFont);DeleteObject(font);
        font=makeFont(std::max(8,static_cast<int>(fontHeight*static_cast<float>(width-12)/widest*.96f)));
        SelectObject(dc,font);
    }
    SetTextColor(dc,color);SetBkMode(dc,TRANSPARENT);
    RECT rect{4,multiline?static_cast<LONG>(height/8):0,static_cast<LONG>(width)-4,static_cast<LONG>(height)};
    DrawTextW(dc,label.c_str(),static_cast<int>(label.size()),&rect,DT_CENTER|DT_NOPREFIX|(multiline?0:DT_VCENTER|DT_SINGLELINE));
    std::vector<uint32_t> pixels(bits,bits+size_t(width)*height);for(auto& p:pixels)p|=0xff000000;
    SelectObject(dc,oldFont);DeleteObject(font);SelectObject(dc,old);DeleteObject(surface);DeleteDC(dc);return pixels;
}
enum class StudioIcon { back,next,print,erase,live,freeze,resume,confirm,cancel,space,backspace };
std::vector<uint32_t> studioIconPixels(StudioIcon icon,UINT width=512,UINT height=160,
    uint32_t background=0xff182023){
    // Small, font-independent symbols. Supersampling keeps their strokes clean
    // before the existing texture mip chain handles distance in the headset.
    constexpr UINT samples=4;
    const UINT rasterWidth=width*samples,rasterHeight=height*samples;
    BITMAPINFO bitmap{};bitmap.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bitmap.bmiHeader.biWidth=rasterWidth;
    bitmap.bmiHeader.biHeight=-static_cast<LONG>(rasterHeight);bitmap.bmiHeader.biPlanes=1;bitmap.bmiHeader.biBitCount=32;
    HDC dc=CreateCompatibleDC(nullptr);void* raw{};HBITMAP surface=CreateDIBSection(dc,&bitmap,DIB_RGB_COLORS,&raw,nullptr,0);
    if(!dc||!surface||!raw){if(surface)DeleteObject(surface);if(dc)DeleteDC(dc);return {};}
    const auto old=SelectObject(dc,surface);auto* bits=static_cast<uint32_t*>(raw);
    std::fill(bits,bits+size_t(rasterWidth)*rasterHeight,background);
    const float unit=static_cast<float>(rasterHeight)*.68f/100.f;
    const auto point=[&](float x,float y){return POINT{static_cast<LONG>(std::lround(rasterWidth*.5f+x*unit)),
        static_cast<LONG>(std::lround(rasterHeight*.5f+y*unit))};};
    LOGBRUSH ink{BS_SOLID,RGB(240,240,230),0};
    HPEN pen=ExtCreatePen(PS_GEOMETRIC|PS_SOLID|PS_ENDCAP_ROUND|PS_JOIN_ROUND,
        std::max(1L,std::lround(7.f*unit)),&ink,0,nullptr);
    HBRUSH fill=CreateSolidBrush(ink.lbColor);
    const auto oldPen=SelectObject(dc,pen),oldBrush=SelectObject(dc,GetStockObject(NULL_BRUSH));
    const auto line=[&](float x,float y,float xx,float yy){
        const auto a=point(x,y),b=point(xx,yy);MoveToEx(dc,a.x,a.y,nullptr);LineTo(dc,b.x,b.y);};
    const auto box=[&](float x,float y,float xx,float yy){
        const auto a=point(x,y),b=point(xx,yy);Rectangle(dc,a.x,a.y,b.x,b.y);};
    switch(icon){
    case StudioIcon::back:case StudioIcon::next:{
        const float direction=icon==StudioIcon::back?1.f:-1.f;
        const POINT points[]{point(16.f*direction,-34),point(-16.f*direction,0),point(16.f*direction,34)};
        Polyline(dc,points,3);break;}
    case StudioIcon::print:
        box(-25,-43,25,-17);box(-40,-17,40,24);
        SelectObject(dc,fill);box(-25,10,25,43);SelectObject(dc,GetStockObject(NULL_BRUSH));
        line(-15,24,15,24);line(25,-3,25,-3);break;
    case StudioIcon::erase:
        line(-32,-25,32,-25);box(-12,-40,12,-25);
        line(-26,-16,-22,41);line(-22,41,22,41);line(22,41,26,-16);
        line(-8,-9,-8,27);line(8,-9,8,27);break;
    case StudioIcon::live:{
        const auto a=point(-42,-23),b=point(42,36);
        RoundRect(dc,a.x,a.y,b.x,b.y,static_cast<int>(14*unit),static_cast<int>(14*unit));
        line(-22,-23,-16,-40);line(-16,-40,13,-40);line(13,-40,20,-23);
        const auto lensA=point(-17,-10),lensB=point(17,24);
        Ellipse(dc,lensA.x,lensA.y,lensB.x,lensB.y);break;}
    case StudioIcon::freeze:
        SelectObject(dc,fill);box(-25,-35,-11,35);box(11,-35,25,35);break;
    case StudioIcon::resume:{
        SelectObject(dc,fill);const POINT points[]{point(-23,-37),point(35,0),point(-23,37)};
        Polygon(dc,points,3);break;}
    case StudioIcon::confirm:{
        const POINT points[]{point(-35,1),point(-11,27),point(36,-29)};Polyline(dc,points,3);break;}
    case StudioIcon::cancel:
        line(-28,-28,28,28);line(-28,28,28,-28);break;
    case StudioIcon::space:
        line(-35,-12,-35,16);line(-35,16,35,16);line(35,16,35,-12);break;
    case StudioIcon::backspace:{
        const POINT points[]{point(-42,0),point(-18,-29),point(40,-29),point(40,29),point(-18,29),point(-42,0)};
        Polyline(dc,points,6);line(-2,-12,22,12);line(-2,12,22,-12);break;}
    }
    GdiFlush();
    std::vector<uint32_t> pixels(size_t(width)*height);
    for(UINT y=0;y<height;++y)for(UINT x=0;x<width;++x){
        UINT blue{},green{},red{};
        for(UINT yy=0;yy<samples;++yy)for(UINT xx=0;xx<samples;++xx){
            const auto p=bits[size_t(y*samples+yy)*rasterWidth+x*samples+xx];
            blue+=p&255;green+=(p>>8)&255;red+=(p>>16)&255;}
        pixels[size_t(y)*width+x]=0xff000000|(red/(samples*samples)<<16)
            |(green/(samples*samples)<<8)|blue/(samples*samples);
    }
    SelectObject(dc,oldBrush);DeleteObject(fill);SelectObject(dc,oldPen);DeleteObject(pen);
    SelectObject(dc,old);DeleteObject(surface);DeleteDC(dc);return pixels;
}
bool createStudioResources(ID3D11Device* device){
    if(resources.studioReady)return true;
    // A recognizable instant camera: rounded ivory housing, separate film base,
    // side handle, lens rings, recessed glass, flash and red shutter button.
    // All parts share the same tracked world transform and native scene depth.
    auto& camera=resources.studioCamera;
    if(!studioBevel(device,camera[0],{.066f,.042f,.033f},.008f,0xffe9e5d9,{0,.006f,0})
        ||!studioBevel(device,camera[1],{.067f,.018f,.049f},.006f,0xff222529,{0,-.053f,-.012f})
        ||!studioBevel(device,camera[2],{.018f,.041f,.033f},.007f,0xff33373a,{.075f,-.003f,.006f})
        ||!studioBevel(device,camera[3],{.054f,.031f,.003f},.003f,0xff24292c,{0,.006f,-.035f})
        ||!studioCylinder(device,camera[4],.025f,-.065f,-.038f,0xffa8afb0,{-.019f,.003f,0})
        ||!studioCylinder(device,camera[5],.022f,-.070f,-.061f,0xff141a1d,{-.019f,.003f,0})
        ||!studioCylinder(device,camera[6],.0175f,-.071f,-.070f,0xff142d40,{-.019f,.003f,0})
        ||!studioBevel(device,camera[7],{.014f,.009f,.002f},.002f,0xfff1e8c5,{.032f,.027f,-.040f})
        ||!studioCylinder(device,camera[8],.007f,-.043f,-.039f,0xffb53528,{.073f,.028f,0})
        ||!studioBox(device,camera[9],{.043f,.003f,.001f},0xff06090b,{0,-.050f,-.062f})
        ||!studioBevel(device,camera[10],{.031f,.027f,.002f},.002f,0xff252c30,{-.012f,.007f,.034f})
        ||!studioCylinder(device,resources.studioMarker,.006f,-.070f,.040f,0xff202327)
        ||!studioCylinder(device,resources.studioMarkerTip,.002f,-.080f,-.070f,0xff101214)
        ||!studioBox(device,resources.studioNotes,{.038f,.003f,.038f},0xfff8e885)
        ||!studioBox(device,resources.studioStickyBack,mgs5vr::studioPaperHalf(false),0xfff8e885)
        ||!studioBox(device,resources.studioPhotoBack,mgs5vr::studioPaperHalf(true),0xfff4f0e7)
        ||!studioBox(device,resources.studioFocus,{.023f,.023f,.0004f},0xffecc03d))return false;
    // The finder is inset toward the left, away from the gripping hand.
    // Translate its own vertices once; its live texture keeps the shared pose.
    {
        RetailGpuAsset finder;
        std::vector<Vertex> vertices;const std::vector<uint16_t> indices{0,1,2,2,1,3};
        for(const auto p:std::array<std::array<float,4>,4>{{{-.027f,-.023f,0,1},{.027f,-.023f,1,1},
            {-.027f,.023f,0,0},{.027f,.023f,1,0}}}){
            Vertex v{};v.position[0]=p[0]-.012f;v.position[1]=p[1]+.007f;v.position[2]=.0365f;
            v.normal[2]=1;v.uv[0]=p[2];v.uv[1]=p[3];vertices.push_back(v);}
        if(!createCabinSurface(device,finder,vertices,indices,{0xff182023},1,1))return false;
        resources.studioViewfinder=std::move(finder);
        resources.studioGallery=resources.studioViewfinder;
    }
    for(size_t i=0;i<44;++i){
        mgs5vr::StudioPaper paper{};auto ink=std::make_shared<mgs5vr::StudioInk>();ink->text=mgs5vr::studioKey(i);paper.ink=ink;
        const auto pixels=i==33?studioIconPixels(StudioIcon::space,256,256):
            i==34?studioIconPixels(StudioIcon::backspace,256,256):
            (i==35||i==43)?studioIconPixels(StudioIcon::erase,256,256):
            i==36?studioIconPixels(StudioIcon::confirm,256,256):studioPaperPixels(paper,256);
        if(!studioQuad(device,resources.studioKeys[i],{.0215f,.0215f,0},pixels,256,256))return false;
    }
    constexpr std::array<StudioIcon,6> buttonIcons{StudioIcon::back,StudioIcon::next,StudioIcon::print,
        StudioIcon::erase,StudioIcon::live,StudioIcon::freeze};
    for(size_t i=0;i<6;++i){
        const auto control=static_cast<mgs5vr::StudioControl>(i);
        const auto position=mgs5vr::studioControlPose({},control).position;
        if(!studioBevel(device,resources.studioButtons[i],mgs5vr::studioControlHalf,.0015f,0xff303b43,
            position+mgs5vr::Vec3{0,0,-.003f})
            ||!studioQuad(device,resources.studioButtonFaces[i],mgs5vr::studioControlHalf,
                studioIconPixels(buttonIcons[i]),512,160))return false;
    }
    if(!studioQuad(device,resources.studioResume,mgs5vr::studioControlHalf,studioIconPixels(StudioIcon::resume),512,160)
        ||!studioQuad(device,resources.studioConfirmControl,mgs5vr::studioControlHalf,studioIconPixels(StudioIcon::confirm,512,160,0xff7c3028),512,160)
        ||!studioQuad(device,resources.studioCancelControl,mgs5vr::studioControlHalf,studioIconPixels(StudioIcon::cancel),512,160)
        ||!studioBox(device,resources.studioButtonHover,{.027f,.010f,.0004f},0xffe4c253,{0,0,-.0005f})
        ||!studioQuad(device,resources.studioCancel,{.0215f,.0215f,0},studioIconPixels(StudioIcon::cancel,256,256),256,256)
        ||!studioQuad(device,resources.studioDelete,{.0215f,.0215f,0},studioIconPixels(StudioIcon::confirm,256,256,0xff7c3028),256,256)
        ||!studioQuad(device,resources.studioPaperConfirmation,{.245f,.047f,0},
            studioTextPixels("DELETE THIS PAPER?\nTHIS CANNOT BE UNDONE",1024,240,0xff602922),1024,240))return false;
    resources.studioReady=true;return true;
}
bool createCabinLabels(ID3D11Device* device){
    constexpr UINT width=512,height=160;
    constexpr std::array<uint32_t,6> stripes{0xff9c3929,0xff9b8141,0xff3d596c,0xff496350,0xff923e39,0xff55524b};
    for(size_t tape=0;tape<7;++tape){
        BITMAPINFO bitmap{};bitmap.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bitmap.bmiHeader.biWidth=width;
        bitmap.bmiHeader.biHeight=-static_cast<LONG>(height);bitmap.bmiHeader.biPlanes=1;bitmap.bmiHeader.biBitCount=32;
        void* bits{};HDC dc=CreateCompatibleDC(nullptr);HBITMAP surface=CreateDIBSection(dc,&bitmap,DIB_RGB_COLORS,&bits,nullptr,0);
        if(!dc||!surface||!bits){if(surface)DeleteObject(surface);if(dc)DeleteDC(dc);return false;}
        const auto old=SelectObject(dc,surface);auto* ink=static_cast<uint32_t*>(bits);
        std::fill(ink,ink+width*height,0xffede5cd);
        for(UINT y=0;y<12;++y)for(UINT x=0;x<width;++x)ink[size_t(y)*width+x]=stripes[tape%6];
        std::wstring caption=tape==6?L"EJECT":std::wstring(mgs5vr::openingTapeLabels[tape].begin(),mgs5vr::openingTapeLabels[tape].end());
        if(tape==1)caption=L"DOWNLOAD MGSV: GZ\nSAVE DATA";
        if(tape==2)caption=L"METAL GEAR\nONLINE";
        if(tape==4)caption=L"DELETE SAVE DATA";
        const int fontHeight=tape==1?40:tape==4?43:54;
        HFONT font=CreateFontW(-fontHeight,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Bahnschrift");
        const auto oldFont=SelectObject(dc,font);SetTextColor(dc,RGB(30,29,24));SetBkMode(dc,TRANSPARENT);
        RECT text{10,caption.find(L'\n')==std::wstring::npos?48:27,502,150};
        DrawTextW(dc,caption.c_str(),static_cast<int>(caption.size()),&text,DT_CENTER|DT_NOPREFIX);
        std::vector<uint32_t> pixels(ink,ink+width*height);for(auto& p:pixels)p|=0xff000000;
        SelectObject(dc,oldFont);DeleteObject(font);SelectObject(dc,old);DeleteObject(surface);DeleteDC(dc);
        std::vector<Vertex> vertices;std::vector<uint16_t> indices;
        // Labels are actual triangles on both broad faces of the casing. The
        // rear UV winding is reversed so turning a tape never mirrors its text.
        for(int side:{1,-1}){
            const float z=side*.00565f;const uint16_t first=static_cast<uint16_t>(vertices.size());
            for(const auto& p:std::array<std::array<float,4>,4>{{{-.046f,.010f,0,1},{.046f,.010f,1,1},{-.046f,.029f,0,0},{.046f,.029f,1,0}}}){
                Vertex v{};v.position[0]=p[0];v.position[1]=p[1];v.position[2]=z;v.normal[2]=static_cast<float>(side);
                v.uv[0]=side>0?p[2]:1-p[2];v.uv[1]=p[3];vertices.push_back(v);}
            for(const auto i:{0,1,2,2,1,3})indices.push_back(first+static_cast<uint16_t>(i));
        }
        if(tape==6){
            const mgs5vr::Vec3 p{};vertices.clear();indices={0,1,2,2,1,3};
            for(const auto& corner:std::array<std::array<float,4>,4>{{{-.029f,.011f,0,1},{.029f,.011f,1,1},{-.029f,-.011f,0,0},{.029f,-.011f,1,0}}}){
                Vertex vertex{};vertex.position[0]=p.x+corner[0];vertex.position[1]=p.y+.0062f;
                vertex.position[2]=p.z+corner[1];vertex.normal[1]=1;vertex.uv[0]=corner[2];vertex.uv[1]=corner[3];vertices.push_back(vertex);}
        }
        auto& label=tape==6?resources.openingEjectLabel:resources.openingLabels[tape];
        if(!createCabinSurface(device,label,vertices,indices,pixels,width,height))return false;
    }
    return true;
}
bool createOpeningResources(ID3D11Device* device){
    if(resources.openingReady)return true;
    if(!device||resources.openingAttempted||!resources.ready)return false;
    resources.openingAttempted=true;
    const auto cassetteModel=openingAssetPath(L"cassette_fmdl",
        L"retail-assets\\Assets\\tpp\\item\\cct\\Scenes\\cct0_main1_def.fmdl");
    const auto cassetteDiffuse=openingAssetPath(L"cassette_diffuse_dds",
        L"retail-assets\\Assets\\tpp\\item\\cct\\Pictures\\cct0_main1_def_c00_bsm.dds");
    const bool cassette=createRetailGpuAsset(device,resources.openingCassette,cassetteModel,cassetteDiffuse,"cassette");
    resources.openingReady=cassette&&createCabinLabels(device);
    if(!resources.openingReady)
        mgs5vr::log("Opening title props unavailable; native title panel remains the fallback");
    return resources.openingReady;
}

bool createNativeMaterialResources(ID3D11Device* device){
    if(!mgs5vr::opticNativeBinocularMaterialEnabled())return false;
    if(resources.nativeMaterialReady)return true;
    if(!device||!resources.ready||!resources.nativeMaterialRecognized||resources.nativeMaterialAttempted)return false;
    resources.nativeMaterialAttempted=true;
    const auto shaderUnavailable=[](const char* reason){mgs5vr::log(std::string("Native binocular material unavailable; late housing retained: ")+reason);return false;};
    // The installer imports this one exact owned shader locally. Its bytes
    // are never packaged with the runtime or replaced by a custom light.
    const auto shaderPath=gameRoot()/L"retail-assets/shaders/dx11/fox3ddf_blin_4mt-ps.dxbc";
    std::ifstream shaderInput(shaderPath,std::ios::binary|std::ios::ate);
    if(!shaderInput||shaderInput.tellg()!=std::streamoff{233604})return shaderUnavailable("owned canonical PS asset missing/size mismatch");
    std::vector<uint8_t> nativeShader(233604);shaderInput.seekg(0);
    if(!shaderInput.read(reinterpret_cast<char*>(nativeShader.data()),static_cast<std::streamsize>(nativeShader.size()))
        ||ownedAssetDigest(nativeShader)!="cabc964687cd259e0189b33968b090980aab7e12a9c603e8560536ae2a9e64fe")return shaderUnavailable("owned canonical PS SHA256 mismatch");
    ComPtr<ID3D11ShaderReflection> reflection;D3D11_SHADER_DESC description{};
    if(FAILED(D3DReflect(nativeShader.data(),nativeShader.size(),IID_PPV_ARGS(&reflection)))
        ||FAILED(reflection->GetDesc(&description))||D3D11_SHVER_GET_TYPE(description.Version)!=D3D11_SHVER_PIXEL_SHADER
        ||D3D11_SHVER_GET_MAJOR(description.Version)!=5||description.InputParameters!=6||description.OutputParameters!=3)return shaderUnavailable("owned canonical PS reflection/profile mismatch");
    for(const auto& expected:std::array<std::tuple<const char*,UINT,UINT>,2>{{{"cPSSystem",0,64},{"cPSMaterial",4,128}}}){
        D3D11_SHADER_INPUT_BIND_DESC binding{};D3D11_SHADER_BUFFER_DESC block{};
        if(FAILED(reflection->GetResourceBindingDescByName(std::get<0>(expected),&binding))
            ||binding.Type!=D3D_SIT_CBUFFER||binding.BindPoint!=std::get<1>(expected)||binding.BindCount!=1
            ||FAILED(reflection->GetConstantBufferByName(std::get<0>(expected))->GetDesc(&block))
            ||block.Size!=std::get<2>(expected))return shaderUnavailable("owned canonical PS typed material/system block mismatch");
    }
    for(UINT i=0;i<3;++i){D3D11_SIGNATURE_PARAMETER_DESC output{};
        if(FAILED(reflection->GetOutputParameterDesc(i,&output))||output.SystemValueType!=D3D_NAME_TARGET
            ||output.SemanticIndex!=i||output.Register!=i||output.Mask!=15||output.ComponentType!=D3D_REGISTER_COMPONENT_FLOAT32)return shaderUnavailable("owned canonical PS output signature mismatch");}
    if(FAILED(device->CreatePixelShader(nativeShader.data(),nativeShader.size(),nullptr,resources.nativeMaterialPixelShader.GetAddressOf())))return shaderUnavailable("owned canonical PS device creation failed");
    const std::array<const wchar_t*,3> keys{L"binocular_normal_dds",L"binocular_specular_dds",L"binocular_material_dds"};
    const std::array<const wchar_t*,3> names{L"tel0_main0_def_nrm.dds",L"tel0_main0_def_srm.dds",L"tel0_main0_def_mtm.dds"};
    const std::array<const char*,3> identities{
        "285c9a61b02b5798e79a5013b49ce3344b1f196ae2a56814ab7db6bfb9028eb4",
        "20f1d6b3c6e13c149a79fc3526a524f442fc3457bceaa5ce775579bd0d43b61d",
        "88c9679a783ece860f6182d4444b0ad07b59771be2d1b0eedab166e4f18b3e13"};
    std::array<ComPtr<ID3D11ShaderResourceView>,3> maps;
    for(size_t i=0;i<maps.size();++i){
        std::string failure;const auto texture=readRetailDds(retailBinocularMaterialPath(keys[i],names[i]),failure);
        if(!texture||texture->sha256!=identities[i]||!createRetailDiffuseView(device,*texture,maps[i],false)){
            mgs5vr::log("Native binocular material unavailable; late housing retained: "+
                (failure.empty()?std::string("owned map identity/resource mismatch"):failure));return false;}
    }
    ComPtr<ID3DBlob> vertex;
    if(!compile(device,nativeMaterialVertexSource,"vs_5_0",vertex.GetAddressOf()))return false;
    constexpr std::array<D3D11_INPUT_ELEMENT_DESC,4> elements{{
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TANGENT",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,32,D3D11_INPUT_PER_VERTEX_DATA,0}}};
    D3D11_BUFFER_DESC view{};view.ByteWidth=sizeof(NativeMaterialConstants);view.Usage=D3D11_USAGE_DEFAULT;
    view.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    D3D11_BUFFER_DESC parameters=view;parameters.ByteWidth=128;
    std::array<float,32> authored{};std::copy(resources.nativeMaterialIndices.begin(),resources.nativeMaterialIndices.end(),authored.begin());
    D3D11_SUBRESOURCE_DATA initial{};initial.pSysMem=authored.data();
    if(FAILED(device->CreateVertexShader(vertex->GetBufferPointer(),vertex->GetBufferSize(),nullptr,resources.nativeMaterialVertexShader.GetAddressOf()))
        ||FAILED(device->CreateInputLayout(elements.data(),static_cast<UINT>(elements.size()),vertex->GetBufferPointer(),vertex->GetBufferSize(),resources.nativeMaterialInputLayout.GetAddressOf()))
        ||FAILED(device->CreateBuffer(&view,nullptr,resources.nativeMaterialView.GetAddressOf()))
        ||FAILED(device->CreateBuffer(&parameters,&initial,resources.nativeMaterialParameters.GetAddressOf())))return false;
    resources.nativeNormal=maps[0];resources.nativeSpecular=maps[1];resources.nativeMaterialMap=maps[2];
    resources.nativeMaterialReady=true;return true;
}

// This state is independent of the late single-target overlay state. The
// three native MRTs and writable DSV remain bound throughout; retaining all
// views locally proves their lifetime and avoids clearing unrelated OM UAVs.
struct NativeMaterialState {
    ID3D11DeviceContext* context{};
    ComPtr<ID3D11DeviceContext1> context1;
    std::array<ComPtr<ID3D11RenderTargetView>,8> targets;
    ComPtr<ID3D11DepthStencilView> depth;
    ComPtr<ID3D11InputLayout> layout;
    ComPtr<ID3D11Buffer> vertex,index,viewParameters,materialParameters,systemParameters;
    ComPtr<ID3D11VertexShader> vertexShader;
    ComPtr<ID3D11PixelShader> pixelShader;
    ComPtr<ID3D11GeometryShader> geometryShader;
    ComPtr<ID3D11HullShader> hullShader;
    ComPtr<ID3D11DomainShader> domainShader;
    std::array<ComPtr<ID3D11ClassInstance>,256> vertexClasses;
    std::array<ComPtr<ID3D11ClassInstance>,256> pixelClasses;
    std::array<ComPtr<ID3D11ShaderResourceView>,4> maps;
    ComPtr<ID3D11ShaderResourceView> meshMask;
    std::array<ComPtr<ID3D11SamplerState>,4> samplers;
    ComPtr<ID3D11RasterizerState> rasterizer;
    ComPtr<ID3D11DepthStencilState> depthState;
    ComPtr<ID3D11BlendState> blend;
    std::array<D3D11_VIEWPORT,D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> viewports{};
    UINT stride{},offset{},indexOffset{},vertexClassCount{},pixelClassCount{},viewportCount{},stencil{},sampleMask{};
    UINT viewFirst{},viewCount{},materialFirst{},materialCount{};
    DXGI_FORMAT indexFormat{};D3D11_PRIMITIVE_TOPOLOGY topology{};
    FLOAT factors[4]{};bool changed{};
    explicit NativeMaterialState(ID3D11DeviceContext* value):context(value){
        std::array<ID3D11RenderTargetView*,8> rawTargets{};ID3D11DepthStencilView* rawDepth{};
        context->OMGetRenderTargets(8,rawTargets.data(),&rawDepth);depth.Attach(rawDepth);
        for(size_t i=0;i<targets.size();++i)targets[i].Attach(rawTargets[i]);
        context->IAGetInputLayout(layout.GetAddressOf());context->IAGetVertexBuffers(0,1,vertex.GetAddressOf(),&stride,&offset);
        context->IAGetIndexBuffer(index.GetAddressOf(),&indexFormat,&indexOffset);context->IAGetPrimitiveTopology(&topology);
        std::array<ID3D11ClassInstance*,256> rawClasses{};vertexClassCount=static_cast<UINT>(rawClasses.size());
        context->VSGetShader(vertexShader.GetAddressOf(),rawClasses.data(),&vertexClassCount);
        vertexClassCount=std::min(vertexClassCount,static_cast<UINT>(rawClasses.size()));
        for(size_t i=0;i<rawClasses.size();++i)vertexClasses[i].Attach(rawClasses[i]);
        rawClasses={};pixelClassCount=static_cast<UINT>(rawClasses.size());
        context->PSGetShader(pixelShader.GetAddressOf(),rawClasses.data(),&pixelClassCount);
        pixelClassCount=std::min(pixelClassCount,static_cast<UINT>(rawClasses.size()));
        for(size_t i=0;i<rawClasses.size();++i)pixelClasses[i].Attach(rawClasses[i]);
        context->GSGetShader(geometryShader.GetAddressOf(),nullptr,nullptr);context->HSGetShader(hullShader.GetAddressOf(),nullptr,nullptr);
        context->DSGetShader(domainShader.GetAddressOf(),nullptr,nullptr);
        if(SUCCEEDED(context->QueryInterface(IID_PPV_ARGS(&context1)))){
            context1->VSGetConstantBuffers1(0,1,viewParameters.GetAddressOf(),&viewFirst,&viewCount);
            context1->PSGetConstantBuffers1(4,1,materialParameters.GetAddressOf(),&materialFirst,&materialCount);
        }else{
            context->VSGetConstantBuffers(0,1,viewParameters.GetAddressOf());context->PSGetConstantBuffers(4,1,materialParameters.GetAddressOf());
        }
        context->PSGetConstantBuffers(0,1,systemParameters.GetAddressOf());
        std::array<ID3D11ShaderResourceView*,4> rawMaps{};context->PSGetShaderResources(0,4,rawMaps.data());
        for(size_t i=0;i<maps.size();++i)maps[i].Attach(rawMaps[i]);context->PSGetShaderResources(15,1,meshMask.GetAddressOf());
        for(size_t i=0;i<samplers.size();++i)context->PSGetSamplers(i==3?8:static_cast<UINT>(i),1,samplers[i].GetAddressOf());
        context->RSGetState(rasterizer.GetAddressOf());context->OMGetDepthStencilState(depthState.GetAddressOf(),&stencil);
        context->OMGetBlendState(blend.GetAddressOf(),factors,&sampleMask);
        viewportCount=static_cast<UINT>(viewports.size());context->RSGetViewports(&viewportCount,viewports.data());
    }
    ~NativeMaterialState(){if(!changed)return;
        context->IASetInputLayout(layout.Get());ID3D11Buffer* originalVertex=vertex.Get();
        context->IASetVertexBuffers(0,1,&originalVertex,&stride,&offset);context->IASetIndexBuffer(index.Get(),indexFormat,indexOffset);
        context->IASetPrimitiveTopology(topology);
        std::array<ID3D11ClassInstance*,256> classes{};for(size_t i=0;i<vertexClassCount;++i)classes[i]=vertexClasses[i].Get();
        context->VSSetShader(vertexShader.Get(),classes.data(),vertexClassCount);
        classes={};for(size_t i=0;i<pixelClassCount;++i)classes[i]=pixelClasses[i].Get();
        context->PSSetShader(pixelShader.Get(),classes.data(),pixelClassCount);
        ID3D11Buffer* originalView=viewParameters.Get();ID3D11Buffer* originalMaterial=materialParameters.Get();
        if(context1){context1->VSSetConstantBuffers1(0,1,&originalView,&viewFirst,&viewCount);
            context1->PSSetConstantBuffers1(4,1,&originalMaterial,&materialFirst,&materialCount);
        }else{context->VSSetConstantBuffers(0,1,&originalView);context->PSSetConstantBuffers(4,1,&originalMaterial);}
        std::array<ID3D11ShaderResourceView*,4> originalMaps{};for(size_t i=0;i<maps.size();++i)originalMaps[i]=maps[i].Get();
        context->PSSetShaderResources(0,4,originalMaps.data());context->RSSetState(rasterizer.Get());
        context->OMSetDepthStencilState(depthState.Get(),stencil);context->OMSetBlendState(blend.Get(),factors,sampleMask);
    }
    bool exactGeometryBinding(ID3D11PixelShader* verified) const{
        if(pixelShader.Get()!=verified||geometryShader||hullShader||domainShader||!depth||!systemParameters||!meshMask
            ||viewportCount!=1||!targets[0]||!targets[1]||!targets[2])return false;
        for(size_t i=3;i<targets.size();++i)if(targets[i])return false;
        for(const auto& sampler:samplers)if(!sampler)return false;
        D3D11_SHADER_RESOURCE_VIEW_DESC meshView{};meshMask->GetDesc(&meshView);
        if(meshView.ViewDimension!=D3D11_SRV_DIMENSION_TEXTURE2D)return false;
        D3D11_DEPTH_STENCIL_DESC nativeDepth{};
        if(depthState)depthState->GetDesc(&nativeDepth);
        else {nativeDepth.DepthEnable=TRUE;nativeDepth.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;
            nativeDepth.DepthFunc=D3D11_COMPARISON_LESS;}
        if(!mgs5vr::opticNativeMaterialDepthEligible(nativeDepth.DepthEnable!=FALSE,
            nativeDepth.DepthWriteMask==D3D11_DEPTH_WRITE_MASK_ALL,static_cast<uint32_t>(nativeDepth.DepthFunc)))return false;
        D3D11_BLEND_DESC nativeBlend{};
        if(blend)blend->GetDesc(&nativeBlend);
        else {nativeBlend.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;}
        for(size_t i=0;i<3;++i){const auto& target=nativeBlend.RenderTarget[nativeBlend.IndependentBlendEnable?i:0];
            if(target.BlendEnable||target.RenderTargetWriteMask!=D3D11_COLOR_WRITE_ENABLE_ALL)return false;}
        D3D11_DEPTH_STENCIL_VIEW_DESC depthView{};depth->GetDesc(&depthView);
        if((depthView.Flags&D3D11_DSV_READ_ONLY_DEPTH)||depthView.ViewDimension!=D3D11_DSV_DIMENSION_TEXTURE2D
            ||depthView.Texture2D.MipSlice!=0)return false;
        ComPtr<ID3D11Resource> depthResource;depth->GetResource(depthResource.GetAddressOf());
        ComPtr<ID3D11Texture2D> depthTexture;if(FAILED(depthResource.As(&depthTexture)))return false;
        D3D11_TEXTURE2D_DESC depthDescription{};depthTexture->GetDesc(&depthDescription);
        const auto& viewport=viewports[0];
        if(!std::isfinite(viewport.Width)||!std::isfinite(viewport.Height)||viewport.Width<=0||viewport.Height<=0
            ||viewport.TopLeftX!=0||viewport.TopLeftY!=0||viewport.Width!=static_cast<float>(depthDescription.Width)
            ||viewport.Height!=static_cast<float>(depthDescription.Height)||depthDescription.ArraySize!=1||depthDescription.SampleDesc.Count!=1)return false;
        for(const auto& target:targets){if(!target)continue;
            D3D11_RENDER_TARGET_VIEW_DESC targetView{};target->GetDesc(&targetView);
            if(targetView.ViewDimension!=D3D11_RTV_DIMENSION_TEXTURE2D||targetView.Texture2D.MipSlice!=0)return false;
            ComPtr<ID3D11Resource> object;target->GetResource(object.GetAddressOf());ComPtr<ID3D11Texture2D> texture;
            if(FAILED(object.As(&texture)))return false;D3D11_TEXTURE2D_DESC description{};texture->GetDesc(&description);
            if(description.Width!=depthDescription.Width||description.Height!=depthDescription.Height
                ||description.ArraySize!=1||description.SampleDesc.Count!=1)return false;
        }
        D3D11_BUFFER_DESC system{};systemParameters->GetDesc(&system);return system.ByteWidth>=64&&system.ByteWidth<=65536;
    }
};
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

std::array<float,16> matrixProduct(const std::array<float,16>& a,const std::array<float,16>& b);

bool drawOpeningAsset(ID3D11DeviceContext* context,const RetailGpuAsset& asset,
    ID3D11RenderTargetView* target,ID3D11DepthStencilView* depth,
    const std::array<float,16>& world,const std::array<float,16>& view,
    const std::array<float,16>& projection,bool emissive=false){
    if(!context||!asset.ready||!target||!depth)return false;
    const auto mvp=matrixProduct(matrixProduct(world,view),projection);
    for(const auto value:mvp)if(!std::isfinite(value))return false;
    Constants constants{};std::copy(mvp.begin(),mvp.end(),std::begin(constants.mvp));
    std::copy(world.begin(),world.end(),std::begin(constants.world));
    constants.baseColor[0]=constants.baseColor[1]=constants.baseColor[2]=constants.baseColor[3]=1.f;
    if(emissive)constants.baseColor[0]=-1.f;
    context->UpdateSubresource(resources.constants.Get(),0,nullptr,&constants,0,0);
    ID3D11RenderTargetView* renderTarget=target;context->OMSetRenderTargets(1,&renderTarget,depth);
    const UINT stride=sizeof(Vertex),offset=0;ID3D11Buffer* vertexBuffer=asset.vertices.Get();
    context->IASetInputLayout(resources.inputLayout.Get());context->IASetVertexBuffers(0,1,&vertexBuffer,&stride,&offset);
    context->IASetIndexBuffer(asset.indices.Get(),DXGI_FORMAT_R16_UINT,0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(resources.vertexShader.Get(),nullptr,0);
    ID3D11Buffer* constantBuffer=resources.constants.Get();context->VSSetConstantBuffers(0,1,&constantBuffer);
    context->PSSetConstantBuffers(0,1,&constantBuffer);context->PSSetShader(resources.pixelShader.Get(),nullptr,0);
    ID3D11ShaderResourceView* diffuse=asset.diffuse.Get();context->PSSetShaderResources(0,1,&diffuse);
    ID3D11SamplerState* sampler=resources.sampler.Get();context->PSSetSamplers(0,1,&sampler);
    context->RSSetState(resources.rasterizer.Get());context->OMSetDepthStencilState(
        projection[14]>0?resources.reversedDepthStencil.Get():resources.depthStencil.Get(),0);
    const FLOAT blendFactor[4]{0,0,0,0};context->OMSetBlendState(resources.blend.Get(),blendFactor,0xffffffffu);
    context->DrawIndexed(asset.indexCount,0,0);return true;
}

bool bindHousingDepth(ID3D11DeviceContext* context,ID3D11Device* device,
    const SavedState& state,bool reversed,ComPtr<ID3D11DepthStencilView>& target,
    const mgs5vr::EyeFrame* eye=nullptr){
    const char* source="native writable depth";
    if(eye&&mgs5vr::nativeEyeDepthView(*eye,device,target.GetAddressOf())){
        ComPtr<ID3D11Resource> depthResource,colorResource;target->GetResource(&depthResource);
        state.renderTarget->GetResource(&colorResource);
        ComPtr<ID3D11Texture2D> depthTexture,colorTexture;
        if(FAILED(depthResource.As(&depthTexture))||FAILED(colorResource.As(&colorTexture)))return false;
        D3D11_TEXTURE2D_DESC a{},b{};depthTexture->GetDesc(&a);colorTexture->GetDesc(&b);
        if(a.Width!=b.Width||a.Height!=b.Height||a.SampleDesc.Count!=b.SampleDesc.Count)return false;
        source="matched native eye depth";
    }else if(state.depthTarget){
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
    depth.StencilEnable=TRUE;depth.StencilReadMask=depth.StencilWriteMask=255;
    depth.FrontFace={D3D11_STENCIL_OP_KEEP,D3D11_STENCIL_OP_KEEP,D3D11_STENCIL_OP_REPLACE,D3D11_COMPARISON_ALWAYS};
    depth.BackFace=depth.FrontFace;
    if(FAILED(device->CreateDepthStencilState(&depth,&resources.markerMaskWrite)))return false;
    depth.StencilWriteMask=0;depth.FrontFace.StencilPassOp=D3D11_STENCIL_OP_KEEP;
    depth.FrontFace.StencilFunc=D3D11_COMPARISON_EQUAL;depth.BackFace=depth.FrontFace;
    if(FAILED(device->CreateDepthStencilState(&depth,&resources.markerMaskRead)))return false;
    D3D11_BUFFER_DESC buffer{};buffer.ByteWidth=16384*sizeof(MarkerVertex);buffer.Usage=D3D11_USAGE_DYNAMIC;
    buffer.BindFlags=D3D11_BIND_VERTEX_BUFFER;buffer.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
    return SUCCEEDED(device->CreateBuffer(&buffer,nullptr,&resources.markerVertices));
}

// Small atlas-free glyphs remain sharp in the independently rendered lens.
// These are native waypoint letters and measured distances, plus the exact
// semantic labels attached to the physical opening tapes.
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
    if(c==':')return {{0,0,4,0,0,4,0}};
    if(c>='0'&&c<='9')return glyphs[c-'0'];
    if(c>='A'&&c<='Z')return glyphs[10+c-'A'];
    return {};
}

void drawWaypoints(ID3D11DeviceContext* context,ID3D11Device* device,
    const std::array<float,16>& view,const std::array<float,16>& projection,mgs5vr::Vec3 camera,
    const mgs5vr::OpticWaypoints& points,ID3D11RenderTargetView* target,const D3D11_VIEWPORT& viewport,bool lens,
    ID3D11DepthStencilView* housingMask=nullptr){
    const auto status=mgs5vr::headCamera().status();
    const auto now=mgs5vr::steadyMilliseconds();
    if(!target||!status.active||points.activation!=status.activation
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
    if(lens)for(const auto color:{black,white}){
        const float width=color==black?.006f:.0025f;
        line(-.05f,0,-.022f,0,width,color);line(.022f,0,.05f,0,width,color);
        line(0,-.05f,0,-.022f,width,color);line(0,.022f,0,.05f,width,color);
    }
    const auto vp=matrixProduct(view,projection);unsigned visible{};
    for(size_t i=0;i<std::min(points.count,points.points.size());++i){
        const auto& point=points.points[i];const auto clip=transformPoint(vp,point.position);
        if(!std::isfinite(clip.w)||clip.w<=.01f)continue;
        const float x=clip.x/clip.w,y=clip.y/clip.w;
        if(!std::isfinite(x)||!std::isfinite(y)||(lens?x*x+y*y>.90f*.90f:std::abs(x)>.95f||std::abs(y)>.9f))continue;
        const auto delta=point.position-camera;const float distance=std::sqrt(mgs5vr::dot(delta,delta));
        if(!std::isfinite(distance)||distance>99999.f)continue;
        ++visible;
        const bool compact=!lens&&!point.letter;
        const float radius=compact?.009f:.032f;
        const auto markerColor=point.letter?amber:red;
        for(const auto color:{black,markerColor}){
            const float width=color==black?(compact?.0018f:.005f):(compact?.0008f:.002f);
            line(x,y+radius,x+radius,y,width,color);line(x+radius,y,x,y-radius,width,color);
            line(x,y-radius,x-radius,y,width,color);line(x-radius,y,x,y+radius,width,color);
        }
        char label[24]{};
        if(point.letter)std::snprintf(label,sizeof(label),"%c %uM",'A'+point.letter-1,static_cast<unsigned>(std::lround(distance)));
        else std::snprintf(label,sizeof(label),"%uM",static_cast<unsigned>(std::lround(distance)));
        const float pixel=lens?.0075f:compact?.0025f:.0035f;const float length=static_cast<float>(std::strlen(label));
        // Keep nearby waypoint and person labels on opposite sides of the
        // marker, leaving the person and the exact aiming point visible.
        const float left=x-(length*6-1)*pixel*.5f,top=y+(point.letter?-.052f:compact?.036f:.112f);
        const float padding=compact?.003f:.008f;
        quad(left-padding,top+padding,left+(length*6-1)*pixel+padding,top-7*pixel-padding,black);
        for(size_t n=0;label[n];++n){const auto glyph=markerGlyph(label[n]);
            for(size_t row=0;row<7;++row)for(size_t col=0;col<5;++col)if(glyph[row]&(1u<<(4-col))){
                const float gx=left+static_cast<float>(n*6+col)*pixel,gy=top-static_cast<float>(row)*pixel;
                quad(gx,gy,gx+pixel,gy-pixel,markerColor);
            }
        }
    }
    if(vertices.empty())return;
    ID3D11ShaderResourceView* none=nullptr;context->PSSetShaderResources(0,1,&none);
    context->OMSetRenderTargets(1,&target,housingMask);
    context->RSSetViewports(1,&viewport);context->RSSetState(resources.markerRasterizer.Get());
    context->OMSetDepthStencilState(housingMask?resources.markerMaskRead.Get():resources.markerDepth.Get(),0);
    context->OMSetBlendState(nullptr,nullptr,0xffffffffu);
    ID3D11Buffer* buffer=resources.markerVertices.Get();const UINT stride=sizeof(MarkerVertex),offset=0;
    context->IASetVertexBuffers(0,1,&buffer,&stride,&offset);context->IASetInputLayout(resources.markerInputLayout.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(resources.markerVertexShader.Get(),nullptr,0);context->PSSetShader(resources.markerPixelShader.Get(),nullptr,0);
    // A busy outpost must not lose every label when it exceeds one upload.
    // Split only at triangle boundaries and keep each upload bounded.
    for(size_t first=0;first<vertices.size();first+=16383){
        const auto count=std::min(size_t{16383},vertices.size()-first);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if(FAILED(context->Map(resources.markerVertices.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))return;
        std::memcpy(mapped.pData,vertices.data()+first,count*sizeof(MarkerVertex));context->Unmap(resources.markerVertices.Get(),0);
        context->Draw(static_cast<UINT>(count),0);
    }
    if(visible&&!resources.markerReported){resources.markerReported=true;mgs5vr::log("Native acquired markers reprojected from world positions");}
}

ID3D11DepthStencilView* binocularMarkerMask(ID3D11DeviceContext* context,ID3D11Device* device,
    const SavedState& state,const std::array<float,16>& world,
    const std::array<float,16>& view,const std::array<float,16>& projection){
    if(!resources.ready&&!createResources(device))return nullptr;
    if(!createMarkerResources(device)||!state.renderTarget)return nullptr;
    ComPtr<ID3D11Resource> resource;state.renderTarget->GetResource(&resource);
    ComPtr<ID3D11Texture2D> color;if(FAILED(resource.As(&color)))return nullptr;
    D3D11_TEXTURE2D_DESC desc{},old{};color->GetDesc(&desc);
    if(resources.markerMaskTexture)resources.markerMaskTexture->GetDesc(&old);
    if(!resources.markerMask||desc.Width!=old.Width||desc.Height!=old.Height
       ||desc.SampleDesc.Count!=old.SampleDesc.Count||desc.SampleDesc.Quality!=old.SampleDesc.Quality){
        resources.markerMask.Reset();resources.markerMaskTexture.Reset();
        desc.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;desc.MipLevels=desc.ArraySize=1;
        desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_DEPTH_STENCIL;
        desc.CPUAccessFlags=desc.MiscFlags=0;
        if(FAILED(device->CreateTexture2D(&desc,nullptr,&resources.markerMaskTexture))
           ||FAILED(device->CreateDepthStencilView(resources.markerMaskTexture.Get(),nullptr,&resources.markerMask)))return nullptr;
    }
    context->ClearDepthStencilView(resources.markerMask.Get(),D3D11_CLEAR_STENCIL,1,0);
    context->OMSetRenderTargets(0,nullptr,resources.markerMask.Get());
    context->OMSetDepthStencilState(resources.markerMaskWrite.Get(),1);
    const auto mvp=matrixProduct(matrixProduct(world,view),projection);Constants constants{};
    std::copy(mvp.begin(),mvp.end(),constants.mvp);std::copy(world.begin(),world.end(),constants.world);
    context->UpdateSubresource(resources.constants.Get(),0,nullptr,&constants,0,0);
    const UINT stride=sizeof(Vertex),offset=0;ID3D11Buffer* vertices=resources.vertices.Get();
    context->IASetInputLayout(resources.inputLayout.Get());context->IASetVertexBuffers(0,1,&vertices,&stride,&offset);
    context->IASetIndexBuffer(resources.indices.Get(),DXGI_FORMAT_R16_UINT,0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(resources.vertexShader.Get(),nullptr,0);context->PSSetShader(nullptr,nullptr,0);
    ID3D11Buffer* buffer=resources.constants.Get();context->VSSetConstantBuffers(0,1,&buffer);
    context->RSSetState(resources.rasterizer.Get());if(state.viewportCount)context->RSSetViewports(1,&state.viewport);
    context->DrawIndexed(resources.indexCount,0,0);
    return resources.markerMask.Get();
}

void drawOpeningLabels(ID3D11DeviceContext* context,ID3D11RenderTargetView* target,
    ID3D11DepthStencilView* depth,const std::array<std::array<float,16>,7>& worlds,
    const std::array<float,16>& view,const std::array<float,16>& projection){
    for(size_t i=1;i<worlds.size();++i)
        drawOpeningAsset(context,resources.openingLabels[i-1],target,depth,worlds[i],view,projection);
}

bool drawLensPortal(ID3D11DeviceContext* context,const std::array<float,16>& world,
    const std::array<float,16>& view,const std::array<float,16>& projection,float magnification,
    const SavedState& state,ID3D11DepthStencilView* depthTarget,ID3D11ShaderResourceView* sceneSource,float reticle=0.f){
    if(magnification<1.f)return false;
    const bool trace=(++resources.lensTraceCalls%120u)==0u;
    const char* failure=nullptr;
    if((!sceneSource&&reticle<2.5f)||!state.renderTarget||!state.viewport.Width||!state.viewport.Height){
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
    constants.sampleCenter[3]=reticle;
    const auto worldView=matrixProduct(world,view);const auto ocular=transformPoint(worldView,ocularCenter);
    const float distance=std::sqrt(ocular.x*ocular.x+ocular.y*ocular.y+ocular.z*ocular.z);
    const float facing=-(ocular.x*worldView[8]+ocular.y*worldView[9]+ocular.z*worldView[10])/std::max(distance,.001f);
    // Never show the portal through its back face or from inside the housing.
    if(facing<=.15f)return false;
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
    context->RSSetState(resources.lensRasterizer.Get());context->OMSetDepthStencilState(
        projection[14]>0?resources.reversedLensDepthStencil.Get():resources.lensDepthStencil.Get(),0);
    const FLOAT blendFactor[4]{0,0,0,0};context->OMSetBlendState(resources.blend.Get(),blendFactor,0xffffffffu);
    context->Draw(4,0);
    if(trace){std::ostringstream message;message<<"Retail optic lens trace rendered center="<<centerX<<","<<centerY<<" magnification="<<magnification;mgs5vr::log(message.str());}
    return true;
}
}

namespace mgs5vr {
bool capturePhysicalOpticScene(ID3D11DeviceContext* context,const std::array<float,16>& view,
    const std::array<float,16>& projection,Vec3 cameraPosition,ID3D11Texture2D** output,bool waypoints,
    const OpticWaypoints* markers) noexcept{
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
        if(copied&&waypoints){
            const auto points=markers?*markers:opticWaypoints();
            const D3D11_VIEWPORT viewport{0,0,static_cast<float>(resources.sceneDescription.Width),static_cast<float>(resources.sceneDescription.Height),0,1};
            drawWaypoints(context,device.Get(),view,projection,cameraPosition,points,resources.sceneOverlayTarget.Get(),viewport,true);
        }
        restore(context,state);
        if(!copied)return false;
        *output=resources.sceneCopy.Get();(*output)->AddRef();
        return true;
    }catch(...){return false;}
}
void drawWorldWaypoints(ID3D11DeviceContext* context,const std::array<float,16>& view,
    const std::array<float,16>& projection,Vec3 cameraPosition,const OpticWaypoints& markers,
    HudMode mode,bool enemyMarkers,const std::array<float,16>* binocularWorld) noexcept{
    if(!context||!markers.count)return;
    try{
        std::lock_guard lock(rendererMutex);
        ComPtr<ID3D11Device> device;context->GetDevice(&device);if(!device)return;
        if(resources.device.Get()!=device.Get()){resources=Resources{};resources.device=device;}
        SavedState state;save(context,state);
        OpticWaypoints visible=markers;visible.count=0;
        for(size_t i=0;i<std::min(markers.count,markers.points.size());++i)
            if(worldMarkerVisible(mode,enemyMarkers,markers.points[i].letter))
                visible.points[visible.count++]=markers.points[i];
        if(state.renderTarget&&state.viewportCount){
            auto* mask=binocularWorld?binocularMarkerMask(context,device.Get(),state,*binocularWorld,view,projection):nullptr;
            drawWaypoints(context,device.Get(),view,projection,cameraPosition,visible,state.renderTarget.Get(),state.viewport,false,mask);
        }
        restore(context,state);
    }catch(...){}
}
bool drawPhysicalWeaponScope(ID3D11DeviceContext* context,const std::array<float,16>& ocularWorld,
    const std::array<float,16>& view,const std::array<float,16>& projection,float radius,float magnification,
    ID3D11Texture2D* sceneSource,bool dotReticle,bool opaqueAtOnePower) noexcept {
    const bool collimator=dotReticle&&magnification==1.f&&!opaqueAtOnePower;
    if(!context||(!sceneSource&&!collimator)||!std::isfinite(radius)||radius<.003f||radius>.06f
       ||!std::isfinite(magnification)||magnification<1.f||magnification>16.f)return false;
    try{
        std::lock_guard lock(rendererMutex);ComPtr<ID3D11Device> device,sourceDevice;
        context->GetDevice(&device);if(sceneSource)sceneSource->GetDevice(&sourceDevice);
        if(!device||(!collimator&&device.Get()!=sourceDevice.Get()))return false;
        if(resources.device.Get()!=device.Get()){resources={};resources.device=device;}
        if(!createLensResources(device.Get()))return false;
        ComPtr<ID3D11ShaderResourceView> image;
        if(!collimator){
            D3D11_TEXTURE2D_DESC desc{};sceneSource->GetDesc(&desc);
            const auto format=shaderResourceFormat(desc.Format);
            if(!desc.Width||!desc.Height||desc.ArraySize!=1||desc.MipLevels!=1||desc.SampleDesc.Count!=1||format==DXGI_FORMAT_UNKNOWN)return false;
            D3D11_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=format;srv.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;srv.Texture2D.MipLevels=1;
            if(FAILED(device->CreateShaderResourceView(sceneSource,&srv,&image)))return false;
        }
        // Reuse the aperture mesh, not the binocular housing or its sockets.
        // Translate its local centre to zero, scale its radius, then apply the
        // current native scope's ocular transform from the same weapon frame.
        const float scale=radius/ocularRadius;
        std::array<float,16> fit{scale,0,0,0,0,scale,0,0,0,0,scale,0,
            -ocularCenter.x*scale,-ocularCenter.y*scale,-ocularCenter.z*scale,1};
        const auto world=matrixProduct(fit,ocularWorld);
        for(const auto value:world)if(!std::isfinite(value))return false;
        SavedState state;save(context,state);if(!state.renderTarget){restore(context,state);return false;}
        ComPtr<ID3D11DepthStencilView> depth;
        if(!bindHousingDepth(context,device.Get(),state,projection[14]>0,depth)){restore(context,state);return false;}
        const bool drawn=drawLensPortal(context,world,view,projection,magnification,state,depth.Get(),image.Get(),collimator?3.f:dotReticle?2.f:1.f);
        restore(context,state);return drawn;
    }catch(...){return false;}
}
namespace {
bool drawBinocularMaterial(ID3D11DeviceContext* context,
    const OpticNativeLightingSource& source,ID3D11PixelShader* exactNativeShader,
    bool nativeProgramGpuJoinVerified,const OpticNativeGeometryPass* ownedPass=nullptr) noexcept{
    if(!context||nativeBinocularMaterialDrawing()||!nativeProgramGpuJoinVerified
        ||!opticLightingHousingSourceEligible(source)
        ||(!ownedPass&&!opticNativeBinocularMaterialShaderVerified(exactNativeShader)))return false;
    if(ownedPass&&(!ownedPass->nativePairVerified||!ownedPass->materialFamilyVerified))return false;
    try{
        const auto fresh=[&]{const auto now=steadyMilliseconds();
            return now>=source.eye.sampleTime&&now-source.eye.sampleTime<=150;};
        if(!fresh())return false;
        std::unique_lock lock(rendererMutex,std::try_to_lock);if(!lock)return false;
        ComPtr<ID3D11Device> device;context->GetDevice(device.GetAddressOf());if(!device)return false;
        if(resources.device.Get()!=device.Get())resources=Resources{};
        const auto eye=source.eye.eye;
        const auto generation=nativeBinocularMaterialGeneration();
        // The scope also covers shader binds and RAII restoration so our
        // temporary material never becomes a cached native binding identity.
        OpticNativeMaterialDrawScope drawing;
        NativeMaterialState state(context);if(!state.exactGeometryBinding(exactNativeShader))return false;
        if(ownedPass){
            if(ownedPass->pixelShader!=reinterpret_cast<uintptr_t>(state.pixelShader.Get())
                ||ownedPass->vertexShader!=reinterpret_cast<uintptr_t>(state.vertexShader.Get())
                ||!opticNativeSceneUploadVerified(context,source,*ownedPass))return false;
            ComPtr<ID3D11Buffer> scene;UINT first{},count=4096;
            if(state.context1)state.context1->VSGetConstantBuffers1(2,1,scene.GetAddressOf(),&first,&count);
            else context->VSGetConstantBuffers(2,1,scene.GetAddressOf());
            if(reinterpret_cast<uintptr_t>(scene.Get())!=ownedPass->sceneBuffer
                ||first!=ownedPass->firstConstant||count!=ownedPass->constantCount)return false;
        }
        if(resources.nativeInsertedValid[eye]&&resources.nativeInsertedGeneration[eye]==generation){const auto& previous=resources.nativeInsertedSources[eye];
            // The callback reports actual new draws, not cache hits; the late
            // housing independently reads the same exact-source success.
            if(opticLightingSameEye(previous.eye,source.eye))return !ownedPass&&previous.nativeCamera==source.nativeCamera
                &&opticLightingMatrixError(previous.binocularWorld,source.binocularWorld)==0
                &&opticLightingMatrixError(previous.view,source.view)==0&&opticLightingMatrixError(previous.projection,source.projection)==0;}
        if(!resources.attempted){resources.device=device;if(!createResources(device.Get()))return false;}
        if(!resources.ready||!createNativeMaterialResources(device.Get())||!fresh())return false;
        const auto worldView=matrixProduct(source.binocularWorld,source.view);
        const auto mvp=matrixProduct(worldView,source.projection);
        for(const auto value:mvp)if(!std::isfinite(value))return false;
        NativeMaterialConstants constants{};
        std::copy(mvp.begin(),mvp.end(),std::begin(constants.mvp));
        std::copy(worldView.begin(),worldView.end(),std::begin(constants.worldView));
        context->UpdateSubresource(resources.nativeMaterialView.Get(),0,nullptr,&constants,0,0);
        // No render target, DSV, native system buffer, mesh mask,
        // sampler, viewport or scissor binding is changed by this insertion.
        state.changed=true;
        const UINT stride=sizeof(Vertex),offset=0;ID3D11Buffer* vertices=resources.vertices.Get();
        context->IASetInputLayout(resources.nativeMaterialInputLayout.Get());context->IASetVertexBuffers(0,1,&vertices,&stride,&offset);
        context->IASetIndexBuffer(resources.indices.Get(),DXGI_FORMAT_R16_UINT,0);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(resources.nativeMaterialVertexShader.Get(),nullptr,0);
        if(ownedPass)context->PSSetShader(resources.nativeMaterialPixelShader.Get(),nullptr,0);
        ID3D11Buffer* view=resources.nativeMaterialView.Get();context->VSSetConstantBuffers(0,1,&view);
        ID3D11Buffer* parameters=resources.nativeMaterialParameters.Get();context->PSSetConstantBuffers(4,1,&parameters);
        const std::array<ID3D11ShaderResourceView*,4> maps{resources.diffuse.Get(),resources.nativeNormal.Get(),
            resources.nativeSpecular.Get(),resources.nativeMaterialMap.Get()};
        context->PSSetShaderResources(0,4,maps.data());context->RSSetState(resources.rasterizer.Get());
        // Use the proven writable native geometry pass's depth comparison,
        // stencil operations/reference and opaque MRT writes unchanged.
        context->DrawIndexed(resources.indexCount,0,0);
        resources.nativeInsertedSources[eye]=source;resources.nativeInsertedValid[eye]=true;
        resources.nativeInsertedGeneration[eye]=generation;
        if(!resources.nativeMaterialReported){resources.nativeMaterialReported=true;
            std::ostringstream message;message<<"Native binocular material inserted source="<<source.eye.sourceSequence
                <<" tracking="<<source.eye.trackingSequence<<" activation="<<source.eye.activation<<" eye="<<source.eye.eye
                <<" native_camera=0x"<<std::hex<<source.nativeCamera<<" native_ps=0x"<<reinterpret_cast<uintptr_t>(exactNativeShader)
                <<" owned_ps=0x"<<reinterpret_cast<uintptr_t>(ownedPass?resources.nativeMaterialPixelShader.Get():exactNativeShader)
                <<std::dec<<" exact_owned_shader=1 program_gpu_join_verified=1"
                <<" source_owned_ps="<<(ownedPass!=nullptr)<<" camera_cpu_upload_verified="<<(ownedPass!=nullptr)
                <<" native_gpu_pair_verified="<<(ownedPass?ownedPass->nativePairVerified:true)
                <<" native_vs_family_verified="<<(ownedPass?ownedPass->materialFamilyVerified:true)
                <<" original_ps_code_verified="<<(!ownedPass)
                <<" authored_maps=4 native_mrt_count=3 writable_native_depth=1";
            log(message.str());}
        return true;
    }catch(...){return false;}
}
bool drawOwnedBinocularMaterial(ID3D11DeviceContext* context,const OpticNativeLightingSource& source,
    const OpticNativeGeometryPass& pass) noexcept {
    return opticLightingGeometryPassEligible(pass)
        &&drawBinocularMaterial(context,source,reinterpret_cast<ID3D11PixelShader*>(pass.pixelShader),true,&pass);
}
}
bool drawNativeBinocularMaterialGBuffer(ID3D11DeviceContext* context,const OpticNativeLightingSource& source,
    ID3D11PixelShader* exactNativeShader,bool nativeProgramGpuJoinVerified) noexcept {
    return drawBinocularMaterial(context,source,exactNativeShader,nativeProgramGpuJoinVerified);
}

bool drawPhysicalBinoculars(ID3D11DeviceContext* context,const std::array<float,16>& world,const std::array<float,16>& view,
    const std::array<float,16>& projection,float magnification,ID3D11Texture2D* sceneSource,bool lensVisible,
    const EyeFrame* sourceEye) noexcept{
    if(!context)return false;
    try{
        std::lock_guard lock(rendererMutex);ComPtr<ID3D11Device> device;context->GetDevice(device.GetAddressOf());if(!device)return false;
        if(resources.device.Get()!=device.Get())resources=Resources{};
        if(!resources.attempted){resources.device=device;if(!createResources(device.Get()))return false;}
        if(!resources.ready||!std::isfinite(magnification)||magnification<1||magnification>4)return false;
        if(createNativeMaterialResources(device.Get()))setOpticNativeMaterialDrawCallback(&drawOwnedBinocularMaterial);
        SavedState state;save(context,state);if(!state.renderTarget){restore(context,state);return false;}
        bool nativeHousing=false;
        if(opticNativeBinocularMaterialEnabled()&&sourceEye&&sourceEye->eye<2&&resources.nativeInsertedValid[sourceEye->eye]
            &&resources.nativeInsertedGeneration[sourceEye->eye]==nativeBinocularMaterialGeneration()){
            const auto& inserted=resources.nativeInsertedSources[sourceEye->eye];
            nativeHousing=opticLightingSameEye(inserted.eye,*sourceEye)
                &&opticLightingMatrixError(inserted.binocularWorld,world)==0
                &&opticLightingMatrixError(inserted.view,view)==0&&opticLightingMatrixError(inserted.projection,projection)==0;}
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
        const bool needsLens=lensVisible&&magnification>1.0001f&&sceneSource;
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
        if(!bindHousingDepth(context,device.Get(),state,projection[14]>0,housingDepth,sourceEye)){restore(context,state);return false;}
        constants.baseColor[0]=1.f;constants.baseColor[1]=1.f;constants.baseColor[2]=1.f;constants.baseColor[3]=1.f;
        context->UpdateSubresource(resources.constants.Get(),0,nullptr,&constants,0,0);const UINT stride=sizeof(Vertex),offset=0;ID3D11Buffer* vertexBuffer=resources.vertices.Get();
        context->IASetInputLayout(resources.inputLayout.Get());context->IASetVertexBuffers(0,1,&vertexBuffer,&stride,&offset);context->IASetIndexBuffer(resources.indices.Get(),DXGI_FORMAT_R16_UINT,0);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context->VSSetShader(resources.vertexShader.Get(),nullptr,0);ID3D11Buffer* constantsBuffer=resources.constants.Get();context->VSSetConstantBuffers(0,1,&constantsBuffer);
        context->PSSetShader(resources.pixelShader.Get(),nullptr,0);ID3D11ShaderResourceView* diffuse=resources.diffuse.Get();context->PSSetShaderResources(0,1,&diffuse);ID3D11SamplerState* sampler=resources.sampler.Get();context->PSSetSamplers(0,1,&sampler);context->RSSetState(resources.rasterizer.Get());context->OMSetDepthStencilState(projection[14]>0?resources.reversedDepthStencil.Get():resources.depthStencil.Get(),0);const FLOAT blendFactor[4]{0,0,0,0};
        // Once this exact eye/body was inserted into native lighting, retain
        // only its physical depth here for the ocular portal. Writing the old
        // fixed-light color would overwrite the native material result.
        context->OMSetBlendState(nativeHousing?resources.nativeHousingDepthOnlyBlend.Get():resources.blend.Get(),blendFactor,0xffffffffu);
        context->DrawIndexed(resources.indexCount,0,0);
        if(needsLens&&!drawLensPortal(context,world,view,projection,magnification,state,housingDepth.Get(),sceneSourceView.Get())){
            restore(context,state);if(!resources.lensReported){resources.lensReported=true;mgs5vr::log("Retail optic lens portal unavailable; optical rendering remains failed closed");}return false;
        }
        restore(context,state);if(!resources.reported){resources.reported=true;mgs5vr::log("Retail binocular FMDL rendered in both native stereo eyes");}return true;
    }catch(...){return false;}
}
bool drawPhysicalIdroid(ID3D11DeviceContext* context,const std::array<float,16>& world,
    const std::array<float,16>& view,const std::array<float,16>& projection) noexcept{
    if(!context)return false;
    try{
        std::lock_guard lock(rendererMutex);ComPtr<ID3D11Device> device;context->GetDevice(device.GetAddressOf());
        if(!device)return false;
        if(resources.device.Get()!=device.Get()){resources=Resources{};resources.device=device;}
        if(!createIdroidResources(device.Get()))return false;
        const auto mvp=matrixProduct(matrixProduct(world,view),projection);
        for(float value:mvp)if(!std::isfinite(value))return false;
        SavedState state;save(context,state);if(!state.renderTarget){restore(context,state);return false;}
        ComPtr<ID3D11DepthStencilView> depth;
        if(!bindHousingDepth(context,device.Get(),state,projection[14]>0,depth)){restore(context,state);return false;}
        IdroidConstants constants{};std::copy(mvp.begin(),mvp.end(),std::begin(constants.mvp));
        std::copy(world.begin(),world.end(),std::begin(constants.world));
        constants.baseColor[0]=constants.baseColor[1]=constants.baseColor[2]=1.f;constants.baseColor[3]=1.f;
        context->UpdateSubresource(resources.idroidConstants.Get(),0,nullptr,&constants,0,0);
        const UINT stride=sizeof(IdroidVertex),offset=0;ID3D11Buffer* vertexBuffer=resources.idroidVertices.Get();
        context->IASetInputLayout(resources.idroidInputLayout.Get());context->IASetVertexBuffers(0,1,&vertexBuffer,&stride,&offset);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(resources.idroidVertexShader.Get(),nullptr,0);
        ID3D11Buffer* constantBuffer=resources.idroidConstants.Get();context->VSSetConstantBuffers(0,1,&constantBuffer);context->PSSetConstantBuffers(0,1,&constantBuffer);
        context->PSSetShader(resources.idroidPixelShader.Get(),nullptr,0);
        context->RSSetState(resources.idroidRasterizer.Get());
        context->OMSetDepthStencilState(projection[14]>0?resources.idroidReversedDepthStencil.Get():resources.idroidDepthStencil.Get(),0);
        const FLOAT blendFactor[4]{0,0,0,0};context->OMSetBlendState(resources.idroidBlend.Get(),blendFactor,0xffffffffu);
        if(resources.idroidVertexCount)context->Draw(resources.idroidVertexCount,0);
        restore(context,state);
        if(!resources.idroidReported){resources.idroidReported=true;mgs5vr::log("Native iDroid front display bound to palm; overlay bezel disabled");}
        return true;
    }catch(...){return false;}
}
bool drawPhysicalIdroidCursor(ID3D11DeviceContext* context,const std::array<float,16>& world,
    const std::array<float,16>& view,const std::array<float,16>& projection) noexcept{
    if(!context)return false;
    try{
        std::lock_guard lock(rendererMutex);ComPtr<ID3D11Device> device;context->GetDevice(device.GetAddressOf());
        if(!device)return false;
        if(resources.device.Get()!=device.Get()){resources=Resources{};resources.device=device;}
        if(!createIdroidResources(device.Get()))return false;
        const auto mvp=matrixProduct(matrixProduct(world,view),projection);
        for(float value:mvp)if(!std::isfinite(value))return false;
        SavedState state;save(context,state);if(!state.renderTarget){restore(context,state);return false;}
        ComPtr<ID3D11DepthStencilView> depth;
        if(!bindHousingDepth(context,device.Get(),state,projection[14]>0,depth)){restore(context,state);return false;}
        IdroidConstants constants{};std::copy(mvp.begin(),mvp.end(),std::begin(constants.mvp));
        std::copy(world.begin(),world.end(),std::begin(constants.world));
        // The cursor is a projection marker, not a lit housing surface. Keep
        // it emissive enough to remain readable over the native blue map.
        constants.baseColor[0]=constants.baseColor[1]=constants.baseColor[2]=3.f;constants.baseColor[3]=1.f;
        context->UpdateSubresource(resources.idroidConstants.Get(),0,nullptr,&constants,0,0);
        const UINT stride=sizeof(IdroidVertex),offset=0;ID3D11Buffer* vertexBuffer=resources.idroidCursorVertices.Get();
        context->IASetInputLayout(resources.idroidInputLayout.Get());context->IASetVertexBuffers(0,1,&vertexBuffer,&stride,&offset);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(resources.idroidVertexShader.Get(),nullptr,0);
        ID3D11Buffer* constantBuffer=resources.idroidConstants.Get();context->VSSetConstantBuffers(0,1,&constantBuffer);context->PSSetConstantBuffers(0,1,&constantBuffer);
        context->PSSetShader(resources.idroidPixelShader.Get(),nullptr,0);
        context->RSSetState(resources.idroidRasterizer.Get());
        context->OMSetDepthStencilState(projection[14]>0?resources.idroidReversedDepthStencil.Get():resources.idroidDepthStencil.Get(),0);
        const FLOAT blendFactor[4]{0,0,0,0};context->OMSetBlendState(resources.idroidBlend.Get(),blendFactor,0xffffffffu);
        context->Draw(resources.idroidCursorVertexCount,0);restore(context,state);return true;
    }catch(...){return false;}
}
bool drawOpeningProps(ID3D11DeviceContext* context,const std::array<std::array<float,16>,7>& worlds,
    const std::array<float,16>& view,const std::array<float,16>& projection,const std::array<float,16>& cabinWorld,
    const EyeFrame& eye,float lidOpen,int selection) noexcept{
    (void)selection;(void)lidOpen;(void)cabinWorld;
    if(!context)return false;
    try{
        std::lock_guard lock(rendererMutex);ComPtr<ID3D11Device> device;context->GetDevice(device.GetAddressOf());
        if(!device)return false;
        if(resources.device.Get()!=device.Get()){resources=Resources{};resources.device=device;}
        if(!resources.attempted){if(!createResources(device.Get()))return false;}
        if(!resources.ready||!createOpeningResources(device.Get())||!resources.openingReady)return false;
        for(const auto& world:worlds)for(const auto value:world)if(!std::isfinite(value))return false;
        SavedState state;save(context,state);if(!state.renderTarget){restore(context,state);return false;}
        ComPtr<ID3D11DepthStencilView> depth;
        if(!bindHousingDepth(context,device.Get(),state,projection[14]>0,depth,&eye)){restore(context,state);return false;}
        // Retain the player's authored controls without a billboard over them.
        // The cassette geometry is independently movable.
        bool cassettes=true;
        for(size_t i=1;i<worlds.size();++i)
            cassettes=drawOpeningAsset(context,resources.openingCassette,state.renderTarget.Get(),depth.Get(),
                worlds[i],view,projection)&&cassettes;
        if(cassettes)
            drawOpeningLabels(context,state.renderTarget.Get(),depth.Get(),worlds,view,projection);
        restore(context,state);
        if(cassettes&&!resources.openingReported){
            resources.openingReported=true;mgs5vr::log("Physical labelled cassettes use the existing native cabin player; native depth join reported separately");
        }
        return cassettes;
    }catch(...){return false;}
}

bool drawFirstPersonEyepatch(ID3D11DeviceContext* context,Pose head,Pose rightEye,const EyeView& eye) noexcept {
    if(!context)return false;
    try {
        std::lock_guard lock(rendererMutex);
        ComPtr<ID3D11Device> device;context->GetDevice(&device);if(!device)return false;
        if(resources.device.Get()!=device.Get()){resources=Resources{};resources.device=device;}
        if(!createMarkerResources(device.Get()))return false;
        if(!resources.eyepatchBlend){
            D3D11_BLEND_DESC blend{};auto& target=blend.RenderTarget[0];
            target.BlendEnable=TRUE;target.SrcBlend=D3D11_BLEND_SRC_ALPHA;
            target.DestBlend=D3D11_BLEND_INV_SRC_ALPHA;target.BlendOp=D3D11_BLEND_OP_ADD;
            target.SrcBlendAlpha=D3D11_BLEND_ONE;target.DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;
            target.BlendOpAlpha=D3D11_BLEND_OP_ADD;target.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
            if(FAILED(device->CreateBlendState(&blend,&resources.eyepatchBlend)))return false;
        }
        struct PatchVertex {float clip[4];float uv[3];};
        if(!resources.eyepatch.ready){
            if(resources.eyepatchAttempted)return false;
            resources.eyepatchAttempted=true;
            const auto root=gameRoot()/L"retail-assets/Assets/tpp/chara/sna";
            std::string failure;
            const auto model=readRetailFmdl(root/L"Scenes/sna0_face0_cov.fmdl",failure,20);
            const auto texture=readRetailDds(root/L"Pictures/sna0_face1_def_bsm.dds",failure);
            if(!model||!texture||model->sha256!="7fd7f6e5c981a957ede9f8113d334c67f5ef706766be7f84a79a41e6fb9bfdd5"
               ||texture->sha256!="adf9bcee7dd55b3d5ebe524f0aa6e0d060cfca8dec540223014f9df5c4299ade"
               ||model->vertices.size()!=142||model->indices.size()!=411){
                log("Native first-person eyepatch unavailable: "+failure);return false;
            }
            constexpr auto vs=R"(
struct V {float4 clip:POSITION;float3 uv:TEXCOORD;};
struct O {float4 position:SV_POSITION;float3 uv:TEXCOORD;};
O main(V v){O o;o.position=v.clip;o.uv=v.uv;return o;})";
            constexpr auto ps=R"(
Texture2D nativeFace:register(t0);SamplerState nativeSampler:register(s0);
float4 main(float4 position:SV_POSITION,float3 uv:TEXCOORD):SV_TARGET{
 // The hidden head still occludes the rear wrapping straps. Keep the actual
 // native front surface; do not expose inside-head straps through the face.
 clip(uv.z-.105);
 float3 native=nativeFace.Sample(nativeSampler,uv.xy).rgb;
 float gray=.18+dot(native,float3(.2126,.7152,.0722))*.45;
 return float4(lerp(native,gray.xxx,.85),.48);})";
            ComPtr<ID3DBlob> vertex,pixel;
            if(!compile(device.Get(),vs,"vs_5_0",&vertex)||!compile(device.Get(),ps,"ps_5_0",&pixel))return false;
            if(FAILED(device->CreateVertexShader(vertex->GetBufferPointer(),vertex->GetBufferSize(),nullptr,&resources.eyepatchVs))
               ||FAILED(device->CreatePixelShader(pixel->GetBufferPointer(),pixel->GetBufferSize(),nullptr,&resources.eyepatchPs)))return false;
            const D3D11_INPUT_ELEMENT_DESC elements[]{
                {"POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
                {"TEXCOORD",0,DXGI_FORMAT_R32G32B32_FLOAT,0,16,D3D11_INPUT_PER_VERTEX_DATA,0}};
            if(FAILED(device->CreateInputLayout(elements,2,vertex->GetBufferPointer(),vertex->GetBufferSize(),&resources.eyepatchLayout)))return false;
            D3D11_BUFFER_DESC vb{};vb.ByteWidth=static_cast<UINT>(model->vertices.size()*sizeof(PatchVertex));
            vb.Usage=D3D11_USAGE_DYNAMIC;vb.BindFlags=D3D11_BIND_VERTEX_BUFFER;vb.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
            D3D11_BUFFER_DESC ib{};ib.ByteWidth=static_cast<UINT>(model->indices.size()*sizeof(uint16_t));
            ib.Usage=D3D11_USAGE_IMMUTABLE;ib.BindFlags=D3D11_BIND_INDEX_BUFFER;
            D3D11_SUBRESOURCE_DATA indices{model->indices.data(),0,0};
            D3D11_SAMPLER_DESC sampler{};sampler.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sampler.MaxLOD=D3D11_FLOAT32_MAX;
            if(FAILED(device->CreateBuffer(&vb,nullptr,&resources.eyepatch.vertices))
               ||FAILED(device->CreateBuffer(&ib,&indices,&resources.eyepatch.indices))
               ||FAILED(device->CreateSamplerState(&sampler,&resources.eyepatchSampler))
               ||!createRetailDiffuseView(device.Get(),*texture,resources.eyepatch.diffuse))return false;
            resources.eyepatch.bindVertices=model->vertices;resources.eyepatch.indexCount=411;resources.eyepatch.ready=true;
            log("Native first-person eyepatch loaded: owned face0 mesh20, 142 vertices, 137 triangles, native diffuse/UVs");
        }
        std::vector<PatchVertex> vertices;vertices.reserve(resources.eyepatch.bindVertices.size());
        for(const auto& native:resources.eyepatch.bindVertices){
            const auto clip=projectEyepatch(head,rightEye,eye,eyepatchHeadPoint({native.position[0],native.position[1],native.position[2]}));
            if(!clip)return false;
            vertices.push_back({{(*clip)[0],(*clip)[1],(*clip)[2],(*clip)[3]},{native.uv[0],native.uv[1],native.position[2]}});
        }
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if(FAILED(context->Map(resources.eyepatch.vertices.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))return false;
        std::memcpy(mapped.pData,vertices.data(),vertices.size()*sizeof(PatchVertex));context->Unmap(resources.eyepatch.vertices.Get(),0);
        SavedState saved;save(context,saved);if(!saved.renderTarget){restore(context,saved);return false;}
        ID3D11RenderTargetView* target=saved.renderTarget.Get();context->OMSetRenderTargets(1,&target,nullptr);
        context->RSSetState(resources.markerRasterizer.Get());context->OMSetDepthStencilState(resources.markerDepth.Get(),0);
        context->OMSetBlendState(resources.eyepatchBlend.Get(),nullptr,0xffffffffu);
        const UINT stride=sizeof(PatchVertex),offset=0;ID3D11Buffer* buffer=resources.eyepatch.vertices.Get();
        context->IASetVertexBuffers(0,1,&buffer,&stride,&offset);context->IASetInputLayout(resources.eyepatchLayout.Get());
        context->IASetIndexBuffer(resources.eyepatch.indices.Get(),DXGI_FORMAT_R16_UINT,0);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(resources.eyepatchVs.Get(),nullptr,0);context->PSSetShader(resources.eyepatchPs.Get(),nullptr,0);
        ID3D11ShaderResourceView* texture=resources.eyepatch.diffuse.Get();context->PSSetShaderResources(0,1,&texture);
        ID3D11SamplerState* sampler=resources.eyepatchSampler.Get();context->PSSetSamplers(0,1,&sampler);
        context->DrawIndexed(resources.eyepatch.indexCount,0,0);restore(context,saved);return true;
    }catch(...){return false;}
}

bool drawAccWristHint(ID3D11DeviceContext* context,const std::array<float,16>& world,
    const std::array<float,16>& view,const std::array<float,16>& projection,std::string_view idroidBinding) noexcept{
    if(!context||idroidBinding.empty()||idroidBinding=="UNBOUND"||idroidBinding.size()>512)return false;
    try{
        std::lock_guard lock(rendererMutex);ComPtr<ID3D11Device> device;context->GetDevice(device.GetAddressOf());
        if(!device)return false;
        if(resources.device.Get()!=device.Get()){resources=Resources{};resources.device=device;}
        if(!createAccWristHintResources(device.Get(),idroidBinding))return false;
        const auto mvp=matrixProduct(matrixProduct(world,view),projection);
        for(float value:mvp)if(!std::isfinite(value))return false;
        SavedState state;save(context,state);if(!state.renderTarget){restore(context,state);return false;}
        ComPtr<ID3D11DepthStencilView> depth;
        if(!bindHousingDepth(context,device.Get(),state,projection[14]>0,depth)){restore(context,state);return false;}
        LensConstants constants{};std::copy(mvp.begin(),mvp.end(),std::begin(constants.mvp));
        context->UpdateSubresource(resources.lensConstants.Get(),0,nullptr,&constants,0,0);
        ID3D11RenderTargetView* target=state.renderTarget.Get();context->OMSetRenderTargets(1,&target,depth.Get());
        const UINT stride=sizeof(LensVertex),offset=0;ID3D11Buffer* vertices=resources.wristHintVertices.Get();
        context->IASetInputLayout(resources.lensInputLayout.Get());context->IASetVertexBuffers(0,1,&vertices,&stride,&offset);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        context->VSSetShader(resources.lensVertexShader.Get(),nullptr,0);
        ID3D11Buffer* buffer=resources.lensConstants.Get();context->VSSetConstantBuffers(0,1,&buffer);
        context->PSSetShader(resources.wristHintPixelShader.Get(),nullptr,0);
        ID3D11ShaderResourceView* texture=resources.wristHintTexture.Get();context->PSSetShaderResources(0,1,&texture);
        ID3D11SamplerState* sampler=resources.lensSampler.Get();context->PSSetSamplers(0,1,&sampler);
        context->RSSetState(resources.lensRasterizer.Get());context->OMSetDepthStencilState(
            projection[14]>0?resources.reversedLensDepthStencil.Get():resources.lensDepthStencil.Get(),0);
        const FLOAT factors[4]{};context->OMSetBlendState(resources.blend.Get(),factors,0xffffffffu);
        context->Draw(4,0);restore(context,state);
        if(!resources.wristHintReported){resources.wristHintReported=true;log("ACC deployment guidance drawn on source LEFT forearm");}
        return true;
    }catch(...){return false;}
}

bool drawCabinStudio(ID3D11DeviceContext* context,const CabinStudioFrame& frame,const CabinStudioMatrices& worlds,
    const std::array<float,16>& view,const std::array<float,16>& projection,const EyeFrame& eye,
    ID3D11Texture2D* cameraScene) noexcept{
    if(!context||!frame.available)return false;
    try{
        std::lock_guard lock(rendererMutex);ComPtr<ID3D11Device> device;context->GetDevice(&device);if(!device)return false;
        if(resources.device.Get()!=device.Get()){resources=Resources{};resources.device=device;}
        if(!resources.attempted&&!createResources(device.Get()))return false;
        if(!resources.ready||!createStudioResources(device.Get()))return false;
        SavedState saved;save(context,saved);if(!saved.renderTarget){restore(context,saved);return false;}
        ComPtr<ID3D11DepthStencilView> depth;
        if(!bindHousingDepth(context,device.Get(),saved,projection[14]>0,depth,&eye)){restore(context,saved);return false;}
        const auto draw=[&](RetailGpuAsset& asset,const std::array<float,16>& world){
            return drawOpeningAsset(context,asset,saved.renderTarget.Get(),depth.Get(),world,view,projection);};
        if(frame.keyboardOnly){
            if(frame.hoverKey>=0&&nativeTextKeyEnabled(static_cast<size_t>(frame.hoverKey)))
                draw(resources.studioFocus,worlds.keys[static_cast<size_t>(frame.hoverKey)]);
            for(size_t key=0;key<44;++key)if(nativeTextKeyEnabled(key))draw(resources.studioKeys[key],worlds.keys[key]);
            restore(context,saved);return true;
        }
        for(auto& part:resources.studioCamera)draw(part,worlds.camera);
        if(cameraScene&&!frame.review){
            resources.studioViewfinder.diffuse.Reset();device->CreateShaderResourceView(cameraScene,nullptr,&resources.studioViewfinder.diffuse);
        }
        if(frame.review){
            if(resources.studioGalleryPhoto!=frame.selectedPhoto){
                resources.studioGallery.diffuse.Reset();resources.studioGalleryPhoto=frame.selectedPhoto;
                if(const auto pixels=studioPhotoPixels(frame.selectedPhoto)){
                    D3D11_TEXTURE2D_DESC desc{};desc.Width=pixels->width;desc.Height=pixels->height;
                    desc.MipLevels=desc.ArraySize=1;desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
                    desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
                    const D3D11_SUBRESOURCE_DATA initial{pixels->bgra.data(),pixels->width*4,0};
                    ComPtr<ID3D11Texture2D> texture;
                    if(SUCCEEDED(device->CreateTexture2D(&desc,&initial,&texture)))
                        device->CreateShaderResourceView(texture.Get(),nullptr,&resources.studioGallery.diffuse);
                }
            }
            if(resources.studioGallery.diffuse)draw(resources.studioGallery,worlds.camera);
        }else if(resources.studioViewfinder.diffuse)draw(resources.studioViewfinder,worlds.camera);
        for(size_t i=0;i<6;++i){
            draw(resources.studioButtons[i],worlds.camera);
            if(frame.hoverControl==static_cast<int>(i))draw(resources.studioButtonHover,worlds.controls[i]);
            auto& face=frame.confirmPhoto&&i==3?resources.studioConfirmControl
                :frame.confirmPhoto&&i==4?resources.studioCancelControl
                :i==5&&frame.freezeRequested?resources.studioResume:resources.studioButtonFaces[i];
            draw(face,worlds.controls[i]);
        }
        std::string status;
        if(frame.confirmPhoto)status="DELETE PHOTO?";
        else{
            if(frame.review){
                const auto selected=std::find(frame.photos.begin(),frame.photos.end(),frame.selectedPhoto);
                status=selected==frame.photos.end()?"NO SAVED PHOTOS":"PHOTO "+std::to_string(selected-frame.photos.begin()+1)
                    +" / "+std::to_string(frame.photos.size());
            }else status="LIVE";
            if(frame.freezeState==StudioFreeze::frozen)status+=" / FROZEN";
            else if(frame.freezeState==StudioFreeze::pending)status+=" / WAIT";
            else if(frame.freezeState==StudioFreeze::unavailable)status+=" / FREEZE FAILED";
            else if(!frame.freezeSupported)status+=" / NO FREEZE";
            if(frame.capacityReached)status+="\nPAPER STORAGE FULL";
        }
        if(status!=resources.studioStatusText||!resources.studioCameraStatus.ready){
            RetailGpuAsset label;if(studioQuad(device.Get(),label,{.032f,.009f,0},studioTextPixels(status),512,160)){
                resources.studioCameraStatus=std::move(label);resources.studioStatusText=status;
            }
        }
        if(resources.studioCameraStatus.ready)draw(resources.studioCameraStatus,worlds.cameraStatus);
        if(frame.cabin){
            draw(resources.studioNotes,worlds.notes);
            draw(resources.studioMarker,worlds.marker);
            draw(resources.studioMarkerTip,worlds.marker);
        }
            for(auto it=resources.studioPapers.begin();it!=resources.studioPapers.end();){
                if(std::none_of(frame.papers.begin(),frame.papers.end(),[&](const auto& p){return p.id==it->first;}))
                    it=resources.studioPapers.erase(it);else ++it;
            }
            for(size_t index=0;index<frame.papers.size()&&index<worlds.papers.size();++index){
                const auto& paper=frame.papers[index];const auto& world=worlds.papers[index];
                if(!frame.cabin&&!paper.field)continue;
                draw(paper.photo?resources.studioPhotoBack:resources.studioStickyBack,world);
                auto& gpu=resources.studioPapers[paper.id];
                if(!gpu.face.ready||gpu.revision!=paper.ink->revision){
                    RetailGpuAsset face;const auto pixels=studioPaperPixels(paper);
                    if(studioQuad(device.Get(),face,studioPaperHalf(paper.photo!=0),pixels,1024,1024,.001f)){
                        gpu.face=std::move(face);gpu.revision=paper.ink->revision;}}
                if(gpu.face.ready)draw(gpu.face,world);
            }
            if(frame.editor>=0){
                if(frame.hoverKey>=0)draw(resources.studioFocus,worlds.keys[static_cast<size_t>(frame.hoverKey)]);
                if(frame.confirmPaper){
                    draw(resources.studioPaperConfirmation,worlds.paperConfirmation);
                    draw(resources.studioCancel,worlds.keys[33]);draw(resources.studioDelete,worlds.keys[43]);
                }else for(size_t key=0;key<44;++key)draw(resources.studioKeys[key],worlds.keys[key]);
            }
        restore(context,saved);return true;
    }catch(...){return false;}
}
bool drawIdroidGuidance(ID3D11DeviceContext* context,const std::array<float,16>& world,
    const std::array<float,16>& view,const std::array<float,16>& projection,
    float width,float aspect,ID3D11Texture2D* source,const EyeFrame& eye) noexcept {
    if(!context||!source||!std::isfinite(width)||width<.2f||width>.6f
       ||!std::isfinite(aspect)||aspect<.5f||aspect>3.f)return false;
    try{
        std::lock_guard lock(rendererMutex);ComPtr<ID3D11Device> device;context->GetDevice(&device);
        if(!device)return false;
        if(resources.device.Get()!=device.Get()){resources=Resources{};resources.device=device;}
        if(!resources.attempted&&!createResources(device.Get()))return false;
        if(!resources.ready)return false;
        if(!resources.idroidGuidance.ready||resources.idroidGuidanceWidth!=width||resources.idroidGuidanceAspect!=aspect){
            RetailGpuAsset surface;
            if(!studioQuad(device.Get(),surface,{width*.5f,width/(2*aspect),0},{0xffffffffu},1,1,0))return false;
            resources.idroidGuidance=std::move(surface);
            resources.idroidGuidanceWidth=width;resources.idroidGuidanceAspect=aspect;
        }
        // Never keep a preceding flight/frame's SRV after a missing capture.
        resources.idroidGuidance.diffuse.Reset();
        if(FAILED(device->CreateShaderResourceView(source,nullptr,&resources.idroidGuidance.diffuse)))return false;
        SavedState saved;save(context,saved);
        ComPtr<ID3D11DepthStencilView> depth;
        const bool ready=saved.renderTarget&&bindHousingDepth(context,device.Get(),saved,projection[14]>0,depth,&eye);
        const bool drawn=ready&&drawOpeningAsset(context,resources.idroidGuidance,saved.renderTarget.Get(),depth.Get(),world,view,projection,true);
        restore(context,saved);return drawn;
    }catch(...){return false;}
}
void stopPhysicalOpticRenderer() noexcept{setOpticNativeMaterialDrawCallback(nullptr);
    std::lock_guard lock(rendererMutex);resources=Resources{};}
}
