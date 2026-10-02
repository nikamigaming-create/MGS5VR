#include "mgs5vr/optic_native_lighting_probe.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/log.hpp"
#include <windows.h>
#include <intrin.h>
#include <bcrypt.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <MinHook.h>
#include <array>
#include <atomic>
#include <cstring>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace mgs5vr {
namespace {
using Microsoft::WRL::ComPtr;
constexpr GUID shaderKey{0x9d45d745,0x6cdb,0x4c10,{0xb1,0x4a,0xc3,0x26,0x06,0x10,0x9a,0xf3}};
constexpr GUID debugNameKey{0x429b8c22,0x9188,0x4b0c,{0x87,0x42,0xac,0xb0,0xbf,0x85,0xc2,0x00}};
constexpr uint32_t signature=0x4f4c5031u;
struct BufferContract {const char* name;UINT slot,bytes;};
constexpr std::array<BufferContract,6> buffers{{
    {"cPSSystem",0,64},{"cPSScene",2,480},{"cPSLight",3,176},
    {"cPSMaterial",4,128},{"cPSObject",5,208},{"PsWork",7,48}}};
struct ShaderContract {const char* name;UINT bytes;const char* sha256;uint32_t bufferMask,group;};
// Exact identities and reflection contracts from locally decoded owned FSOP
// records. No shader bytecode or texture content is included in the runtime.
constexpr std::array<ShaderContract,14> shaders{{
    {"SunLight_DL_CS",402509,"0183860375a6f3d407545838a3915a21bed8c21f8b13f3a2a7a4682bce557371",0x16,0},
    {"SunLight_DL_CS_ES",434750,"dc7a41fa561710fa87098e38201f6cef7288f1593a158373bac4289ff05a56aa",0x16,0},
    {"SunLight_DL",262736,"35b868a83447f081095874b18403678aac15e1677c8789bc31848c33825b3a89",0x06,0},
    {"SunLight_DL_ES",294825,"0b9fcfb13e7d76122444c076c9b42cc9f9d277b7d9e9926ff97b19944210645e",0x06,0},
    {"SunLight_ES",233813,"739719ae18183285f90326eeeca67b97c8019737fdc69d9e75360b9b0c6173f4",0x06,0},
    {"SSLighting2_SH_MultiBlend",182767,"6f084f49b3cc71ed74199b0f8530b38804e03ae14a69575d4506de51d383c005",0x3a,1},
    {"SSLighting2_SH_SkyLight_Encode",164887,"fce6a4032e8b3bb84aabbad8d9ba7b297bcdfc04ed892216f3a1e0f0fab2eeba",0x10,2},
    {"DeferredRenderingFilmic",155624,"a40e3ad156d33bf939024f96a6aa0d23b2f9e779572fd5e7b3a05543362c4d6a",0x13,3},
    {"Tonemap",104770,"0419c966fdbb1e40da35521b63c94365ada73bfa300e3d29e57003322477d3ac",0x01,4},
    {"Tonemap_1DLUT",108777,"60245bb30dc264dc610c4661063c1b3f809974e00399ff625c3ffad9d560645e",0x0b,4},
    {"ColorCorrection",83812,"f3101e1bff9c55f3f14432ca020b379c5dd4fd53406ae124add0dfe73931e40b",0x01,5},
    {"Thermography",152682,"ac9e23b59929f017b0c902d2a0cfed4ad426fe3e165c6fe5b306fdc1ed4f196b",0x11,6},
    {"CheckMarker",130309,"804e418e99ea820c4df99e144d62d814942c84e769bdefd7b1107a108fef7769",0x10,6},
    {"fox3ddf_blin_4mt",233604,"cabc964687cd259e0189b33968b090980aab7e12a9c603e8560536ae2a9e64fe",0x09,7}}};
struct StrippedIdentity {UINT bytes;const char* sha256;};
// Microsoft D3DStripShader(DEBUG_INFO) applied privately to each corresponding
// owned record. Only SDBG was removed; every RDEF/signature/SHEX/STAT byte was
// independently checked unchanged. The same reflection contract is required.
constexpr std::array<StrippedIdentity,14> stripped{{
    {14808,"6b830ec4fac72fe5d205402999f80a2437e1c64afc90c488eedae3933f080813"},
    {16300,"0f3d58cc7126634cbcbfe43605eb6a0b0d683e532081d695053dea066796b2f7"},
    {7140,"bdf257641b1a561ebe8d9d7b95c53fc08f1fe5b757aa9ba4012b18619ef64c8c"},
    {8632,"17738b2b016d3fd9084f238bd1e6df2c3264213bc2536aed8dc2cb93d1fd1812"},
    {5724,"ffc71bf7bec862d486b10f8286b130f0b87265148543661a9d78cc21391e22d0"},
    {5216,"8d2ecd7bcbddfc9c9895f65638fda52118b4f1be5aa237fd3beeee071f673c63"},
    {3448,"940021a4802f6b153e0043f66da2c869b8cbfbb3a0fe49b2866ae6a0852349ba"},
    {5184,"a55f290c62ece096ab1f0d3931a1b84a4bb0ba8742f4354880df8e7c4e0a642f"},
    {3156,"fd2b97bd0e1ba85396fe3dcf3ed81b56501f12f5ac6678d315c4efe800ba72b2"},
    {3984,"ba38cac3d93d27686a553811c01e2e5e387cf6350b1bc96c26aa39845cbe978a"},
    {2256,"1aa9051d38e2704372f0198a20fec057d0665959bd7a1004bdcfc2629ab164bf"},
    {4072,"ddeadb986f5f4a9f389dc93912f798e3c51425cd210c95e6130b6d08e3149f0c"},
    {2716,"713d625a67e35d478ba35bf6d8e9f7494f0d39e30b88f6505390af75268b206d"},
    {6124,"098a0aa7e4043a89027da859bf7b429cc7528917803ce01f3313e8722280065d"}}};
struct ShaderTag {uint32_t magic{},index{},variant{};};
using CreatePs=HRESULT(STDMETHODCALLTYPE*)(ID3D11Device*,const void*,SIZE_T,ID3D11ClassLinkage*,ID3D11PixelShader**);
using SetPs=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11PixelShader*,ID3D11ClassInstance* const*,UINT);
using MapBuffer=HRESULT(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Resource*,UINT,D3D11_MAP,UINT,D3D11_MAPPED_SUBRESOURCE*);
using UnmapBuffer=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Resource*,UINT);
using UpdateBuffer=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Resource*,UINT,const D3D11_BOX*,const void*,UINT,UINT);
using CopyBuffer=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Resource*,ID3D11Resource*);
using CopyBufferRegion=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Resource*,UINT,UINT,UINT,UINT,ID3D11Resource*,UINT,const D3D11_BOX*);
using SetTargets=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,ID3D11RenderTargetView* const*,ID3D11DepthStencilView*);
using SetTargetsUavs=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,ID3D11RenderTargetView* const*,ID3D11DepthStencilView*,UINT,UINT,ID3D11UnorderedAccessView* const*,const UINT*);
using DrawIndirect=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Buffer*,UINT);
using NativeProgramFlush=void(__fastcall*)(uintptr_t,uintptr_t);
NativeProgramFlush originalNativeProgramFlush{};
CreatePs originalCreate{};
SetPs originalImmediateSet{},originalDeferredSet{};
MapBuffer originalImmediateMap{},originalDeferredMap{};
UnmapBuffer originalImmediateUnmap{},originalDeferredUnmap{};
UpdateBuffer originalImmediateUpdate{},originalDeferredUpdate{};
CopyBuffer originalImmediateCopy{},originalDeferredCopy{};
CopyBufferRegion originalImmediateCopyRegion{},originalDeferredCopyRegion{};
SetTargets originalImmediateTargets{},originalDeferredTargets{};
SetTargetsUavs originalImmediateTargetsUavs{},originalDeferredTargetsUavs{};
DrawIndirect originalImmediateIndexedIndirect{},originalDeferredIndexedIndirect{};
DrawIndirect originalImmediateIndirect{},originalDeferredIndirect{};
std::atomic_bool enabled{};
std::atomic_bool nativeMaterialEnabled{},diagnosticsEnabled{};
std::atomic_uint64_t generation{1};
std::atomic_uint64_t scopesSeen{},scopesEligible{},knownBindings{};
std::atomic_uint64_t nativeFlushScopes{};
std::atomic<OpticNativeMaterialDrawCallback> materialDrawCallback{};
std::mutex mutex;
bool installed{};
struct Bound {ID3D11DeviceContext* context{};ID3D11PixelShader* shader{};ShaderTag tag{};uintptr_t caller{};};
thread_local Bound bound;
thread_local OpticNativeLightingSource source;
struct NativeFlushBinding {uintptr_t state{},contextOwner{};OpticNativeLightingSource source{};};
thread_local NativeFlushBinding nativeFlushBinding;
struct OpaqueFamily {const char* name;UINT bytes;const char* sha256;};
// Unique full VS identities in the owned bank, independently reflected to
// the same ordinary three-output material family and system/dither inputs.
// Stripped VS programs can be shared by unrelated materials and are unknown.
constexpr std::array<OpaqueFamily,3> opaqueFamilies{{
    {"fox3ddf_blin_4mt",175918,"846cc18d27f3b5a5b1a0cae2ffaceff806a21f65f4a8090d5e7237dcf8632696"},
    {"fox3ddf_blin_3mt",176157,"8100da94a35b92d33d3526342b9709e0b3df637078958e85b2cc6ad99f36a641"},
    {"fox3ddf_blin_2mt",176157,"39bbeee940bd1de83a3fe13e0d397fd7f10b2aa011c9ff7200256ea9510c9bd8"}}};
struct FamilyCache {ComPtr<ID3D11VertexShader> shader;uint32_t family{};};
std::array<FamilyCache,16> familyCache;
std::array<FamilyCache,16> negativeFamilyCache;
size_t familyCacheCursor{},negativeFamilyCacheCursor{};
uint64_t nextFamilyHashWindow{};UINT familyHashAttempts{};
std::array<bool,3> reportedFamilies{};
std::array<uintptr_t,8> unknownVertexPrograms{};
struct TargetCount {ID3D11DeviceContext* context{};UINT colors{};bool depth{};};
thread_local TargetCount targetCount;
// Draw observers may see only post/lighting work when native geometry is
// recorded elsewhere. Count actual OM binds independently, without assigning
// an absent head transaction to another thread or admitting a material draw.
std::array<std::atomic_uint64_t,9> headTargetBindCounts,otherTargetBindCounts;
std::atomic_uint32_t threeTargetBindReports{};
struct SceneBuffer {
    ComPtr<ID3D11Buffer> buffer;
    ComPtr<ID3D11Resource> resource;
    uintptr_t context{};
    uint32_t byteWidth{};
    uint64_t generation{};
    uint64_t writeSerial{};
    OpticNativeLightingSource uploadSource{};
    bool complete{};
    std::array<unsigned char,65536> bytes{};
};
std::array<SceneBuffer,4> sceneBuffers;
std::array<std::atomic_uintptr_t,4> sceneResourceIds;
std::array<std::atomic_uint64_t,4> sceneWriteSerials;
struct MappedScene {
    ID3D11Resource* resource{};ID3D11DeviceContext* context{};
    const void* data{};uint32_t byteWidth{};uint64_t generation{},writeSerial{};
    OpticNativeLightingSource source{};
};
thread_local std::array<MappedScene,4> mappedScenes;
thread_local uint64_t geometrySequence{},geometryActivation{},geometryTracking{};
thread_local uint32_t geometryEye{},geometryAttempts{};
std::array<uintptr_t,4> geometryShaders;
uint64_t geometryDraws{},nativeGpuPairs{},nativePairRejects{},opaqueFamilyBindings{},sceneUploads{},sceneUploadRejects{},geometryReady{},materialInsertions{};
uint64_t headDrawObservations{},targetContextMatches{},threeMrtTargetMatches{},nativeBindMatches{};
std::array<uint64_t,3> drawKindObservations{},drawKindThreeMrtMatches{};
bool nativeGeometryBindLayout{};
struct Snapshot {
    ComPtr<ID3D11Buffer> staging;
    UINT byteWidth{},offset{},bytes{};
    uintptr_t native{};
    std::array<float,120> data{};
};
struct Texture {
    uintptr_t resource{};
    UINT slot{},dimension{},format{},width{},height{},depth{},array{},mips{};
};
struct Record {
    bool issued{},ready{},accepted{},cameraVerified{},transpose{};
    uint64_t generation{},queuedAt{};
    UINT shaderIndex{},identityVariant{},contextType{};
    uintptr_t context{},pixelShader{};
    OpticNativeLightingSource source;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11Query> completion;
    std::array<Snapshot,6> snapshots;
    std::array<Texture,16> textures;
    std::array<UINT,2> renderFormats{};
    D3D11_VIEWPORT viewport{};
    float viewError{},projectionError{};
};
constexpr size_t maximumPending=24;
std::array<Record,maximumPending> pending;
std::array<std::array<EyeFrame,2>,64> accepted;
size_t acceptedCursor{};
uint64_t selectedSequence{},selectedActivation{},selectedTracking{},nextSelection{};
uint32_t selectedMask{},reportCount{},tagCount{},dropCount{};
unsigned unknownShaderCount{};
std::array<std::array<uintptr_t,4>,2> unknownBindings;
uint64_t nextUnknownBinding{};
uint64_t unknownAttemptWindow{};
unsigned unknownAttemptCount{};
uintptr_t nativeBase{};
bool nativeProgramLayout{};
uint64_t nextStatus{},captureAttempts{},queuedRecords{},queueFailures{},acceptedPairs{};

std::string digest(const void* bytes,UINT count){
    struct Provider {BCRYPT_ALG_HANDLE handle{};~Provider(){if(handle)BCryptCloseAlgorithmProvider(handle,0);}} provider;
    struct Hash {BCRYPT_HASH_HANDLE handle{};~Hash(){if(handle)BCryptDestroyHash(handle);}} hash;
    if(BCryptOpenAlgorithmProvider(&provider.handle,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0
        ||BCryptCreateHash(provider.handle,&hash.handle,nullptr,0,nullptr,0,0)<0
        ||BCryptHashData(hash.handle,reinterpret_cast<PUCHAR>(const_cast<void*>(bytes)),count,0)<0)return {};
    std::array<UCHAR,32> value{};
    if(BCryptFinishHash(hash.handle,value.data(),static_cast<ULONG>(value.size()),0)<0)return {};
    std::ostringstream result;result<<std::hex<<std::setfill('0');
    for(auto byte:value)result<<std::setw(2)<<static_cast<unsigned>(byte);
    return result.str();
}
bool reflectionMatches(const void* bytes,SIZE_T count,const ShaderContract& contract){
    ComPtr<ID3D11ShaderReflection> reflection;
    if(FAILED(D3DReflect(bytes,count,IID_PPV_ARGS(&reflection))))return false;
    D3D11_SHADER_DESC shader{};
    if(FAILED(reflection->GetDesc(&shader))||D3D11_SHVER_GET_TYPE(shader.Version)!=D3D11_SHVER_PIXEL_SHADER)return false;
    for(size_t i=0;i<buffers.size();++i){
        if(!(contract.bufferMask&(1u<<i)))continue;
        const auto& expected=buffers[i];D3D11_SHADER_INPUT_BIND_DESC binding{};D3D11_SHADER_BUFFER_DESC buffer{};
        auto* block=reflection->GetConstantBufferByName(expected.name);
        if(FAILED(reflection->GetResourceBindingDescByName(expected.name,&binding))
            ||binding.Type!=D3D_SIT_CBUFFER||binding.BindPoint!=expected.slot||binding.BindCount!=1
            ||!block||FAILED(block->GetDesc(&buffer))||buffer.Size!=expected.bytes)return false;
    }
    return true;
}
ShaderTag tagOf(ID3D11PixelShader* shader) noexcept {
    ShaderTag result{};UINT bytes=sizeof(result);
    if(!shader||FAILED(shader->GetPrivateData(shaderKey,&bytes,&result))||bytes!=sizeof(result)
        ||result.magic!=signature||result.index>=shaders.size()||result.variant>1)return {};
    return result;
}
HRESULT STDMETHODCALLTYPE createPs(ID3D11Device* device,const void* bytes,SIZE_T size,
    ID3D11ClassLinkage* linkage,ID3D11PixelShader** output){
    const auto result=originalCreate(device,bytes,size,linkage,output);
    if(!diagnosticsEnabled.load()||FAILED(result)||!output||!*output)return result;
    try{
        bool verified=false;
        for(size_t i=0;i<shaders.size();++i){const auto& contract=shaders[i];
            const bool debugStripped=size==stripped[i].bytes;
            if(size!=contract.bytes&&!debugStripped)continue;
            if(!bytes||size<32||std::memcmp(bytes,"DXBC",4))break;
            uint32_t total{};std::memcpy(&total,static_cast<const std::byte*>(bytes)+24,sizeof(total));
            const auto* identity=debugStripped?stripped[i].sha256:contract.sha256;
            if(total!=size||digest(bytes,static_cast<UINT>(size))!=identity||!reflectionMatches(bytes,size,contract))continue;
            const ShaderTag tag{signature,static_cast<uint32_t>(i),debugStripped?1u:0u};
            if(SUCCEEDED((*output)->SetPrivateData(shaderKey,sizeof(tag),&tag))){
                verified=true;
                std::lock_guard lock(mutex);
                if(tagCount++<32)log(std::string("Optic native lighting shader verified name=")+contract.name
                    +" sha256="+identity+" identity_variant="+(debugStripped?"debug_stripped":"full")
                    +" byte_count="+std::to_string(size)+" reflection_verified=1 read_only=1");
            }
            break;
        }
        if(!verified&&bytes&&size>=512&&size<=512*1024&&!std::memcmp(bytes,"DXBC",4)){
            uint32_t total{};std::memcpy(&total,static_cast<const std::byte*>(bytes)+24,sizeof(total));
            bool admitted=false;
            if(total==size){std::lock_guard lock(mutex);if(unknownShaderCount<8){++unknownShaderCount;admitted=true;}}
            if(admitted){
                // Bounded identity evidence only. Unknown shaders are never
                // tagged or assigned a native material/light owner by shape.
                ComPtr<ID3D11ShaderReflection> reflection;D3D11_SHADER_DESC description{};
                const bool reflected=SUCCEEDED(D3DReflect(bytes,size,IID_PPV_ARGS(&reflection)))
                    &&SUCCEEDED(reflection->GetDesc(&description));
                std::ostringstream line;line<<"Optic native lighting unknown PS byte_count="<<size
                    <<" sha256="<<digest(bytes,static_cast<UINT>(size))<<" reflected="<<reflected
                    <<" constant_buffers="<<description.ConstantBuffers<<" bound_resources="<<description.BoundResources
                    <<" tagged=0 owner_verified=0 read_only=1";log(line.str());
            }
        }
    }catch(...){}
    return result;
}
void setBound(ID3D11DeviceContext* context,ID3D11PixelShader* shader,uintptr_t caller) noexcept {
    if(nativeBinocularMaterialDrawing())return;
    if(enabled.load()){bound={context,shader,diagnosticsEnabled.load()?tagOf(shader):ShaderTag{},caller};if(bound.tag.magic==signature)++knownBindings;}
}
void STDMETHODCALLTYPE immediateSet(ID3D11DeviceContext* context,ID3D11PixelShader* shader,
    ID3D11ClassInstance* const* instances,UINT count){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    originalImmediateSet(context,shader,instances,count);setBound(context,shader,caller);
}
void STDMETHODCALLTYPE deferredSet(ID3D11DeviceContext* context,ID3D11PixelShader* shader,
    ID3D11ClassInstance* const* instances,UINT count){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    originalDeferredSet(context,shader,instances,count);setBound(context,shader,caller);
}
template<class T> bool readNative(uintptr_t address,T& value) noexcept {
    SIZE_T bytes{};
    return address&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),
        &value,sizeof(value),&bytes)&&bytes==sizeof(value);
}
bool sameSource(const OpticNativeLightingSource& a,const OpticNativeLightingSource& b) noexcept {
    return opticLightingSourceEligible(a)&&opticLightingSourceEligible(b)
        &&opticLightingSameEye(a.eye,b.eye)&&a.nativeCamera==b.nativeCamera
        &&opticLightingMatrixError(a.view,b.view)==0&&opticLightingMatrixError(a.projection,b.projection)==0
        &&a.binocularWorldValid==b.binocularWorldValid
        &&(!a.binocularWorldValid||opticLightingMatrixError(a.binocularWorld,b.binocularWorld)==0);
}
bool freshSource(const OpticNativeLightingSource& value) noexcept {
    const auto now=steadyMilliseconds();return opticLightingSourceEligible(value)
        &&now>=value.eye.sampleTime&&now-value.eye.sampleTime<=150;
}
void __fastcall nativeProgramFlush(uintptr_t state,uintptr_t contextOwner){
    nativeFlushBinding.state=nativeFlushBinding.contextOwner=0;
    originalNativeProgramFlush(state,contextOwner);
    if(enabled.load()&&!nativeBinocularMaterialDrawing()&&opticLightingHousingSourceEligible(source)){
        nativeFlushBinding={state,contextOwner,source};++nativeFlushScopes;}
}
bool registeredSceneResource(ID3D11Resource* resource) noexcept {
    if(!enabled.load()||!resource)return false;
    const auto id=reinterpret_cast<uintptr_t>(resource);
    for(const auto& known:sceneResourceIds)if(known.load()==id)return true;
    return false;
}
struct NativeShaderObject {uint32_t references{},handle{};uintptr_t shader{};};
struct NativeVertexObject {NativeShaderObject object;uintptr_t bytes{};UINT byteCount{},padding{};};
struct NativeShaderTriplet {uint32_t references{},handle{};uintptr_t layout{},vertex{},pixel{};};
struct NativeProgramState {uintptr_t vertex{},pixel{},triplet{};};
static_assert(sizeof(NativeShaderObject)==16&&sizeof(NativeVertexObject)==32&&sizeof(NativeShaderTriplet)==32);
bool liveNativeKind(uint32_t refs,uint32_t handle,uint32_t kind) noexcept {
    return refs>0&&refs<=static_cast<uint32_t>(INT32_MAX)&&(handle>>26)==kind;
}
uint32_t vertexFamily(const NativeVertexObject& value,ID3D11VertexShader* shader){
    for(const auto& cached:familyCache)if(cached.shader.Get()==shader)return cached.family;
    for(const auto& cached:negativeFamilyCache)if(cached.shader.Get()==shader)return 0;
    const bool eligibleSize=std::any_of(opaqueFamilies.begin(),opaqueFamilies.end(),[&](const auto& f){return f.bytes==value.byteCount;});
    const auto id=reinterpret_cast<uintptr_t>(shader);
    auto unknown=std::find(unknownVertexPrograms.begin(),unknownVertexPrograms.end(),id);
    const auto unknownEmpty=std::find(unknownVertexPrograms.begin(),unknownVertexPrograms.end(),uintptr_t{});
    const bool reportUnknown=diagnosticsEnabled.load()&&unknown==unknownVertexPrograms.end()&&unknownEmpty!=unknownVertexPrograms.end();
    if(!eligibleSize&&!reportUnknown)return 0;
    if(!value.bytes||value.byteCount<512||value.byteCount>524288)return 0;
    const auto now=steadyMilliseconds();
    if(now>=nextFamilyHashWindow){nextFamilyHashWindow=now+500;familyHashAttempts=0;}
    if(familyHashAttempts>=8)return 0;++familyHashAttempts;
    std::vector<unsigned char> bytes(value.byteCount);SIZE_T copied{};
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(value.bytes),bytes.data(),bytes.size(),&copied)
        ||copied!=bytes.size()||std::memcmp(bytes.data(),"DXBC",4)!=0)return 0;
    uint32_t total{};std::memcpy(&total,bytes.data()+24,4);if(total!=bytes.size())return 0;
    const auto sha=digest(bytes.data(),static_cast<UINT>(bytes.size()));uint32_t family{};
    for(size_t i=0;i<opaqueFamilies.size();++i)if(value.byteCount==opaqueFamilies[i].bytes&&sha==opaqueFamilies[i].sha256){
        ComPtr<ID3D11ShaderReflection> reflection;D3D11_SHADER_DESC description{};
        D3D11_SHADER_INPUT_BIND_DESC binding{};D3D11_SHADER_BUFFER_DESC block{};
        D3D11_SHADER_VARIABLE_DESC sceneVariable{};D3D11_SHADER_TYPE_DESC view{},projection{};
        if(SUCCEEDED(D3DReflect(bytes.data(),bytes.size(),IID_PPV_ARGS(&reflection)))
            &&SUCCEEDED(reflection->GetDesc(&description))&&D3D11_SHVER_GET_TYPE(description.Version)==D3D11_SHVER_VERTEX_SHADER
            &&SUCCEEDED(reflection->GetResourceBindingDescByName("cVSScene",&binding))
            &&binding.Type==D3D_SIT_CBUFFER&&binding.BindPoint==2&&binding.BindCount==1
            &&SUCCEEDED(reflection->GetConstantBufferByName("cVSScene")->GetDesc(&block))&&block.Size==480
            &&SUCCEEDED(reflection->GetConstantBufferByName("cVSScene")->GetVariableByName("g_vsScene")->GetDesc(&sceneVariable))
            &&sceneVariable.StartOffset==0&&sceneVariable.Size==480
            &&SUCCEEDED(reflection->GetConstantBufferByName("cVSScene")->GetVariableByName("g_vsScene")->GetType()->GetMemberTypeByName("m_view")->GetDesc(&view))
            &&SUCCEEDED(reflection->GetConstantBufferByName("cVSScene")->GetVariableByName("g_vsScene")->GetType()->GetMemberTypeByName("m_projection")->GetDesc(&projection))
            &&view.Offset==128&&projection.Offset==64){
            // The full owned hash also fixes the nested matrix member offsets;
            // reflection validates the typed scene block independently.
            family=static_cast<uint32_t>(i)+1;
        }
        break;
    }
    if(family){auto& cache=familyCache[familyCacheCursor++%familyCache.size()];cache.shader=shader;cache.family=family;}
    else if(eligibleSize){auto& cache=negativeFamilyCache[negativeFamilyCacheCursor++%negativeFamilyCache.size()];cache.shader=shader;cache.family=0;}
    if(diagnosticsEnabled.load()&&((family&&!reportedFamilies[family-1])||(!family&&reportUnknown))){
        if(family)reportedFamilies[family-1]=true;else *unknownEmpty=id;
        std::ostringstream line;line<<"Optic native retained VS family native_vs=0x"<<std::hex<<id<<std::dec
            <<" byte_count="<<value.byteCount<<" sha256="<<sha<<" family_verified="<<(family!=0)
            <<" family="<<(family?opaqueFamilies[family-1].name:"unknown")<<" native_pair_verified=1 read_only=1";log(line.str());}
    return family;
}
std::pair<bool,uint32_t> nativeProgramPair(ID3D11DeviceContext* context,ID3D11VertexShader* vertex,ID3D11PixelShader* pixel){
    if(!nativeFlushBinding.state||!nativeFlushBinding.contextOwner||!sameSource(nativeFlushBinding.source,source))return {};
    uintptr_t nativeContext{};NativeProgramState state{};NativeVertexObject vs{};NativeShaderObject ps{};NativeShaderTriplet triplet{};
    if(!readNative(nativeFlushBinding.contextOwner+8,nativeContext)||nativeContext!=reinterpret_cast<uintptr_t>(context)
        ||!readNative(nativeFlushBinding.state+0x318,state)||!state.vertex||!state.pixel||!state.triplet
        ||!readNative(state.vertex,vs)||!readNative(state.pixel,ps)||!readNative(state.triplet,triplet)
        ||!liveNativeKind(vs.object.references,vs.object.handle,7)||!liveNativeKind(ps.references,ps.handle,8)
        ||!liveNativeKind(triplet.references,triplet.handle,14)||!triplet.layout
        ||vs.object.shader!=reinterpret_cast<uintptr_t>(vertex)||ps.shader!=reinterpret_cast<uintptr_t>(pixel)
        ||triplet.vertex!=vs.object.shader||triplet.pixel!=ps.shader)return {};
    const auto family=vertexFamily(vs,vertex);
    uintptr_t contextAgain{};NativeProgramState stateAgain{};NativeVertexObject vsAgain{};NativeShaderObject psAgain{};NativeShaderTriplet tripletAgain{};
    if(!readNative(nativeFlushBinding.contextOwner+8,contextAgain)||contextAgain!=nativeContext
        ||!readNative(nativeFlushBinding.state+0x318,stateAgain)||std::memcmp(&state,&stateAgain,sizeof(state))
        ||!readNative(state.vertex,vsAgain)||std::memcmp(&vs,&vsAgain,sizeof(vs))
        ||!readNative(state.pixel,psAgain)||std::memcmp(&ps,&psAgain,sizeof(ps))
        ||!readNative(state.triplet,tripletAgain)||std::memcmp(&triplet,&tripletAgain,sizeof(triplet)))return {};
    return {true,family};
}
void stampSceneWrite(ID3D11Resource* resource) noexcept {
    if(!enabled.load()||!resource)return;
    const auto id=reinterpret_cast<uintptr_t>(resource);
    for(size_t i=0;i<sceneResourceIds.size();++i)if(sceneResourceIds[i].load()==id)++sceneWriteSerials[i];
}
SceneBuffer* knownSceneBuffer(ID3D11DeviceContext* context,ID3D11Resource* resource) noexcept {
    for(auto& value:sceneBuffers)if(value.resource.Get()==resource&&value.context==reinterpret_cast<uintptr_t>(context)
        &&value.generation==generation.load())return &value;
    return nullptr;
}
void invalidateSceneWrite(ID3D11DeviceContext* context,ID3D11Resource* resource) noexcept {
    if(!registeredSceneResource(resource))return;
    std::unique_lock lock(mutex,std::try_to_lock);if(!lock)return;
    if(auto* known=knownSceneBuffer(context,resource))known->complete=false;
}
void mappedScene(ID3D11DeviceContext* context,ID3D11Resource* resource,UINT subresource,
    D3D11_MAP type,UINT flags,const D3D11_MAPPED_SUBRESOURCE* mapped) noexcept {
    if(subresource||!mapped||!registeredSceneResource(resource))return;
    try{
        std::unique_lock lock(mutex,std::try_to_lock);if(!lock)return;
        auto* known=knownSceneBuffer(context,resource);if(!known)return;
        known->complete=false; // A later unknown write cannot reuse prior bytes.
        if(!mapped->pData||!opticLightingWritableMap(static_cast<uint32_t>(type),flags)
            ||!freshSource(source)||nativeBinocularMaterialDrawing()){++sceneUploadRejects;return;}
        auto entry=std::find_if(mappedScenes.begin(),mappedScenes.end(),[&](const auto& value){
            return !value.resource||(value.resource==resource&&value.context==context);});
        if(entry==mappedScenes.end()){++sceneUploadRejects;return;}
        *entry={resource,context,mapped->pData,known->byteWidth,known->generation,
            sceneWriteSerials[static_cast<size_t>(known-sceneBuffers.data())].load(),source};
    }catch(...){}
}
void unmappingScene(ID3D11DeviceContext* context,ID3D11Resource* resource,UINT subresource) noexcept {
    if(!enabled.load()||!resource||subresource)return;
    try{
        auto found=std::find_if(mappedScenes.begin(),mappedScenes.end(),[&](const auto& value){
            return value.resource==resource&&value.context==context;});
        if(found==mappedScenes.end())return;
        const auto mapped=*found;*found={};
        std::unique_lock lock(mutex,std::try_to_lock);if(!lock)return;
        auto* known=knownSceneBuffer(context,resource);if(!known)return;
        known->complete=false;
        const auto index=static_cast<size_t>(known-sceneBuffers.data());
        if(mapped.generation!=generation.load()||known->byteWidth!=mapped.byteWidth
            ||mapped.writeSerial!=sceneWriteSerials[index].load()
            ||!freshSource(source)||!sameSource(mapped.source,source)){++sceneUploadRejects;return;}
        SIZE_T count{};
        if(!ReadProcessMemory(GetCurrentProcess(),mapped.data,known->bytes.data(),known->byteWidth,&count)
            ||count!=known->byteWidth){++sceneUploadRejects;return;}
        if(mapped.writeSerial!=sceneWriteSerials[index].load()){++sceneUploadRejects;return;}
        known->uploadSource=mapped.source;known->writeSerial=mapped.writeSerial;known->complete=true;++sceneUploads;
    }catch(...){}
}
struct SceneUpdate {
    OpticNativeLightingSource source{};ID3D11Resource* resource{};
    uint64_t generation{},writeSerial{};std::vector<unsigned char> bytes;
};
std::optional<SceneUpdate> prepareSceneUpdate(ID3D11DeviceContext* context,ID3D11Resource* resource,
    UINT subresource,const D3D11_BOX* box,const void* data) noexcept {
    if(subresource||!registeredSceneResource(resource))return {};
    try{
        std::unique_lock lock(mutex,std::try_to_lock);if(!lock)return {};
        auto* known=knownSceneBuffer(context,resource);if(!known)return {};
        known->complete=false;
        if(box||!data||!freshSource(source)||nativeBinocularMaterialDrawing()){++sceneUploadRejects;return {};}
        SceneUpdate upload;upload.source=source;upload.resource=resource;upload.generation=generation.load();
        upload.writeSerial=sceneWriteSerials[static_cast<size_t>(known-sceneBuffers.data())].load();
        upload.bytes.resize(known->byteWidth);SIZE_T count{};
        if(!ReadProcessMemory(GetCurrentProcess(),data,upload.bytes.data(),upload.bytes.size(),&count)
            ||count!=upload.bytes.size()){++sceneUploadRejects;return {};}
        return upload;
    }catch(...){return {};}
}
void finishSceneUpdate(ID3D11DeviceContext* context,const std::optional<SceneUpdate>& upload) noexcept {
    if(!upload)return;
    try{
        std::unique_lock lock(mutex,std::try_to_lock);if(!lock)return;
        auto* known=knownSceneBuffer(context,upload->resource);if(!known)return;
        if(upload->generation!=generation.load()||upload->bytes.size()!=known->byteWidth
            ||upload->writeSerial!=sceneWriteSerials[static_cast<size_t>(known-sceneBuffers.data())].load()
            ||!freshSource(source)||!sameSource(upload->source,source)){++sceneUploadRejects;return;}
        std::copy(upload->bytes.begin(),upload->bytes.end(),known->bytes.begin());
        known->uploadSource=upload->source;known->writeSerial=upload->writeSerial;known->complete=true;++sceneUploads;
    }catch(...){}
}
bool sceneUploadMatches(const OpticNativeGeometryPass& pass,const OpticNativeLightingSource& current,
    ID3D11DeviceContext* context) noexcept {
    if(!freshSource(current)||!sameSource(source,current)||pass.generation!=generation.load())return false;
    for(const auto& known:sceneBuffers){if(known.buffer.Get()!=reinterpret_cast<ID3D11Buffer*>(pass.sceneBuffer)
        ||known.context!=reinterpret_cast<uintptr_t>(context)||!known.complete||known.generation!=pass.generation
        ||known.writeSerial!=sceneWriteSerials[static_cast<size_t>(&known-sceneBuffers.data())].load())continue;
        if(!opticLightingSceneRangeEligible(known.byteWidth,pass.firstConstant,pass.constantCount))return false;
        OpticLightingSceneUpload upload;upload.source=known.uploadSource;
        upload.context=known.context;upload.buffer=pass.sceneBuffer;upload.generation=known.generation;
        upload.byteWidth=known.byteWidth;upload.firstConstant=pass.firstConstant;upload.constantCount=pass.constantCount;
        upload.complete=true;const auto at=static_cast<size_t>(pass.firstConstant)*16;
        std::memcpy(upload.projection.data(),known.bytes.data()+at+64,64);
        std::memcpy(upload.view.data(),known.bytes.data()+at+128,64);
        return opticLightingSceneUploadMatches(upload,current,reinterpret_cast<uintptr_t>(context),
            pass.sceneBuffer,pass.generation,pass.firstConstant,pass.constantCount);
    }
    return false;
}
void countTargets(ID3D11DeviceContext* context,UINT count,ID3D11RenderTargetView* const* targets,
    ID3D11DepthStencilView* depth) noexcept {
    if(!enabled.load()||count==D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL)return;
    UINT colors{};if(targets&&count<=8)for(UINT i=0;i<count;++i)if(targets[i])++colors;
    targetCount={context,colors,depth!=nullptr};
    if(diagnosticsEnabled.load()&&count<=8){
        const bool head=source.nativeCamera!=0;
        (head?headTargetBindCounts:otherTargetBindCounts)[colors].fetch_add(1);
        if(colors==3&&depth&&context){
            const auto type=context->GetType();
            const uint32_t bit=1u<<((head?2u:0u)+(type==D3D11_DEVICE_CONTEXT_DEFERRED?1u:0u));
            if(!(threeTargetBindReports.fetch_or(bit)&bit))try{
                std::ostringstream line;line<<"Optic native three-MRT target bind head_source_present="<<head
                    <<" source="<<source.eye.sourceSequence<<" tracking="<<source.eye.trackingSequence
                    <<" activation="<<source.eye.activation<<" eye="<<source.eye.eye
                    <<" binocular_world_valid="<<source.binocularWorldValid
                    <<" context=0x"<<std::hex<<reinterpret_cast<uintptr_t>(context)<<std::dec
                    <<" context_type="<<static_cast<UINT>(type)<<" thread="<<GetCurrentThreadId()
                    <<" depth_bound=1 material_admitted=0 read_only=1";log(line.str());
            }catch(...){}
        }
    }
}
HRESULT STDMETHODCALLTYPE immediateMap(ID3D11DeviceContext* c,ID3D11Resource* r,UINT s,D3D11_MAP t,UINT f,D3D11_MAPPED_SUBRESOURCE* m){
    if(t!=D3D11_MAP_READ)stampSceneWrite(r);const auto result=originalImmediateMap(c,r,s,t,f,m);if(SUCCEEDED(result))mappedScene(c,r,s,t,f,m);return result;}
HRESULT STDMETHODCALLTYPE deferredMap(ID3D11DeviceContext* c,ID3D11Resource* r,UINT s,D3D11_MAP t,UINT f,D3D11_MAPPED_SUBRESOURCE* m){
    if(t!=D3D11_MAP_READ)stampSceneWrite(r);const auto result=originalDeferredMap(c,r,s,t,f,m);if(SUCCEEDED(result))mappedScene(c,r,s,t,f,m);return result;}
void STDMETHODCALLTYPE immediateUnmap(ID3D11DeviceContext* c,ID3D11Resource* r,UINT s){unmappingScene(c,r,s);originalImmediateUnmap(c,r,s);}
void STDMETHODCALLTYPE deferredUnmap(ID3D11DeviceContext* c,ID3D11Resource* r,UINT s){unmappingScene(c,r,s);originalDeferredUnmap(c,r,s);}
void STDMETHODCALLTYPE immediateUpdate(ID3D11DeviceContext* c,ID3D11Resource* r,UINT s,const D3D11_BOX* b,const void* d,UINT row,UINT depth){
    stampSceneWrite(r);const auto upload=prepareSceneUpdate(c,r,s,b,d);originalImmediateUpdate(c,r,s,b,d,row,depth);finishSceneUpdate(c,upload);}
void STDMETHODCALLTYPE deferredUpdate(ID3D11DeviceContext* c,ID3D11Resource* r,UINT s,const D3D11_BOX* b,const void* d,UINT row,UINT depth){
    stampSceneWrite(r);const auto upload=prepareSceneUpdate(c,r,s,b,d);originalDeferredUpdate(c,r,s,b,d,row,depth);finishSceneUpdate(c,upload);}
void STDMETHODCALLTYPE immediateCopy(ID3D11DeviceContext* c,ID3D11Resource* d,ID3D11Resource* s){stampSceneWrite(d);invalidateSceneWrite(c,d);originalImmediateCopy(c,d,s);}
void STDMETHODCALLTYPE deferredCopy(ID3D11DeviceContext* c,ID3D11Resource* d,ID3D11Resource* s){stampSceneWrite(d);invalidateSceneWrite(c,d);originalDeferredCopy(c,d,s);}
void STDMETHODCALLTYPE immediateCopyRegion(ID3D11DeviceContext* c,ID3D11Resource* d,UINT ds,UINT x,UINT y,UINT z,ID3D11Resource* s,UINT ss,const D3D11_BOX* b){
    stampSceneWrite(d);invalidateSceneWrite(c,d);originalImmediateCopyRegion(c,d,ds,x,y,z,s,ss,b);}
void STDMETHODCALLTYPE deferredCopyRegion(ID3D11DeviceContext* c,ID3D11Resource* d,UINT ds,UINT x,UINT y,UINT z,ID3D11Resource* s,UINT ss,const D3D11_BOX* b){
    stampSceneWrite(d);invalidateSceneWrite(c,d);originalDeferredCopyRegion(c,d,ds,x,y,z,s,ss,b);}
void STDMETHODCALLTYPE immediateTargets(ID3D11DeviceContext* c,UINT n,ID3D11RenderTargetView* const* r,ID3D11DepthStencilView* d){
    originalImmediateTargets(c,n,r,d);countTargets(c,n,r,d);}
void STDMETHODCALLTYPE deferredTargets(ID3D11DeviceContext* c,UINT n,ID3D11RenderTargetView* const* r,ID3D11DepthStencilView* d){
    originalDeferredTargets(c,n,r,d);countTargets(c,n,r,d);}
void STDMETHODCALLTYPE immediateTargetsUavs(ID3D11DeviceContext* c,UINT n,ID3D11RenderTargetView* const* r,ID3D11DepthStencilView* d,UINT start,UINT count,ID3D11UnorderedAccessView* const* u,const UINT* initial){
    originalImmediateTargetsUavs(c,n,r,d,start,count,u,initial);countTargets(c,n,r,d);}
void STDMETHODCALLTYPE deferredTargetsUavs(ID3D11DeviceContext* c,UINT n,ID3D11RenderTargetView* const* r,ID3D11DepthStencilView* d,UINT start,UINT count,ID3D11UnorderedAccessView* const* u,const UINT* initial){
    originalDeferredTargetsUavs(c,n,r,d,start,count,u,initial);countTargets(c,n,r,d);}
// Indirect draw arguments remain GPU-owned. Observe the exact bound native
// pass, then forward the original buffer and byte offset without reading,
// modifying, replaying or waiting for its contents.
void STDMETHODCALLTYPE immediateIndexedIndirect(ID3D11DeviceContext* c,ID3D11Buffer* args,UINT offset){
    observeOpticNativeLightingDraw(c,OpticNativeLightingDrawKind::indexedIndirect);originalImmediateIndexedIndirect(c,args,offset);}
void STDMETHODCALLTYPE deferredIndexedIndirect(ID3D11DeviceContext* c,ID3D11Buffer* args,UINT offset){
    observeOpticNativeLightingDraw(c,OpticNativeLightingDrawKind::indexedIndirect);originalDeferredIndexedIndirect(c,args,offset);}
void STDMETHODCALLTYPE immediateIndirect(ID3D11DeviceContext* c,ID3D11Buffer* args,UINT offset){
    observeOpticNativeLightingDraw(c,OpticNativeLightingDrawKind::indirect);originalImmediateIndirect(c,args,offset);}
void STDMETHODCALLTYPE deferredIndirect(ID3D11DeviceContext* c,ID3D11Buffer* args,UINT offset){
    observeOpticNativeLightingDraw(c,OpticNativeLightingDrawKind::indirect);originalDeferredIndirect(c,args,offset);}
std::optional<OpticNativeGeometryPass> inspectGeometryPass(ID3D11DeviceContext* context) {
    if(diagnosticsEnabled.load()){
        ++headDrawObservations;
        if(targetCount.context==context)++targetContextMatches;
        if(targetCount.context==context&&targetCount.colors==3&&targetCount.depth)++threeMrtTargetMatches;
        if(bound.context==context&&bound.caller==nativeBase+0x248953u)++nativeBindMatches;
    }
    if(targetCount.context!=context||targetCount.colors!=3||!targetCount.depth
        ||!opticLightingHousingSourceEligible(source)||!nativeGeometryBindLayout
        ||bound.context!=context||bound.caller!=nativeBase+0x248953u)return {};
    if(geometrySequence!=source.eye.sourceSequence||geometryActivation!=source.eye.activation
        ||geometryTracking!=source.eye.trackingSequence||geometryEye!=source.eye.eye){
        geometrySequence=source.eye.sourceSequence;geometryActivation=source.eye.activation;
        geometryTracking=source.eye.trackingSequence;geometryEye=source.eye.eye;geometryAttempts=0;
    }
    if(geometryAttempts>=8)return {}; ++geometryAttempts;
    ComPtr<ID3D11PixelShader> ps;ComPtr<ID3D11VertexShader> vs;
    context->PSGetShader(&ps,nullptr,nullptr);context->VSGetShader(&vs,nullptr,nullptr);
    if(!ps||!vs||ps.Get()!=bound.shader)return {};
    const auto [pairVerified,family]=nativeProgramPair(context,vs.Get(),ps.Get());
    if(pairVerified)++nativeGpuPairs;else ++nativePairRejects;
    if(family)++opaqueFamilyBindings;
    std::array<ID3D11RenderTargetView*,8> raw{};ID3D11DepthStencilView* rawDepth{};
    context->OMGetRenderTargets(8,raw.data(),&rawDepth);
    std::array<ComPtr<ID3D11RenderTargetView>,8> targets;ComPtr<ID3D11DepthStencilView> depth;depth.Attach(rawDepth);
    for(size_t i=0;i<targets.size();++i)targets[i].Attach(raw[i]);
    if(!depth||!targets[0]||!targets[1]||!targets[2])return {};
    for(size_t i=3;i<targets.size();++i)if(targets[i])return {};
    ++geometryDraws;
    D3D11_DEPTH_STENCIL_VIEW_DESC depthView{};depth->GetDesc(&depthView);
    ComPtr<ID3D11DepthStencilState> ds;UINT stencil{};context->OMGetDepthStencilState(&ds,&stencil);
    D3D11_DEPTH_STENCIL_DESC dd{};if(ds)ds->GetDesc(&dd);
    else {dd.DepthEnable=TRUE;dd.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;dd.DepthFunc=D3D11_COMPARISON_LESS;}
    ComPtr<ID3D11BlendState> blend;FLOAT factors[4]{};UINT mask{};context->OMGetBlendState(&blend,factors,&mask);
    D3D11_BLEND_DESC bd{};if(blend)blend->GetDesc(&bd);else bd.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
    bool opaque=true;for(size_t i=0;i<3;++i){const auto& b=bd.RenderTarget[bd.IndependentBlendEnable?i:0];
        opaque=opaque&&!b.BlendEnable&&b.RenderTargetWriteMask==D3D11_COLOR_WRITE_ENABLE_ALL;}
    const bool writable=(depthView.Flags&D3D11_DSV_READ_ONLY_DEPTH)==0
        &&opticNativeMaterialDepthEligible(dd.DepthEnable!=FALSE,dd.DepthWriteMask==D3D11_DEPTH_WRITE_MASK_ALL,
            static_cast<uint32_t>(dd.DepthFunc));
    const auto id=reinterpret_cast<uintptr_t>(ps.Get());
    if(diagnosticsEnabled.load()&&std::find(geometryShaders.begin(),geometryShaders.end(),id)==geometryShaders.end()){
        const auto empty=std::find(geometryShaders.begin(),geometryShaders.end(),uintptr_t{});
        if(empty!=geometryShaders.end()){*empty=id;std::ostringstream line;
            line<<"Optic native three-MRT geometry candidate source="<<source.eye.sourceSequence
                <<" tracking="<<source.eye.trackingSequence<<" activation="<<source.eye.activation<<" eye="<<source.eye.eye
                <<" context=0x"<<std::hex<<reinterpret_cast<uintptr_t>(context)<<" native_ps=0x"<<id
                <<" native_vs=0x"<<reinterpret_cast<uintptr_t>(vs.Get())<<std::dec
                <<" native_bind_caller_verified=1 mrt_count=3 depth_flags="<<depthView.Flags
                <<" depth_enable="<<(dd.DepthEnable!=FALSE)<<" depth_write_mask="<<static_cast<UINT>(dd.DepthWriteMask)
                <<" depth_comparison="<<static_cast<UINT>(dd.DepthFunc)<<" stencil_enable="<<(dd.StencilEnable!=FALSE)
                <<" stencil_reference="<<stencil<<" opaque_mrt_writes="<<opaque<<" writable_depth="<<writable
                <<" camera_upload_verified=0 shader_owner_verified=0 native_pair_verified="<<pairVerified
                <<" material_family_verified="<<(family!=0)<<" read_only=1";
            for(size_t i=0;i<3;++i){D3D11_RENDER_TARGET_VIEW_DESC view{};targets[i]->GetDesc(&view);
                line<<" mrt"<<i<<"_format="<<static_cast<UINT>(view.Format);}
            log(line.str());}
    }
    if(!pairVerified||!family||!writable||!opaque||depthView.ViewDimension!=D3D11_DSV_DIMENSION_TEXTURE2D||depthView.Texture2D.MipSlice)return {};
    ComPtr<ID3D11Resource> depthObject;depth->GetResource(&depthObject);ComPtr<ID3D11Texture2D> depthTexture;
    if(FAILED(depthObject.As(&depthTexture)))return {};D3D11_TEXTURE2D_DESC size{};depthTexture->GetDesc(&size);
    D3D11_VIEWPORT viewport{};UINT count=1;context->RSGetViewports(&count,&viewport);
    if(count!=1||viewport.TopLeftX!=0||viewport.TopLeftY!=0||viewport.Width!=static_cast<float>(size.Width)
        ||viewport.Height!=static_cast<float>(size.Height)||size.ArraySize!=1||size.SampleDesc.Count!=1)return {};
    for(size_t i=0;i<3;++i){D3D11_RENDER_TARGET_VIEW_DESC view{};targets[i]->GetDesc(&view);
        if(view.ViewDimension!=D3D11_RTV_DIMENSION_TEXTURE2D||view.Texture2D.MipSlice)return {};
        ComPtr<ID3D11Resource> object;targets[i]->GetResource(&object);ComPtr<ID3D11Texture2D> texture;
        if(FAILED(object.As(&texture)))return {};D3D11_TEXTURE2D_DESC d{};texture->GetDesc(&d);
        if(d.Width!=size.Width||d.Height!=size.Height||d.ArraySize!=1||d.SampleDesc.Count!=1)return {};
    }
    ComPtr<ID3D11Buffer> buffer;UINT first{},constants=4096;ComPtr<ID3D11DeviceContext1> context1;
    if(SUCCEEDED(context->QueryInterface(IID_PPV_ARGS(&context1))))context1->VSGetConstantBuffers1(2,1,&buffer,&first,&constants);
    else context->VSGetConstantBuffers(2,1,&buffer);
    if(!buffer)return {};D3D11_BUFFER_DESC description{};buffer->GetDesc(&description);
    if(!(description.BindFlags&D3D11_BIND_CONSTANT_BUFFER)||!opticLightingSceneRangeEligible(description.ByteWidth,first,constants))return {};
    ComPtr<ID3D11Resource> resource;if(FAILED(buffer.As(&resource)))return {};
    auto* known=knownSceneBuffer(context,resource.Get());
    if(!known){auto empty=std::find_if(sceneBuffers.begin(),sceneBuffers.end(),[](const auto& value){return !value.buffer;});
        if(empty==sceneBuffers.end())return {};known=&*empty;
        known->buffer=buffer;known->resource=resource;known->context=reinterpret_cast<uintptr_t>(context);
        known->byteWidth=description.ByteWidth;known->generation=generation.load();known->complete=false;
        sceneResourceIds[static_cast<size_t>(known-sceneBuffers.data())]=reinterpret_cast<uintptr_t>(resource.Get());
    }
    OpticNativeGeometryPass pass{reinterpret_cast<uintptr_t>(ps.Get()),reinterpret_cast<uintptr_t>(vs.Get()),
        reinterpret_cast<uintptr_t>(buffer.Get()),first,constants,generation.load(),true,false,true,true};
    pass.cameraUploadVerified=sceneUploadMatches(pass,source,context);
    if(pass.cameraUploadVerified){++geometryReady;return pass;}
    return {};
}
// This emits unclassified D3D binding evidence, never a light/material identity.
// COM references are local to the draw and released before it returns. No
// native render target/depth resource is retained across a pass or resize.
void reportUnknownBinding(ID3D11DeviceContext* context,uint64_t now){
    if(now<nextUnknownBinding)return;
    if(bound.context==context){const auto cached=reinterpret_cast<uintptr_t>(bound.shader);
        for(const auto& group:unknownBindings)if(std::find(group.begin(),group.end(),cached)!=group.end())return;}
    bool room=false;for(const auto& group:unknownBindings)for(auto id:group)if(!id)room=true;
    if(!room)return;
    if(now>=unknownAttemptWindow){unknownAttemptWindow=now+500;unknownAttemptCount=0;}
    if(unknownAttemptCount>=256)return;
    ++unknownAttemptCount;
    ComPtr<ID3D11PixelShader> shader;context->PSGetShader(&shader,nullptr,nullptr);if(!shader)return;
    const auto id=reinterpret_cast<uintptr_t>(shader.Get());
    std::array<ID3D11RenderTargetView*,8> rawTargets{};ID3D11DepthStencilView* rawDepth{};
    context->OMGetRenderTargets(8,rawTargets.data(),&rawDepth);
    std::array<ComPtr<ID3D11RenderTargetView>,8> targets;ComPtr<ID3D11DepthStencilView> depth;depth.Attach(rawDepth);
    unsigned colors{};for(size_t i=0;i<targets.size();++i){targets[i].Attach(rawTargets[i]);if(targets[i])++colors;}
    if(!colors)return; // Depth-only shadow draws do not consume either budget.
    auto& group=unknownBindings[colors>1?1:0];
    if(std::find(group.begin(),group.end(),id)!=group.end())return;
    const auto slot=std::find(group.begin(),group.end(),uintptr_t{});if(slot==group.end())return;
    *slot=id;nextUnknownBinding=now+100;
    std::array<char,128> name{};UINT nameBytes=static_cast<UINT>(name.size()-1);
    if(FAILED(shader->GetPrivateData(debugNameKey,&nameBytes,name.data())))name[0]=0;
    for(char& c:name)if(c&&static_cast<unsigned char>(c)<32)c='?';
    const auto& eye=source.eye;
    std::ostringstream line;line<<"Optic native lighting unknown binding source="<<eye.sourceSequence
        <<" tracking="<<eye.trackingSequence<<" activation="<<eye.activation<<" sample_ms="<<eye.sampleTime
        <<" eye="<<eye.eye<<" head_pass=1 pixel_shader=0x"<<std::hex<<id<<" context=0x"
        <<reinterpret_cast<uintptr_t>(context)<<" native_camera=0x"<<source.nativeCamera
        <<" last_setter_caller=";
    const bool cacheCurrent=bound.context==context&&bound.shader==shader.Get();
    if(cacheCurrent&&bound.caller>=nativeBase&&bound.caller-nativeBase<0xa000000u)
        line<<"rva:0x"<<bound.caller-nativeBase;
    else line<<"unknown";
    line<<std::dec<<" bind_cache_current="<<cacheCurrent<<" debug_name="<<std::quoted(name.data())
        <<" colored_targets="<<colors<<" shader_owner_verified=0 material_owner_verified=0 coherent_frame=0 read_only=1";
    const auto resource=[&](const char* kind,size_t index,uintptr_t view,UINT format,ID3D11Resource* object){
        line<<' '<<kind<<index<<"_view=0x"<<std::hex<<view<<" resource=0x"<<reinterpret_cast<uintptr_t>(object)
            <<std::dec<<" format="<<format;
        ComPtr<ID3D11Texture2D> texture;
        if(object&&SUCCEEDED(object->QueryInterface(IID_PPV_ARGS(&texture)))){D3D11_TEXTURE2D_DESC d{};texture->GetDesc(&d);
            line<<" size="<<d.Width<<','<<d.Height<<" samples="<<d.SampleDesc.Count
                <<" array="<<d.ArraySize<<" mips="<<d.MipLevels;}
    };
    for(size_t i=0;i<targets.size();++i)if(targets[i]){D3D11_RENDER_TARGET_VIEW_DESC d{};targets[i]->GetDesc(&d);
        ComPtr<ID3D11Resource> object;targets[i]->GetResource(&object);
        resource("mrt",i,reinterpret_cast<uintptr_t>(targets[i].Get()),static_cast<UINT>(d.Format),object.Get());}
    if(depth){D3D11_DEPTH_STENCIL_VIEW_DESC d{};depth->GetDesc(&d);ComPtr<ID3D11Resource> object;depth->GetResource(&object);
        resource("depth",0,reinterpret_cast<uintptr_t>(depth.Get()),static_cast<UINT>(d.Format),object.Get());
        line<<" depth_view_flags="<<d.Flags<<" depth_view_dimension="<<static_cast<UINT>(d.ViewDimension);}
    ComPtr<ID3D11DepthStencilState> depthState;UINT stencilReference{};
    context->OMGetDepthStencilState(&depthState,&stencilReference);
    D3D11_DEPTH_STENCIL_DESC depthDescription{};
    if(depthState)depthState->GetDesc(&depthDescription);
    else {depthDescription.DepthEnable=TRUE;depthDescription.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;
        depthDescription.DepthFunc=D3D11_COMPARISON_LESS;}
    line<<" depth_enable="<<(depthDescription.DepthEnable!=FALSE)
        <<" depth_write_mask="<<static_cast<UINT>(depthDescription.DepthWriteMask)
        <<" depth_comparison="<<static_cast<UINT>(depthDescription.DepthFunc)
        <<" stencil_enable="<<(depthDescription.StencilEnable!=FALSE)<<" stencil_reference="<<stencilReference;
    ComPtr<ID3D11BlendState> blendState;FLOAT blendFactors[4]{};UINT sampleMask{};
    context->OMGetBlendState(&blendState,blendFactors,&sampleMask);D3D11_BLEND_DESC blendDescription{};
    if(blendState)blendState->GetDesc(&blendDescription);
    else blendDescription.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
    line<<" independent_blend="<<(blendDescription.IndependentBlendEnable!=FALSE)<<" blend_targets=";
    for(size_t i=0;i<colors;++i){const auto& d=blendDescription.RenderTarget[blendDescription.IndependentBlendEnable?i:0];
        if(i)line<<',';line<<(d.BlendEnable!=FALSE)<<':'<<static_cast<UINT>(d.RenderTargetWriteMask);}
    // The exact guarded native setter proves only the raw program-handle
    // field and dedicated type-4 program/name lookup layout. Its relation to this GPU shader
    // is still unknown, especially if another native worker changed it.
    if(nativeProgramLayout){
        uintptr_t graphics{},records{};uint32_t handle{},capacity{};uint64_t hash{};
        const bool haveHandle=readNative(nativeBase+0x2b743a8u,graphics)&&graphics&&readNative(graphics+0x738u,handle);
        const uint32_t index=handle>>17;
        const auto nameOffset=haveHandle&&readNative(graphics+0x10u,capacity)
            ?opticLightingProgramNameOffset(handle,capacity):std::nullopt;
        const bool haveName=nameOffset&&readNative(graphics+8,records)&&records
            &&records<=std::numeric_limits<uintptr_t>::max()-*nameOffset
            &&readNative(records+*nameOffset,hash);
        line<<" guarded_program_handle_read="<<haveHandle<<" raw_program_handle=0x"<<std::hex<<handle
            <<" raw_program_manager=0x"<<graphics<<" record_stride=80 record_index="<<std::dec<<index
            <<" record_capacity="<<capacity
            <<" raw_name_read="<<haveName<<" raw_name48=0x"<<std::hex<<(hash&0xffffffffffffull)<<std::dec
            <<" program_gpu_join_verified=0";
    }
    log(line.str());
}
bool wasAccepted(const EyeFrame& eye){
    for(const auto& pair:accepted)for(const auto& candidate:pair)
        if(candidate.joined&&opticLightingSameEye(eye,candidate))return true;
    return false;
}
void textureSummary(ID3D11ShaderResourceView* view,UINT slot,Texture& out){
    if(!view)return;
    ComPtr<ID3D11Resource> resource;view->GetResource(&resource);if(!resource)return;
    D3D11_SHADER_RESOURCE_VIEW_DESC desc{};view->GetDesc(&desc);
    out.resource=reinterpret_cast<uintptr_t>(resource.Get());out.slot=slot;
    out.dimension=static_cast<UINT>(desc.ViewDimension);out.format=static_cast<UINT>(desc.Format);
    ComPtr<ID3D11Texture2D> texture2;
    if(SUCCEEDED(resource.As(&texture2))){D3D11_TEXTURE2D_DESC d{};texture2->GetDesc(&d);
        out.width=d.Width;out.height=d.Height;out.array=d.ArraySize;out.mips=d.MipLevels;return;}
    ComPtr<ID3D11Texture3D> texture3;
    if(SUCCEEDED(resource.As(&texture3))){D3D11_TEXTURE3D_DESC d{};texture3->GetDesc(&d);
        out.width=d.Width;out.height=d.Height;out.depth=d.Depth;out.mips=d.MipLevels;}
}
bool prepareSnapshot(ID3D11Device* device,ID3D11Buffer* native,UINT first,UINT constants,
    UINT required,Snapshot& out){
    if(!native)return false;
    D3D11_BUFFER_DESC original{};native->GetDesc(&original);
    const uint64_t offset=uint64_t{first}*16;
    if(!original.ByteWidth||original.ByteWidth>65536||!(original.BindFlags&D3D11_BIND_CONSTANT_BUFFER)
        ||offset+required>original.ByteWidth||uint64_t{constants}*16<required)return false;
    D3D11_BUFFER_DESC staging{};staging.ByteWidth=original.ByteWidth;
    staging.Usage=D3D11_USAGE_STAGING;staging.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    if(FAILED(device->CreateBuffer(&staging,nullptr,&out.staging)))return false;
    out.byteWidth=original.ByteWidth;out.offset=static_cast<UINT>(offset);out.bytes=required;
    out.native=reinterpret_cast<uintptr_t>(native);return true;
}
bool queueRecord(ID3D11DeviceContext* context,UINT shaderIndex,Record& record,uint64_t now){
    Record value;value.source=source;value.shaderIndex=shaderIndex;value.queuedAt=now;
    value.generation=generation.load();value.context=reinterpret_cast<uintptr_t>(context);
    value.contextType=static_cast<UINT>(context->GetType());context->GetDevice(&value.device);
    if(!value.device)return false;
    ComPtr<ID3D11PixelShader> current;context->PSGetShader(&current,nullptr,nullptr);
    const auto currentTag=tagOf(current.Get());
    if(current.Get()!=bound.shader||currentTag.magic!=signature||currentTag.index!=shaderIndex)return false;
    value.pixelShader=reinterpret_cast<uintptr_t>(current.Get());
    value.identityVariant=currentTag.variant;
    ComPtr<ID3D11DeviceContext1> context1;context->QueryInterface(IID_PPV_ARGS(&context1));
    std::array<ComPtr<ID3D11Buffer>,6> native;
    const auto mask=shaders[shaderIndex].bufferMask|2u; // Always inspect the bound scene camera, even if unused by this PS.
    for(size_t i=0;i<buffers.size();++i){if(!(mask&(1u<<i)))continue;
        UINT first{},count=4096;
        if(context1)context1->PSGetConstantBuffers1(buffers[i].slot,1,native[i].GetAddressOf(),&first,&count);
        else context->PSGetConstantBuffers(buffers[i].slot,1,native[i].GetAddressOf());
        const bool required=(shaders[shaderIndex].bufferMask&(1u<<i))!=0;
        if(!prepareSnapshot(value.device.Get(),native[i].Get(),first,count,buffers[i].bytes,value.snapshots[i])){
            if(required)return false;
            native[i].Reset(); // Scene-less native shaders can remain honestly unjoined.
        }
    }
    D3D11_QUERY_DESC query{};query.Query=D3D11_QUERY_EVENT;
    if(FAILED(value.device->CreateQuery(&query,&value.completion)))return false;
    std::array<ID3D11ShaderResourceView*,16> views{};context->PSGetShaderResources(0,16,views.data());
    for(UINT i=0;i<views.size();++i){ComPtr<ID3D11ShaderResourceView> view;view.Attach(views[i]);textureSummary(view.Get(),i,value.textures[i]);}
    std::array<ID3D11RenderTargetView*,2> targets{};context->OMGetRenderTargets(2,targets.data(),nullptr);
    for(size_t i=0;i<targets.size();++i){ComPtr<ID3D11RenderTargetView> target;target.Attach(targets[i]);
        if(target){D3D11_RENDER_TARGET_VIEW_DESC d{};target->GetDesc(&d);value.renderFormats[i]=static_cast<UINT>(d.Format);}}
    UINT count=1;context->RSGetViewports(&count,&value.viewport);if(count!=1)value.viewport={};
    // The copies and their event are recorded on the exact native draw context.
    // Deferred commands must really execute before immediate GetData can succeed.
    for(size_t i=0;i<native.size();++i)if(value.snapshots[i].staging)
        context->CopyResource(value.snapshots[i].staging.Get(),native[i].Get());
    context->End(value.completion.Get());value.issued=true;record=std::move(value);return true;
}
void validateCamera(Record& value){
    const auto& scene=value.snapshots[1];
    if(!scene.staging||scene.bytes!=480)return;
    OpticLightingMatrix actualProjection{},actualView{};
    std::copy_n(scene.data.begin()+16,16,actualProjection.begin());
    std::copy_n(scene.data.begin()+32,16,actualView.begin());
    const float directView=opticLightingMatrixError(actualView,value.source.view);
    const float directProjection=opticLightingMatrixError(actualProjection,value.source.projection);
    const float transposeView=opticLightingMatrixError(actualView,value.source.view,true);
    const float transposeProjection=opticLightingMatrixError(actualProjection,value.source.projection,true);
    value.transpose=std::max(transposeView,transposeProjection)<std::max(directView,directProjection);
    value.viewError=value.transpose?transposeView:directView;
    value.projectionError=value.transpose?transposeProjection:directProjection;
    // This only certifies an observed correspondence, never guesses a matrix
    // layout for rendering. Both fields must use the same convention.
    value.cameraVerified=value.viewError<=.003f&&value.projectionError<=.003f;
}
void report(const Record& value){
    const auto& contract=shaders[value.shaderIndex];const auto& eye=value.source.eye;
    std::ostringstream line;line<<std::setprecision(9)<<"Optic native lighting sample source="<<eye.sourceSequence
        <<" tracking="<<eye.trackingSequence<<" activation="<<eye.activation<<" sample_ms="<<eye.sampleTime
        <<" eye="<<eye.eye<<" head_pass=1 shader="<<contract.name
        <<" sha256="<<(value.identityVariant?stripped[value.shaderIndex].sha256:contract.sha256)
        <<" identity_variant="<<(value.identityVariant?"debug_stripped":"full")
        <<" context_type="<<value.contextType<<" camera=0x"<<std::hex<<value.source.nativeCamera
        <<" context=0x"<<value.context<<" pixel_shader=0x"<<value.pixelShader<<std::dec
        <<" gpu_complete=1 accepted_stereo="<<value.accepted<<" native_camera_verified="<<value.cameraVerified
        <<" coherent_frame="<<(value.accepted&&value.cameraVerified)
        <<" observed_matrix_transpose="<<value.transpose<<" view_error="<<value.viewError
        <<" projection_error="<<value.projectionError<<" binocular_material_owner_verified=0 read_only=1"
        <<" viewport="<<value.viewport.Width<<','<<value.viewport.Height
        <<" render_formats="<<value.renderFormats[0]<<','<<value.renderFormats[1];
    for(size_t i=0;i<value.snapshots.size();++i){const auto& snapshot=value.snapshots[i];if(!snapshot.staging)continue;
        line<<' '<<buffers[i].name<<"_buffer=0x"<<std::hex<<snapshot.native<<std::dec
            <<" offset_bytes="<<snapshot.offset<<" bound_bytes="<<snapshot.byteWidth<<" values=";
        for(UINT n=0;n<snapshot.bytes/4;++n){if(n)line<<',';line<<snapshot.data[n];}
    }
    line<<" textures=";
    for(const auto& texture:value.textures)if(texture.resource){line<<texture.slot<<":0x"<<std::hex<<texture.resource<<std::dec
        <<':'<<texture.dimension<<':'<<texture.format<<':'<<texture.width<<'x'<<texture.height<<'x'<<texture.depth
        <<':'<<texture.array<<':'<<texture.mips<<';';}
    log(line.str());
}
}

OpticNativeLightingScope::OpticNativeLightingScope(const OpticNativeLightingSource& value) noexcept:previous_(source){
    source=enabled.load()&&opticLightingSourceEligible(value)?value:OpticNativeLightingSource{};
    source.binocularWorldValid=source.binocularWorldValid&&opticLightingHousingSourceEligible(source);
    if(enabled.load()){++scopesSeen;if(source.nativeCamera)++scopesEligible;}
}
OpticNativeLightingScope::OpticNativeLightingScope(const EyeFrame& eye,uintptr_t camera,
    const OpticLightingMatrix& view,const OpticLightingMatrix& projection,bool headPass,
    const OpticLightingMatrix* binocularWorld) noexcept
    :OpticNativeLightingScope(OpticNativeLightingSource{eye,camera,view,projection,headPass,
        binocularWorld?*binocularWorld:OpticLightingMatrix{},binocularWorld!=nullptr}){}
OpticNativeLightingScope::~OpticNativeLightingScope(){source=previous_;}

void setOpticNativeMaterialDrawCallback(OpticNativeMaterialDrawCallback value) noexcept {
    materialDrawCallback.store(value);
}
bool opticNativeSceneUploadVerified(ID3D11DeviceContext* context,const OpticNativeLightingSource& current,
    const OpticNativeGeometryPass& pass) noexcept {
    if(!context||!enabled.load()||!opticLightingGeometryPassEligible(pass))return false;
    try{std::unique_lock lock(mutex,std::try_to_lock);return lock&&sceneUploadMatches(pass,current,context);}
    catch(...){return false;}
}

bool opticNativeBinocularMaterialEnabled() noexcept {return enabled.load()&&nativeMaterialEnabled.load();}
void installOpticNativeLightingProbe(ID3D11Device* device,bool materialEnabled,bool diagnosticEnabled) noexcept {
    if(!opticNativeLightingObserversRequired(materialEnabled,diagnosticEnabled)||!device)return;
    try{
        std::lock_guard lock(mutex);if(installed){nativeMaterialEnabled=materialEnabled;diagnosticsEnabled=diagnosticEnabled;enabled=true;return;}
        nativeBase=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        constexpr std::array<unsigned char,14> programSetter{0x48,0x8b,0x05,0x11,0xa3,0x90,0x02,
            0x89,0x88,0x38,0x07,0x00,0x00,0xc3};
        // RVA269c50 rejects other handle types, extracts index>>17, and
        // returns manager+8[index*80]. RVA269165 stores the authored name.
        constexpr std::array<unsigned char,38> programRecord{0x85,0xc9,0x75,0x03,0x33,0xc0,0xc3,
            0x8b,0xc1,0x24,0x7f,0x3c,0x04,0x75,0xf5,0xc1,0xe9,0x11,0x48,0x8d,0x04,0x89,
            0x48,0x8b,0x0d,0x3b,0xa7,0x90,0x02,0x48,0xc1,0xe0,0x04,0x48,0x03,0x41,0x08,0xc3};
        constexpr std::array<unsigned char,7> programName{0x49,0x8b,0x06,0x48,0x89,0x46,0x08};
        std::array<unsigned char,14> actual{};
        std::array<unsigned char,38> recordBytes{};std::array<unsigned char,7> nameBytes{};
        nativeProgramLayout=nativeBase&&readNative(nativeBase+0x26a090u,actual)&&actual==programSetter
            &&readNative(nativeBase+0x269c50u,recordBytes)&&recordBytes==programRecord
            &&readNative(nativeBase+0x269165u,nameBytes)&&nameBytes==programName;
        std::array<unsigned char,101> geometryFlush{};std::array<unsigned char,96> geometryBind{};
        nativeGeometryBindLayout=nativeBase&&readNative(nativeBase+0x248900u,geometryFlush)
            &&digest(geometryFlush.data(),static_cast<UINT>(geometryFlush.size()))=="76bcfcf534c1839c5974516088da7a808fb247dc288c3b3ba31c605554b20d00"
            &&readNative(nativeBase+0x24b770u,geometryBind)
            &&digest(geometryBind.data(),static_cast<UINT>(geometryBind.size()))=="6392b2f5feabdd8de14fe14083795be135f6598f0723693dabeee92723fd0cd6";
        std::array<unsigned char,224> tripletGetter{};std::array<unsigned char,304> tripletFactory{};std::array<unsigned char,203> tripletSource{};
        nativeGeometryBindLayout=nativeGeometryBindLayout&&readNative(nativeBase+0x2b5960u,tripletGetter)
            &&digest(tripletGetter.data(),static_cast<UINT>(tripletGetter.size()))=="546577b1ee50986324bbeac3c70b38fa6bd7ea3473fa11599494f638f8659c54"
            &&readNative(nativeBase+0x19f6750u,tripletFactory)
            &&digest(tripletFactory.data(),static_cast<UINT>(tripletFactory.size()))=="82519c091263ff3846a243c6bf545cd943b0859f2ada40a4d894c737bf32a5ae"
            &&readNative(nativeBase+0x19f6680u,tripletSource)
            &&digest(tripletSource.data(),static_cast<UINT>(tripletSource.size()))=="0d4d61e5c63377a1604611f07f0677903dc6cbfb656d2adeee33a0096f8b0692";
        ComPtr<ID3D11DeviceContext> immediate,deferred;device->GetImmediateContext(&immediate);
        if(!immediate||FAILED(device->CreateDeferredContext(0,&deferred)))return;
        auto** d=*reinterpret_cast<void***>(device);
        auto** i=*reinterpret_cast<void***>(immediate.Get());auto** c=*reinterpret_cast<void***>(deferred.Get());
        std::array<void*,22> created{};size_t count{};
        const auto hook=[&](void* address,void* replacement,void** original){
            if(MH_CreateHook(address,replacement,original)!=MH_OK)return false;
            created[count++]=address;return true;
        };
        // Install creation classification once so a later diagnostic enable
        // does not require a duplicate detour. It forwards without inspecting
        // bytecode while diagnostics are disabled.
        bool okay=hook(d[15],reinterpret_cast<void*>(&createPs),reinterpret_cast<void**>(&originalCreate))
            &&hook(i[9],reinterpret_cast<void*>(&immediateSet),reinterpret_cast<void**>(&originalImmediateSet));
        if(okay&&i[9]!=c[9])okay=hook(c[9],reinterpret_cast<void*>(&deferredSet),reinterpret_cast<void**>(&originalDeferredSet));
        const auto pair=[&](UINT slot,void* immediateHook,void* deferredHook,void** immediateOriginal,void** deferredOriginal){
            if(!hook(i[slot],immediateHook,immediateOriginal))return false;
            if(i[slot]==c[slot]){*deferredOriginal=*immediateOriginal;return true;}
            return hook(c[slot],deferredHook,deferredOriginal);
        };
        if(okay)okay=pair(14,reinterpret_cast<void*>(&immediateMap),reinterpret_cast<void*>(&deferredMap),reinterpret_cast<void**>(&originalImmediateMap),reinterpret_cast<void**>(&originalDeferredMap))
            &&pair(15,reinterpret_cast<void*>(&immediateUnmap),reinterpret_cast<void*>(&deferredUnmap),reinterpret_cast<void**>(&originalImmediateUnmap),reinterpret_cast<void**>(&originalDeferredUnmap))
            &&pair(48,reinterpret_cast<void*>(&immediateUpdate),reinterpret_cast<void*>(&deferredUpdate),reinterpret_cast<void**>(&originalImmediateUpdate),reinterpret_cast<void**>(&originalDeferredUpdate))
            &&pair(47,reinterpret_cast<void*>(&immediateCopy),reinterpret_cast<void*>(&deferredCopy),reinterpret_cast<void**>(&originalImmediateCopy),reinterpret_cast<void**>(&originalDeferredCopy))
            &&pair(46,reinterpret_cast<void*>(&immediateCopyRegion),reinterpret_cast<void*>(&deferredCopyRegion),reinterpret_cast<void**>(&originalImmediateCopyRegion),reinterpret_cast<void**>(&originalDeferredCopyRegion))
            &&pair(33,reinterpret_cast<void*>(&immediateTargets),reinterpret_cast<void*>(&deferredTargets),reinterpret_cast<void**>(&originalImmediateTargets),reinterpret_cast<void**>(&originalDeferredTargets))
            &&pair(34,reinterpret_cast<void*>(&immediateTargetsUavs),reinterpret_cast<void*>(&deferredTargetsUavs),reinterpret_cast<void**>(&originalImmediateTargetsUavs),reinterpret_cast<void**>(&originalDeferredTargetsUavs))
            &&pair(39,reinterpret_cast<void*>(&immediateIndexedIndirect),reinterpret_cast<void*>(&deferredIndexedIndirect),reinterpret_cast<void**>(&originalImmediateIndexedIndirect),reinterpret_cast<void**>(&originalDeferredIndexedIndirect))
            &&pair(40,reinterpret_cast<void*>(&immediateIndirect),reinterpret_cast<void*>(&deferredIndirect),reinterpret_cast<void**>(&originalImmediateIndirect),reinterpret_cast<void**>(&originalDeferredIndirect));
        if(okay&&nativeGeometryBindLayout)okay=hook(reinterpret_cast<void*>(nativeBase+0x248900u),
            reinterpret_cast<void*>(&nativeProgramFlush),reinterpret_cast<void**>(&originalNativeProgramFlush));
        if(okay)for(size_t n=0;n<count;++n)if(MH_EnableHook(created[n])!=MH_OK){okay=false;break;}
        if(!okay){for(size_t n=0;n<count;++n){MH_DisableHook(created[n]);MH_RemoveHook(created[n]);}
            log("Optic native lighting probe unavailable; native rendering unchanged");return;}
        installed=true;nativeMaterialEnabled=materialEnabled;diagnosticsEnabled=diagnosticEnabled;enabled=true;
        log(std::string("Optic native lighting integration enabled: native_material_enabled=")+(materialEnabled?"1":"0")
            +" diagnostics_enabled="+(diagnosticEnabled?"1":"0")+" final_bind_layout_verified="
            +(nativeGeometryBindLayout?"1":"0")+" cpu_scene_upload_hooks=1 indirect_draw_hooks=1 diagnostic_reads_no_wait=1");
    }catch(...){enabled=false;nativeMaterialEnabled=false;diagnosticsEnabled=false;}
}
void invalidateOpticNativeLightingProbe() noexcept {
    invalidateNativeBinocularMaterial();
    generation.fetch_add(1);
    try{std::lock_guard lock(mutex);pending={};accepted={};selectedSequence=selectedActivation=selectedTracking=nextSelection=0;
        selectedMask=0;acceptedCursor=0;nextStatus=0;
        for(auto& id:sceneResourceIds)id.store(0);
        sceneBuffers={};geometryShaders={};familyCache={};negativeFamilyCache={};
        familyCacheCursor=negativeFamilyCacheCursor=0;nextFamilyHashWindow=0;familyHashAttempts=0;}catch(...){}
}
void stopOpticNativeLightingProbe() noexcept {
    enabled=false;nativeMaterialEnabled=false;diagnosticsEnabled=false;invalidateOpticNativeLightingProbe();
    // Detours remain forwarding, matching the existing process-stop lifecycle.
}
void observeOpticNativeLightingDraw(ID3D11DeviceContext* context,OpticNativeLightingDrawKind kind) noexcept {
    // Eligibility is checked once on entry to the source scope, not per draw.
    if(!enabled.load()||!context||!source.nativeCamera||nativeBinocularMaterialDrawing())return;
    try{
        const auto now=steadyMilliseconds();
        if(now<source.eye.sampleTime||now-source.eye.sampleTime>150)return;
        std::unique_lock lock(mutex,std::try_to_lock);if(!lock||!enabled.load())return;
        const auto kindIndex=static_cast<size_t>(kind);if(kindIndex>=drawKindObservations.size())return;
        if(diagnosticsEnabled.load()){
            ++drawKindObservations[kindIndex];
            if(targetCount.context==context&&targetCount.colors==3&&targetCount.depth)++drawKindThreeMrtMatches[kindIndex];
        }
        const auto geometry=inspectGeometryPass(context);
        const auto callback=materialDrawCallback.load();
        if(geometry&&callback&&nativeMaterialEnabled.load()){const auto frozen=source;lock.unlock();
            const bool inserted=callback(context,frozen,*geometry);
            if(!lock.try_lock())return;
            if(inserted)++materialInsertions;
        }
        if(!diagnosticsEnabled.load()||reportCount>=128)return;
        if(bound.context!=context||bound.tag.magic!=signature||bound.tag.index>=shaders.size()){
            reportUnknownBinding(context,now);return;
        }
        if(selectedSequence!=source.eye.sourceSequence||selectedActivation!=source.eye.activation
            ||selectedTracking!=source.eye.trackingSequence){
            if(now<nextSelection)return;
            selectedSequence=source.eye.sourceSequence;selectedActivation=source.eye.activation;
            selectedTracking=source.eye.trackingSequence;selectedMask=0;nextSelection=now+500;
        }
        const uint32_t bit=1u<<(shaders[bound.tag.index].group*2+source.eye.eye);
        if(selectedMask&bit)return;
        // One attempt per group/eye prevents repeated buffer allocations on a
        // missing binding. A later source transaction can retry after 500 ms.
        selectedMask|=bit;
        ++captureAttempts;
        const auto free=std::find_if(pending.begin(),pending.end(),[](const auto& value){return !value.issued;});
        if(free!=pending.end()&&queueRecord(context,bound.tag.index,*free,now))++queuedRecords;
        else ++queueFailures;
    }catch(...){}
}
bool opticNativeBinocularMaterialShaderVerified(ID3D11PixelShader* shader) noexcept {
    const auto tag=tagOf(shader);
    return tag.magic==signature&&tag.index==13;
}
void acceptOpticNativeLightingFrame(const std::array<EyeFrame,2>& eyes) noexcept {
    if(!diagnosticsEnabled.load()||!eyes[0].joined||!eyes[1].joined||eyes[0].eye!=0||eyes[1].eye!=1
        ||!eyes[0].sourceSequence||eyes[0].sourceSequence!=eyes[1].sourceSequence
        ||eyes[0].trackingSequence!=eyes[1].trackingSequence||eyes[0].activation!=eyes[1].activation)return;
    try{std::lock_guard lock(mutex);accepted[acceptedCursor++%accepted.size()]=eyes;++acceptedPairs;
        for(auto& value:pending)if(value.issued&&!value.accepted)value.accepted=wasAccepted(value.source.eye);
    }catch(...){}
}
void pumpOpticNativeLightingProbe(ID3D11DeviceContext* immediate) noexcept {
    if(!diagnosticsEnabled.load()||!immediate||immediate->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return;
    try{
        ComPtr<ID3D11Device> device;immediate->GetDevice(&device);
        const auto now=steadyMilliseconds();std::unique_lock lock(mutex,std::try_to_lock);if(!lock)return;
        for(auto& value:pending){if(!value.issued)continue;
            if(value.generation!=generation.load()||value.device.Get()!=device.Get()
                ||now<value.queuedAt||now-value.queuedAt>3000){
                if(dropCount++<16)log("Optic native lighting readback expired/device changed; coherent_frame=0 read_only=1");
                value={};continue;
            }
            if(!value.ready){
                BOOL complete=FALSE;
                const auto result=immediate->GetData(value.completion.Get(),&complete,sizeof(complete),D3D11_ASYNC_GETDATA_DONOTFLUSH);
                if(result==S_FALSE)continue;
                if(FAILED(result)){value={};continue;}
                if(!complete)continue;
                bool mapped=true;
                for(auto& snapshot:value.snapshots){if(!snapshot.staging)continue;
                    D3D11_MAPPED_SUBRESOURCE bytes{};
                    const auto map=immediate->Map(snapshot.staging.Get(),0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&bytes);
                    if(FAILED(map)){mapped=false;break;}
                    std::memcpy(snapshot.data.data(),static_cast<const std::byte*>(bytes.pData)+snapshot.offset,snapshot.bytes);
                    immediate->Unmap(snapshot.staging.Get(),0);
                }
                if(!mapped)continue;
                value.ready=true;validateCamera(value);value.accepted=value.accepted||wasAccepted(value.source.eye);
            }
            // Preserve raw parameter evidence on an unjoined timeout, but do
            // not describe it as a same-frame native/VR lighting publication.
            if(value.accepted||now-value.queuedAt>=2000){
                if(reportCount++<128)report(value);value={};
            }
        }
        if(now>=nextStatus){
            nextStatus=now+5000;
            std::ostringstream status;status<<"Optic native lighting probe status scopes="<<scopesSeen.load()
                <<" eligible_head_scopes="<<scopesEligible.load()<<" tagged_shaders="<<tagCount
                <<" known_bindings="<<knownBindings.load()<<" capture_attempts="<<captureAttempts
                <<" queued="<<queuedRecords<<" queue_failures="<<queueFailures<<" accepted_pairs="<<acceptedPairs
                <<" reports="<<reportCount<<" pending="<<std::count_if(pending.begin(),pending.end(),[](const auto& v){return v.issued;})
                <<" geometry_bind_layout_verified="<<nativeGeometryBindLayout<<" geometry_draws="<<geometryDraws
                <<" head_draw_observations="<<headDrawObservations<<" target_context_matches="<<targetContextMatches
                <<" three_mrt_target_matches="<<threeMrtTargetMatches<<" native_bind_matches="<<nativeBindMatches
                <<" native_flush_scopes="<<nativeFlushScopes.load()<<" native_gpu_pairs="<<nativeGpuPairs
                <<" native_pair_rejects="<<nativePairRejects<<" opaque_family_bindings="<<opaqueFamilyBindings
                <<" material_callback_ready="<<(materialDrawCallback.load()!=nullptr)
                <<" scene_buffers="<<std::count_if(sceneBuffers.begin(),sceneBuffers.end(),[](const auto& v){return v.buffer!=nullptr;})
                <<" scene_uploads="<<sceneUploads<<" scene_upload_rejects="<<sceneUploadRejects
                <<" geometry_ready="<<geometryReady<<" material_insertions="<<materialInsertions
                <<" native_material_enabled="<<nativeMaterialEnabled.load()<<" diagnostics_enabled=1 diagnostic_reads_no_wait=1";log(status.str());
            std::ostringstream targets;targets<<"Optic native target-bind counts head=";
            for(size_t i=0;i<headTargetBindCounts.size();++i){if(i)targets<<',';targets<<headTargetBindCounts[i].load();}
            targets<<" outside_head=";
            for(size_t i=0;i<otherTargetBindCounts.size();++i){if(i)targets<<',';targets<<otherTargetBindCounts[i].load();}
            targets<<" indices=colored_target_count_0_to_8 material_admitted=0 read_only=1";log(targets.str());
            std::ostringstream draws;draws<<"Optic native draw-kind counts observed=";
            for(size_t i=0;i<drawKindObservations.size();++i){if(i)draws<<',';draws<<drawKindObservations[i];}
            draws<<" three_mrt=";
            for(size_t i=0;i<drawKindThreeMrtMatches.size();++i){if(i)draws<<',';draws<<drawKindThreeMrtMatches[i];}
            draws<<" order=direct,indexed_indirect,indirect read_only=1";log(draws.str());
        }
    }catch(...){}
}
}
