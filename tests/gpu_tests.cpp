#include "mgs5vr/mailbox.hpp"
#include "mgs5vr/gpu_timing.hpp"
#include "mgs5vr/eye_resampler.hpp"
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
static bool supportsFences(Device& device){
    ComPtr<ID3D11Device5> device5;ComPtr<ID3D11DeviceContext4> context4;ComPtr<ID3D11Fence> fence;
    return SUCCEEDED(device.device.As(&device5))&&SUCCEEDED(device.context.As(&context4))
        &&SUCCEEDED(device5->CreateFence(0,D3D11_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));
}
static void describeCompletion(const TextureChannel& channel,const TextureSlot& slot,ID3D11DeviceContext* context){
    if(channel.producerCompletion.fence){std::cerr<<" fence_marker="<<slot.producerMarker<<" completed="<<channel.producerCompletion.fence->GetCompletedValue();}
    else if(slot.published.sequence){BOOL done=FALSE;const auto result=context->GetData(slot.producerComplete.Get(),&done,sizeof(done),D3D11_ASYNC_GETDATA_DONOTFLUSH);
        std::cerr<<" query_hr="<<std::hex<<result<<std::dec<<" complete="<<done;}
    std::cerr<<'\n';
}
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
static bool consumeEventually(TextureConsumer& consumer,TextureMailbox& mailbox,ID3D11DeviceContext* producer){
    for(int n=0;n<100;++n){mailbox.poll(producer);if(consumer.consume(mailbox.latest()))return true;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    if(const auto channel=mailbox.latest()){
        std::cerr<<"Mailbox timeout epoch="<<channel->epoch<<" retired="<<channel->retired.load()<<" cached="<<consumer.frame().sequence<<'\n';
        for(size_t n=0;n<channel->slots.size();++n){const auto& slot=channel->slots[n];
            std::cerr<<" slot="<<n<<" state="<<static_cast<int>(slot.state.load())<<" sequence="<<slot.published.sequence;
            describeCompletion(*channel,slot,producer);
        }
    }
    return false;
}
static size_t slotsIn(const std::shared_ptr<TextureChannel>& channel,TextureSlotState state){
    size_t count=0;for(const auto& slot:channel->slots)if(slot.state.load()==state)++count;return count;
}
static bool readyEventually(TextureMailbox& mailbox,ID3D11DeviceContext* producer,size_t count){
    for(int n=0;n<100;++n){mailbox.poll(producer);if(slotsIn(mailbox.latest(),TextureSlotState::ready)==count)return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    for(const auto& slot:mailbox.latest()->slots){std::cerr<<"Ready timeout sequence="<<slot.published.sequence<<" state="<<static_cast<int>(slot.state.load());
        describeCompletion(*mailbox.latest(),slot,producer);}
    return false;
}
int main(){try{
    Device producer,reader;TextureMailbox mailbox;TextureConsumer consumer(reader.device.Get());
    const bool producerFences=supportsFences(producer),consumerFences=supportsFences(reader);
    GpuTiming timing;
    ComPtr<ID3D11DeviceContext> deferred;
    checkHr(producer.device->CreateDeferredContext(0,&deferred),"Create timing refusal fixture");
    require(!timing.poll(deferred.Get()),"GPU timing refuses deferred-context readback");
    require(timing.begin(producer.context.Get()),"GPU timing starts on the immediate context");
    require(!timing.begin(producer.context.Get()),"nested timing cannot overwrite an active query");
    timing.end(producer.context.Get());producer.context->Flush();
    require(!timing.poll(reader.context.Get()),"timing results cannot be read from another device");
    std::optional<double> measured;
    for(unsigned i=0;i<100&&!measured;++i){measured=timing.poll(producer.context.Get());if(!measured)std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    require(measured&&*measured>=0&&*measured<100,"real GPU timestamp interval arrives without a blocking query");
    GpuTiming splitTiming;
    ComPtr<ID3D11DeviceContext> secondDeferred;
    checkHr(producer.device->CreateDeferredContext(0,&secondDeferred),"Create split timing fixture");
    require(splitTiming.begin(deferred.Get()),"timestamp start records in the native deferred context");
    splitTiming.end(secondDeferred.Get());
    ComPtr<ID3D11CommandList> firstList,secondList;
    checkHr(deferred->FinishCommandList(FALSE,&firstList),"Finish timestamp start list");
    checkHr(secondDeferred->FinishCommandList(FALSE,&secondList),"Finish timestamp end list");
    splitTiming.beginExecution(producer.context.Get());
    producer.context->ExecuteCommandList(firstList.Get(),FALSE);producer.context->ExecuteCommandList(secondList.Get(),FALSE);
    splitTiming.endExecution(producer.context.Get());
    producer.context->Flush();measured.reset();
    for(unsigned i=0;i<100&&!measured;++i){measured=splitTiming.poll(producer.context.Get());if(!measured)std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    require(measured&&*measured>=0&&*measured<100&&!splitTiming.pending(),"GPU time spans split native command lists and drains after playback");
    ComPtr<ID3D11Query> event;const D3D11_QUERY_DESC eventDesc{D3D11_QUERY_EVENT,0};
    checkHr(producer.device->CreateQuery(&eventDesc,&event),"Create standalone completion event");
    producer.context->End(event.Get());producer.context->Flush();BOOL eventDone=FALSE;HRESULT eventResult=S_FALSE;
    const auto submitted=producer.context->GetData(event.Get(),&eventDone,sizeof(eventDone),0);
    require(submitted==S_OK||submitted==S_FALSE,"owner submits each event once without waiting for completion");
    eventDone=FALSE;
    for(int n=0;n<100&&eventResult==S_FALSE;++n){eventResult=producer.context->GetData(event.Get(),&eventDone,sizeof(eventDone),D3D11_ASYNC_GETDATA_DONOTFLUSH);
        if(eventResult==S_FALSE)std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    if(eventResult!=S_OK||!eventDone)std::cerr<<"Standalone EVENT hr="<<std::hex<<eventResult<<std::dec<<" done="<<eventDone<<'\n';
    require(eventResult==S_OK&&eventDone,"standalone GPU EVENT completes after submission with non-flushing owner polls");
    auto source=texture(producer,64,32);const float red[]{1,0,0,1},green[]{0,1,0,1};clear(producer,source.Get(),red);
    ComPtr<ID3D11RenderTargetView> target;checkHr(producer.device->CreateRenderTargetView(source.Get(),nullptr,&target),"Create state fixture");
    auto* bound=target.Get();producer.context->OMSetRenderTargets(1,&bound,nullptr);
    EyeFrame eye;eye.sourceSequence=77;eye.trackingSequence=91;eye.eye=1;eye.joined=true;
    require(mailbox.publish(source.Get(),producer.context.Get(),eye),"first frame must publish");
    require(static_cast<bool>(mailbox.latest()->producerCompletion.fence)==producerFences,"producer prefers its own supported completion fence");
    require(slotsIn(mailbox.latest(),TextureSlotState::producerPending)==1,"submission remains unavailable until the producer polls its completion event");
    require(!consumer.consume(mailbox.latest())&&!consumer.frame().sequence,"consumer cannot acquire a slot solely because its keyed mutex was released");
    require(consumer.usingCompletionFence()==consumerFences,"consumer selects completion support on its own device");
    bool deferredRefused=false;try{mailbox.poll(deferred.Get());}catch(const std::invalid_argument&){deferredRefused=true;}
    require(deferredRefused,"mailbox completion cannot be polled from a deferred context");
    bool wrongPollRefused=false;try{mailbox.poll(reader.context.Get());}catch(const std::invalid_argument&){wrongPollRefused=true;}
    require(wrongPollRefused,"XR device cannot poll the native producer completion queries");
    ComPtr<ID3D11RenderTargetView> after;producer.context->OMGetRenderTargets(1,&after,nullptr);
    require(after.Get()==target.Get(),"capture must preserve game render targets");
    require(consumeEventually(consumer,mailbox,producer.context.Get()),"consumer must acquire first GPU-complete shared frame");
    require(slotsIn(mailbox.latest(),TextureSlotState::consumerPending)==1,"shared slot stays unavailable until the consumer polls its own copy/release completion");
    require(pixel(reader,consumer.texture(),63,31)==std::array<unsigned char,4>{255,0,0,255},"actual GPU copied pixel must remain red");
    const auto first=consumer.frame();require(first.epoch&&first.sequence,"first capture must have a transaction");
    require(consumer.eye().sourceSequence==77&&consumer.eye().trackingSequence==91&&consumer.eye().eye==1,"exact source-eye metadata travels under the same mutex as its GPU pixels");
    require(!consumer.consume(mailbox.latest()),"consumer must not invent a new frame while producer is paused");
    require(pixel(reader,consumer.texture(),0,0)==std::array<unsigned char,4>{255,0,0,255},"paused producer retains valid cached pixels");
    clear(producer,source.Get(),green);
    require(mailbox.publish(source.Get(),producer.context.Get()),"next frame must publish after acknowledgment");
    require(consumeEventually(consumer,mailbox,producer.context.Get()),"updated content must arrive");
    require(pixel(reader,consumer.texture(),10,10)==std::array<unsigned char,4>{0,255,0,255},"updated source must replace cached red pixels");
    require(consumer.frame().sequence>first.sequence,"transaction advances with actual new content");
    require(!consumer.eye().sourceSequence&&!consumer.eye().joined,"ordinary capture clears prior eye metadata");
    producer.context->OMSetRenderTargets(0,nullptr,nullptr);target.Reset();after.Reset();source.Reset();
    source=texture(producer,16,64);clear(producer,source.Get(),red);
    require(mailbox.publish(source.Get(),producer.context.Get()),"resolution change must allocate new shared texture");
    require(consumeEventually(consumer,mailbox,producer.context.Get()),"consumer must reopen resized texture");
    require(consumer.frame().epoch!=first.epoch,"resize creates a new epoch");
    require(pixel(reader,consumer.texture(),15,63)[0]==255,"resized texture copies all rows");
    const auto beforeReset=consumer.frame();const auto oldEpoch=mailbox.latest();mailbox.invalidate();
    require(!mailbox.latest(),"device reset detaches prior producer");
    require(!consumer.consume(mailbox.latest())&&!consumer.frame().sequence&&!consumer.texture(),
        "null latest after invalidation clears the retained retired cache");
    require(oldEpoch->retired.load()&&!consumer.consume(oldEpoch),"retired epochs cannot be consumed even while an old shared pointer is retained");
    require(!consumer.frame().sequence&&!consumer.texture(),"retiring the current epoch clears its cache identity instead of resurrecting old pixels");
    require(mailbox.publish(source.Get(),producer.context.Get()),"new frame after reset publishes");
    require(consumeEventually(consumer,mailbox,producer.context.Get()),"new frame after reset consumes");
    require(consumer.frame().epoch!=beforeReset.epoch,"device reset cannot reuse prior frame epoch");
    bool mismatch=false;try{mailbox.publish(source.Get(),reader.context.Get());}catch(const std::exception&){mismatch=true;}
    require(mismatch,"cross-device game context rejected before copying");
    UINT quality=0;checkHr(producer.device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM,4,&quality),"Check MSAA");
    require(quality>0,"test GPU must support 4x MSAA");
    auto msaa=texture(producer,32,32,4);clear(producer,msaa.Get(),green);
    require(mailbox.publish(msaa.Get(),producer.context.Get()),"MSAA source resolves into mailbox");
    require(consumeEventually(consumer,mailbox,producer.context.Get()),"resolved source consumes");
    require(pixel(reader,consumer.texture(),20,20)[1]==255,"MSAA resolve produces correct pixels");
    auto left=texture(producer,48,24),right=texture(producer,48,24);clear(producer,left.Get(),red);clear(producer,right.Get(),green);
    std::array<EyeFrame,2> eyes{};
    for(uint32_t n=0;n<2;++n){eyes[n].sourceSequence=100;eyes[n].trackingSequence=200;eyes[n].eye=n;}
    require(mailbox.publishStereo({left.Get(),right.Get()},producer.context.Get(),eyes),"both native eye textures publish as one atomic GPU transaction");
    require(consumeEventually(consumer,mailbox,producer.context.Get()),"consumer receives both slices together");
    require(pixel(reader,consumer.texture(),47,23,0)==std::array<unsigned char,4>{255,0,0,255},"left array slice retains its own rendered pixels");
    require(pixel(reader,consumer.texture(),47,23,1)==std::array<unsigned char,4>{0,255,0,255},"right array slice is not a duplicated left image");
    require(consumer.eyes()[0].sourceSequence==100&&consumer.eyes()[1].sourceSequence==100&&consumer.eyes()[1].eye==1,"both image metadata records share the atomic publication");
    EyeResampler resampler;auto output=texture(reader,24,12);
    resampler.copy(reader.context.Get(),consumer.texture(),0,output.Get());
    require(pixel(reader,output.Get(),23,11)==std::array<unsigned char,4>{255,0,0,255},
        "stable XR output retains the full left eye including its last row and column");
    resampler.copy(reader.context.Get(),consumer.texture(),1,output.Get());
    require(pixel(reader,output.Get(),0,0)==std::array<unsigned char,4>{0,255,0,255},
        "resampling the right eye cannot copy the left slice");
    const std::array<unsigned char,16> checker{0,0,0,255,255,255,255,255,255,255,255,255,0,0,0,255};
    D3D11_TEXTURE2D_DESC checkerDesc{};checkerDesc.Width=checkerDesc.Height=2;
    checkerDesc.ArraySize=checkerDesc.MipLevels=1;checkerDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;checkerDesc.SampleDesc.Count=1;
    D3D11_SUBRESOURCE_DATA checkerData{checker.data(),8,16};ComPtr<ID3D11Texture2D> checkerSource;
    checkHr(reader.device->CreateTexture2D(&checkerDesc,&checkerData,&checkerSource),"Create high-resolution contrast fixture");
    auto onePixel=texture(reader,1,1);resampler.copy(reader.context.Get(),checkerSource.Get(),0,onePixel.Get());
    const auto filtered=pixel(reader,onePixel.Get(),0,0);
    require(filtered[0]>=186&&filtered[0]<=190&&filtered[1]==filtered[0]&&filtered[2]==filtered[0]&&filtered[3]==255,
        "supersampled text contrast filters in linear light and returns display-encoded pixels");
    checkerDesc.Width=checkerDesc.Height=1;checkerDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;checkerDesc.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> srgbOutput;checkHr(reader.device->CreateTexture2D(&checkerDesc,nullptr,&srgbOutput),"Create runtime sRGB output fixture");
    resampler.copy(reader.context.Get(),checkerSource.Get(),0,srgbOutput.Get());
    require(pixel(reader,srgbOutput.Get(),0,0)==filtered,"typed sRGB and UNORM runtime outputs preserve the same display bytes");
    bool badSlice=false;try{resampler.copy(reader.context.Get(),checkerSource.Get(),1,srgbOutput.Get());}catch(const std::invalid_argument&){badSlice=true;}
    require(badSlice,"a missing eye slice is rejected instead of presenting a duplicated eye");
    eyes[1].sourceSequence=101;bool rejected=false;
    try{mailbox.publishStereo({left.Get(),right.Get()},producer.context.Get(),eyes);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"GPU publisher rejects alternate simulation frames before touching shared images");
    // A separate pool exercises pressure and retirement without relying on GPU
    // speed: only explicit owner polling may advance the CPU availability gate.
    TextureMailbox pooled;TextureConsumer poolConsumer(reader.device.Get());
    const float blue[]{0,0,1,1};
    const std::array<const float*,3> leftColors{red,green,blue},rightColors{green,blue,red};
    for(size_t n=0;n<textureMailboxSlots;++n){
        clear(producer,left.Get(),leftColors[n]);clear(producer,right.Get(),rightColors[n]);
        for(uint32_t e=0;e<2;++e){eyes[e].sourceSequence=300+n;eyes[e].trackingSequence=400+n;eyes[e].eye=e;}
        require(pooled.publishStereo({left.Get(),right.Get()},producer.context.Get(),eyes),"all three bounded slots accept distinct stereo transactions");
    }
    const auto pool=pooled.latest();
    if(producerFences){
        require(pool->slots[0].producerMarker>0&&pool->slots[0].producerMarker<pool->slots[1].producerMarker
            &&pool->slots[1].producerMarker<pool->slots[2].producerMarker,"each pending slot retains its immutable increasing owner-fence marker");
    }
    require(!pooled.publishStereo({left.Get(),right.Get()},producer.context.Get(),eyes),"a full pool skips new input without overwriting unread or pending slots");
    require(readyEventually(pooled,producer.context.Get(),textureMailboxSlots),"all three producer events complete while native image publication is paused");
    const auto newest=pool->slots[2].published;
    require(poolConsumer.consume(pool)&&poolConsumer.frame()==newest,"consumer selects the newest GPU-ready transaction rather than the first slot");
    require(poolConsumer.eyes()[0].sourceSequence==302&&poolConsumer.eyes()[1].trackingSequence==402,
        "newest stereo pixels retain their exact paired source/tracking generations");
    require(pixel(reader,poolConsumer.texture(),0,0,0)==std::array<unsigned char,4>{0,0,255,255}
        &&pixel(reader,poolConsumer.texture(),0,0,1)==std::array<unsigned char,4>{255,0,0,255},"newest selection copies both distinct stereo slices from the same slot");
    require(slotsIn(pool,TextureSlotState::consumerPending)==textureMailboxSlots,
        "older ready slots are acknowledged through real keyed release/events instead of being guessed free");
    require(!pooled.publishStereo({left.Get(),right.Get()},producer.context.Get(),eyes),
        "producer cannot reuse consumer-pending slots even after GPU readback has completed");
    for(int n=0;n<100&&slotsIn(pool,TextureSlotState::free)!=textureMailboxSlots;++n){
        require(!poolConsumer.consume(pool),"draining consumer events never invents a fresh or older frame");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    require(slotsIn(pool,TextureSlotState::free)==textureMailboxSlots,"consumer completion safely returns all three slots to the producer");
    require(poolConsumer.frame()==newest,"retiring skipped frames cannot move accepted frame identity backwards");
    eyes[0].sourceSequence=eyes[1].sourceSequence=303;
    require(pooled.publishStereo({left.Get(),right.Get()},producer.context.Get(),eyes),"a fully acknowledged slot can be reused");
    require(consumeEventually(poolConsumer,pooled,producer.context.Get()),"reused slot produces fresh paired-eye content");
    const auto resetFrame=poolConsumer.frame();poolConsumer.reset();
    require(pool->retired.load(),"consumer reset retires an epoch with pending GPU work instead of dropping its acknowledgment query");
    require(pooled.publishStereo({left.Get(),right.Get()},producer.context.Get(),eyes),"producer replaces a retired consumer epoch");
    require(consumeEventually(poolConsumer,pooled,producer.context.Get())&&poolConsumer.frame().epoch!=resetFrame.epoch,
        "consumer reset cannot wedge the pool or reintroduce a prior epoch");
    // Force compatibility independently on each owner. Fence/event selection is
    // internal capability policy, not a user setting or a weaker ownership gate.
    using Preference=TextureCompletionPreference;
    constexpr std::array<std::array<Preference,2>,3> combinations{{
        {Preference::eventOnly,Preference::eventOnly},
        {Preference::preferFence,Preference::eventOnly},
        {Preference::eventOnly,Preference::preferFence}}};
    for(const auto& choice:combinations){
        TextureMailbox compatible(choice[0]);TextureConsumer receiver(reader.device.Get(),choice[1]);
        for(size_t n=0;n<textureMailboxSlots;++n){
            clear(producer,left.Get(),leftColors[n]);clear(producer,right.Get(),rightColors[n]);
            for(uint32_t e=0;e<2;++e){eyes[e].sourceSequence=500+n;eyes[e].trackingSequence=600+n;eyes[e].eye=e;}
            require(compatible.publishStereo({left.Get(),right.Get()},producer.context.Get(),eyes),"mixed completion backends submit all three stereo slots");
        }
        const auto channel=compatible.latest();
        require(static_cast<bool>(channel->producerCompletion.fence)==(choice[0]==Preference::preferFence&&producerFences),"forced EVENT producer uses only its local event backend");
        require(!compatible.publishStereo({left.Get(),right.Get()},producer.context.Get(),eyes),"compatibility backend cannot overwrite a full pool");
        require(readyEventually(compatible,producer.context.Get(),textureMailboxSlots),"last paused publication completes on either producer backend");
        require(receiver.consume(channel)&&receiver.eyes()[0].sourceSequence==502&&receiver.eyes()[1].trackingSequence==602,"mixed owners keep newest paired-eye metadata exact");
        require(receiver.usingCompletionFence()==(choice[1]==Preference::preferFence&&consumerFences),"forced EVENT consumer uses only its local event backend");
        require(pixel(reader,receiver.texture(),0,0,0)==std::array<unsigned char,4>{0,0,255,255}
            &&pixel(reader,receiver.texture(),0,0,1)==std::array<unsigned char,4>{255,0,0,255},"mixed completion backends preserve distinct newest stereo pixels");
        require(slotsIn(channel,TextureSlotState::consumerPending)==textureMailboxSlots,"compatibility slots await real consumer completion");
        for(int n=0;n<100&&slotsIn(channel,TextureSlotState::free)!=textureMailboxSlots;++n){
            require(!receiver.consume(channel),"compatibility completion does not publish an old frame");std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        require(slotsIn(channel,TextureSlotState::free)==textureMailboxSlots,"either consumer backend safely drains all acknowledgments");
        const auto previous=receiver.frame();compatible.invalidate();
        require(!receiver.consume(compatible.latest())&&!receiver.frame().sequence&&!receiver.texture(),"null invalidation clears either backend's retained cache");
        require(compatible.publishStereo({left.Get(),right.Get()},producer.context.Get(),eyes),"either completion backend recovers after retirement");
        require(consumeEventually(receiver,compatible,producer.context.Get())&&receiver.frame().epoch!=previous.epoch,"mixed completion reset cannot resurrect an old epoch");
    }
    std::cout<<checks<<" real D3D11 checks passed across two hardware devices. No headset proof implied.\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
