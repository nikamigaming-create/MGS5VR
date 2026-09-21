#include "mgs5vr/render_camera.hpp"
#include "mgs5vr/head_camera.hpp"
#include "mgs5vr/idroid_rig.hpp"
#include "mgs5vr/opening_selector.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/log.hpp"
#include "mgs5vr/optic_renderer.hpp"
#include "mgs5vr/optic_markers.hpp"
#include "mgs5vr/small_animal.hpp"
#include "mgs5vr/player_visibility.hpp"
#include "mgs5vr/scene_capture.hpp"
#include "mgs5vr/ui_renderer.hpp"
#include "mgs5vr/menu_surface.hpp"
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
#include <vector>

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
using PublisherFn=uintptr_t(*)(void*);
PublisherFn originalPublisher{};
thread_local uintptr_t publicationOwner{};
uintptr_t base{};
mgs5vr::RenderBuild renderBuild=mgs5vr::RenderBuild::phantomPain_1_0_15_4;
mgs5vr::RenderLayout layout=mgs5vr::phantomPainRender;
struct Pair {
    uintptr_t camera{},identity{};
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
std::atomic_uint64_t lastCameraPublication{};
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
std::atomic_bool trackedNearReported{};
thread_local uintptr_t stereoTarget{};
thread_local uint32_t sceneRenderPass{};
thread_local mgs5vr::EyeFrame drawingEye{};
thread_local bool clipProjection{},gpuProjection{},insideStereo{};
std::atomic_uint64_t sceneCalls{},scenePairs{},sceneRejected{},sceneCopies{};
std::atomic_uint64_t duplicatePresentsSkipped{};
std::atomic_uint32_t sceneContextType{99};
std::atomic_uint32_t sceneFailure{};
std::atomic<DWORD> sceneThread{};
__declspec(noinline) uintptr_t publishCamera(void* publisher){
    // GZ replaces temporary source-camera objects while a stable scene-camera
    // publisher continues to own the viewport. Do not bind HMD activation to
    // the address of that temporary pose buffer.
    struct Scope {
        uintptr_t previous{publicationOwner};
        explicit Scope(void* owner){publicationOwner=reinterpret_cast<uintptr_t>(owner);}
        ~Scope(){publicationOwner=previous;}
    } scope(publisher);
    return originalPublisher(publisher);
}
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
    lastCameraPublication.store(p.tick);
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
    if(!slot)slot=&*std::min_element(latest.begin(),latest.end(),[](const auto& a,const auto& b){return a.tick<b.tick;});
    *slot=p;
}
__declspec(noinline) void* listener(void* object,void* output,const float* input){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    if(!enabled.load()||caller!=base+layout.listenerReturn)return originalListener(object,output,input);
    primaryListener=0;primaryListenerSequence=0;
    // This exact camera publication selects its listener through publisher+0x60.
    // An explicitly selected alternate listener transform remains native.
    if(!current.applied||!current.validInverse||!current.sequence||!object
        ||reinterpret_cast<uintptr_t>(input)!=current.camera+layout.cameraPose
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
    if(!enabled.load()||caller!=base+layout.virtualListenerReturn||primaryListener!=reinterpret_cast<uintptr_t>(object)
        ||!primaryListenerSequence||primaryListenerSequence!=current.sequence
        ||(source!=current.camera+layout.cameraPose&&source!=current.camera+layout.alternateListenerPose))return originalVirtualListener(object,output,input);
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
    const bool visibilityClip=enabled.load()&&visibilityViewport&&caller==base+layout.clipReturn
        &&reinterpret_cast<uintptr_t>(output)==visibilityViewport+layout.clipProjection;
    // Visibility is prepared before scene replay. Only its owned clip matrix
    // needs the closer plane here; the native center camera/GPU matrix stays
    // untouched. On TPP 1.0.15.4, the final two arguments are near and far.
    if(visibilityClip&&layout.cameraNearPlane&&!current.sample.controllers.frontEnd)
        h=mgs5vr::trackedNearPlane(h,i);
    const auto result=originalProjection(output,a,b,c,d,e,f,g,h,i);
    if(enabled.load()&&layout.uiProjectionReturn&&caller==base+layout.uiProjectionReturn)mgs5vr::applyUiEyeProjection(output);
    if(visibilityClip){
        std::array<float,16> matrix{};std::memcpy(matrix.data(),output,sizeof(matrix));
        if(mgs5vr::widenVisibilityProjection(matrix,current.sample.headPose,current.sample.views)){
            std::memcpy(output,matrix.data(),sizeof(matrix));
            if(++visibilityUpdates==1)mgs5vr::log("Native visibility projection covers both tracked eyes with a symmetric turn margin");
        }
    }
    if(!enabled.load()||!eyeViewport)return result;
    const bool clip=caller==base+layout.clipReturn&&reinterpret_cast<uintptr_t>(output)==eyeViewport+layout.clipProjection;
    const bool gpu=caller==base+layout.gpuReturn&&reinterpret_cast<uintptr_t>(output)==eyeViewport+layout.gpuProjection;
    if(!clip&&!gpu)return result;
    // The optical pass shares the native scene's visibility preparation.
    // Its narrow zoom belongs ONLY to the raster projection, never to the
    // clipping/frustum preparation reused by the subsequent full-size eyes.
    if(clip&&drawingEye.eye==2){clipProjection=true;return result;}
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
    if(enabled.load()&&!insideStereo&&caller==base+layout.viewportReturn&&current.applied&&current.validInverse
        &&current.sample.stereoTracked&&now>=current.sample.sampleTime&&now-current.sample.sampleTime<=150){
        const auto camera=field<uintptr_t>(input,layout.viewportCamera);
        if(camera&&std::memcmp(reinterpret_cast<void*>(camera+0x70),current.view.data(),sizeof(current.view))==0)
            visibilityViewport=reinterpret_cast<uintptr_t>(input);
    }
    const auto result=originalViewport(input,history);
    visibilityViewport=priorVisibility;
    if(enabled.load()&&!insideStereo&&caller==base+layout.viewportReturn&&current.applied&&current.validInverse&&current.sample.stereoTracked){
        const auto camera=field<uintptr_t>(input,layout.viewportCamera);
        if(camera&&std::memcmp(reinterpret_cast<void*>(camera+0x70),current.view.data(),sizeof(current.view))==0){
            std::lock_guard lock(sceneMutex);sceneSource={reinterpret_cast<uintptr_t>(input),camera,current};
        }
    }
    return result;
}
struct NativeRestore {
    uintptr_t camera{},viewport{};
    std::array<float,2> cameraPlanes{};
    float cameraFocal{},viewportAspect{};
    std::array<unsigned char,0xc0> cameraMatrices{};
    std::array<unsigned char,0x240> viewportMatrices{};
    NativeRestore(uintptr_t c,uintptr_t v):camera(c),viewport(v){
        std::memcpy(cameraMatrices.data(),reinterpret_cast<void*>(c+0x30),cameraMatrices.size());
        std::memcpy(viewportMatrices.data(),reinterpret_cast<void*>(v+layout.viewportMatrices),viewportMatrices.size());
        if(layout.cameraNearPlane)std::memcpy(cameraPlanes.data(),reinterpret_cast<void*>(c+layout.cameraNearPlane),sizeof(cameraPlanes));
        if(layout.cameraFocalScale)std::memcpy(&cameraFocal,reinterpret_cast<void*>(c+layout.cameraFocalScale),sizeof(cameraFocal));
        std::memcpy(&viewportAspect,reinterpret_cast<void*>(v+layout.viewportScale),sizeof(viewportAspect));
    }
    void restore() const{
        std::memcpy(reinterpret_cast<void*>(camera+0x30),cameraMatrices.data(),cameraMatrices.size());
        std::memcpy(reinterpret_cast<void*>(viewport+layout.viewportMatrices),viewportMatrices.data(),viewportMatrices.size());
        if(layout.cameraNearPlane)std::memcpy(reinterpret_cast<void*>(camera+layout.cameraNearPlane),cameraPlanes.data(),sizeof(float));
        if(layout.cameraFocalScale)std::memcpy(reinterpret_cast<void*>(camera+layout.cameraFocalScale),&cameraFocal,sizeof(cameraFocal));
        std::memcpy(reinterpret_cast<void*>(viewport+layout.viewportScale),&viewportAspect,sizeof(viewportAspect));
    }
    void applyNativeProjectionScales(mgs5vr::EyeFov fov) const{
        if(!layout.cameraFocalScale)return;
        std::array<float,16> projection{};
        std::memcpy(projection.data(),viewportMatrices.data()+layout.gpuProjection-layout.viewportMatrices,sizeof(projection));
        const auto scales=mgs5vr::nativeProjectionScales(cameraFocal,viewportAspect,projection,fov);
        if(!scales)return;
        // Updating only the final matrix leaves camera/FOV consumers on the
        // desktop focal and aspect scales. Publish matching native parameters
        // for this replay, then restore them before the next eye/native pass.
        std::memcpy(reinterpret_cast<void*>(camera+layout.cameraFocalScale),&scales->focal,sizeof(float));
        std::memcpy(reinterpret_cast<void*>(viewport+layout.viewportScale),&scales->aspect,sizeof(float));
        static std::atomic_bool reported{};
        if(!reported.exchange(true))mgs5vr::log("Native camera focal/aspect parameters match the replayed eye projection: focal="
            +std::to_string(cameraFocal)+" -> "+std::to_string(scales->focal)+" aspect="
            +std::to_string(viewportAspect)+" -> "+std::to_string(scales->aspect));
    }
    void applyTrackedNearPlane() const{
        if(!layout.cameraNearPlane)return;
        const float nearClip=mgs5vr::trackedNearPlane(cameraPlanes[0],cameraPlanes[1]);
        if(!std::isfinite(nearClip)||nearClip==cameraPlanes[0])return;
        // Change the source field only during this exact scene replay, not
        // just projection Z terms after the builder. Native deferred depth
        // reconstruction and both clip/GPU matrices then consume one plane.
        std::memcpy(reinterpret_cast<void*>(camera+layout.cameraNearPlane),&nearClip,sizeof(nearClip));
        if(!trackedNearReported.exchange(true))mgs5vr::log("TPP tracked scene uses a 2 cm native near plane; native camera restored between passes");
    }
    void preserveOpticVisibility() const{
        if(renderBuild!=mgs5vr::RenderBuild::phantomPain_1_0_15_4)return;
        // TPP viewport builder 0x1b9490 derives six normalized world-space
        // visibility planes at +0x440..+0x49f from clip*view. The optical pass
        // may change its raster camera, but reuses the head scene's prepared
        // draw/LOD lists. Preserve that exact wide visibility volume instead
        // of replacing it with a volume centered on the hand-held optic.
        constexpr size_t planes=0x440,bytes=6*4*sizeof(float);
        std::memcpy(reinterpret_cast<void*>(viewport+planes),
            viewportMatrices.data()+planes-layout.viewportMatrices,bytes);
    }
    ~NativeRestore(){restore();mgs5vr::clearUiRenderSource();eyeViewport=0;stereoTarget=0;drawingEye={};insideStereo=false;}
};
// The magnified native draw changes shared visibility/LOD state.
// Preserve both full head draws while the independent lens is rendered, then
// finish their overlays from the same source transaction. This keeps three
// native draws and never borrows an optic image from the preceding frame.
struct HeadSceneCopy {
    mgs5vr::ComPtr<ID3D11Texture2D> texture;
    uint64_t source{};
    bool transfer(ID3D11DeviceContext* context,uint64_t sourceId,bool restore){
        if(!context)return false;
        mgs5vr::ComPtr<ID3D11Texture2D> target;
        if(!mgs5vr::sceneSourceTexture(context,target.GetAddressOf()))return false;
        D3D11_TEXTURE2D_DESC desc{},prior{};target->GetDesc(&desc);
        mgs5vr::ComPtr<ID3D11Device> device,copyDevice;target->GetDevice(&device);
        if(texture){texture->GetDesc(&prior);texture->GetDevice(&copyDevice);}
        const bool compatible=texture&&copyDevice.Get()==device.Get()&&desc.Width==prior.Width
            &&desc.Height==prior.Height&&desc.Format==prior.Format&&desc.SampleDesc.Count==prior.SampleDesc.Count;
        if(restore&&(!compatible||source!=sourceId))return false;
        if(!restore&&!compatible){
            texture.Reset();desc.BindFlags=0;desc.MiscFlags=desc.CPUAccessFlags=0;desc.Usage=D3D11_USAGE_DEFAULT;
            if(FAILED(device->CreateTexture2D(&desc,nullptr,texture.GetAddressOf())))return false;
        }
        ID3D11RenderTargetView* targets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
        ID3D11DepthStencilView* depth{};
        context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,targets,&depth);
        context->OMSetRenderTargets(0,nullptr,nullptr);
        context->CopyResource(restore?target.Get():texture.Get(),restore?texture.Get():target.Get());
        context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,targets,depth);
        for(auto* view:targets)if(view)view->Release();
        if(depth)depth->Release();
        if(!restore)source=sourceId;
        return true;
    }
};
thread_local std::array<HeadSceneCopy,2> opticHeadScenes;
__declspec(noinline) uintptr_t registerTarget(void* graphics,void* target){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const bool second=enabled.load()&&insideStereo&&sceneRenderPass>0&&stereoTarget
        &&reinterpret_cast<uintptr_t>(target)==stereoTarget&&caller==base+layout.registerReturn;
    const auto before=second?field<uint32_t>(graphics,layout.presentCount):0;
    // The active D3D11 implementation performs required GPU setup before
    // appending to its present vector. Always execute that native setup.
    const auto result=originalRegisterTarget(graphics,target);
    if(second&&before){
        const auto after=field<uint32_t>(graphics,layout.presentCount);
        const auto capacity=field<uint32_t>(graphics,layout.presentCapacity);
        const auto data=field<uintptr_t>(graphics,layout.presentData);
        if(after==before+1&&after<=capacity&&data
            &&field<uintptr_t>(reinterpret_cast<void*>(data),size_t(before-1)*8)==stereoTarget
            &&field<uintptr_t>(reinterpret_cast<void*>(data),size_t(before)*8)==stereoTarget){
            // The render job has not published this vector to its presentation
            // worker yet. Remove only the append produced by this eye's call.
            std::memcpy(static_cast<unsigned char*>(graphics)+layout.presentCount,&before,sizeof(before));
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
    bool contains=false;auto candidate=field<uintptr_t>(render,layout.renderViewports);
    for(unsigned n=0;candidate&&n<16;++n){if(candidate==source.viewport){contains=true;break;}candidate=field<uintptr_t>(reinterpret_cast<void*>(candidate),layout.viewportNext);}
    const auto now=mgs5vr::steadyMilliseconds();
    // The first camera update can precede tracked skin publication. Do not
    // submit that exposed third-person arm pose as the first VR eye pair.
    // Native Pause stops animation publication. Its existing world geometry
    // remains drawable from fresh tracked eye cameras; requiring another skin
    // update would black out the menu. A spatial menu can only be anchored
    // after an accepted gameplay view, and all camera/transaction checks below
    // still apply. Gameplay continues to require its current tracked skin.
    if(mgs5vr::controllerRigEnabled()&&!source.pair.sample.rigSequence&&!source.pair.sample.menuOpen
       &&!source.pair.sample.controllers.frontEnd&&!source.pair.sample.controllers.avatarEditor
       &&!source.pair.sample.controllers.authoredCamera){++sceneRejected;return originalScene(render,graphics,task,worker);}
    if(!contains||source.pair.sample.activation!=status.activation||now<source.pair.sample.sampleTime||now-source.pair.sample.sampleTime>150
        ||std::memcmp(reinterpret_cast<void*>(source.grCamera+0x30),source.pair.world.data(),sizeof(source.pair.world))){++sceneRejected;return originalScene(render,graphics,task,worker);}
    const auto contextOwner=field<uintptr_t>(graphics,layout.graphicsContext);
    auto* context=contextOwner?field<ID3D11DeviceContext*>(reinterpret_cast<void*>(contextOwner),8):nullptr;
    if(!context){++sceneRejected;return originalScene(render,graphics,task,worker);}
    sceneContextType.store(context->GetType());
    NativeRestore saved(source.grCamera,source.viewport);insideStereo=true;stereoTarget=field<uintptr_t>(render,layout.renderTarget);
    alignas(16) std::array<float,16> authoredView{},authoredProjection{};
    // Every native UI camera inherits this viewport's aspect, including the
    // field HUD. Preserve its canvas projection before any eye replay changes
    // the scalar; a title-only snapshot leaves gameplay correction disabled.
    std::memcpy(authoredProjection.data(),saved.viewportMatrices.data(),sizeof(authoredProjection));
    if(source.pair.sample.controllers.frontEnd){
        // Title layout retains the native publication camera; the stereo
        // replay alone supplies the tracked cabin cameras.
        authoredView=source.pair.view;
    }
    mgs5vr::publishOpticMarkerFrame(source.pair.sample);
    const auto markerSnapshot=mgs5vr::opticWaypoints();
    const auto cameraCount=pairCount.load();uintptr_t result{};bool complete=true;
    mgs5vr::beginSceneTiming(context,id);
    const bool titleSurface=source.pair.sample.controllers.frontEnd
        &&!source.pair.sample.controllers.scriptedDemo;
    // The front end has no player-hand interaction. Exclude this player's
    // verified model groups from BOTH eye replays as well as the panel source;
    // hiding only the source leaves tracked arms floating around the title.
    // The guard restores the exact native visibility flags on every exit.
    mgs5vr::MenuCapturePlayerExclusion excludeTitlePlayer(titleSurface?source.pair.sample.playerOwner:0,
        titleSurface&&source.pair.sample.controllers.openingSelector);
    // The telescope has one real ocular. Draw its own narrow-angle native
    // scene after both ordinary HMD eyes. All three draws use the
    // same simulation/hand publication and only one native present is queued.
    const auto& optic=source.pair.sample.controllers.optic;
    const auto& scope=source.pair.sample.weaponScope;
    const bool scopeAtEye=std::any_of(source.pair.sample.views.begin(),source.pair.sample.views.end(),
        [&](const auto& eye){return mgs5vr::weaponScopeEyeVisible(scope,eye.pose);});
    const auto scopeView=!optic.held&&!source.pair.sample.menuOpen&&scopeAtEye
        ?mgs5vr::weaponScopeSceneView(scope):std::nullopt;
    const bool binocularAtEye=optic.held&&optic.active&&std::any_of(
        source.pair.sample.views.begin(),source.pair.sample.views.end(),
        [&](const auto& eye){return mgs5vr::binocularEyeVisible(optic.pose,eye.pose);});
    const auto opticView=binocularAtEye?mgs5vr::binocularSceneView(optic.pose,
        source.pair.sample.controllers.magnification):scopeView;
    mgs5vr::ComPtr<ID3D11Texture2D> opticScene;
    const uint32_t extraPass=opticView?1u:0u;
    for(uint32_t pass=0;pass<(extraPass?5u:2u);++pass){
        sceneRenderPass=pass;
        const bool headPreparation=extraPass&&pass<2;
        const bool lensPass=extraPass&&pass==2;
        const bool restoreHead=extraPass&&pass>=3;
        const uint32_t eye=restoreHead?pass-3:pass;
        // Share this pass role with native UI workers and custom markers.
        // Equipping binoculars is not recon vision; the source-frame optic
        // must be aligned with the eye. A rifle's lens is never binoculars.
        const auto hudView=mgs5vr::hudViewForPass(eye,optic.held&&optic.active
            &&optic.pose.tracked&&optic.pose.kind==mgs5vr::OpticKind::binocular);
        saved.restore();eyeViewport=source.viewport;clipProjection=gpuProjection=false;
        drawingEye={lensPass?*opticView:source.pair.sample.views[eye],id,source.pair.sample.trackingSequence,status.activation,source.pair.sample.sampleTime,eye,false,false};
        // FOX sky/lighting expects a centered render projection. Keep the
        // runtime's asymmetric optics separately and crop the enclosing image
        // at submission. The handheld camera alone owns magnification.
        drawingEye.displayFov=drawingEye.view.fov;
        drawingEye.magnification=1.f;
        const auto renderFov=mgs5vr::enclosingEyeFov(drawingEye.displayFov);
        if(!renderFov){complete=false;sceneFailure=4;break;}
        drawingEye.view.fov=*renderFov;
        const auto native=mgs5vr::nativeEyePose(source.pair.sample.nativePose,source.pair.sample.headPose,drawingEye.view.pose);
        alignas(16) auto nativeValues=values(native);
        alignas(16) std::array<float,16> eyeWorld{},eyeView{};
        alignas(16) std::array<unsigned char,0x140> inverseInput{};
        std::memcpy(inverseInput.data()+layout.inversePose,nativeValues.data(),sizeof(nativeValues));
        originalWorld(nativeValues.data(),eyeWorld.data());originalView(inverseInput.data(),eyeView.data());
        if(std::max(inverseError(eyeWorld,eyeView),inverseError(eyeView,eyeWorld))>=0.003f){complete=false;sceneFailure=3;break;}
        std::memcpy(reinterpret_cast<void*>(source.grCamera+0x30),eyeWorld.data(),sizeof(eyeWorld));
        std::memcpy(reinterpret_cast<void*>(source.grCamera+0x70),eyeView.data(),sizeof(eyeView));
        // Until per-eye temporal resources are isolated, publish a zero-motion
        // camera history. This avoids introducing the opposite eye's history.
        std::memcpy(reinterpret_cast<void*>(source.grCamera+0xb0),eyeView.data(),sizeof(eyeView));
        if(!titleSurface)saved.applyTrackedNearPlane();
        originalViewport(reinterpret_cast<void*>(source.viewport),0);
        if(!clipProjection||!gpuProjection){complete=false;sceneFailure=4;break;}
        if(lensPass)saved.preserveOpticVisibility();
        // Keep native visibility preparation independent of the telescope's
        // narrow raster FOV. Update scalar consumers only after clip/GPU build.
        // The optical raster has its own narrow projection. Keep the shared
        // camera's focal/LOD consumers on the HMD scale: those native jobs
        // outlive this draw and must not cull the peripheral world as though
        // the user were looking only through the magnified camera.
        saved.applyNativeProjectionScales(lensPass
            ?mgs5vr::enclosingEyeFov(source.pair.sample.views[0].fov).value_or(source.pair.sample.views[0].fov)
            :drawingEye.view.fov);
        std::memcpy(reinterpret_cast<void*>(source.viewport+layout.previousView),eyeView.data(),sizeof(eyeView));
        std::memcpy(reinterpret_cast<void*>(source.viewport+layout.previousProjection),reinterpret_cast<void*>(source.viewport+layout.gpuProjection),sizeof(eyeView));
        drawingEye.projected=true;
        const auto eyeProjection=field<std::array<float,16>>(reinterpret_cast<void*>(source.viewport),layout.gpuProjection);
        mgs5vr::setUiRenderSource(drawingEye,source.grCamera,eyeView,eyeProjection,source.pair.sample,authoredView,authoredProjection,hudView);
        mgs5vr::ReconModelVisibilityScope reconVisibility(source.pair.sample.controllers.hudMode,
            hudView,source.pair.sample.controllers.binocularActorGlow);
        if(!restoreHead)result=originalScene(render,graphics,task,worker);
        mgs5vr::clearUiRenderSource();
        // Native passes may finish and replace the current deferred context.
        const auto afterOwner=field<uintptr_t>(graphics,layout.graphicsContext);
        auto* afterContext=afterOwner?field<ID3D11DeviceContext*>(reinterpret_cast<void*>(afterOwner),8):nullptr;
        if(headPreparation||restoreHead){
            if(!opticHeadScenes[eye].transfer(afterContext,id,restoreHead)){complete=false;sceneFailure=8;break;}
            if(headPreparation)continue;
        }
        if(lensPass){
            // This texture is separate from the stereo mailbox. Capturing an
            // unfinished eye into the mailbox can publish a partial pair and
            // later overwrite pixels that the XR compositor is still reading.
            std::array<float,16> opticProjection{};
            std::memcpy(opticProjection.data(),reinterpret_cast<void*>(source.viewport+layout.gpuProjection),sizeof(opticProjection));
            if(!mgs5vr::capturePhysicalOpticScene(afterContext,eyeView,opticProjection,native.position,opticScene.GetAddressOf(),
                mgs5vr::worldHudVisible(source.pair.sample.controllers.hudMode,hudView),&markerSnapshot))
                mgs5vr::log("Physical optic scene copy unavailable; retaining normal head views");
            continue;
        }
        if(afterContext&&titleSurface&&source.pair.sample.controllers.openingSelector){
            // The title camera is the only authenticated native cabin anchor
            // currently available. Keep the props in that same FOX frame so
            // they move with the authored helicopter shot instead of becoming
            // a head-locked overlay. The shared layout keeps the radio and all
            // six semantic tapes reachable and large enough to read.
            // Use the same title-camera basis as the existing spatial panel.
            // Raw nativePose is the FOX camera heading; placing OpenXR-local
            // props directly on it mirrors the rack away from the viewer.
            const auto& cabin=source.pair.sample.controllers;
            const auto titleOrigin=cabin.openingWorldAnchored?cabin.openingWorldOrigin:
                mgs5vr::nativeTrackedPose(source.pair.sample.nativePose,source.pair.sample.headPose,cabin.openingOrigin);
            const auto offsets=mgs5vr::openingPropOffsets();
            const auto scales=mgs5vr::openingPropScales();
            const auto propWorld=[&](mgs5vr::Vec3 offset,float scale,mgs5vr::Quat orientation=mgs5vr::Quat{}){
                const auto prop=mgs5vr::compose(titleOrigin,mgs5vr::Pose{orientation,offset});
                alignas(16) auto propValues=values(prop);std::array<float,16> world{};
                originalWorld(propValues.data(),world.data());
                // FMDL vertices are authored in meters. Uniformly scale only
                // the local basis, never the translated cabin position.
                for(size_t i=0;i<12;++i)world[i]*=scale;
                return world;
            };
            std::array<std::array<float,16>,7> openingWorlds{};
            for(size_t i=0;i<openingWorlds.size();++i)
                openingWorlds[i]=propWorld(offsets[i],scales[i]);
             // The rack is an owned visual aid.  Animals are never drawn from
             // poses supplied by the VR layer: TppBuddyDog2/TppRat must own
             // their meshes, bones, animation, and world position.
            mgs5vr::drawOpeningProps(afterContext,openingWorlds,eyeView,eyeProjection,
                 source.pair.sample.controllers.openingSelection);
        }
        if(afterContext&&source.pair.sample.controllers.cabinPlay){
            // Cabin play uses the full native scene replay. Do not replace it
            // with the small retained title/loading panel; add animals to the
            // same world frame after the helicopter scene is rendered.
            const auto& cabin=source.pair.sample.controllers;
            const auto cabinOrigin=cabin.openingWorldAnchored?cabin.openingWorldOrigin:
                mgs5vr::nativeTrackedPose(source.pair.sample.nativePose,source.pair.sample.headPose,cabin.openingOrigin);
            const auto propWorld=[&](mgs5vr::Pose local,float scale){
                const auto worldPose=mgs5vr::compose(cabinOrigin,local);
                alignas(16) auto valuesAt=values(worldPose);std::array<float,16> world{};
                originalWorld(valuesAt.data(),world.data());
                for(size_t i=0;i<12;++i)world[i]*=scale;
                return world;
            };
             const auto offsets=mgs5vr::openingPropOffsets();
            const auto scales=mgs5vr::openingPropScales();
            std::array<std::array<float,16>,7> openingWorlds{};
            for(size_t i=0;i<openingWorlds.size();++i)
                openingWorlds[i]=propWorld(mgs5vr::Pose{{},offsets[i]},scales[i]);
             // Keep only the owned rack props.  Real cabin animals are
             // rendered by the native scene; no fallback mesh may impersonate
             // an absent actor.
             mgs5vr::drawOpeningProps(afterContext,openingWorlds,eyeView,eyeProjection,-1);
        }
        if(afterContext&&optic.held&&optic.pose.tracked&&optic.pose.kind==mgs5vr::OpticKind::binocular){
            const auto body=mgs5vr::nativeTrackedPose(source.pair.sample.nativePose,
                source.pair.sample.headPose,optic.pose.renderBody);
            alignas(16) auto bodyValues=values(body);
            alignas(16) std::array<float,16> bodyWorld{};
            std::array<float,16> projection{};
            originalWorld(bodyValues.data(),bodyWorld.data());
            std::memcpy(projection.data(),reinterpret_cast<void*>(source.viewport+layout.gpuProjection),sizeof(projection));
            mgs5vr::drawPhysicalBinoculars(afterContext,bodyWorld,eyeView,projection,
                source.pair.sample.controllers.magnification,opticScene.Get(),
                // Raising the ocular to either eye opens its physical lens
                // for both eyes. Independent pupil-radius gates left the
                // other eye looking at opaque brown glass at normal IPD.
                binocularAtEye);
        }
        if(afterContext&&scopeView&&mgs5vr::weaponScopeEyeVisible(scope,source.pair.sample.views[eye].pose)){
            const auto ocular=mgs5vr::nativeTrackedPose(source.pair.sample.nativePose,source.pair.sample.headPose,scope.ocular);
            alignas(16) auto ocularValues=values(ocular);
            alignas(16) std::array<float,16> ocularWorld{},projection{};
            originalWorld(ocularValues.data(),ocularWorld.data());
            std::memcpy(projection.data(),reinterpret_cast<void*>(source.viewport+layout.gpuProjection),sizeof(projection));
            mgs5vr::drawPhysicalWeaponScope(afterContext,ocularWorld,eyeView,projection,scope.radius,scope.magnification,opticScene.Get());
        }
        if(afterContext&&source.pair.sample.menuOpen&&source.pair.sample.menuIdroid){
            if(const auto idroid=mgs5vr::trackedIdroidPose(source.pair.sample)){
                alignas(16) auto bodyValues=values(idroid->body);
                alignas(16) std::array<float,16> bodyWorld{},projection{};
                originalWorld(bodyValues.data(),bodyWorld.data());
                std::memcpy(projection.data(),reinterpret_cast<void*>(source.viewport+layout.gpuProjection),sizeof(projection));
                mgs5vr::drawPhysicalIdroid(afterContext,bodyWorld,eyeView,projection);
                // The pointer originates at the normal right-hand OpenXR aim
                // pose. Convert that exact hit back onto the same physical
                // screen pose used by the native menu projection; never use
                // the grip, the HMD center, or a second overlay space.
                if(const auto ray=mgs5vr::trackedIdroidRay(source.pair.sample)){
                    const float screenWidth=source.pair.sample.controllers.idroidScreenWidth>0
                        ?source.pair.sample.controllers.idroidScreenWidth:mgs5vr::idroidScreenWidth;
                    const float screenHeight=screenWidth*9.f/16.f;
                    const auto cursor=mgs5vr::compose(idroid->screen,mgs5vr::Pose{{},
                        {(ray->hit.u-.5f)*screenWidth,
                         (.5f-ray->hit.v)*screenHeight,.014f}});
                    alignas(16) auto cursorValues=values(cursor);
                    alignas(16) std::array<float,16> cursorWorld{};
                    originalWorld(cursorValues.data(),cursorWorld.data());
                    mgs5vr::drawPhysicalIdroidCursor(afterContext,cursorWorld,eyeView,projection);
                    static std::atomic_bool reported{};
                    if(!reported.exchange(true))mgs5vr::log("iDroid pointer ray bound to right-hand OpenXR aim and front-display projection");
                }
            }
        }
        const auto& wrist=source.pair.sample;
        if(!titleSurface&&!wrist.menuOpen&&mgs5vr::worldHudVisible(wrist.controllers.hudMode,hudView))
            mgs5vr::drawWorldWaypoints(afterContext,eyeView,eyeProjection,native.position,markerSnapshot);
        // The equipment category screen is native UI orders 133..136. The
        // worker-side UI hook remaps those real pixels to the wrist picker;
        // there is deliberately no generated instruction-card draw here.
        try{if(mgs5vr::captureSceneEye(afterContext,drawingEye))++sceneCopies;else {complete=false;sceneFailure=5;}}
        catch(const std::exception& ex){complete=false;sceneFailure=7;mgs5vr::log(std::string("Native eye capture: ")+ex.what());}
    }
    if(titleSurface){
        // Late native Title layers finish after this scene callback. Leave a
        // clean native-camera pass last, then copy its completed menu at
        // Present for the next stereo pair. Never feed an eye containing the
        // spatial panel back into the panel's image.
        saved.restore();sceneRenderPass=2+extraPass;eyeViewport=0;drawingEye={};
        mgs5vr::clearUiRenderSource();
        result=originalScene(render,graphics,task,worker);
    }
    if(pairCount.load()!=cameraCount){complete=false;sceneFailure=6;}
    const auto timingOwner=field<uintptr_t>(graphics,layout.graphicsContext);
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
    if(caller==base+layout.worldReturn){
        if(const auto menu=mgs5vr::nativeMenuOpen()){
            mgs5vr::headCamera().setNativeMenuOpen(*menu,mgs5vr::nativeIdroidOpen());
        }
        current={};current.camera=source-layout.cameraPose;
        current.identity=layout.publisher?publicationOwner:current.camera;
        primaryListener=0;primaryListenerSequence=0;
        std::array<float,8> native{};std::memcpy(native.data(),input,sizeof(native));
        current.nativeInput=native;
        current.sample={pose(native.data()),{},0,0,false};
        if(nativePairVerified.load()&&current.identity)current.sample=mgs5vr::headCamera().resolveCurrent(current.identity,current.sample.nativePose);
        alignas(16) auto adjusted=values(current.sample.nativePose);
        // Title's animated UI builds geometry from the native publication.
        // Keep that source intact, and move only the two render cameras into
        // the cabin. Moving the source first deforms its menu before UI replay.
    const bool replace=current.sample.applied&&!current.sample.controllers.frontEnd&&!current.sample.controllers.avatarEditor;
        auto* result=originalWorld(replace?static_cast<void*>(adjusted.data()):input,output);
        std::memcpy(current.world.data(),output,sizeof(current.world));
        current.applied=current.sample.applied;current.haveWorld=true;
        return result;
    }
    // The inverse builder invokes this function synchronously in the same native
    // publication. Reuse the exact pose selected for the world matrix.
    if(caller==base+layout.inverseWorldReturn&&current.haveWorld&&current.camera+layout.cameraPose==source
       &&current.applied&&!current.sample.controllers.frontEnd&&!current.sample.controllers.avatarEditor){
        alignas(16) auto adjusted=values(current.sample.nativePose);
        return originalWorld(adjusted.data(),output);
    }
    return originalWorld(input,output);
}
__declspec(noinline) float* view(void* input,float* output){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    auto* result=originalView(input,output);
    if(enabled.load()&&caller==base+layout.viewReturn&&current.haveWorld&&current.camera==reinterpret_cast<uintptr_t>(input)+layout.viewInputToCamera){
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
        const auto* matrix=reinterpret_cast<const float*>(eyeViewport+layout.gpuProjection);
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
    std::memcpy(next.matrices.data(),bytes.data()+layout.viewportMatrices,sizeof(next.matrices));
    std::memcpy(&next.camera,bytes.data()+layout.viewportCamera,sizeof(next.camera));
    std::memcpy(&next.width,bytes.data()+layout.viewportWidth,sizeof(next.width));std::memcpy(&next.height,bytes.data()+layout.viewportHeight,sizeof(next.height));
    std::memcpy(&next.scale,bytes.data()+layout.viewportScale,sizeof(next.scale));
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
void installRenderCamera(uintptr_t moduleBase,const std::filesystem::path& directory,RenderBuild build){
    renderBuild=build;layout=renderLayout(build);
    const bool gz=build==RenderBuild::groundZeroes_1_0_0_5;
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
    constexpr std::array<unsigned char,8> gzExtentsEntry{0xf3,0x0f,0x10,0x0d,0x3c,0x5c,0x5e,0};
    constexpr std::array<unsigned char,11> gzViewportEntry{0x48,0x8b,0xc4,0x57,0x48,0x81,0xec,0xb0,0,0,0};
    constexpr std::array<unsigned char,11> gzPublisherEntry{0x48,0x8b,0xc4,0x55,0x57,0x41,0x54,0x41,0x56,0x41,0x57};
    const auto directCall=[&](uintptr_t returnRva,uintptr_t targetRva){
        std::array<unsigned char,5> bytes{};SIZE_T copied{};int32_t displacement{};
        if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(moduleBase+returnRva-5),bytes.data(),bytes.size(),&copied)
            ||copied!=bytes.size()||bytes[0]!=0xe8)return false;
        std::memcpy(&displacement,bytes.data()+1,4);
        return static_cast<int64_t>(returnRva)+displacement==static_cast<int64_t>(targetRva);
    };
    if(!matches(moduleBase+layout.world,worldEntry)||!matches(moduleBase+layout.view,viewEntry)
        ||!(gz?matches(moduleBase+layout.extents,gzExtentsEntry):matches(moduleBase+layout.extents,extentsEntry))
        ||!(gz?matches(moduleBase+layout.viewport,gzViewportEntry):matches(moduleBase+layout.viewport,viewportEntry))
        ||!matches(moduleBase+layout.projection,projectionEntry)||!matches(moduleBase+layout.scene,sceneEntry)
        ||!matches(moduleBase+layout.registerTarget,registerEntry)||!matches(moduleBase+layout.listener,listenerEntry)
        ||!matches(moduleBase+layout.virtualListener,virtualListenerEntry)
        ||(gz&&!matches(moduleBase+layout.publisher,gzPublisherEntry))
        ||(!gz&&(!matches(moduleBase+0x438123,listenerCaller)||!matches(moduleBase+0x43813f,virtualListenerCaller)))
        ||!directCall(layout.worldReturn,layout.world)||!directCall(layout.viewReturn,layout.view)
        ||!directCall(layout.inverseWorldReturn,layout.world)||!directCall(layout.viewportReturn,layout.viewport)
        ||!directCall(layout.clipReturn,layout.projection)||!directCall(layout.gpuReturn,layout.projection)
        ||!directCall(layout.listenerReturn,layout.listener)||!directCall(layout.virtualListenerReturn,layout.virtualListener))
        throw std::runtime_error("Native scene/matrix/listener signature mismatch");
    // These are the native GPU-projection builder's focal loads, not a field
    // borrowed from the other game: TPP +0x10c, GZ +0xf8.
    constexpr std::array<unsigned char,8> tppFocalLoad{0xf3,0x0f,0x10,0x88,0x0c,0x01,0,0};
    constexpr std::array<unsigned char,8> gzFocalLoad{0xf3,0x0f,0x10,0x88,0xf8,0,0,0};
    if(!(gz?matches(moduleBase+0xf425a7,gzFocalLoad):matches(moduleBase+0x1b96ee,tppFocalLoad)))
        throw std::runtime_error("Native perspective focal-field signature mismatch");
    base=moduleBase;
    if(!directory.empty()){
        std::filesystem::create_directories(directory);
        evidence.open(directory/("matrices-"+std::to_string(GetCurrentProcessId())+".jsonl"));
        if(!evidence)throw std::runtime_error("Cannot open native matrix evidence");
        evidence<<std::setprecision(9)<<"{\"schema\":1,\"image_base\":"<<base<<",\"paired_native_publication\":true,\"joined_to_present\":false}\n";
    }
    struct Hook {uintptr_t rva;void* wrapper;void** original;};
    std::vector<Hook> hooks{
        {layout.world,reinterpret_cast<void*>(&world),reinterpret_cast<void**>(&originalWorld)},
        {layout.view,reinterpret_cast<void*>(&view),reinterpret_cast<void**>(&originalView)},
        {layout.extents,reinterpret_cast<void*>(&extents),reinterpret_cast<void**>(&originalExtents)},
        {layout.viewport,reinterpret_cast<void*>(&viewport),reinterpret_cast<void**>(&originalViewport)},
        {layout.projection,reinterpret_cast<void*>(&projection),reinterpret_cast<void**>(&originalProjection)},
        {layout.scene,reinterpret_cast<void*>(&scene),reinterpret_cast<void**>(&originalScene)},
        {layout.registerTarget,reinterpret_cast<void*>(&registerTarget),reinterpret_cast<void**>(&originalRegisterTarget)},
        {layout.listener,reinterpret_cast<void*>(&listener),reinterpret_cast<void**>(&originalListener)},
        {layout.virtualListener,reinterpret_cast<void*>(&virtualListener),reinterpret_cast<void**>(&originalVirtualListener)}};
    if(layout.publisher)hooks.push_back({layout.publisher,reinterpret_cast<void*>(&publishCamera),reinterpret_cast<void**>(&originalPublisher)});
    for(const auto& hook:hooks){const auto result=MH_CreateHook(reinterpret_cast<void*>(base+hook.rva),hook.wrapper,hook.original);
        if(result!=MH_OK)throw std::runtime_error(std::string("Native scene hook: ")+MH_StatusToString(result));
    }
    for(const auto& hook:hooks)if(MH_EnableHook(reinterpret_cast<void*>(base+hook.rva))!=MH_OK){
        for(const auto& installed:hooks)MH_DisableHook(reinterpret_cast<void*>(base+installed.rva));
        throw std::runtime_error("Cannot enable native scene/matrix hooks");
    }
    enabled.store(true);
    if(!gz){
        try{installUiRenderer(base);}catch(const std::exception& ex){log(std::string("Native UI integration unavailable: ")+ex.what());}
        try{installOpticMarkers(base);}catch(const std::exception& ex){log(std::string("Native optic markers unavailable: ")+ex.what());}
    }
    log(gz?"Ground Zeroes 1.0.0.5 native scene adapter installed":"The Phantom Pain 1.0.15.4 native scene adapter installed");
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
            <<",\"identity\":"<<p.identity<<",\"thread\":"<<p.thread<<",\"tracking_sequence\":"<<p.sample.trackingSequence<<",\"applied\":"<<(p.applied?"true":"false")
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
        evidence<<",\"viewport_matrix_offset\":"<<layout.viewportMatrices<<",\"viewport_matrices\":";array(v.matrices);evidence<<",\"gr_camera_world_view_previous\":";array(v.cameraMatrices);
        evidence<<",\"stack\":[";for(USHORT n=0;n<v.stackCount;++n){if(n)evidence<<',';evidence<<"\"0x"<<std::hex<<reinterpret_cast<uintptr_t>(v.stack[n])<<std::dec<<'"';}evidence<<"]}\n";
    }
    evidence.flush();++reports;
}
EyeFrame observeRenderPresent(void*) noexcept {
    if(!enabled.load())return {};
    const auto publication=lastCameraPublication.load(),now=GetTickCount64();
    // A loading/Start Mission screen can present indefinitely without running
    // the scene-camera publisher. Do not strand its native confirmation behind
    // an empty stereo submission. Short producer gaps retain their eye pair.
    if(publication&&now>=publication&&now-publication>500)headCamera().awaitScene();
    const auto frame=++presentCount;if(frame%300!=1)return {};
    PresentTrace next;next.frame=frame;next.tick=GetTickCount64();next.thread=GetCurrentThreadId();
    next.count=CaptureStackBackTrace(1,static_cast<DWORD>(next.stack.size()),next.stack.data(),nullptr);
    try{
        {std::unique_lock lock(latestMutex,std::try_to_lock);if(lock.owns_lock())presentTrace=next;}
        // GZ's renderer can be exercised independently of the TPP owner/rig
        // observer. It must still report its native image/pose transaction.
        if(renderBuild==RenderBuild::groundZeroes_1_0_0_5)mgs5vr::reportRenderCamera();
    }catch(...){}
    return {}; // Scene-capture command-list metadata owns the image/pose join.
}
void stopRenderCamera() noexcept {enabled.store(false);stopOpticMarkers();stopUiRenderer();headCamera().cancel();try{reportRenderCamera();evidence.close();}catch(...){}}
}
