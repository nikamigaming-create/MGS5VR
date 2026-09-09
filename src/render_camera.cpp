#include "mgs5vr/render_camera.hpp"
#include "mgs5vr/head_camera.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/log.hpp"
#include "mgs5vr/scene_capture.hpp"
#include "mgs5vr/ui_renderer.hpp"
#include "mgs5vr/controller_rig.hpp"
#include <windows.h>
#include <intrin.h>
#include <MinHook.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <stdexcept>

namespace {
using MatrixFn=float*(*)(void*,float*);
MatrixFn originalWorld{},originalView{};
using ExtentsFn=uintptr_t(*)(void*,float*);
ExtentsFn originalExtents{};
using ViewportFn=uintptr_t(*)(void*,uint8_t);
using ProjectionFn=uintptr_t(*)(float*,float,float,float,float,float,float,float,float,float);
using SceneFn=uintptr_t(*)(void*,void*,void*,uint32_t);
using RegisterTargetFn=uintptr_t(*)(void*,void*);
using ListenerFn=void*(*)(void*,void*,const float*);
ViewportFn originalViewport{};ProjectionFn originalProjection{};SceneFn originalScene{};
RegisterTargetFn originalRegisterTarget{};
ListenerFn originalListener{},originalVirtualListener{};
uintptr_t base{};
struct Pair {
    uintptr_t camera{};
    uint64_t sequence{},tick{};
    DWORD thread{};
    mgs5vr::HeadCameraSample sample{};
    std::array<float,8> nativeInput{};
    std::array<float,16> world{},view{};
    bool haveWorld{},applied{},validInverse{};
    float inverseError{};
};
thread_local Pair current;
thread_local uintptr_t primaryListener{};
thread_local uint64_t primaryListenerSequence{};
std::array<Pair,8> latest{};
Pair lastMatrixFailure{};
std::mutex latestMutex;
struct ListenerProof {
    uintptr_t camera{},listener{};
    uint64_t sequence{},tracking{},activation{},tick{};
    std::array<float,8> native{},submitted{},consumed{},virtualConsumed{};
    bool primaryAccepted{},virtualAccepted{};
};
ListenerProof listenerProof;
std::atomic_uint64_t listenerUpdates{},virtualListenerUpdates{},listenerFailures{};
std::atomic_uint64_t sequence{},pairCount{},missed{};
std::atomic_bool enabled{},nativePairVerified{};
std::ofstream evidence;
unsigned reports{};
struct PresentTrace { uint64_t frame{},tick{}; DWORD thread{}; USHORT count{}; std::array<void*,24> stack{}; };
PresentTrace presentTrace;
std::atomic_uint64_t presentCount{};
struct ViewportSample {
    uintptr_t viewport{},camera{},caller{};
    uint64_t tick{};DWORD thread{};
    int width{},height{};float scale{};
    std::array<float,4> extents{};
    std::array<float,144> matrices{}; // Native viewport bytes +0x280 through +0x4bf.
    std::array<float,48> cameraMatrices{}; // GrCamera world, view, previous view.
    USHORT stackCount{};std::array<void*,32> stack{};
};
std::array<ViewportSample,8> viewports{};
struct SceneSource {uintptr_t viewport{},grCamera{};Pair pair;};
SceneSource sceneSource;
std::mutex sceneMutex;
thread_local uintptr_t eyeViewport{};
thread_local uintptr_t visibilityViewport{};
std::atomic_uint64_t visibilityUpdates{};
thread_local uintptr_t stereoTarget{};
thread_local mgs5vr::EyeFrame drawingEye{};
thread_local bool clipProjection{},gpuProjection{},insideStereo{};
std::atomic_uint64_t sceneCalls{},scenePairs{},sceneRejected{},sceneCopies{};
std::atomic_uint64_t duplicatePresentsSkipped{};
std::atomic_uint32_t sceneContextType{99};
std::atomic_uint32_t sceneFailure{};
std::atomic<DWORD> sceneThread{};
template<class T> T field(const void* object,size_t offset){T out{};std::memcpy(&out,static_cast<const unsigned char*>(object)+offset,sizeof(out));return out;}
mgs5vr::Pose pose(const float* values){return {{values[0],values[1],values[2],values[3]},{values[4],values[5],values[6]}};}
std::array<float,8> values(mgs5vr::Pose p){return {p.orientation.x,p.orientation.y,p.orientation.z,p.orientation.w,p.position.x,p.position.y,p.position.z,1};}
float inverseError(const std::array<float,16>& a,const std::array<float,16>& b){
    float error=0;
    for(size_t row=0;row<4;++row)for(size_t col=0;col<4;++col){
        double value=0;for(size_t k=0;k<4;++k)value+=static_cast<double>(a[row*4+k])*b[k*4+col];
        if(!std::isfinite(value))return INFINITY;
        error=std::max(error,static_cast<float>(std::abs(value-(row==col?1.0:0.0))));
    }
    return error;
}
void record(Pair& p){
    p.tick=GetTickCount64();p.thread=GetCurrentThreadId();p.sequence=++sequence;
    p.inverseError=std::max(inverseError(p.world,p.view),inverseError(p.view,p.world));
    p.validInverse=p.inverseError<0.003f;
    if(p.validInverse&&!p.applied)nativePairVerified.store(true);
    if(!p.validInverse&&p.applied){mgs5vr::headCamera().cancel(mgs5vr::HeadCameraStop::matrixMismatch);nativePairVerified.store(false);}
    ++pairCount;
    std::unique_lock lock(latestMutex,std::try_to_lock);
    if(!lock.owns_lock()){++missed;return;}
    if(!p.validInverse&&p.applied)lastMatrixFailure=p;
    Pair* slot=nullptr;
    for(auto& item:latest)if(item.camera==p.camera){slot=&item;break;}
    if(!slot)for(auto& item:latest)if(!item.camera){slot=&item;break;}
    if(slot)*slot=p;else ++missed;
}
__declspec(noinline) void* listener(void* object,void* output,const float* input){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    if(!enabled.load()||caller!=base+0x438132)return originalListener(object,output,input);
    primaryListener=0;primaryListenerSequence=0;
    // This exact camera publication selects its listener through publisher+0x60.
    // An explicitly selected alternate listener transform remains native.
    if(!current.applied||!current.validInverse||!current.sequence||!object
        ||reinterpret_cast<uintptr_t>(input)!=current.camera+0xf0
        ||std::memcmp(input,current.nativeInput.data(),sizeof(current.nativeInput)))return originalListener(object,output,input);
    const auto tracked=mgs5vr::trackedListenerPose(current.sample,mgs5vr::headCamera().status(),mgs5vr::steadyMilliseconds());
    if(!tracked)return originalListener(object,output,input);
    alignas(16) const auto adjusted=values(*tracked);
    auto* result=originalListener(object,output,adjusted.data());
    ListenerProof proof;proof.camera=current.camera;proof.listener=reinterpret_cast<uintptr_t>(object);
    proof.sequence=current.sequence;proof.tracking=current.sample.trackingSequence;proof.activation=current.sample.activation;
    proof.tick=GetTickCount64();proof.native=current.nativeInput;proof.submitted=adjusted;
    std::memcpy(proof.consumed.data(),static_cast<unsigned char*>(object)+0x20,sizeof(proof.consumed));
    proof.primaryAccepted=field<int32_t>(output,0)==0&&proof.consumed==adjusted;
    ++listenerUpdates;
    if(proof.primaryAccepted){primaryListener=proof.listener;primaryListenerSequence=current.sequence;}
    else ++listenerFailures;
    {std::unique_lock lock(latestMutex,std::try_to_lock);if(lock.owns_lock())listenerProof=proof;}
    return result;
}
__declspec(noinline) void* virtualListener(void* object,void* output,const float* input){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const auto source=reinterpret_cast<uintptr_t>(input);
    if(!enabled.load()||caller!=base+0x43814e||primaryListener!=reinterpret_cast<uintptr_t>(object)
        ||!primaryListenerSequence||primaryListenerSequence!=current.sequence
        ||(source!=current.camera+0xf0&&source!=current.camera+0x130))return originalVirtualListener(object,output,input);
    const auto tracked=mgs5vr::trackedListenerPose(current.sample,mgs5vr::headCamera().status(),mgs5vr::steadyMilliseconds());
    if(!tracked)return originalVirtualListener(object,output,input);
    alignas(16) const auto adjusted=values(*tracked);
    auto* result=originalVirtualListener(object,output,adjusted.data());
    std::array<float,8> consumed{};std::memcpy(consumed.data(),static_cast<unsigned char*>(object)+0x40,sizeof(consumed));
    const bool accepted=field<int32_t>(output,0)==0&&consumed==adjusted;
    ++virtualListenerUpdates;if(!accepted)++listenerFailures;
    {std::unique_lock lock(latestMutex,std::try_to_lock);
        if(lock.owns_lock()&&listenerProof.sequence==current.sequence&&listenerProof.listener==primaryListener){
            listenerProof.virtualConsumed=consumed;listenerProof.virtualAccepted=accepted;
        }
    }
    primaryListener=0;primaryListenerSequence=0;
    return result;
}
__declspec(noinline) uintptr_t projection(float* output,float a,float b,float c,float d,float e,float f,float g,float h,float i){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const auto result=originalProjection(output,a,b,c,d,e,f,g,h,i);
    if(enabled.load()&&caller==base+0x2e68a0)mgs5vr::applyUiEyeProjection(output);
    if(enabled.load()&&visibilityViewport&&caller==base+0x1b9691
        &&reinterpret_cast<uintptr_t>(output)==visibilityViewport+0x300){
        std::array<float,16> matrix{};std::memcpy(matrix.data(),output,sizeof(matrix));
        if(mgs5vr::widenVisibilityProjection(matrix,current.sample.headPose,current.sample.views)){
            std::memcpy(output,matrix.data(),sizeof(matrix));
            if(++visibilityUpdates==1)mgs5vr::log("Native visibility projection covers both tracked eyes with a symmetric turn margin");
        }
    }
    if(!enabled.load()||!eyeViewport)return result;
    const bool clip=caller==base+0x1b9691&&reinterpret_cast<uintptr_t>(output)==eyeViewport+0x300;
    const bool gpu=caller==base+0x1b9724&&reinterpret_cast<uintptr_t>(output)==eyeViewport+0x280;
    if(!clip&&!gpu)return result;
    std::array<float,16> matrix{};std::memcpy(matrix.data(),output,sizeof(matrix));
    if(!mgs5vr::setEyeProjection(matrix,drawingEye.view.fov))return result;
    std::memcpy(output,matrix.data(),sizeof(matrix));if(clip)clipProjection=true;if(gpu)gpuProjection=true;
    return result;
}
__declspec(noinline) uintptr_t viewport(void* input,uint8_t history){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    // Native visibility is prepared before scene replay. Widen its clip matrix
    // while the native builder derives the frustum planes, using this same
    // center-head publication. Neither eye's image projection is widened here.
    const auto priorVisibility=visibilityViewport;
    const auto now=mgs5vr::steadyMilliseconds();
    if(enabled.load()&&!insideStereo&&caller==base+0x4380b9&&current.applied&&current.validInverse
        &&current.sample.stereoTracked&&now>=current.sample.sampleTime&&now-current.sample.sampleTime<=150){
        const auto camera=field<uintptr_t>(input,0x570);
        if(camera&&std::memcmp(reinterpret_cast<void*>(camera+0x70),current.view.data(),sizeof(current.view))==0)
            visibilityViewport=reinterpret_cast<uintptr_t>(input);
    }
    const auto result=originalViewport(input,history);
    visibilityViewport=priorVisibility;
    if(enabled.load()&&!insideStereo&&caller==base+0x4380b9&&current.applied&&current.validInverse&&current.sample.stereoTracked){
        const auto camera=field<uintptr_t>(input,0x570);
        if(camera&&std::memcmp(reinterpret_cast<void*>(camera+0x70),current.view.data(),sizeof(current.view))==0){
            std::lock_guard lock(sceneMutex);sceneSource={reinterpret_cast<uintptr_t>(input),camera,current};
        }
    }
    return result;
}
struct NativeRestore {
    uintptr_t camera{},viewport{};
    std::array<unsigned char,0xc0> cameraMatrices{};
    std::array<unsigned char,0x240> viewportMatrices{};
    NativeRestore(uintptr_t c,uintptr_t v):camera(c),viewport(v){
        std::memcpy(cameraMatrices.data(),reinterpret_cast<void*>(c+0x30),cameraMatrices.size());
        std::memcpy(viewportMatrices.data(),reinterpret_cast<void*>(v+0x280),viewportMatrices.size());
    }
    void restore() const{
        std::memcpy(reinterpret_cast<void*>(camera+0x30),cameraMatrices.data(),cameraMatrices.size());
        std::memcpy(reinterpret_cast<void*>(viewport+0x280),viewportMatrices.data(),viewportMatrices.size());
    }
    ~NativeRestore(){restore();mgs5vr::clearUiRenderSource();eyeViewport=0;stereoTarget=0;drawingEye={};insideStereo=false;}
};
__declspec(noinline) uintptr_t registerTarget(void* graphics,void* target){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const bool second=enabled.load()&&insideStereo&&drawingEye.eye==1&&stereoTarget
        &&reinterpret_cast<uintptr_t>(target)==stereoTarget&&caller==base+0x1bef27;
    const auto before=second?field<uint32_t>(graphics,0x110):0;
    // The active D3D11 implementation performs required GPU setup before
    // appending to its present vector. Always execute that native setup.
    const auto result=originalRegisterTarget(graphics,target);
    if(second&&before){
        const auto after=field<uint32_t>(graphics,0x110);
        const auto capacity=field<uint32_t>(graphics,0x114);
        const auto data=field<uintptr_t>(graphics,0x118);
        if(after==before+1&&after<=capacity&&data
            &&field<uintptr_t>(reinterpret_cast<void*>(data),size_t(before-1)*8)==stereoTarget
            &&field<uintptr_t>(reinterpret_cast<void*>(data),size_t(before)*8)==stereoTarget){
            // The render job has not published this vector to its presentation
            // worker yet. Remove only the append produced by this eye's call.
            std::memcpy(static_cast<unsigned char*>(graphics)+0x110,&before,sizeof(before));
            ++duplicatePresentsSkipped;
        }
    }
    return result;
}
__declspec(noinline) uintptr_t scene(void* render,void* graphics,void* task,uint32_t worker){
    const auto id=++sceneCalls;sceneThread.store(GetCurrentThreadId());
    const auto status=mgs5vr::headCamera().status();
    if(!enabled.load()||!status.active||insideStereo||!mgs5vr::sceneCaptureAvailable())return originalScene(render,graphics,task,worker);
    SceneSource source;{std::lock_guard lock(sceneMutex);source=sceneSource;}
    bool contains=false;auto candidate=field<uintptr_t>(render,0xa0);
    for(unsigned n=0;candidate&&n<16;++n){if(candidate==source.viewport){contains=true;break;}candidate=field<uintptr_t>(reinterpret_cast<void*>(candidate),0x30);}
    const auto now=mgs5vr::steadyMilliseconds();
    // The first camera update can precede tracked skin publication. Do not
    // submit that exposed third-person arm pose as the first VR eye pair.
    if(mgs5vr::controllerRigEnabled()&&!source.pair.sample.rigSequence){++sceneRejected;return originalScene(render,graphics,task,worker);}
    if(!contains||source.pair.sample.activation!=status.activation||now<source.pair.sample.sampleTime||now-source.pair.sample.sampleTime>150
        ||std::memcmp(reinterpret_cast<void*>(source.grCamera+0x30),source.pair.world.data(),sizeof(source.pair.world))){++sceneRejected;return originalScene(render,graphics,task,worker);}
    const auto contextOwner=field<uintptr_t>(graphics,0x150);
    auto* context=contextOwner?field<ID3D11DeviceContext*>(reinterpret_cast<void*>(contextOwner),8):nullptr;
    if(!context){++sceneRejected;return originalScene(render,graphics,task,worker);}
    sceneContextType.store(context->GetType());
    NativeRestore saved(source.grCamera,source.viewport);insideStereo=true;stereoTarget=field<uintptr_t>(render,0x98);
    const auto cameraCount=pairCount.load();uintptr_t result{};bool complete=true;
    mgs5vr::beginSceneTiming(context,id);
    for(uint32_t eye=0;eye<2;++eye){
        saved.restore();eyeViewport=source.viewport;clipProjection=gpuProjection=false;
        drawingEye={source.pair.sample.views[eye],id,source.pair.sample.trackingSequence,status.activation,source.pair.sample.sampleTime,eye,false,false};
        // Centered native rendering avoids the observed lighting coverage gap.
        // Preserve the requested optical centers separately, then submit only
        // their exact pixel region with its matching angular bounds.
        drawingEye.displayFov=drawingEye.view.fov;
        drawingEye.magnification=source.pair.sample.controllers.magnification;
        const auto optical=mgs5vr::opticalFov(drawingEye.displayFov,drawingEye.magnification);
        const auto renderFov=optical?mgs5vr::enclosingEyeFov(*optical):std::nullopt;
        if(!renderFov){complete=false;sceneFailure=4;break;}
        drawingEye.view.fov=*renderFov;
        const auto native=mgs5vr::nativeEyePose(source.pair.sample.nativePose,source.pair.sample.headPose,drawingEye.view.pose);
        alignas(16) auto nativeValues=values(native);
        alignas(16) std::array<float,16> eyeWorld{},eyeView{};
        alignas(16) std::array<unsigned char,0x140> inverseInput{};
        std::memcpy(inverseInput.data()+0x120,nativeValues.data(),sizeof(nativeValues));
        originalWorld(nativeValues.data(),eyeWorld.data());originalView(inverseInput.data(),eyeView.data());
        if(std::max(inverseError(eyeWorld,eyeView),inverseError(eyeView,eyeWorld))>=0.003f){complete=false;sceneFailure=3;break;}
        std::memcpy(reinterpret_cast<void*>(source.grCamera+0x30),eyeWorld.data(),sizeof(eyeWorld));
        std::memcpy(reinterpret_cast<void*>(source.grCamera+0x70),eyeView.data(),sizeof(eyeView));
        // Until per-eye temporal resources are isolated, publish a zero-motion
        // camera history. This avoids introducing the opposite eye's history.
        std::memcpy(reinterpret_cast<void*>(source.grCamera+0xb0),eyeView.data(),sizeof(eyeView));
        originalViewport(reinterpret_cast<void*>(source.viewport),0);
        if(!clipProjection||!gpuProjection){complete=false;sceneFailure=4;break;}
        std::memcpy(reinterpret_cast<void*>(source.viewport+0x3c0),eyeView.data(),sizeof(eyeView));
        std::memcpy(reinterpret_cast<void*>(source.viewport+0x400),reinterpret_cast<void*>(source.viewport+0x280),sizeof(eyeView));
        drawingEye.projected=true;
        mgs5vr::setUiRenderSource(drawingEye,source.grCamera,eyeView,source.pair.sample);
        result=originalScene(render,graphics,task,worker);
        mgs5vr::clearUiRenderSource();
        // Native passes may finish and replace the current deferred context.
        const auto afterOwner=field<uintptr_t>(graphics,0x150);
        auto* afterContext=afterOwner?field<ID3D11DeviceContext*>(reinterpret_cast<void*>(afterOwner),8):nullptr;
        try{if(mgs5vr::captureSceneEye(afterContext,drawingEye))++sceneCopies;else {complete=false;sceneFailure=5;}}
        catch(const std::exception& ex){complete=false;sceneFailure=7;mgs5vr::log(std::string("Native eye capture: ")+ex.what());}
    }
    if(pairCount.load()!=cameraCount){complete=false;sceneFailure=6;}
    const auto timingOwner=field<uintptr_t>(graphics,0x150);
    mgs5vr::endSceneTiming(timingOwner?field<ID3D11DeviceContext*>(reinterpret_cast<void*>(timingOwner),8):context,id,complete);
    if(complete){++scenePairs;}
    else {
        ++sceneRejected;mgs5vr::cancelSceneEyes(id);mgs5vr::headCamera().cancel(mgs5vr::HeadCameraStop::matrixMismatch);
        mgs5vr::log("Native scene pair stopped: reason="+std::to_string(sceneFailure.load())+" eye="+std::to_string(drawingEye.eye));
        saved.restore();eyeViewport=0;stereoTarget=0;drawingEye={};insideStereo=false;
        return originalScene(render,graphics,task,worker);
    }
    return result;
}
__declspec(noinline) float* world(void* input,float* output){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    if(!enabled.load())return originalWorld(input,output);
    const auto source=reinterpret_cast<uintptr_t>(input);
    if(caller==base+0x437c64){
        if(const auto menu=mgs5vr::nativeMenuOpen())mgs5vr::headCamera().setNativeMenuOpen(*menu);
        current={};current.camera=source-0xf0;
        primaryListener=0;primaryListenerSequence=0;
        std::array<float,8> native{};std::memcpy(native.data(),input,sizeof(native));
        current.nativeInput=native;
        current.sample={pose(native.data()),{},0,0,false};
        if(nativePairVerified.load())current.sample=mgs5vr::headCamera().resolveCurrent(current.camera,current.sample.nativePose);
        alignas(16) auto adjusted=values(current.sample.nativePose);
        auto* result=originalWorld(current.sample.applied?static_cast<void*>(adjusted.data()):input,output);
        std::memcpy(current.world.data(),output,sizeof(current.world));
        current.applied=current.sample.applied;current.haveWorld=true;
        return result;
    }
    // The inverse builder invokes this function synchronously in the same native
    // publication. Reuse the exact pose selected for the world matrix.
    if(caller==base+0x438c66&&current.haveWorld&&current.camera+0xf0==source&&current.applied){
        alignas(16) auto adjusted=values(current.sample.nativePose);
        return originalWorld(adjusted.data(),output);
    }
    return originalWorld(input,output);
}
__declspec(noinline) float* view(void* input,float* output){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    auto* result=originalView(input,output);
    if(enabled.load()&&caller==base+0x437c90&&current.haveWorld&&current.camera==reinterpret_cast<uintptr_t>(input)+0x30){
        std::memcpy(current.view.data(),output,sizeof(current.view));
        try{record(current);}catch(...){}
        current.haveWorld=false;
    }
    return result;
}
__declspec(noinline) uintptr_t extents(void* input,float* output){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const auto result=originalExtents(input,output);
    if(insideStereo&&eyeViewport==reinterpret_cast<uintptr_t>(input)&&gpuProjection){
        const auto* matrix=reinterpret_cast<const float*>(eyeViewport+0x280);
        output[0]=1/matrix[0];output[1]=1/matrix[5];output[2]=matrix[14];output[3]=matrix[10];
    }
    thread_local uint64_t last{};const auto now=GetTickCount64();
    if(!enabled.load()||now-last<2000)return result;
    last=now;
    ViewportSample next;next.viewport=reinterpret_cast<uintptr_t>(input);next.caller=caller;next.tick=now;next.thread=GetCurrentThreadId();
    next.stackCount=CaptureStackBackTrace(1,static_cast<DWORD>(next.stack.size()),next.stack.data(),nullptr);
    std::array<unsigned char,0x5e4> bytes{};SIZE_T copied{};
    if(!ReadProcessMemory(GetCurrentProcess(),input,bytes.data(),bytes.size(),&copied)||copied!=bytes.size())return result;
    std::memcpy(next.extents.data(),output,sizeof(next.extents));
    std::memcpy(next.matrices.data(),bytes.data()+0x280,sizeof(next.matrices));
    std::memcpy(&next.camera,bytes.data()+0x570,sizeof(next.camera));
    std::memcpy(&next.width,bytes.data()+0x5d8,sizeof(next.width));std::memcpy(&next.height,bytes.data()+0x5dc,sizeof(next.height));
    std::memcpy(&next.scale,bytes.data()+0x5e0,sizeof(next.scale));
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(next.camera+0x30),next.cameraMatrices.data(),sizeof(next.cameraMatrices),&copied)
        ||copied!=sizeof(next.cameraMatrices))return result;
    try{
        std::unique_lock lock(latestMutex,std::try_to_lock);
        if(lock.owns_lock())for(auto& slot:viewports)if(!slot.viewport||slot.viewport==next.viewport){slot=next;break;}
    }catch(...){}
    return result;
}
template<size_t N> bool matches(uintptr_t address,const std::array<unsigned char,N>& expected){
    std::array<unsigned char,N> actual{};SIZE_T copied{};
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),actual.data(),N,&copied)&&copied==N&&actual==expected;
}
}
namespace mgs5vr {
void installRenderCamera(uintptr_t moduleBase,const std::filesystem::path& directory){
    constexpr std::array<unsigned char,10> worldEntry{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x70};
    constexpr std::array<unsigned char,8> viewEntry{0x48,0x8b,0xc4,0x48,0x89,0x58,0x08,0x55};
    constexpr std::array<unsigned char,8> extentsEntry{0xf3,0x0f,0x10,0x0d,0x24,0x23,0xee,0x01};
    constexpr std::array<unsigned char,9> viewportEntry{0x40,0x57,0x48,0x81,0xec,0xa0,0,0,0};
    constexpr std::array<unsigned char,10> projectionEntry{0x48,0x8b,0xc4,0x48,0x81,0xec,0x88,0,0,0};
    constexpr std::array<unsigned char,12> sceneEntry{0x40,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57};
    constexpr std::array<unsigned char,17> registerEntry{0x40,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0x6c,0x24,0xd9};
    constexpr std::array<unsigned char,16> listenerEntry{0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x57};
    constexpr std::array<unsigned char,17> virtualListenerEntry{0x41,0x0f,0x28,0,0xc7,0x02,0,0,0,0,0x48,0x8b,0xc2,0x0f,0x29,0x41,0x40};
    constexpr std::array<unsigned char,15> listenerCaller{0x4c,0x8b,0xc0,0x48,0x8d,0x55,0x07,0x48,0x8b,0xcf,0xe8,0x5e,0x23,0x93,0x01};
    constexpr std::array<unsigned char,15> virtualListenerCaller{0x4c,0x8b,0xc0,0x48,0x8d,0x55,0x07,0x48,0x8b,0xcf,0xe8,0x02,0x24,0x93,0x01};
    if(!matches(moduleBase+0x438ac0,worldEntry)||!matches(moduleBase+0x438c20,viewEntry)||!matches(moduleBase+0x1c4fa0,extentsEntry)
        ||!matches(moduleBase+0x1b9490,viewportEntry)||!matches(moduleBase+0x241b00,projectionEntry)||!matches(moduleBase+0x1beec0,sceneEntry)
        ||!matches(moduleBase+0x2496a0,registerEntry)||!matches(moduleBase+0x1d6a490,listenerEntry)
        ||!matches(moduleBase+0x1d6a550,virtualListenerEntry)||!matches(moduleBase+0x438123,listenerCaller)
        ||!matches(moduleBase+0x43813f,virtualListenerCaller))throw std::runtime_error("Native scene/matrix/listener signature mismatch");
    base=moduleBase;
    if(!directory.empty()){
        std::filesystem::create_directories(directory);
        evidence.open(directory/("matrices-"+std::to_string(GetCurrentProcessId())+".jsonl"));
        if(!evidence)throw std::runtime_error("Cannot open native matrix evidence");
        evidence<<std::setprecision(9)<<"{\"schema\":1,\"image_base\":"<<base<<",\"paired_native_publication\":true,\"joined_to_present\":false}\n";
    }
    struct Hook {uintptr_t rva;void* wrapper;void** original;};
    const std::array<Hook,9> hooks{{
        {0x438ac0,reinterpret_cast<void*>(&world),reinterpret_cast<void**>(&originalWorld)},
        {0x438c20,reinterpret_cast<void*>(&view),reinterpret_cast<void**>(&originalView)},
        {0x1c4fa0,reinterpret_cast<void*>(&extents),reinterpret_cast<void**>(&originalExtents)},
        {0x1b9490,reinterpret_cast<void*>(&viewport),reinterpret_cast<void**>(&originalViewport)},
        {0x241b00,reinterpret_cast<void*>(&projection),reinterpret_cast<void**>(&originalProjection)},
        {0x1beec0,reinterpret_cast<void*>(&scene),reinterpret_cast<void**>(&originalScene)},
        {0x2496a0,reinterpret_cast<void*>(&registerTarget),reinterpret_cast<void**>(&originalRegisterTarget)},
        {0x1d6a490,reinterpret_cast<void*>(&listener),reinterpret_cast<void**>(&originalListener)},
        {0x1d6a550,reinterpret_cast<void*>(&virtualListener),reinterpret_cast<void**>(&originalVirtualListener)}}};
    for(const auto& hook:hooks){const auto result=MH_CreateHook(reinterpret_cast<void*>(base+hook.rva),hook.wrapper,hook.original);
        if(result!=MH_OK)throw std::runtime_error(std::string("Native scene hook: ")+MH_StatusToString(result));
    }
    for(const auto& hook:hooks)if(MH_EnableHook(reinterpret_cast<void*>(base+hook.rva))!=MH_OK){
        for(const auto& installed:hooks)MH_DisableHook(reinterpret_cast<void*>(base+installed.rva));
        throw std::runtime_error("Cannot enable native scene/matrix hooks");
    }
    enabled.store(true);
    try{installUiRenderer(base);}catch(const std::exception& ex){log(std::string("Native UI integration unavailable: ")+ex.what());}
    log("Native camera matrix integration installed; head control remains off until explicitly toggled");
    log("Native listener integration installed; guarded by the active source camera publication");
}
void reportRenderCamera(){
    if(!evidence||reports>=1024)return;
    std::array<Pair,8> snapshot;
    PresentTrace presentSnapshot;Pair failureSnapshot;std::array<ViewportSample,8> viewportSnapshot;ListenerProof audioSnapshot;
    {std::lock_guard lock(latestMutex);snapshot=latest;presentSnapshot=presentTrace;failureSnapshot=lastMatrixFailure;viewportSnapshot=viewports;audioSnapshot=listenerProof;}
    const auto status=headCamera().status();
    reportUiRenderer(evidence);
    evidence<<"{\"event\":\"native_scene_pairs\",\"calls\":"<<sceneCalls.load()<<",\"pairs\":"<<scenePairs.load()
        <<",\"copies\":"<<sceneCopies.load()<<",\"rejected\":"<<sceneRejected.load()<<",\"thread\":"<<sceneThread.load()
        <<",\"d3d_context_type\":"<<sceneContextType.load()<<",\"failure\":"<<sceneFailure.load()
        <<",\"duplicate_presents_skipped\":"<<duplicatePresentsSkipped.load()<<"}\n";
    const auto array=[&](const auto& data){evidence<<'[';for(size_t n=0;n<data.size();++n){if(n)evidence<<',';if(std::isfinite(data[n]))evidence<<data[n];else evidence<<"null";}evidence<<']';};
    if(audioSnapshot.sequence){
        evidence<<"{\"event\":\"native_listener\",\"updates\":"<<listenerUpdates.load()<<",\"virtual_updates\":"<<virtualListenerUpdates.load()
            <<",\"failures\":"<<listenerFailures.load()<<",\"camera\":"<<audioSnapshot.camera<<",\"listener\":"<<audioSnapshot.listener
            <<",\"source_sequence\":"<<audioSnapshot.sequence<<",\"tracking_sequence\":"<<audioSnapshot.tracking
            <<",\"activation\":"<<audioSnapshot.activation<<",\"tick_ms\":"<<audioSnapshot.tick
            <<",\"primary_accepted\":"<<(audioSnapshot.primaryAccepted?"true":"false")
            <<",\"virtual_accepted\":"<<(audioSnapshot.virtualAccepted?"true":"false")<<",\"native_pose\":";array(audioSnapshot.native);
        evidence<<",\"submitted_pose\":";array(audioSnapshot.submitted);evidence<<",\"consumed_pose\":";array(audioSnapshot.consumed);
        evidence<<",\"virtual_consumed_pose\":";array(audioSnapshot.virtualConsumed);evidence<<"}\n";
    }
    evidence<<"{\"event\":\"native_command_capture\",\"finish_execute_taggedFinish_taggedExecute_complete_failure_pendingFamilies_pendingLists\":";
    array(sceneCaptureCounters());evidence<<"}\n";
    for(const auto& p:snapshot)if(p.camera){
        evidence<<"{\"tick_ms\":"<<p.tick<<",\"camera\":\"0x"<<std::hex<<p.camera<<std::dec<<"\",\"sequence\":"<<p.sequence
            <<",\"thread\":"<<p.thread<<",\"tracking_sequence\":"<<p.sample.trackingSequence<<",\"applied\":"<<(p.applied?"true":"false")
            <<",\"inverse_valid\":"<<(p.validInverse?"true":"false")
            <<",\"player_sequence\":"<<p.sample.playerSequence<<",\"player_owner\":"<<p.sample.playerOwner
            <<",\"rig_sequence\":"<<p.sample.rigSequence<<",\"predicted_xr_time\":"<<p.sample.controllers.predictedXrTime
            <<",\"player_head\":";array(std::array<float,3>{p.sample.playerHead.x,p.sample.playerHead.y,p.sample.playerHead.z});
        evidence<<",\"pose\":";array(values(p.sample.nativePose));
        evidence<<",\"world\":";array(p.world);evidence<<",\"view\":";array(p.view);
        evidence<<",\"inverse_error\":"<<p.inverseError<<",\"active\":"<<(status.active?"true":"false")
            <<",\"stop_reason\":"<<static_cast<unsigned>(status.reason)<<",\"cancellations\":"<<status.cancellations
            <<",\"awaiting_player\":"<<(status.awaitingPlayer?"true":"false")<<"}\n";
    }
    if(failureSnapshot.sequence){
        evidence<<"{\"event\":\"matrix_failure\",\"sequence\":"<<failureSnapshot.sequence<<",\"inverse_error\":"<<failureSnapshot.inverseError
            <<",\"pose\":";array(values(failureSnapshot.sample.nativePose));
        evidence<<",\"world\":";array(failureSnapshot.world);evidence<<",\"view\":";array(failureSnapshot.view);evidence<<"}\n";
    }
    if(presentSnapshot.count){
        evidence<<"{\"event\":\"present_stack\",\"frame\":"<<presentSnapshot.frame<<",\"tick_ms\":"<<presentSnapshot.tick
            <<",\"thread\":"<<presentSnapshot.thread<<",\"stack\":[";
        for(USHORT n=0;n<presentSnapshot.count;++n){if(n)evidence<<',';evidence<<"\"0x"<<std::hex<<reinterpret_cast<uintptr_t>(presentSnapshot.stack[n])<<std::dec<<'"';}
        evidence<<"]}\n";
    }
    for(const auto& v:viewportSnapshot)if(v.viewport){
        evidence<<"{\"event\":\"viewport_extents\",\"viewport\":\"0x"<<std::hex<<v.viewport<<"\",\"gr_camera\":\"0x"<<v.camera
            <<"\",\"caller_rva\":\"0x"<<v.caller-base<<std::dec<<"\",\"tick_ms\":"<<v.tick<<",\"thread\":"<<v.thread
            <<",\"width\":"<<v.width<<",\"height\":"<<v.height<<",\"scale\":"<<v.scale<<",\"extents\":";array(v.extents);
        evidence<<",\"viewport_280_4bf\":";array(v.matrices);evidence<<",\"gr_camera_world_view_previous\":";array(v.cameraMatrices);
        evidence<<",\"stack\":[";for(USHORT n=0;n<v.stackCount;++n){if(n)evidence<<',';evidence<<"\"0x"<<std::hex<<reinterpret_cast<uintptr_t>(v.stack[n])<<std::dec<<'"';}evidence<<"]}\n";
    }
    evidence.flush();++reports;
}
EyeFrame observeRenderPresent(void*) noexcept {
    if(!enabled.load())return {};
    const auto frame=++presentCount;if(frame%300!=1)return {};
    PresentTrace next;next.frame=frame;next.tick=GetTickCount64();next.thread=GetCurrentThreadId();
    next.count=CaptureStackBackTrace(1,static_cast<DWORD>(next.stack.size()),next.stack.data(),nullptr);
    try{std::unique_lock lock(latestMutex,std::try_to_lock);if(lock.owns_lock())presentTrace=next;}catch(...){}
    return {}; // Scene-capture command-list metadata owns the image/pose join.
}
void stopRenderCamera() noexcept {enabled.store(false);stopUiRenderer();headCamera().cancel();try{reportRenderCamera();evidence.close();}catch(...){}}
}
