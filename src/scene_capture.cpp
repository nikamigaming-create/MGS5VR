#include "mgs5vr/scene_capture.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/log.hpp"
#include <MinHook.h>
#include <unordered_map>
#include <atomic>
#include <vector>
#include <stdexcept>

namespace mgs5vr {
namespace {
using FinishFn=HRESULT(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,BOOL,ID3D11CommandList**);
using ExecuteFn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11CommandList*,BOOL);
FinishFn originalFinish{};ExecuteFn originalExecute{};
struct Packet {
    ComPtr<IDXGISwapChain> swap;
    ComPtr<ID3D11Device> device;
    std::array<ComPtr<ID3D11Texture2D>,2> textures;
    std::array<EyeFrame,2> eyes{};
    uint32_t mask{},executedMask{};
    bool canceled{};
};
struct CopyTag {std::shared_ptr<Packet> packet;uint32_t mask{};};
struct CommandBatch {ComPtr<ID3D11CommandList> commands;std::vector<CopyTag> copies;};
std::mutex mutex;
ComPtr<IDXGISwapChain> target;
std::array<std::shared_ptr<Packet>,8> pool;
std::unordered_map<uint64_t,std::shared_ptr<Packet>> families;
std::unordered_map<ID3D11DeviceContext*,std::vector<CopyTag>> recording;
std::unordered_map<ID3D11CommandList*,CommandBatch> finished;
std::shared_ptr<Packet> completed;
uint64_t published{};std::atomic_uint64_t executed{};
std::atomic_uint64_t finishCalls{},executeCalls{},taggedFinishes{},taggedExecutions{},lastFailure{};
HRESULT STDMETHODCALLTYPE finish(ID3D11DeviceContext* context,BOOL restore,ID3D11CommandList** output){
    ++finishCalls;
    const auto result=originalFinish(context,restore,output);
    try{
        std::lock_guard lock(mutex);
        const auto found=recording.find(context);
        if(found!=recording.end()){
            if(SUCCEEDED(result)&&output&&*output){
                // A scene may cross several native command lists. Retain each
                // copy's list identity; only execution of both eyes completes it.
                finished[*output]={*output,std::move(found->second)};
                ++taggedFinishes;
                if(finished.size()>32){finished.clear();families.clear();}
            }
            recording.erase(found);
        }
    }catch(...){}
    return result;
}
void STDMETHODCALLTYPE execute(ID3D11DeviceContext* context,ID3D11CommandList* commands,BOOL restore){
    ++executeCalls;
    originalExecute(context,commands,restore);
    try{
        std::lock_guard lock(mutex);const auto found=finished.find(commands);
        if(found!=finished.end()){
            ++taggedExecutions;
            ComPtr<ID3D11Device> device;context->GetDevice(&device);
            for(const auto& copy:found->second.copies){auto packet=copy.packet;
                if(!packet->canceled&&device.Get()==packet->device.Get()){
                    packet->executedMask|=copy.mask;
                    for(uint32_t n=0;n<2;++n)if(copy.mask&(1u<<n))packet->eyes[n].joined=true;
                    if(packet->mask==3&&packet->executedMask==3){completed=packet;families.erase(packet->eyes[0].sourceSequence);++executed;}
                }
            }
            finished.erase(found);
        }
    }catch(...){}
}
}
void installSceneCapture(ID3D11Device* device){
    ComPtr<ID3D11DeviceContext> deferred,immediate;
    checkHr(device->CreateDeferredContext(0,&deferred),"Create native command-capture probe");device->GetImmediateContext(&immediate);
    auto** deferredTable=*reinterpret_cast<void***>(deferred.Get());auto** immediateTable=*reinterpret_cast<void***>(immediate.Get());
    const auto hook=[&](void* address,void* function,void** original){
        const auto created=MH_CreateHook(address,function,original);
        if(created!=MH_OK)throw std::runtime_error(std::string("Native command hook: ")+MH_StatusToString(created));
        if(MH_EnableHook(address)!=MH_OK)throw std::runtime_error("Cannot enable native command-list capture");
    };
    hook(deferredTable[114],reinterpret_cast<void*>(&finish),reinterpret_cast<void**>(&originalFinish));
    hook(immediateTable[58],reinterpret_cast<void*>(&execute),reinterpret_cast<void**>(&originalExecute));
    log("Native command-list eye capture installed; no images are synthesized");
}
void observeSceneSwapchain(IDXGISwapChain* swap){std::lock_guard lock(mutex);target=swap;}
bool sceneCaptureAvailable(){std::lock_guard lock(mutex);return target!=nullptr;}
bool captureSceneEye(ID3D11DeviceContext* context,const EyeFrame& eye){
    if(!context||eye.eye>1||!eye.projected||!eye.sourceSequence){lastFailure=1;return false;}
    std::shared_ptr<Packet> packet;ComPtr<IDXGISwapChain> swap;
    {std::lock_guard lock(mutex);swap=target;
        if(!swap){lastFailure=2;return false;}
        if(eye.eye==0){
            for(auto& item:pool)if(!item||item.use_count()==1){if(!item)item=std::make_shared<Packet>();packet=item;break;}
            if(!packet){lastFailure=3;return false;}
            packet->mask=packet->executedMask=0;packet->eyes={};packet->canceled=false;packet->swap=swap;
            families[eye.sourceSequence]=packet;
        }else {const auto found=families.find(eye.sourceSequence);if(found==families.end()){lastFailure=4;return false;}packet=found->second;}
    }
    if(eye.eye==1&&(packet->mask!=1||packet->eyes[0].sourceSequence!=eye.sourceSequence
        ||packet->eyes[0].trackingSequence!=eye.trackingSequence||packet->swap.Get()!=swap.Get())){lastFailure=5;return false;}
    ComPtr<ID3D11Texture2D> source;checkHr(swap->GetBuffer(0,IID_PPV_ARGS(&source)),"Get native scene output");
    ComPtr<ID3D11Device> device,contextDevice;source->GetDevice(&device);context->GetDevice(&contextDevice);
    if(device.Get()!=contextDevice.Get()){lastFailure=6;return false;}
    if(packet->device.Get()!=device.Get()){packet->textures[0].Reset();packet->textures[1].Reset();}
    D3D11_TEXTURE2D_DESC desc{},old{};source->GetDesc(&desc);
    if(desc.SampleDesc.Count!=1||desc.ArraySize!=1||desc.MipLevels!=1){lastFailure=7;return false;}
    if(packet->textures[eye.eye])packet->textures[eye.eye]->GetDesc(&old);
    if(!packet->textures[eye.eye]||packet->device.Get()!=device.Get()||desc.Width!=old.Width||desc.Height!=old.Height||desc.Format!=old.Format){
        desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;desc.MiscFlags=desc.CPUAccessFlags=0;desc.Usage=D3D11_USAGE_DEFAULT;
        packet->textures[eye.eye].Reset();checkHr(device->CreateTexture2D(&desc,nullptr,&packet->textures[eye.eye]),"Create native eye output");
    }
    context->CopyResource(packet->textures[eye.eye].Get(),source.Get());
    {std::lock_guard lock(mutex);
        packet->device=device;packet->eyes[eye.eye]=eye;packet->mask|=1u<<eye.eye;
        if(context->GetType()==D3D11_DEVICE_CONTEXT_IMMEDIATE){
            packet->executedMask|=1u<<eye.eye;packet->eyes[eye.eye].joined=true;
            if(packet->mask==3&&packet->executedMask==3){completed=packet;families.erase(eye.sourceSequence);++executed;}
        }else recording[context].push_back({packet,1u<<eye.eye});
    }
    return true;
}
void cancelSceneEyes(uint64_t source){std::lock_guard lock(mutex);
    const auto found=families.find(source);if(found!=families.end()){found->second->canceled=true;families.erase(found);}
}
bool publishSceneEyes(IDXGISwapChain* swap,ID3D11DeviceContext* context,TextureMailbox& destination){
    std::shared_ptr<Packet> packet;
    {std::lock_guard lock(mutex);packet=completed;}
    if(!packet||packet->swap.Get()!=swap||packet->eyes[0].sourceSequence<=published
        ||!readyEyePair(packet->eyes,packet->eyes[0].activation,steadyMilliseconds()))return false;
    if(!destination.publishStereo({packet->textures[0].Get(),packet->textures[1].Get()},context,packet->eyes))return false;
    published=packet->eyes[0].sourceSequence;
    if(published%120==1)log("Native stereo source="+std::to_string(published)+" executed command pairs="+std::to_string(executed.load()));
    return true;
}
void invalidateSceneCapture(){std::lock_guard lock(mutex);target.Reset();recording.clear();finished.clear();families.clear();completed.reset();}
std::array<uint64_t,8> sceneCaptureCounters(){std::lock_guard lock(mutex);
    return {finishCalls.load(),executeCalls.load(),taggedFinishes.load(),taggedExecutions.load(),executed.load(),lastFailure.load(),families.size(),finished.size()};
}
}
