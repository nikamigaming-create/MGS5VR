#include "mgs5vr/mailbox.hpp"
#include <stdexcept>
#include <sstream>
#include <chrono>
#include <limits>
#include "mgs5vr/log.hpp"

namespace mgs5vr {
void checkHr(HRESULT r,const char* operation) {
    if(FAILED(r)) { std::ostringstream s; s<<operation<<" HRESULT=0x"<<std::hex<<static_cast<unsigned long>(r); throw std::runtime_error(s.str()); }
}
namespace {
using CaptureClock=std::chrono::steady_clock;
double elapsed(CaptureClock::time_point a,CaptureClock::time_point b){return std::chrono::duration<double,std::milli>(b-a).count();}
void reportTransfer(bool producer,const std::array<double,4>& sample) noexcept {try{
    struct Stats {CaptureClock::time_point since=CaptureClock::now();std::array<double,4> total{},maximum{};uint64_t count{};};
    thread_local std::array<Stats,2> stats;auto& s=stats[producer?0:1];++s.count;
    for(size_t i=0;i<sample.size();++i){s.total[i]+=sample[i];s.maximum[i]=std::max(s.maximum[i],sample[i]);}
    const auto now=CaptureClock::now();if(now-s.since<std::chrono::seconds(5))return;
    std::ostringstream line;line<<"Mailbox "<<(producer?"producer":"consumer")<<" ms acquire_queue_flush_release=";
    for(size_t i=0;i<sample.size();++i){if(i)line<<';';line<<s.total[i]/static_cast<double>(s.count)<<','<<s.maximum[i];}
    log(line.str());s={};
}catch(...){} }
struct FlowSample {double pollMs{};uint64_t polls{},completed{},pending{},skipped{},discarded{},timeouts{};};
void reportSubmission(bool producer,bool fence,const std::array<double,3>& sample) noexcept {try{
    struct Stats {CaptureClock::time_point since=CaptureClock::now();std::array<double,3> total{},maximum{};uint64_t count{};};
    thread_local std::array<Stats,4> stats;auto& s=stats[(producer?0:2)+(fence?0:1)];++s.count;
    for(size_t n=0;n<sample.size();++n){s.total[n]+=sample[n];s.maximum[n]=std::max(s.maximum[n],sample[n]);}
    const auto now=CaptureClock::now();if(now-s.since<std::chrono::seconds(5))return;
    std::ostringstream line;line<<"Mailbox "<<(producer?"producer":"consumer")<<" submission backend="<<(fence?"fence":"event")<<" samples="<<s.count;
    constexpr std::array<const char*,3> names{"signal_ms_mean_max","flush_ms_mean_max","admission_ms_mean_max"};
    for(size_t n=0;n<sample.size();++n)line<<' '<<names[n]<<'='<<s.total[n]/static_cast<double>(s.count)<<','<<s.maximum[n];
    log(line.str());s={};
}catch(...){} }
bool unsupportedFence(HRESULT result){return result==E_NOINTERFACE||result==E_NOTIMPL||result==DXGI_ERROR_UNSUPPORTED;}
void initializeCompletion(TextureCompletion& completion,ID3D11Device* device,ID3D11DeviceContext* context,TextureCompletionPreference preference){
    completion={};if(preference==TextureCompletionPreference::eventOnly)return;
    ComPtr<ID3D11Device5> device5;
    const auto deviceResult=device->QueryInterface(IID_PPV_ARGS(&device5));
    if(unsupportedFence(deviceResult))return;checkHr(deviceResult,"Query mailbox fence device");
    const auto contextResult=context->QueryInterface(IID_PPV_ARGS(&completion.context));
    if(unsupportedFence(contextResult)){completion={};return;}checkHr(contextResult,"Query mailbox fence context");
    const auto fenceResult=device5->CreateFence(0,D3D11_FENCE_FLAG_NONE,IID_PPV_ARGS(&completion.fence));
    if(unsupportedFence(fenceResult)){completion={};return;}checkHr(fenceResult,"Create mailbox completion fence");
}
void reportFlow(bool producer,const FlowSample& sample) noexcept {try{
    struct Stats {CaptureClock::time_point since=CaptureClock::now();FlowSample sum{};double maximum{};};
    thread_local std::array<Stats,2> stats;auto& s=stats[producer?0:1];
    s.sum.pollMs+=sample.pollMs;s.maximum=std::max(s.maximum,sample.pollMs);
    s.sum.polls+=sample.polls;s.sum.completed+=sample.completed;s.sum.pending+=sample.pending;
    s.sum.skipped+=sample.skipped;s.sum.discarded+=sample.discarded;s.sum.timeouts+=sample.timeouts;
    const auto now=CaptureClock::now();if(now-s.since<std::chrono::seconds(5))return;
    std::ostringstream line;line<<"Mailbox "<<(producer?"producer":"consumer")<<" GPU-ready polls="<<s.sum.polls
        <<" completed="<<s.sum.completed<<" pending="<<s.sum.pending<<" skipped="<<s.sum.skipped<<" discarded="<<s.sum.discarded
        <<" timeouts="<<s.sum.timeouts<<" ms_poll_total_max="<<s.sum.pollMs<<','<<s.maximum;log(line.str());s={};
}catch(...){} }
bool complete(ID3D11DeviceContext* context,const TextureCompletion& completion,ID3D11Query* query,uint64_t marker,FlowSample& sample){
    BOOL done=FALSE;const auto begin=CaptureClock::now();HRESULT result=S_OK;
    if(completion.fence){
        if(!marker||marker>completion.nextValue)throw std::runtime_error("Invalid mailbox fence marker");
        const auto value=completion.fence->GetCompletedValue();
        if(value==std::numeric_limits<uint64_t>::max())throw std::runtime_error("Mailbox fence device removed");
        done=value>=marker;
    }else result=context->GetData(query,&done,sizeof(done),D3D11_ASYNC_GETDATA_DONOTFLUSH);
    sample.pollMs+=elapsed(begin,CaptureClock::now());++sample.polls;
    if(result==S_FALSE||(result==S_OK&&!done)){++sample.pending;return false;}
    if(result!=S_OK)throw std::runtime_error("Mailbox GPU completion query failed: "+std::to_string(result));
    ++sample.completed;return true;
}
class KeyRelease {
public:
    KeyRelease(TextureChannel& ownerChannel,TextureSlot& transferSlot,IDXGIKeyedMutex* m,UINT64 key,ID3D11DeviceContext* c,double acquireMs)
        :channel(ownerChannel),slot(transferSlot),mutex(m),releaseKey(key),context(c),acquireTime(acquireMs){}
    ~KeyRelease() {if(!finished){channel.retired.store(true,std::memory_order_release);if(owned)mutex->ReleaseSync(releaseKey);context->Flush();}}
    std::array<double,4> finish(TextureCompletion& completion,ID3D11Query* query,uint64_t& marker) {
        const auto copied=CaptureClock::now();const auto result=mutex->ReleaseSync(releaseKey);owned=false;
        if(result!=S_OK)throw std::runtime_error("Mailbox keyed release failed: "+std::to_string(result));
        const auto released=CaptureClock::now();
        // Signal only after actual release. The immutable marker belongs to this
        // owner queue; no peer context/fence is called or waited on here.
        const auto submitting=CaptureClock::now();
        if(completion.fence){
            if(completion.nextValue>=std::numeric_limits<uint64_t>::max()-1)throw std::runtime_error("Mailbox fence sequence exhausted");
            marker=++completion.nextValue;
            checkHr(completion.context->Signal(completion.fence.Get(),marker),"Signal mailbox completion fence");
        }else{marker=0;context->End(query);}
        const auto signaled=CaptureClock::now();context->Flush();const auto flushed=CaptureClock::now();
        if(!completion.fence){
            // Compatibility admission is once per submitted EVENT, never in
            // completion polling. Its result is not used to publish readiness.
            BOOL admitted=FALSE;const auto submission=context->GetData(query,&admitted,sizeof(admitted),0);
            if(submission!=S_OK&&submission!=S_FALSE)throw std::runtime_error("Mailbox event submission failed: "+std::to_string(submission));
        }
        const auto submitted=CaptureClock::now();
        slot.state.store(releaseKey==1?TextureSlotState::producerPending:TextureSlotState::consumerPending,std::memory_order_release);
        finished=true;
        reportSubmission(releaseKey==1,static_cast<bool>(completion.fence),{elapsed(submitting,signaled),elapsed(signaled,flushed),elapsed(flushed,submitted)});
        return {acquireTime,elapsed(start,copied),elapsed(submitting,submitted),elapsed(copied,released)};
    }
    KeyRelease(const KeyRelease&)=delete;
    KeyRelease& operator=(const KeyRelease&)=delete;
private:
    TextureChannel& channel;
    TextureSlot& slot;
    IDXGIKeyedMutex* mutex;
    UINT64 releaseKey;
    ID3D11DeviceContext* context;
    double acquireTime{};
    bool owned{true},finished{};
    CaptureClock::time_point start=CaptureClock::now();
};
bool acquire(IDXGIKeyedMutex* m,UINT64 key) {
    const auto r=m->AcquireSync(key,0);
    if(r==static_cast<HRESULT>(WAIT_TIMEOUT)||r==DXGI_ERROR_WAIT_TIMEOUT) return false;
    // WAIT_ABANDONED is a success-severity HRESULT but an invalid resource.
    if(r!=S_OK) throw std::runtime_error("DXGI keyed mutex unavailable/abandoned: "+std::to_string(r));
    return true;
}
bool supported(DXGI_FORMAT f) {
    return f==DXGI_FORMAT_R8G8B8A8_UNORM||f==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
        ||f==DXGI_FORMAT_B8G8R8A8_UNORM||f==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
}
}
std::shared_ptr<TextureChannel> TextureMailbox::latest() const { std::lock_guard lock(pointerMutex_); return channel_; }
void TextureMailbox::invalidate() {
    std::lock_guard producer(producerMutex_);
    std::lock_guard pointer(pointerMutex_);
    if(channel_)channel_->retired.store(true,std::memory_order_release);
    channel_.reset();
}
void TextureMailbox::pollLocked(const std::shared_ptr<TextureChannel>& channel,ID3D11DeviceContext* context){
    if(!context||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)throw std::invalid_argument("Mailbox polling requires the producer immediate context");
    if(!channel||channel->retired.load(std::memory_order_acquire))return;
    ComPtr<ID3D11Device> device;context->GetDevice(&device);
    if(device.Get()!=channel->producerDevice.Get())throw std::invalid_argument("Mailbox polling context/device mismatch");
    FlowSample sample;
    try{for(auto& slot:channel->slots){
        if(slot.state.load(std::memory_order_acquire)==TextureSlotState::producerPending
            &&complete(context,channel->producerCompletion,slot.producerComplete.Get(),slot.producerMarker,sample))slot.state.store(TextureSlotState::ready,std::memory_order_release);
    }}catch(...){channel->retired.store(true,std::memory_order_release);throw;}
    reportFlow(true,sample);
}
void TextureMailbox::poll(ID3D11DeviceContext* context){
    std::lock_guard producer(producerMutex_);pollLocked(latest(),context);
}
bool TextureMailbox::publish(ID3D11Texture2D* source,ID3D11DeviceContext* context,EyeFrame eye) {
    return publishImages({source,nullptr},1,context,{eye,{}});
}
bool TextureMailbox::publishStereo(const std::array<ID3D11Texture2D*,2>& sources,ID3D11DeviceContext* context,const std::array<EyeFrame,2>& eyes){
    if(!eyes[0].sourceSequence||eyes[0].sourceSequence!=eyes[1].sourceSequence||eyes[0].trackingSequence!=eyes[1].trackingSequence)
        throw std::invalid_argument("Stereo images must share one native scene transaction");
    return publishImages(sources,2,context,eyes);
}
bool TextureMailbox::publishImages(const std::array<ID3D11Texture2D*,2>& sources,uint32_t count,ID3D11DeviceContext* context,const std::array<EyeFrame,2>& eyes) {
    auto* source=sources[0];
    if(!source||!context||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE) throw std::invalid_argument("missing D3D11 source/immediate context");
    std::lock_guard producer(producerMutex_);
    D3D11_TEXTURE2D_DESC desc{}; source->GetDesc(&desc);
    if(!supported(desc.Format)||!desc.Width||!desc.Height||desc.ArraySize!=1||desc.MipLevels!=1)
        throw std::runtime_error("unsupported capture format/shape; only single RGBA8/BGRA8 textures are supported");
    ComPtr<ID3D11Device> device, contextDevice;
    source->GetDevice(&device); context->GetDevice(&contextDevice);
    if(device.Get()!=contextDevice.Get()) throw std::runtime_error("capture texture/context device mismatch");
    if(count==2){
        if(!sources[1])throw std::invalid_argument("Missing right native eye image");
        D3D11_TEXTURE2D_DESC right{};sources[1]->GetDesc(&right);ComPtr<ID3D11Device> rightDevice;sources[1]->GetDevice(&rightDevice);
        if(right.Width!=desc.Width||right.Height!=desc.Height||right.Format!=desc.Format||right.ArraySize!=1||right.MipLevels!=1
            ||right.SampleDesc.Count!=desc.SampleDesc.Count||rightDevice.Get()!=device.Get())throw std::invalid_argument("Native eye image mismatch");
    }
    auto channel=latest();
    if(!channel||channel->retired.load(std::memory_order_acquire)||channel->producerDevice.Get()!=device.Get()||channel->desc.Width!=desc.Width
        ||channel->desc.Height!=desc.Height||channel->desc.Format!=desc.Format||channel->desc.ArraySize!=count) {
        if(channel)channel->retired.store(true,std::memory_order_release);
        channel=std::make_shared<TextureChannel>();
        channel->producerDevice=device;
        initializeCompletion(channel->producerCompletion,device.Get(),context,preference_);
        channel->desc=desc;
        channel->desc.SampleDesc={1,0};
        channel->desc.ArraySize=count;
        channel->desc.Usage=D3D11_USAGE_DEFAULT;
        channel->desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
        channel->desc.CPUAccessFlags=0;
        channel->desc.MiscFlags=D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
        for(auto& slot:channel->slots){
            checkHr(device->CreateTexture2D(&channel->desc,nullptr,&slot.texture),"Create mailbox texture");
            checkHr(slot.texture.As(&slot.mutex),"Query producer keyed mutex");
            ComPtr<IDXGIResource> resource;
            checkHr(slot.texture.As(&resource),"Query shared resource");
            checkHr(resource->GetSharedHandle(&slot.sharedHandle),"Get shared handle");
            if(!channel->producerCompletion.fence){const D3D11_QUERY_DESC query{D3D11_QUERY_EVENT,0};
                checkHr(device->CreateQuery(&query,&slot.producerComplete),"Create producer completion query");}
        }
        ComPtr<IDXGIDevice> dxgi; ComPtr<IDXGIAdapter> adapter; DXGI_ADAPTER_DESC adapterDesc{};
        checkHr(device.As(&dxgi),"Query producer DXGI device");
        checkHr(dxgi->GetAdapter(&adapter),"Get producer adapter");
        checkHr(adapter->GetDesc(&adapterDesc),"Get producer adapter description");
        channel->adapterLuid=adapterDesc.AdapterLuid;
        channel->epoch=++epoch_;
        std::lock_guard pointer(pointerMutex_); channel_=channel;
    }
    pollLocked(channel,context);
    TextureSlot* claimed=nullptr;
    for(auto& slot:channel->slots){auto expected=TextureSlotState::free;
        if(slot.state.compare_exchange_strong(expected,TextureSlotState::producerOwned,std::memory_order_acq_rel)){claimed=&slot;break;}}
    if(!claimed){FlowSample skipped;skipped.skipped=1;reportFlow(true,skipped);return false;}
    auto& slot=*claimed;const auto acquiring=CaptureClock::now();
    try{if(!acquire(slot.mutex.Get(),0)){
        slot.state.store(TextureSlotState::free,std::memory_order_release);
        reportTransfer(true,{elapsed(acquiring,CaptureClock::now()),0,0,0});FlowSample skipped;skipped.skipped=skipped.timeouts=1;reportFlow(true,skipped);return false;}}
    catch(...){channel->retired.store(true,std::memory_order_release);throw;}
    KeyRelease release(*channel,slot,slot.mutex.Get(),1,context,elapsed(acquiring,CaptureClock::now()));
    for(uint32_t n=0;n<count;++n){
        if(desc.SampleDesc.Count>1)context->ResolveSubresource(slot.texture.Get(),n,sources[n],0,desc.Format);
        else context->CopySubresourceRegion(slot.texture.Get(),n,0,0,0,sources[n],0,nullptr);
    }
    checkHr(device->GetDeviceRemovedReason(),"Producer device health");
    slot.published={channel->epoch,++sequence_};slot.eyes=eyes;
    const auto sample=release.finish(channel->producerCompletion,slot.producerComplete.Get(),slot.producerMarker);reportTransfer(true,sample);
    return true;
}
TextureConsumer::TextureConsumer(ID3D11Device* device,TextureCompletionPreference preference):device_(device),preference_(preference) {
    if(!device) throw std::invalid_argument("missing consumer device");
    device_->GetImmediateContext(&context_);
}
TextureConsumer::~TextureConsumer(){reset();}
void TextureConsumer::reset() {
    if(channel_)channel_->retired.store(true,std::memory_order_release);
    cached_.Reset();slots_={};completion_={};channel_.reset();frame_={};eyes_={};
}
bool TextureConsumer::consume(const std::shared_ptr<TextureChannel>& channel) {
    if(channel_&&channel_->retired.load(std::memory_order_acquire))reset();
    if(!channel)return false;
    if(channel->retired.load(std::memory_order_acquire)){if(channel_==channel)reset();return false;}
    if(channel_!=channel) {
        reset();
        channel_=channel;
        try{initializeCompletion(completion_,device_.Get(),context_.Get(),preference_);
        for(size_t n=0;n<slots_.size();++n){
            auto& slot=slots_[n];
            checkHr(device_->OpenSharedResource(channel->slots[n].sharedHandle,IID_PPV_ARGS(&slot.shared)),"Open mailbox on XR device");
            checkHr(slot.shared.As(&slot.mutex),"Query consumer keyed mutex");
            if(!completion_.fence){const D3D11_QUERY_DESC query{D3D11_QUERY_EVENT,0};
                checkHr(device_->CreateQuery(&query,&slot.complete),"Create consumer completion query");}
        }
        auto desc=channel->desc;desc.MiscFlags=0;
        checkHr(device_->CreateTexture2D(&desc,nullptr,&cached_),"Create consumer cache");
        }catch(...){reset();throw;}
    }
    FlowSample flow;std::array<double,4> transfer{};bool attempted=false,submitted=false,fresh=false;
    try{
        // Only the XR immediate context polls these queries. A native producer
        // sees free only after this copy/release is GPU-complete.
        for(size_t n=0;n<slots_.size();++n){auto& slot=channel->slots[n];
            if(slot.state.load(std::memory_order_acquire)==TextureSlotState::consumerPending
                &&complete(context_.Get(),completion_,slots_[n].complete.Get(),slots_[n].marker,flow))slot.state.store(TextureSlotState::free,std::memory_order_release);
        }
        size_t newest=slots_.size();uint64_t sequence=frame_.sequence;
        for(size_t n=0;n<slots_.size();++n){const auto& slot=channel->slots[n];
            if(slot.state.load(std::memory_order_acquire)==TextureSlotState::ready&&slot.published.sequence>sequence){newest=n;sequence=slot.published.sequence;}}
        for(size_t n=0;n<slots_.size();++n){auto& slot=channel->slots[n];
            if(slot.state.load(std::memory_order_acquire)!=TextureSlotState::ready||slot.published.sequence>sequence)continue;
            auto expected=TextureSlotState::ready;
            if(!slot.state.compare_exchange_strong(expected,TextureSlotState::consumerOwned,std::memory_order_acq_rel))continue;
            const auto acquiring=CaptureClock::now();attempted=true;
            if(!acquire(slots_[n].mutex.Get(),1)){
                transfer[0]+=elapsed(acquiring,CaptureClock::now());++flow.timeouts;
                slot.state.store(TextureSlotState::ready,std::memory_order_release);continue;}
            KeyRelease release(*channel,slot,slots_[n].mutex.Get(),0,context_.Get(),elapsed(acquiring,CaptureClock::now()));
            if(n==newest){
                context_->CopyResource(cached_.Get(),slots_[n].shared.Get());
                checkHr(device_->GetDeviceRemovedReason(),"Consumer device health");
            }
            const auto sample=release.finish(completion_,slots_[n].complete.Get(),slots_[n].marker);submitted=true;
            for(size_t i=0;i<sample.size();++i)transfer[i]+=sample[i];
            if(n==newest){
                // The cache copy and subsequent XR use share this immediate
                // command stream. Publish its matching metadata together now;
                // the query gates shared-slot reuse, not cache pixel identity.
                frame_=slot.published;eyes_=slot.eyes;fresh=true;
            }else ++flow.discarded;
        }
        if(attempted)reportTransfer(false,transfer);
    }catch(...){channel->retired.store(true,std::memory_order_release);if(submitted)context_->Flush();throw;}
    reportFlow(false,flow);
    // A producer reset/resize may retire this epoch while its copy is queued.
    // Finish releasing the old slots but never expose their pixels as fresh.
    if(channel->retired.load(std::memory_order_acquire)){reset();return false;}
    return fresh;
}
}
