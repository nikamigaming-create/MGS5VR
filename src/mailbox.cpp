#include "mgs5vr/mailbox.hpp"
#include <stdexcept>
#include <sstream>

namespace mgs5vr {
void checkHr(HRESULT r,const char* operation) {
    if(FAILED(r)) { std::ostringstream s; s<<operation<<" HRESULT=0x"<<std::hex<<static_cast<unsigned long>(r); throw std::runtime_error(s.str()); }
}
namespace {
class KeyRelease {
public:
    KeyRelease(IDXGIKeyedMutex* m,UINT64 key,ID3D11DeviceContext* c):mutex(m),releaseKey(key),context(c){}
    ~KeyRelease() { context->Flush(); mutex->ReleaseSync(releaseKey); }
    KeyRelease(const KeyRelease&)=delete;
    KeyRelease& operator=(const KeyRelease&)=delete;
private:
    IDXGIKeyedMutex* mutex;
    UINT64 releaseKey;
    ID3D11DeviceContext* context;
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
    channel_.reset();
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
    if(!source||!context) throw std::invalid_argument("missing D3D11 source/context");
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
    if(!channel||channel->producerDevice.Get()!=device.Get()||channel->desc.Width!=desc.Width
        ||channel->desc.Height!=desc.Height||channel->desc.Format!=desc.Format||channel->desc.ArraySize!=count) {
        channel=std::make_shared<TextureChannel>();
        channel->producerDevice=device;
        channel->desc=desc;
        channel->desc.SampleDesc={1,0};
        channel->desc.ArraySize=count;
        channel->desc.Usage=D3D11_USAGE_DEFAULT;
        channel->desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
        channel->desc.CPUAccessFlags=0;
        channel->desc.MiscFlags=D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
        checkHr(device->CreateTexture2D(&channel->desc,nullptr,&channel->texture),"Create mailbox texture");
        checkHr(channel->texture.As(&channel->mutex),"Query producer keyed mutex");
        ComPtr<IDXGIResource> resource;
        checkHr(channel->texture.As(&resource),"Query shared resource");
        checkHr(resource->GetSharedHandle(&channel->sharedHandle),"Get shared handle");
        ComPtr<IDXGIDevice> dxgi; ComPtr<IDXGIAdapter> adapter; DXGI_ADAPTER_DESC adapterDesc{};
        checkHr(device.As(&dxgi),"Query producer DXGI device");
        checkHr(dxgi->GetAdapter(&adapter),"Get producer adapter");
        checkHr(adapter->GetDesc(&adapterDesc),"Get producer adapter description");
        channel->adapterLuid=adapterDesc.AdapterLuid;
        channel->epoch=++epoch_;
        std::lock_guard pointer(pointerMutex_); channel_=channel;
    }
    if(!acquire(channel->mutex.Get(),0)) return false;
    KeyRelease release(channel->mutex.Get(),1,context);
    for(uint32_t n=0;n<count;++n){
        if(desc.SampleDesc.Count>1)context->ResolveSubresource(channel->texture.Get(),n,sources[n],0,desc.Format);
        else context->CopySubresourceRegion(channel->texture.Get(),n,0,0,0,sources[n],0,nullptr);
    }
    checkHr(device->GetDeviceRemovedReason(),"Producer device health");
    channel->published={channel->epoch,++sequence_};
    channel->eyes=eyes;
    return true;
}
TextureConsumer::TextureConsumer(ID3D11Device* device):device_(device) {
    if(!device) throw std::invalid_argument("missing consumer device");
    device_->GetImmediateContext(&context_);
}
void TextureConsumer::reset() { cached_.Reset(); shared_.Reset(); mutex_.Reset(); channel_.reset(); frame_={};eyes_={}; }
bool TextureConsumer::consume(const std::shared_ptr<TextureChannel>& channel) {
    if(!channel) return false;
    if(channel_!=channel) {
        reset();
        checkHr(device_->OpenSharedResource(channel->sharedHandle,IID_PPV_ARGS(&shared_)),"Open mailbox on XR device");
        checkHr(shared_.As(&mutex_),"Query consumer keyed mutex");
        auto desc=channel->desc; desc.MiscFlags=0;
        checkHr(device_->CreateTexture2D(&desc,nullptr,&cached_),"Create consumer cache");
        channel_=channel;
    }
    if(!acquire(mutex_.Get(),1)) return false;
    KeyRelease release(mutex_.Get(),0,context_.Get());
    context_->CopyResource(cached_.Get(),shared_.Get());
    checkHr(device_->GetDeviceRemovedReason(),"Consumer device health");
    frame_=channel->published;
    eyes_=channel->eyes;
    return true;
}
}
