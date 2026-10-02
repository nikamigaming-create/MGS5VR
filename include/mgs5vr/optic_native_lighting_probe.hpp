#pragma once
#include "stereo.hpp"
#include "optic_material_lifecycle.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <optional>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11PixelShader;

namespace mgs5vr {
using OpticLightingMatrix=std::array<float,16>;

// The owned FMDL stores sampler names in section 6 and texture types in
// section 7. They are distinct identities; the type record references the
// section-6 binding, while that binding references the texture path.
struct OpticBinocularTextureRecord {
    uint64_t bindingName{},typeName{},path{};
    uint32_t typeBindingIndex{},pathIndex{};
};
inline bool opticBinocularTextureRecordEligible(size_t slot,const OpticBinocularTextureRecord& record) noexcept {
    constexpr std::array<uint64_t,4> bindings{0x7c0a0bc43d21ull,0xa64b8af6eab0ull,0x403e7df259cfull,0x98e9a785dd63ull};
    constexpr std::array<uint64_t,4> types{0x41ea7be81b61ull,0xcc4305511ae0ull,0x104d6b98b10eull,0x8e6f2dfd5885ull};
    constexpr std::array<uint64_t,4> paths{0x1568643e638c1c21ull,0x156a46fa1ab1bafdull,0x156bd2b731b98391ull,0x1568ed887bb8a4d9ull};
    return slot<bindings.size()&&record.bindingName==bindings[slot]&&record.typeName==types[slot]
        &&record.path==paths[slot]&&record.typeBindingIndex==slot&&record.pathIndex==slot;
}
inline bool opticNativeLightingObserversRequired(bool materialEnabled,bool diagnosticsEnabled) noexcept {
    return materialEnabled||diagnosticsEnabled;
}

// Native RVA269d30 handles the four MatParamIndex names: positive integer
// x is scaled by exact float bits3b808081; all other x values are preserved.
inline float opticNativeMaterialIndex(float authored) noexcept {
    if(std::isfinite(authored)&&authored>=1.f&&authored<2147483648.f)
        return static_cast<float>(static_cast<int32_t>(authored))*std::bit_cast<float>(0x3b808081u);
    return authored;
}

// D3D11 comparison values. Equal-only prepasses, disabled depth, and
// read-only depth cannot accept new physical geometry at this draw.
inline bool opticNativeMaterialDepthEligible(bool enabled,bool writes,uint32_t comparison) noexcept {
    return enabled&&writes&&(comparison==2u||comparison==4u||comparison==5u||comparison==7u);
}

// Owned native RVA269c50 accepts type 4 and indexes an 80-byte program
// record; RVA269165 writes its name at +8. Bounds belong to the current
// program manager, not the separate generic resource table.
inline std::optional<size_t> opticLightingProgramNameOffset(uint32_t handle,uint32_t capacity) noexcept {
    const auto index=handle>>17;
    if(!handle||(handle&0x7fu)!=4u||!capacity||capacity>32768u||index>=capacity)return {};
    return static_cast<size_t>(index)*80u+8u;
}

// Metadata comes from the immutable, verified native scene replay. A newest
// camera/tracking publication is not a substitute for this source transaction.
struct OpticNativeLightingSource {
    EyeFrame eye{};
    uintptr_t nativeCamera{};
    OpticLightingMatrix view{},projection{};
    bool headPass{};
    OpticLightingMatrix binocularWorld{};
    bool binocularWorldValid{};
};
inline bool opticLightingBodyWorldEligible(const OpticLightingMatrix& world) noexcept {
    for(const auto v:world)if(!std::isfinite(v))return false;
    if(std::abs(world[3])>.0001f||std::abs(world[7])>.0001f||std::abs(world[11])>.0001f
        ||std::abs(world[15]-1.f)>.0001f)return false;
    const float determinant=world[0]*(world[5]*world[10]-world[6]*world[9])
        -world[4]*(world[1]*world[10]-world[2]*world[9])
        +world[8]*(world[1]*world[6]-world[2]*world[5]);
    return std::isfinite(determinant)&&std::abs(determinant)>=.25f&&std::abs(determinant)<=4.f;
}
inline bool opticLightingSameEye(const EyeFrame& a,const EyeFrame& b) noexcept {
    return a.sourceSequence&&a.sourceSequence==b.sourceSequence
        &&a.trackingSequence&&a.trackingSequence==b.trackingSequence
        &&a.activation&&a.activation==b.activation&&a.sampleTime==b.sampleTime
        &&a.eye<2&&a.eye==b.eye&&a.projected&&b.projected
        &&a.magnification==1.f&&b.magnification==1.f
        &&a.view.pose.position.x==b.view.pose.position.x
        &&a.view.pose.position.y==b.view.pose.position.y
        &&a.view.pose.position.z==b.view.pose.position.z
        &&a.view.pose.orientation.x==b.view.pose.orientation.x
        &&a.view.pose.orientation.y==b.view.pose.orientation.y
        &&a.view.pose.orientation.z==b.view.pose.orientation.z
        &&a.view.pose.orientation.w==b.view.pose.orientation.w
        &&a.view.fov.left==b.view.fov.left&&a.view.fov.right==b.view.fov.right
        &&a.view.fov.up==b.view.fov.up&&a.view.fov.down==b.view.fov.down;
}
inline float opticLightingMatrixError(const OpticLightingMatrix& actual,
    const OpticLightingMatrix& expected,bool transposed=false) noexcept {
    float error{};
    for(size_t i=0;i<16;++i){
        const float a=actual[i],b=expected[transposed?(i%4)*4+i/4:i];
        if(!std::isfinite(a)||!std::isfinite(b))return std::numeric_limits<float>::infinity();
        error=std::max(error,std::abs(a-b));
    }
    return error;
}
inline bool opticLightingSourceEligible(const OpticNativeLightingSource& source) noexcept {
    const auto& p=source.eye.view.pose;const auto& f=source.eye.view.fov;
    const float norm=p.orientation.x*p.orientation.x+p.orientation.y*p.orientation.y
        +p.orientation.z*p.orientation.z+p.orientation.w*p.orientation.w;
    return source.headPass&&source.nativeCamera&&source.eye.eye<2
        &&source.eye.projected&&source.eye.sourceSequence&&source.eye.trackingSequence
        &&source.eye.activation&&source.eye.sampleTime&&source.eye.magnification==1.f
        &&std::isfinite(p.position.x)&&std::isfinite(p.position.y)&&std::isfinite(p.position.z)
        &&std::isfinite(norm)&&norm>=.25f&&norm<=4.f
        &&std::isfinite(f.left)&&std::isfinite(f.right)&&std::isfinite(f.up)&&std::isfinite(f.down)
        &&f.left<f.right&&f.down<f.up
        &&std::any_of(source.view.begin(),source.view.end(),[](float v){return v!=0.f;})
        &&std::any_of(source.projection.begin(),source.projection.end(),[](float v){return v!=0.f;})
        &&std::isfinite(opticLightingMatrixError(source.view,source.view))
        &&std::isfinite(opticLightingMatrixError(source.projection,source.projection));
}
inline bool opticLightingHousingSourceEligible(const OpticNativeLightingSource& source) noexcept {
    return opticLightingSourceEligible(source)&&source.binocularWorldValid
        &&opticLightingBodyWorldEligible(source.binocularWorld);
}

inline bool opticLightingSceneRangeEligible(uint32_t width,uint32_t first,uint32_t count) noexcept {
    return width>=480&&width<=65536&&first<=4096&&count>=30&&count<=4096
        &&uint64_t{first}*16+480<=width;
}
inline bool opticLightingWritableMap(uint32_t type,uint32_t flags) noexcept {
    // D3D11_MAP_WRITE / WRITE_DISCARD / WRITE_NO_OVERWRITE; only the
    // documented optional DO_NOT_WAIT flag is accepted.
    return (type==2u||type==4u||type==5u)&&(flags==0u||flags==0x100000u);
}
struct OpticLightingSceneUpload {
    OpticNativeLightingSource source{};
    uintptr_t context{},buffer{};
    uint64_t generation{};
    uint32_t byteWidth{},firstConstant{},constantCount{};
    OpticLightingMatrix view{},projection{};
    bool complete{};
};
inline bool opticLightingSceneUploadMatches(const OpticLightingSceneUpload& upload,
    const OpticNativeLightingSource& current,uintptr_t context,uintptr_t buffer,
    uint64_t generation,uint32_t first,uint32_t count) noexcept {
    if(!upload.complete||!context||!buffer||!generation||upload.context!=context||upload.buffer!=buffer
        ||upload.generation!=generation||upload.firstConstant!=first||upload.constantCount!=count
        ||!opticLightingSceneRangeEligible(upload.byteWidth,first,count)
        ||!opticLightingSourceEligible(current)||!opticLightingSourceEligible(upload.source)
        ||!opticLightingSameEye(upload.source.eye,current.eye)||upload.source.nativeCamera!=current.nativeCamera
        ||upload.source.binocularWorldValid!=current.binocularWorldValid
        ||(current.binocularWorldValid&&opticLightingMatrixError(upload.source.binocularWorld,current.binocularWorld)!=0)
        ||opticLightingMatrixError(upload.source.view,current.view)!=0
        ||opticLightingMatrixError(upload.source.projection,current.projection)!=0)return false;
    const auto direct=std::max(opticLightingMatrixError(upload.view,current.view),
        opticLightingMatrixError(upload.projection,current.projection));
    const auto transpose=std::max(opticLightingMatrixError(upload.view,current.view,true),
        opticLightingMatrixError(upload.projection,current.projection,true));
    return std::min(direct,transpose)<=.003f;
}

// A candidate is observed at a genuine opaque/writable three-MRT head draw.
// Its CPU scene upload must still match this exact source/range when used.
struct OpticNativeGeometryPass {
    uintptr_t pixelShader{},vertexShader{},sceneBuffer{};
    uint32_t firstConstant{},constantCount{};
    uint64_t generation{};
    bool nativeBindingVerified{},cameraUploadVerified{};
    bool nativePairVerified{},materialFamilyVerified{};
};
inline bool opticLightingGeometryPassEligible(const OpticNativeGeometryPass& pass) noexcept {
    return pass.pixelShader&&pass.vertexShader&&pass.sceneBuffer&&pass.generation
        &&pass.firstConstant<=4096&&pass.constantCount>=30&&pass.constantCount<=4096
        &&pass.nativeBindingVerified&&pass.cameraUploadVerified&&pass.nativePairVerified&&pass.materialFamilyVerified;
}
using OpticNativeMaterialDrawCallback=bool(*)(ID3D11DeviceContext*,const OpticNativeLightingSource&,
    const OpticNativeGeometryPass&) noexcept;
void setOpticNativeMaterialDrawCallback(OpticNativeMaterialDrawCallback) noexcept;
bool opticNativeSceneUploadVerified(ID3D11DeviceContext*,const OpticNativeLightingSource&,
    const OpticNativeGeometryPass&) noexcept;

// Product admission and optional diagnostics are independent. Observers
// forward native calls unchanged; only the separately
// registered optics callback can insert an owned material, after all gates.
// No native buffer or texture is written. Direct draws use the existing
// wrappers; the probe observes the two separate indirect draw APIs without
// duplicating any direct Draw detour or inspecting GPU argument buffers.
// The 128-report cap is process-wide; invalidation does not renew that budget.
void installOpticNativeLightingProbe(ID3D11Device*,bool materialEnabled,bool diagnosticsEnabled=false) noexcept;
bool opticNativeBinocularMaterialEnabled() noexcept;
// Resize/device loss discards provenance and readbacks without disabling the
// configured probe; the replacement native device can create tagged shaders.
void invalidateOpticNativeLightingProbe() noexcept;
void stopOpticNativeLightingProbe() noexcept;
enum class OpticNativeLightingDrawKind : uint32_t { direct,indexedIndirect,indirect };
void observeOpticNativeLightingDraw(ID3D11DeviceContext*,
    OpticNativeLightingDrawKind=OpticNativeLightingDrawKind::direct) noexcept;
// Exact creation-time whole-DXBC identity plus required reflected layout.
// This verifies shader code, not a native program-object/GPU ownership join.
bool opticNativeBinocularMaterialShaderVerified(ID3D11PixelShader*) noexcept;
// Call on the game's immediate context at Present. Both query and Map are
// polled once, with DONOTFLUSH/DO_NOT_WAIT; this never waits or flushes the GPU.
void pumpOpticNativeLightingProbe(ID3D11DeviceContext*) noexcept;
// Call only AFTER the existing AcceptedStereoFrame::accept succeeds. This is
// evidence gating, not a replacement for any stereo or freshness guard.
void acceptOpticNativeLightingFrame(const std::array<EyeFrame,2>&) noexcept;

class OpticNativeLightingScope {
public:
    explicit OpticNativeLightingScope(const OpticNativeLightingSource&) noexcept;
    OpticNativeLightingScope(const EyeFrame&,uintptr_t nativeCamera,
        const OpticLightingMatrix& nativeView,const OpticLightingMatrix& nativeProjection,
        bool headPass,const OpticLightingMatrix* binocularWorld=nullptr) noexcept;
    ~OpticNativeLightingScope();
    OpticNativeLightingScope(const OpticNativeLightingScope&)=delete;
    OpticNativeLightingScope& operator=(const OpticNativeLightingScope&)=delete;
private:
    OpticNativeLightingSource previous_{};
};
}
