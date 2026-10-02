#include "mgs5vr/optic_native_lighting_probe.hpp"
#include <iostream>
#include <limits>

int main(){
    using namespace mgs5vr;
    unsigned checks{},failed{};
    const auto check=[&](bool condition,const char* label){
        ++checks;if(!condition){++failed;std::cerr<<label<<'\n';}
    };
    check(!opticNativeLightingObserversRequired(false,false),"explicit product and diagnostics opt-out installs no observer");
    check(opticNativeLightingObserversRequired(true,false),"ordinary product admission does not depend on diagnostics");
    check(opticNativeLightingObserversRequired(false,true),"diagnostic-only admission remains available");
    check(opticNativeLightingObserversRequired(true,true),"combined product and diagnostics admission");
    constexpr std::array<OpticBinocularTextureRecord,4> textureRecords{{
        {0x7c0a0bc43d21ull,0x41ea7be81b61ull,0x1568643e638c1c21ull,0,0},
        {0xa64b8af6eab0ull,0xcc4305511ae0ull,0x156a46fa1ab1bafdull,1,1},
        {0x403e7df259cfull,0x104d6b98b10eull,0x156bd2b731b98391ull,2,2},
        {0x98e9a785dd63ull,0x8e6f2dfd5885ull,0x1568ed887bb8a4d9ull,3,3}}};
    for(size_t i=0;i<textureRecords.size();++i){
        check(opticBinocularTextureRecordEligible(i,textureRecords[i]),"distinct sampler/type identities admit exact typed owned texture");
        auto texture=textureRecords[i];texture.bindingName=texture.typeName;
        check(!opticBinocularTextureRecordEligible(i,texture),"type hash cannot substitute for sampler binding hash");
        texture=textureRecords[i];texture.typeName=texture.bindingName;
        check(!opticBinocularTextureRecordEligible(i,texture),"sampler hash cannot substitute for texture type hash");
        texture=textureRecords[i];texture.typeBindingIndex=(texture.typeBindingIndex+1)%4;
        check(!opticBinocularTextureRecordEligible(i,texture),"texture type declaration must reference its own binding");
        texture=textureRecords[i];texture.pathIndex=(texture.pathIndex+1)%4;
        check(!opticBinocularTextureRecordEligible(i,texture),"binding must reference its exact texture path index");
        texture=textureRecords[i];texture.path=textureRecords[(i+1)%4].path;
        check(!opticBinocularTextureRecordEligible(i,texture),"another authored map cannot substitute for this map");
    }
    check(!opticBinocularTextureRecordEligible(4,textureRecords[0]),"unknown material map slot rejected");
    EyeFrame observed;observed.view.fov={-.8f,.8f,.7f,-.7f};
    observed.sourceSequence=42;observed.trackingSequence=17;observed.activation=3;
    observed.sampleTime=100;observed.projected=true;observed.eye=0;
    auto accepted=observed;accepted.joined=true;
    check(opticLightingSameEye(observed,accepted),"exact accepted metadata matches");
    const auto mismatch=[&](auto mutate,const char* label){auto value=accepted;mutate(value);
        check(!opticLightingSameEye(observed,value),label);};
    mismatch([](auto& v){++v.sourceSequence;},"different scene source rejected");
    mismatch([](auto& v){++v.trackingSequence;},"different tracking rejected");
    mismatch([](auto& v){++v.activation;},"different activation rejected");
    mismatch([](auto& v){++v.sampleTime;},"different sample time rejected");
    mismatch([](auto& v){v.eye=1;},"opposite eye rejected");
    mismatch([](auto& v){v.projected=false;},"unprojected source rejected");
    mismatch([](auto& v){v.magnification=2;},"magnified lens metadata rejected");
    mismatch([](auto& v){v.view.pose.position.x=.01f;},"different camera pose rejected");
    mismatch([](auto& v){v.view.pose.orientation.w=-1;},"different camera quaternion rejected");
    mismatch([](auto& v){v.view.fov.left=-.7f;},"different rendered FOV rejected");
    auto lens=observed;lens.eye=2;
    check(!opticLightingSameEye(lens,lens),"lens cannot match itself as head eye");
    auto empty=observed;empty.sourceSequence=0;
    check(!opticLightingSameEye(empty,empty),"zero source cannot join");
    empty=observed;empty.trackingSequence=0;
    check(!opticLightingSameEye(empty,empty),"zero tracking cannot join");
    empty=observed;empty.activation=0;
    check(!opticLightingSameEye(empty,empty),"zero activation cannot join");

    OpticLightingMatrix identity{};identity[0]=identity[5]=identity[10]=identity[15]=1;
    OpticNativeLightingSource source{observed,0x1234,identity,identity,true};
    check(opticLightingSourceEligible(source),"verified head source eligible");
    check(!opticLightingHousingSourceEligible(source),"missing immutable housing world cannot enter native geometry");
    source.binocularWorld=identity;source.binocularWorldValid=true;
    check(opticLightingHousingSourceEligible(source),"immutable finite housing world joins verified head source");
    auto bodySource=source;bodySource.headPass=false;
    check(!opticLightingHousingSourceEligible(bodySource),"housing insertion excludes lens/restore pass");
    bodySource=source;bodySource.binocularWorld[15]=0;
    check(!opticLightingHousingSourceEligible(bodySource),"nonaffine housing world rejected");
    bodySource=source;bodySource.binocularWorld[0]=0;
    check(!opticLightingHousingSourceEligible(bodySource),"singular housing world rejected");
    bodySource=source;bodySource.binocularWorld[12]=std::numeric_limits<float>::infinity();
    check(!opticLightingHousingSourceEligible(bodySource),"nonfinite housing translation rejected");
    const auto ineligible=[&](auto mutate,const char* label){auto value=source;mutate(value);
        check(!opticLightingSourceEligible(value),label);};
    ineligible([](auto& v){v.headPass=false;},"explicit lens role excluded");
    ineligible([](auto& v){v.nativeCamera=0;},"missing native owner excluded");
    ineligible([](auto& v){v.eye.eye=2;},"lens eye excluded");
    ineligible([](auto& v){v.eye.projected=false;},"unprojected eye excluded");
    ineligible([](auto& v){v.eye.sampleTime=0;},"missing source sample excluded");
    ineligible([](auto& v){v.eye.magnification=10;},"magnified source excluded");
    ineligible([](auto& v){v.view={};},"missing native view excluded");
    ineligible([](auto& v){v.projection={};},"missing native projection excluded");
    ineligible([](auto& v){v.view[12]=std::numeric_limits<float>::quiet_NaN();},"nonfinite native matrix excluded");
    ineligible([](auto& v){v.eye.view.pose.position.x=std::numeric_limits<float>::infinity();},"nonfinite camera position excluded");
    ineligible([](auto& v){v.eye.view.pose.orientation={0,0,0,0};},"invalid camera quaternion excluded");
    ineligible([](auto& v){v.eye.view.fov.up=v.eye.view.fov.down;},"invalid rendered FOV excluded");

    auto matrix=identity;matrix[12]=123;matrix[13]=-2;
    OpticLightingMatrix transpose{};
    for(size_t i=0;i<16;++i)transpose[i]=matrix[(i%4)*4+i/4];
    check(opticLightingMatrixError(matrix,matrix)==0,"exact native matrix has zero error");
    check(opticLightingMatrixError(transpose,matrix,true)==0,"observed transpose has zero error");
    check(opticLightingMatrixError(transpose,matrix)>100,"different matrix convention cannot match directly");
    auto near=matrix;near[2]=.002f;
    check(opticLightingMatrixError(near,matrix)<=.003f,"bounded absolute matrix error admitted");
    near[2]=.004f;
    check(opticLightingMatrixError(near,matrix)>.003f,"matrix error beyond bound rejected");
    near[2]=std::numeric_limits<float>::quiet_NaN();
    check(!std::isfinite(opticLightingMatrixError(near,matrix)),"NaN matrix cannot certify correspondence");

    check(opticLightingProgramNameOffset(4,1)==8,"type-4 first program name is record+8");
    check(opticLightingProgramNameOffset((1u<<17)|4u,2)==88,"dedicated program table uses 80-byte stride");
    check(!opticLightingProgramNameOffset(0,1),"missing native program rejected");
    check(!opticLightingProgramNameOffset(5,1),"different resource handle type rejected");
    check(!opticLightingProgramNameOffset(4,0),"empty native program table rejected");
    check(!opticLightingProgramNameOffset(4,32769),"implausibly large native program table rejected");
    check(!opticLightingProgramNameOffset((2u<<17)|4u,2),"program index at capacity rejected");
    check(opticLightingProgramNameOffset((32767u<<17)|4u,32768)==2621368,
        "last bounded program record admitted without integer overflow");
    check(opticNativeMaterialIndex(100)==100.f*std::bit_cast<float>(0x3b808081u),"authored native material100 has exact recovered normalization");
    check(opticNativeMaterialIndex(132.9f)==opticNativeMaterialIndex(132),"native material callback truncates positive index");
    check(opticNativeMaterialIndex(0)==0,"native material callback preserves zero");
    check(opticNativeMaterialIndex(-1)==-1,"native material callback preserves negative index");
    check(opticNativeMaterialDepthEligible(true,true,2),"native less writable geometry pass admitted");
    check(opticNativeMaterialDepthEligible(true,true,4),"native less-equal writable geometry pass admitted");
    check(opticNativeMaterialDepthEligible(true,true,5),"native reversed greater geometry pass admitted");
    check(opticNativeMaterialDepthEligible(true,true,7),"native reversed greater-equal geometry pass admitted");
    check(!opticNativeMaterialDepthEligible(false,true,2),"disabled native depth rejected");
    check(!opticNativeMaterialDepthEligible(true,false,2),"native depth prepass read-only writes rejected");
    for(const uint32_t comparison:{0u,1u,3u,6u,8u,9u})
        check(!opticNativeMaterialDepthEligible(true,true,comparison),"non-geometry native depth comparison rejected");
    check(!nativeBinocularMaterialDrawing(),"native material recursion guard initially clear");
    {
        OpticNativeMaterialDrawScope outer;
        check(nativeBinocularMaterialDrawing(),"native material drawing excludes recursive diagnostic draw");
        {OpticNativeMaterialDrawScope inner;}
        check(nativeBinocularMaterialDrawing(),"nested material scope preserves outer drawing guard");
    }
    check(!nativeBinocularMaterialDrawing(),"material scope restores drawing guard");
    const auto generation=nativeBinocularMaterialGeneration();invalidateNativeBinocularMaterial();
    check(nativeBinocularMaterialGeneration()==generation+1,"invalidation rejects prior native material generation");

    // An observed CPU upload can only certify this complete, immutable head
    // source and this exact buffer range. Asymmetric matrices distinguish
    // consistent transpose from a misleading mixed convention.
    check(opticLightingSceneRangeEligible(480,0,30),"smallest complete scene range admitted");
    check(!opticLightingSceneRangeEligible(479,0,30),"partial scene buffer rejected");
    check(!opticLightingSceneRangeEligible(480,0,29),"short bound scene range rejected");
    check(opticLightingSceneRangeEligible(496,1,30),"offset scene range fitting buffer admitted");
    check(!opticLightingSceneRangeEligible(480,1,30),"offset scene data beyond buffer rejected");
    check(opticLightingSceneRangeEligible(65536,4066,30),"last complete scene offset admitted");
    check(!opticLightingSceneRangeEligible(65536,4067,30),"scene offset crossing maximum buffer rejected");
    check(!opticLightingSceneRangeEligible(65537,0,30),"buffer larger than D3D11 constant limit rejected");
    check(!opticLightingSceneRangeEligible(65536,0,4097),"implausibly large bound constant count rejected");
    check(!opticLightingSceneRangeEligible(65536,std::numeric_limits<uint32_t>::max(),30),
        "offset overflow cannot become a complete scene range");
    check(!opticLightingSceneRangeEligible(std::numeric_limits<uint32_t>::max(),0,30),
        "byte width overflow cannot enter scene range");
    check(!opticLightingSceneRangeEligible(65536,0,std::numeric_limits<uint32_t>::max()),
        "constant count overflow cannot enter scene range");
    for(const uint32_t mapType:{2u,4u,5u}){
        check(opticLightingWritableMap(mapType,0),"writable map without optional flags admitted");
        check(opticLightingWritableMap(mapType,0x100000u),"writable nonblocking map admitted");
        check(!opticLightingWritableMap(mapType,0x100001u),"unknown flag mixed with DO_NOT_WAIT rejected");
    }
    for(const uint32_t mapType:{0u,1u,3u,6u,std::numeric_limits<uint32_t>::max()})
        check(!opticLightingWritableMap(mapType,0),"read or unknown map type cannot certify CPU scene writes");
    check(!opticLightingWritableMap(4,1),"unrecognized map flag rejected");

    auto sceneSource=source;
    sceneSource.view=matrix;
    sceneSource.projection=identity;
    sceneSource.projection[2]=.5f;sceneSource.projection[8]=.125f;
    OpticLightingSceneUpload upload{sceneSource,0x1000,0x2000,7,480,0,30,
                                   sceneSource.view,sceneSource.projection,true};
    const auto matches=[&](const auto& candidate,const auto& current){
        return opticLightingSceneUploadMatches(candidate,current,0x1000,0x2000,7,0,30);
    };
    check(matches(upload,sceneSource),"complete direct CPU upload matches exact source");
    const auto rejectUpload=[&](auto mutate,const char* label){auto candidate=upload;mutate(candidate);
        check(!matches(candidate,sceneSource),label);};
    rejectUpload([](auto& u){u.complete=false;},"partial upload cannot certify scene camera");
    rejectUpload([](auto& u){u.context=0x1001;},"different context upload rejected");
    rejectUpload([](auto& u){u.buffer=0x2001;},"different scene buffer upload rejected");
    rejectUpload([](auto& u){++u.generation;},"prior or different buffer generation rejected");
    rejectUpload([](auto& u){u.generation=0;},"missing upload generation rejected");
    rejectUpload([](auto& u){u.firstConstant=1;},"different first bound constant rejected");
    rejectUpload([](auto& u){++u.constantCount;},"different bound constant count rejected");
    rejectUpload([](auto& u){u.byteWidth=479;},"incomplete CPU scene byte range rejected");
    rejectUpload([](auto& u){++u.source.eye.sourceSequence;},"upload from different scene source rejected");
    rejectUpload([](auto& u){++u.source.eye.trackingSequence;},"upload from different tracking publication rejected");
    rejectUpload([](auto& u){++u.source.eye.activation;},"upload from previous camera activation rejected");
    rejectUpload([](auto& u){++u.source.eye.sampleTime;},"upload from different scene sample rejected");
    rejectUpload([](auto& u){u.source.eye.eye=1;},"upload from opposite head eye rejected");
    rejectUpload([](auto& u){u.source.eye.eye=2;},"lens upload cannot enter head geometry pass");
    rejectUpload([](auto& u){u.source.headPass=false;},"non-head upload cannot certify head pass");
    rejectUpload([](auto& u){++u.source.nativeCamera;},"upload from different native camera rejected");
    rejectUpload([](auto& u){u.source.eye.view.pose.position.y=.001f;},"upload from different exact eye pose rejected");
    rejectUpload([](auto& u){u.source.eye.view.fov.up-=.001f;},"upload from different rendered eye FOV rejected");
    rejectUpload([](auto& u){u.source.view[12]+=.001f;},"immutable source view must match exactly before CPU tolerance");
    rejectUpload([](auto& u){u.source.projection[8]+=.001f;},"immutable source projection must match exactly before CPU tolerance");
    rejectUpload([](auto& u){u.view[12]=std::numeric_limits<float>::quiet_NaN();},"NaN CPU view cannot certify source join");
    rejectUpload([](auto& u){u.projection[8]=std::numeric_limits<float>::infinity();},"infinite CPU projection cannot certify source join");
    rejectUpload([](auto& u){u.source.view[12]=std::numeric_limits<float>::quiet_NaN();},"nonfinite immutable upload source rejected");

    const auto rejectCurrent=[&](auto mutate,const char* label){auto current=sceneSource;mutate(current);
        check(!matches(upload,current),label);};
    rejectCurrent([](auto& s){s.eye.eye=1;},"current opposite eye rejects otherwise valid previous upload");
    rejectCurrent([](auto& s){++s.eye.sourceSequence;},"new current scene source rejects previous upload");
    rejectCurrent([](auto& s){s.headPass=false;},"current lens role rejects head upload");
    rejectCurrent([](auto& s){s.nativeCamera=0;},"missing current native camera rejects upload");
    rejectCurrent([](auto& s){s.view[12]=std::numeric_limits<float>::quiet_NaN();},"nonfinite current source rejected");
    check(!opticLightingSceneUploadMatches(upload,sceneSource,0,0x2000,7,0,30),"null current context rejected");
    check(!opticLightingSceneUploadMatches(upload,sceneSource,0x1000,0,7,0,30),"null current buffer rejected");
    check(!opticLightingSceneUploadMatches(upload,sceneSource,0x1000,0x2000,0,0,30),"null current generation rejected");

    auto cpuNear=upload;cpuNear.view[1]+=.003f;cpuNear.projection[1]+=.003f;
    check(matches(cpuNear,sceneSource),"CPU upload at absolute tolerance boundary admitted");
    cpuNear.view[1]=.0031f;
    check(!matches(cpuNear,sceneSource),"CPU view beyond absolute tolerance rejected");
    cpuNear=upload;cpuNear.projection[1]=.0031f;
    check(!matches(cpuNear,sceneSource),"CPU projection beyond absolute tolerance rejected");
    auto cpuTranspose=upload;
    for(size_t i=0;i<16;++i){
        cpuTranspose.view[i]=sceneSource.view[(i%4)*4+i/4];
        cpuTranspose.projection[i]=sceneSource.projection[(i%4)*4+i/4];
    }
    check(matches(cpuTranspose,sceneSource),"consistent transposed view and projection admitted");
    cpuTranspose.projection=sceneSource.projection;
    check(!matches(cpuTranspose,sceneSource),"transposed view with direct projection rejected");
    cpuTranspose=upload;
    for(size_t i=0;i<16;++i)cpuTranspose.projection[i]=sceneSource.projection[(i%4)*4+i/4];
    check(!matches(cpuTranspose,sceneSource),"direct view with transposed projection rejected");
    auto bodyUpload=upload;auto carriedSource=sceneSource;
    carriedSource.binocularWorldValid=true;carriedSource.binocularWorld={1,0,0,0,0,1,0,0,0,0,1,0,1,2,3,1};
    bodyUpload.source=carriedSource;
    check(matches(bodyUpload,carriedSource),"exact immutable body carry accompanies camera upload");
    bodyUpload.source.binocularWorld[12]+=1;
    check(!matches(bodyUpload,carriedSource),"different carried body world rejected");
    bodyUpload.source=carriedSource;bodyUpload.source.binocularWorldValid=false;
    check(!matches(bodyUpload,carriedSource),"different body carry validity rejected");
    OpticNativeGeometryPass geometry{1,2,3,0,30,4,true,true,true,true};
    check(opticLightingGeometryPassEligible(geometry),"complete proved geometry candidate admitted");
    const auto badGeometry=[&](auto mutate,const char* label){auto value=geometry;mutate(value);
        check(!opticLightingGeometryPassEligible(value),label);};
    badGeometry([](auto& v){v.pixelShader=0;},"missing native PS rejected");
    badGeometry([](auto& v){v.vertexShader=0;},"missing native VS rejected");
    badGeometry([](auto& v){v.sceneBuffer=0;},"missing scene buffer rejected");
    badGeometry([](auto& v){v.generation=0;},"missing scene generation rejected");
    badGeometry([](auto& v){v.constantCount=29;},"partial scene range rejected");
    badGeometry([](auto& v){v.firstConstant=4097;},"scene first outside range rejected");
    badGeometry([](auto& v){v.nativeBindingVerified=false;},"unknown native bind rejected");
    badGeometry([](auto& v){v.cameraUploadVerified=false;},"unknown camera upload rejected");
    badGeometry([](auto& v){v.nativePairVerified=false;},"unknown native GPU pair rejected");
    badGeometry([](auto& v){v.materialFamilyVerified=false;},"three targets without known material family rejected");
    std::cout<<checks<<" optic native lighting boundary checks; failures="<<failed<<'\n';
    return failed?1:0;
}
