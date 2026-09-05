#include "mgs5vr/mailbox.hpp"
#include <array>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <atomic>

using namespace mgs5vr;
static int checks{};
static void require(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
struct Device {
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    Device(){const D3D_FEATURE_LEVEL level=D3D_FEATURE_LEVEL_11_0;
        checkHr(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,&level,1,D3D11_SDK_VERSION,
            &device,nullptr,&context),"Create test GPU device");}
};
static ComPtr<ID3D11Texture2D> texture(Device& d,UINT w,UINT h,UINT samples=1){
    D3D11_TEXTURE2D_DESC desc{};desc.Width=w;desc.Height=h;desc.ArraySize=desc.MipLevels=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=samples;
    desc.BindFlags=D3D11_BIND_RENDER_TARGET;ComPtr<ID3D11Texture2D> t;
    checkHr(d.device->CreateTexture2D(&desc,nullptr,&t),"Create test source");return t;
}
static void clear(Device& d,ID3D11Texture2D* t,const float* color){
    ComPtr<ID3D11RenderTargetView> rtv;
    checkHr(d.device->CreateRenderTargetView(t,nullptr,&rtv),"Create test target");d.context->ClearRenderTargetView(rtv.Get(),color);
}
static std::array<unsigned char,4> pixel(Device& d,ID3D11Texture2D* t,UINT x,UINT y,UINT slice=0){
    D3D11_TEXTURE2D_DESC desc{};t->GetDesc(&desc);desc.BindFlags=desc.MiscFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;checkHr(d.device->CreateTexture2D(&desc,nullptr,&staging),"Create pixel readback");
    d.context->CopyResource(staging.Get(),t);D3D11_MAPPED_SUBRESOURCE map{};
    checkHr(d.context->Map(staging.Get(),slice,D3D11_MAP_READ,0,&map),"Read copied pixels");
    const auto* p=static_cast<unsigned char*>(map.pData)+y*map.RowPitch+x*4;
    const std::array<unsigned char,4> result{p[0],p[1],p[2],p[3]};d.context->Unmap(staging.Get(),slice);return result;
}
static bool consumeEventually(TextureConsumer& consumer,TextureMailbox& mailbox){
    for(int n=0;n<100;++n){if(consumer.consume(mailbox.latest()))return true;std::this_thread::sleep_for(std::chrono::milliseconds(1));}return false;
}
int main(){try{
    Device producer,reader;TextureMailbox mailbox;TextureConsumer consumer(reader.device.Get());
    auto source=texture(producer,64,32);const float red[]{1,0,0,1},green[]{0,1,0,1};clear(producer,source.Get(),red);
    ComPtr<ID3D11RenderTargetView> target;checkHr(producer.device->CreateRenderTargetView(source.Get(),nullptr,&target),"Create state fixture");
    auto* bound=target.Get();producer.context->OMSetRenderTargets(1,&bound,nullptr);
    EyeFrame eye;eye.sourceSequence=77;eye.trackingSequence=91;eye.eye=1;eye.joined=true;
    require(mailbox.publish(source.Get(),producer.context.Get(),eye),"first frame must publish");
    require(!mailbox.publish(source.Get(),producer.context.Get()),"unread frame must not be overwritten");
    ComPtr<ID3D11RenderTargetView> after;producer.context->OMGetRenderTargets(1,&after,nullptr);
    require(after.Get()==target.Get(),"capture must preserve game render targets");
    require(consumeEventually(consumer,mailbox),"consumer must acquire first shared frame");
    require(pixel(reader,consumer.texture(),63,31)==std::array<unsigned char,4>{255,0,0,255},"actual GPU copied pixel must remain red");
    const auto first=consumer.frame();require(first.epoch&&first.sequence,"first capture must have a transaction");
    require(consumer.eye().sourceSequence==77&&consumer.eye().trackingSequence==91&&consumer.eye().eye==1,"exact source-eye metadata travels under the same mutex as its GPU pixels");
    require(!consumer.consume(mailbox.latest()),"consumer must not invent a new frame while producer is paused");
    require(pixel(reader,consumer.texture(),0,0)==std::array<unsigned char,4>{255,0,0,255},"paused producer retains valid cached pixels");
    clear(producer,source.Get(),green);
    require(mailbox.publish(source.Get(),producer.context.Get()),"next frame must publish after acknowledgment");
    require(consumeEventually(consumer,mailbox),"updated content must arrive");
    require(pixel(reader,consumer.texture(),10,10)==std::array<unsigned char,4>{0,255,0,255},"updated source must replace cached red pixels");
    require(consumer.frame().sequence>first.sequence,"transaction advances with actual new content");
    require(!consumer.eye().sourceSequence&&!consumer.eye().joined,"ordinary capture clears prior eye metadata");
    producer.context->OMSetRenderTargets(0,nullptr,nullptr);target.Reset();after.Reset();source.Reset();
    source=texture(producer,16,64);clear(producer,source.Get(),red);
    require(mailbox.publish(source.Get(),producer.context.Get()),"resolution change must allocate new shared texture");
    require(consumeEventually(consumer,mailbox),"consumer must reopen resized texture");
    require(consumer.frame().epoch!=first.epoch,"resize creates a new epoch");
    require(pixel(reader,consumer.texture(),15,63)[0]==255,"resized texture copies all rows");
    const auto beforeReset=consumer.frame();mailbox.invalidate();
    require(!mailbox.latest(),"device reset detaches prior producer");
    require(mailbox.publish(source.Get(),producer.context.Get()),"new frame after reset publishes");
    require(consumeEventually(consumer,mailbox),"new frame after reset consumes");
    require(consumer.frame().epoch!=beforeReset.epoch,"device reset cannot reuse prior frame epoch");
    bool mismatch=false;try{mailbox.publish(source.Get(),reader.context.Get());}catch(const std::exception&){mismatch=true;}
    require(mismatch,"cross-device game context rejected before copying");
    UINT quality=0;checkHr(producer.device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM,4,&quality),"Check MSAA");
    require(quality>0,"test GPU must support 4x MSAA");
    auto msaa=texture(producer,32,32,4);clear(producer,msaa.Get(),green);
    require(mailbox.publish(msaa.Get(),producer.context.Get()),"MSAA source resolves into mailbox");
    require(consumeEventually(consumer,mailbox),"resolved source consumes");
    require(pixel(reader,consumer.texture(),20,20)[1]==255,"MSAA resolve produces correct pixels");
    auto left=texture(producer,48,24),right=texture(producer,48,24);clear(producer,left.Get(),red);clear(producer,right.Get(),green);
    std::array<EyeFrame,2> eyes{};
    for(uint32_t n=0;n<2;++n){eyes[n].sourceSequence=100;eyes[n].trackingSequence=200;eyes[n].eye=n;}
    require(mailbox.publishStereo({left.Get(),right.Get()},producer.context.Get(),eyes),"both native eye textures publish as one atomic GPU transaction");
    require(consumeEventually(consumer,mailbox),"consumer receives both slices together");
    require(pixel(reader,consumer.texture(),47,23,0)==std::array<unsigned char,4>{255,0,0,255},"left array slice retains its own rendered pixels");
    require(pixel(reader,consumer.texture(),47,23,1)==std::array<unsigned char,4>{0,255,0,255},"right array slice is not a duplicated left image");
    require(consumer.eyes()[0].sourceSequence==100&&consumer.eyes()[1].sourceSequence==100&&consumer.eyes()[1].eye==1,"both image metadata records share the atomic publication");
    eyes[1].sourceSequence=101;bool rejected=false;
    try{mailbox.publishStereo({left.Get(),right.Get()},producer.context.Get(),eyes);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"GPU publisher rejects alternate simulation frames before touching shared images");
    std::cout<<checks<<" real D3D11 checks passed across two hardware devices. No headset proof implied.\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
