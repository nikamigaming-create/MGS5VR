#include "mgs5vr/native_video.hpp"
#include "mgs5vr/native_video_timing.hpp"
#include "mgs5vr/log.hpp"
#include "mgs5vr/input_bridge.hpp"
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>
#include <cstring>
#include <cmath>
#include <stdexcept>

namespace mgs5vr {
namespace {
int64_t captureClock(){
    LARGE_INTEGER counter{},frequency{};QueryPerformanceCounter(&counter);QueryPerformanceFrequency(&frequency);
    return counter.QuadPart/frequency.QuadPart*10000000
        +counter.QuadPart%frequency.QuadPart*10000000/frequency.QuadPart;
}
struct VideoFrame {
    std::unique_ptr<unsigned char[]> bytes;
    size_t length{};
    int64_t clock{};
    uint64_t slot{};
    bool rgba{};
    FrameId image{};
    EyeFrame eye{};
};
void videoType(IMFMediaType* type,UINT width,UINT height,const GUID& format){
    checkHr(type->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Video),"Video major type");
    checkHr(type->SetGUID(MF_MT_SUBTYPE,format),"Video subtype");
    checkHr(MFSetAttributeSize(type,MF_MT_FRAME_SIZE,width,height),"Video size");
    checkHr(MFSetAttributeRatio(type,MF_MT_FRAME_RATE,30,1),"Video rate");
    checkHr(MFSetAttributeRatio(type,MF_MT_PIXEL_ASPECT_RATIO,1,1),"Video pixels");
    checkHr(type->SetUINT32(MF_MT_INTERLACE_MODE,MFVideoInterlace_Progressive),"Video scan");
}
}
struct NativeVideoRecorder::State {
    std::filesystem::path request,path;
    std::string requested;
    uint64_t polled{},started{},budgetChecked{};
    bool budgetStopped{};
    UINT width{},height{};DXGI_FORMAT format{};
    EyeFov renderFov{},displayFov{};bool stereo{};uint32_t eyeIndex{};
    NativeVideoCadence cadence;
    std::atomic_bool finished{true};
    bool stopping{},captureFailed{};
    uint64_t missedCadence{},queueDropped{},stagingBusy{},stagingDiscarded{};
    struct Slot {ComPtr<ID3D11Texture2D> texture;int64_t clock{};uint64_t captureSlot{};FrameId image{};EyeFrame eye{};};
    std::array<Slot,3> slots;
    std::mutex mutex;std::condition_variable wake;std::deque<VideoFrame> queue;std::thread encoder;
    explicit State(const wchar_t* requestName){
        std::array<wchar_t,32768> exe{};
        if(GetModuleFileNameW(nullptr,exe.data(),static_cast<DWORD>(exe.size())))
            request=std::filesystem::path(exe.data()).parent_path()/requestName;
    }
    ~State(){stop();if(encoder.joinable())encoder.join();}
    void stop(bool failed=false){
        {std::lock_guard lock(mutex);stopping=true;captureFailed=captureFailed||failed;
            for(auto& slot:slots)if(slot.clock){++stagingDiscarded;slot.clock=0;}}
        wake.notify_one();
    }
    void encode(){
        const auto com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
        bool media=false;uint64_t frames{},encodedFrames{},repeatedFrames{};int64_t first{},last{};
        NativeVideoTimeline timeline;
        struct Accepted {int64_t clock{};uint64_t captureSlot{},encodedSlot{},repeatsBefore{};FrameId image{};EyeFrame eye{};};
        std::vector<Accepted> accepted;
        std::string error;
        try {
            checkHr(MFStartup(MF_VERSION,MFSTARTUP_LITE),"Start video encoder");media=true;
            ComPtr<IMFAttributes> attributes;checkHr(MFCreateAttributes(&attributes,2),"Video attributes");
            attributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS,TRUE);
            attributes->SetUINT32(MF_SINK_WRITER_DISABLE_THROTTLING,TRUE);
            ComPtr<IMFSinkWriter> writer;
            checkHr(MFCreateSinkWriterFromURL(path.c_str(),nullptr,attributes.Get(),&writer),"Create MP4");
            ComPtr<IMFMediaType> output,input;checkHr(MFCreateMediaType(&output),"Video output");
            videoType(output.Get(),width,height,MFVideoFormat_H264);
            output->SetUINT32(MF_MT_AVG_BITRATE,20000000);
            DWORD stream{};checkHr(writer->AddStream(output.Get(),&stream),"Video stream");
            checkHr(MFCreateMediaType(&input),"Video input");videoType(input.Get(),width,height,MFVideoFormat_RGB32);
            input->SetUINT32(MF_MT_DEFAULT_STRIDE,width*4);
            checkHr(writer->SetInputMediaType(stream,input.Get(),nullptr),"Video RGB input");
            checkHr(writer->BeginWriting(),"Begin video");
            log("Native video recording "+path.string()+" at "+std::to_string(width)+"x"+std::to_string(height)+" / 30 FPS");
            ComPtr<IMFMediaBuffer> previous;
            const auto write=[&](IMFMediaBuffer* buffer,uint64_t slot){
                ComPtr<IMFSample> sample;checkHr(MFCreateSample(&sample),"Video sample");
                checkHr(sample->AddBuffer(buffer),"Video sample buffer");
                checkHr(sample->SetSampleTime(nativeVideoSlotTime(slot)),"Video sample timestamp");
                checkHr(sample->SetSampleDuration(nativeVideoSlotTime(slot+1)-nativeVideoSlotTime(slot)),"Video sample duration");
                checkHr(writer->WriteSample(stream,sample.Get()),"Encode native eye");
                ++encodedFrames;
            };
            for(;;){
                VideoFrame frame;
                {std::unique_lock lock(mutex);wake.wait(lock,[&]{return stopping||!queue.empty();});
                    if(queue.empty()&&stopping)break;
                    frame=std::move(queue.front());queue.pop_front();}
                const auto placement=timeline.accept(frame.clock,frame.slot);
                if(!placement)throw std::runtime_error("Native video source timestamps/slots are unordered or exceed the take bound");
                if(!first)first=frame.clock;
                // Submit a complete rational CFR timeline ourselves. Repeats
                // use the preceding accepted source pixels and are explicit in
                // the ledger; the encoder must not invent unaccounted slots.
                while(encodedFrames<placement->encodedSlot){
                    if(!previous)throw std::runtime_error("Native video cadence has no preceding source frame");
                    write(previous.Get(),encodedFrames);++repeatedFrames;
                }
                ComPtr<IMFMediaBuffer> buffer;
                const auto length=static_cast<DWORD>(frame.length);
                checkHr(MFCreateMemoryBuffer(length,&buffer),"Video buffer");
                BYTE* bytes{};checkHr(buffer->Lock(&bytes,nullptr,nullptr),"Video buffer lock");
                std::memcpy(bytes,frame.bytes.get(),length);
                // Channel conversion belongs to encoding, not the OpenXR
                // frame loop. Every source row was copied in full; source
                // timestamps and accepted/encoded slot accounting stay intact.
                const auto normalized=normalizeNativeVideoPixels({bytes,length},frame.rgba);
                buffer->Unlock();
                if(!normalized)throw std::runtime_error("Native video pixel buffer is not complete RGB32");
                buffer->SetCurrentLength(length);
                write(buffer.Get(),placement->encodedSlot);previous=buffer;
                last=frame.clock;++frames;
                accepted.push_back({frame.clock,frame.slot,placement->encodedSlot,placement->repeatsBefore,frame.image,frame.eye});
                // Let capture clients admit their first action only after an
                // actual source frame has reached the encoder for this take.
                if(frames==1)log("Native video first frame encoded "+path.string()+" qpc_100ns="+std::to_string(first));
            }
            checkHr(writer->Finalize(),"Finish MP4");
        }catch(const std::exception& e){error=e.what();log("Native video stopped: "+error);}
        if(media)MFShutdown();if(SUCCEEDED(com))CoUninitialize();
        uint64_t missed{},queueLoss{},busy{},discarded{};bool captureError{};NativeVideoCadence schedule;
        {std::lock_guard lock(mutex);queue.clear();stopping=true;
            missed=missedCadence;queueLoss=queueDropped;busy=stagingBusy;discarded=stagingDiscarded;
            captureError=captureFailed;schedule=cadence;}
        // The render thread never waits for the timestamp ledger's file I/O.
        // Publish the complete sidecar atomically so readers cannot observe a
        // partially written accepted-sample array.
        {
            const auto dropped=missed+queueLoss+busy+discarded;
            const auto temporary=path.string()+".json.tmp",destination=path.string()+".json";
            std::ofstream metadata(temporary);
            metadata<<"{\"schema\":2,\"frames\":"<<frames<<",\"width\":"<<width<<",\"height\":"<<height
                <<",\"source_dxgi_format\":"<<static_cast<unsigned>(format)
                <<",\"first_qpc_100ns\":"<<first<<",\"last_qpc_100ns\":"<<last<<",\"dropped\":"<<dropped
                <<",\"complete\":"<<(error.empty()&&!captureError?"true":"false")
                <<",\"capture_schedule_origin_qpc_100ns\":"<<schedule.origin()
                <<",\"scheduled_slots\":"<<schedule.slots()<<",\"missed_source_cadence_slots\":"<<missed
                <<",\"queue_dropped_samples\":"<<queueLoss<<",\"staging_busy_samples\":"<<busy
                <<",\"staging_discarded_samples\":"<<discarded
                <<",\"encoded_frames\":"<<encodedFrames<<",\"repeated_frames\":"<<repeatedFrames
                <<",\"timestamp_semantics\":\"source_qpc_is_actual_copy_time;encoded_slot_is_30Hz_CFR;gap_slots_repeat_preceding_source\""
                <<",\"source_lineage_schema\":1,\"source_sample_clock\":\"steady_ms\",\"accepted_samples\":[";
            for(size_t i=0;i<accepted.size();++i){const auto& item=accepted[i];if(i)metadata<<',';
                metadata<<"{\"qpc_100ns\":"<<item.clock<<",\"capture_slot\":"<<item.captureSlot
                    <<",\"encoded_slot\":"<<item.encodedSlot<<",\"repeats_before\":"<<item.repeatsBefore
                    <<",\"image_epoch\":"<<item.image.epoch<<",\"image_sequence\":"<<item.image.sequence
                    <<",\"source_sequence\":"<<item.eye.sourceSequence<<",\"tracking_sequence\":"<<item.eye.trackingSequence
                    <<",\"activation\":"<<item.eye.activation<<",\"source_sample_ms\":"<<item.eye.sampleTime
                    <<",\"source_eye\":"<<item.eye.eye<<",\"source_projected\":"<<(item.eye.projected?"true":"false")
                    <<",\"source_joined\":"<<(item.eye.joined?"true":"false")<<'}';}
            metadata<<']';
            if(stereo){
                metadata<<",\"eye\":"<<eyeIndex<<",\"render_fov\":["<<renderFov.left<<','<<renderFov.right<<','<<renderFov.up<<','<<renderFov.down
                    <<"],\"display_fov\":["<<displayFov.left<<','<<displayFov.right<<','<<displayFov.up<<','<<displayFov.down<<']';
            }
            metadata<<"}\n";metadata.flush();const bool saved=static_cast<bool>(metadata);metadata.close();
            std::error_code publishError;
            if(saved)std::filesystem::rename(temporary,destination,publishError);
            if(!saved||publishError)log("Native video timestamp metadata could not be finalized");
        }
        log("Native video finalized frames="+std::to_string(frames));finished.store(true);
    }
    void poll(){
        const auto now=steadyMilliseconds();if(now-polled<300)return;polled=now;
        if(!finished.load()&&!budgetStopped&&now-budgetChecked>=1000){
            budgetChecked=now;
            std::error_code error;
            const auto space=std::filesystem::space(path.parent_path(),error);
            if(now-started>=120000||error||space.available<25ull*1024*1024*1024){
                budgetStopped=true;
                log("Native video reached its two-minute or 25 GiB free-space limit; finalizing take");
                stop();
            }
        }
        std::ifstream input(request);std::string next;std::getline(input,next);
        while(!next.empty()&&(next.back()=='\r'||next.back()=='\n'))next.pop_back();
        if(next==requested)return;
        if(!finished.load()){stop();return;}
        if(encoder.joinable())encoder.join();
        requested=next;path.clear();slots={};cadence={};started=budgetChecked=0;budgetStopped=false;
        if(next.empty())return;
        auto candidate=std::filesystem::path(std::u8string(next.begin(),next.end()));
        if(!candidate.is_absolute()||candidate.extension()!=L".mp4"||std::filesystem::exists(candidate)
           ||std::filesystem::exists(candidate.string()+".json")||std::filesystem::exists(candidate.string()+".json.tmp")){
            log("Native video request requires a new absolute MP4 path");return;
        }
        std::filesystem::create_directories(candidate.parent_path());
        if(std::filesystem::space(candidate.parent_path()).available<25ull*1024*1024*1024){
            log("Native video requires at least 25 GiB free before starting");return;
        }
        path=candidate;
    }
    void frame(ID3D11Device* device,ID3D11DeviceContext* context,ID3D11Texture2D* source,uint32_t slice,const EyeFrame* eye,FrameId image){
        poll();if(path.empty()||!source||!context||!device)return;
        D3D11_TEXTURE2D_DESC desc{};source->GetDesc(&desc);
        const bool rgba=desc.Format==DXGI_FORMAT_R8G8B8A8_UNORM||desc.Format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB||desc.Format==DXGI_FORMAT_R8G8B8A8_TYPELESS;
        const bool bgra=desc.Format==DXGI_FORMAT_B8G8R8A8_UNORM||desc.Format==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB||desc.Format==DXGI_FORMAT_B8G8R8A8_TYPELESS;
        if((!rgba&&!bgra)||slice>=desc.ArraySize||desc.SampleDesc.Count!=1)return;
        const bool projected=eye&&eye->projected&&valid(eye->view.fov)&&valid(eye->displayFov);
        if(!slots[0].texture){
            stereo=projected;
            renderFov=stereo?eye->view.fov:EyeFov{};displayFov=stereo?eye->displayFov:EyeFov{};eyeIndex=stereo?eye->eye:0;
            width=desc.Width&~1u;height=desc.Height&~1u;format=desc.Format;
            if(!width||!height||width>4096||height>4096)return;
            desc.Width=width;desc.Height=height;desc.ArraySize=desc.MipLevels=1;desc.Usage=D3D11_USAGE_STAGING;
            desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.BindFlags=desc.MiscFlags=0;
            for(auto& slot:slots)checkHr(device->CreateTexture2D(&desc,nullptr,&slot.texture),"Native video staging");
            {std::lock_guard lock(mutex);stopping=captureFailed=false;
                missedCadence=queueDropped=stagingBusy=stagingDiscarded=0;queue.clear();}
            started=budgetChecked=steadyMilliseconds();
            finished.store(false);encoder=std::thread([this]{encode();});
        }
        if(finished.load())return;
        {std::lock_guard lock(mutex);if(stopping)return;}
        if((desc.Width&~1u)!=width||(desc.Height&~1u)!=height||desc.Format!=format){
            log("Native video texture changed; finishing current take");stop();return;
        }
        const auto sameFov=[](EyeFov a,EyeFov b){
            return std::abs(a.left-b.left)<.00001f&&std::abs(a.right-b.right)<.00001f
                &&std::abs(a.up-b.up)<.00001f&&std::abs(a.down-b.down)<.00001f;
        };
        // Every frame in one take must share the projection described by its
        // metadata. A menu/VR transition starts a new take, not mixed aspect ratios.
        if(projected!=stereo||(stereo&&(eye->eye!=eyeIndex||!sameFov(eye->view.fov,renderFov)||!sameFov(eye->displayFov,displayFov)))){
            log("Native video projection changed; finishing current take");stop();return;
        }
        for(size_t attempt=0;attempt<slots.size();++attempt){
            std::array<int64_t,3> clocks{};
            for(size_t i=0;i<slots.size();++i)clocks[i]=slots[i].clock;
            const auto oldest=oldestNativeVideoSlot(clocks);if(!oldest)break;
            auto& slot=slots[*oldest];
            D3D11_MAPPED_SUBRESOURCE mapped{};
            const auto result=context->Map(slot.texture.Get(),0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&mapped);
            // Do not let a ready newer slot overtake the oldest GPU copy.
            // Neither polling nor waiting for that copy belongs on rendering.
            if(result==DXGI_ERROR_WAS_STILL_DRAWING)break;
            checkHr(result,"Read native video frame");
            VideoFrame copy;copy.clock=slot.clock;copy.slot=slot.captureSlot;copy.rgba=rgba;
            copy.image=slot.image;copy.eye=slot.eye;
            copy.length=static_cast<size_t>(width)*height*4;
            // Copy all pixels before handoff. Avoid initializing a second
            // full-eye buffer that the row copy immediately overwrites.
            copy.bytes=std::make_unique_for_overwrite<unsigned char[]>(copy.length);
            for(UINT y=0;y<height;++y){
                auto* dst=copy.bytes.get()+static_cast<size_t>(y)*width*4;
                const auto* src=static_cast<const unsigned char*>(mapped.pData)+static_cast<size_t>(y)*mapped.RowPitch;
                std::memcpy(dst,src,static_cast<size_t>(width)*4);
            }
            context->Unmap(slot.texture.Get(),0);slot.clock=0;
            {std::lock_guard lock(mutex);if(queue.size()<4&&!stopping)queue.push_back(std::move(copy));else ++queueDropped;}
            wake.notify_one();
        }
        const auto clock=captureClock();std::optional<NativeVideoDue> due;
        {std::lock_guard lock(mutex);if(stopping)return;
            due=cadence.due(clock);if(due)missedCadence+=due->missed;}
        if(!due)return;
        if(due->slot>=nativeVideoMaxSlots){stop(true);return;}
        for(auto& slot:slots)if(!slot.clock){
            const D3D11_BOX box{0,0,0,width,height,1};
            context->CopySubresourceRegion(slot.texture.Get(),0,0,0,0,source,slice,&box);
            // Keep the transaction selected when this GPU copy was queued.
            // The source texture and caller's current eye can change before Map.
            slot.clock=clock;slot.captureSlot=due->slot;slot.image=image;slot.eye=eye?*eye:EyeFrame{};return;
        }
        {std::lock_guard lock(mutex);++stagingBusy;}
    }
};
NativeVideoRecorder::NativeVideoRecorder(const wchar_t* requestName):state_(std::make_unique<State>(requestName)){}
NativeVideoRecorder::~NativeVideoRecorder()=default;
void NativeVideoRecorder::frame(ID3D11Device* device,ID3D11DeviceContext* context,ID3D11Texture2D* source,uint32_t slice,const EyeFrame* eye,FrameId image) noexcept {
    try{state_->frame(device,context,source,slice,eye,image);}catch(const std::exception& e){state_->stop(true);log(std::string("Native capture unavailable: ")+e.what());}
}
}
