#include "mgs5vr/native_video.hpp"
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

namespace mgs5vr {
namespace {
int64_t captureClock(){
    LARGE_INTEGER counter{},frequency{};QueryPerformanceCounter(&counter);QueryPerformanceFrequency(&frequency);
    return counter.QuadPart/frequency.QuadPart*10000000
        +counter.QuadPart%frequency.QuadPart*10000000/frequency.QuadPart;
}
struct VideoFrame {std::vector<unsigned char> bytes;int64_t clock{};};
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
    int64_t scheduled{};
    std::atomic_bool finished{true};
    bool stopping{};uint64_t dropped{};
    struct Slot {ComPtr<ID3D11Texture2D> texture;int64_t clock{};};
    std::array<Slot,3> slots;
    std::mutex mutex;std::condition_variable wake;std::deque<VideoFrame> queue;std::thread encoder;
    State(){
        std::array<wchar_t,32768> exe{};
        if(GetModuleFileNameW(nullptr,exe.data(),static_cast<DWORD>(exe.size())))
            request=std::filesystem::path(exe.data()).parent_path()/L"mgs5vr-recording.txt";
    }
    ~State(){stop();if(encoder.joinable())encoder.join();}
    void stop(){
        {std::lock_guard lock(mutex);stopping=true;}
        wake.notify_one();for(auto& slot:slots)slot.clock=0;
    }
    void encode(){
        const auto com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
        bool media=false;uint64_t frames{};int64_t first{},last{};
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
            for(;;){
                VideoFrame frame;
                {std::unique_lock lock(mutex);wake.wait(lock,[&]{return stopping||!queue.empty();});
                    if(queue.empty()&&stopping)break;
                    frame=std::move(queue.front());queue.pop_front();}
                if(!first)first=frame.clock;
                ComPtr<IMFMediaBuffer> buffer;ComPtr<IMFSample> sample;
                const auto length=static_cast<DWORD>(frame.bytes.size());
                checkHr(MFCreateMemoryBuffer(length,&buffer),"Video buffer");
                BYTE* bytes{};checkHr(buffer->Lock(&bytes,nullptr,nullptr),"Video buffer lock");
                std::memcpy(bytes,frame.bytes.data(),length);buffer->Unlock();buffer->SetCurrentLength(length);
                checkHr(MFCreateSample(&sample),"Video sample");sample->AddBuffer(buffer.Get());
                sample->SetSampleTime(frame.clock-first);sample->SetSampleDuration(10000000/30);
                checkHr(writer->WriteSample(stream,sample.Get()),"Encode native eye");
                last=frame.clock;++frames;
            }
            checkHr(writer->Finalize(),"Finish MP4");
        }catch(const std::exception& e){error=e.what();log("Native video stopped: "+error);}
        if(media)MFShutdown();if(SUCCEEDED(com))CoUninitialize();
        {std::lock_guard lock(mutex);queue.clear();stopping=true;
            std::ofstream metadata(path.string()+".json");
            metadata<<"{\"frames\":"<<frames<<",\"width\":"<<width<<",\"height\":"<<height
                <<",\"first_qpc_100ns\":"<<first<<",\"last_qpc_100ns\":"<<last<<",\"dropped\":"<<dropped
                <<",\"complete\":"<<(error.empty()?"true":"false")<<"}\n";}
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
        requested=next;path.clear();slots={};scheduled=0;started=budgetChecked=0;budgetStopped=false;
        if(next.empty())return;
        auto candidate=std::filesystem::path(std::u8string(next.begin(),next.end()));
        if(!candidate.is_absolute()||candidate.extension()!=L".mp4"||std::filesystem::exists(candidate)){
            log("Native video request requires a new absolute MP4 path");return;
        }
        std::filesystem::create_directories(candidate.parent_path());
        if(std::filesystem::space(candidate.parent_path()).available<25ull*1024*1024*1024){
            log("Native video requires at least 25 GiB free before starting");return;
        }
        path=candidate;
    }
    void frame(ID3D11Device* device,ID3D11DeviceContext* context,ID3D11Texture2D* source,uint32_t slice){
        poll();if(path.empty()||!source||!context||!device)return;
        D3D11_TEXTURE2D_DESC desc{};source->GetDesc(&desc);
        const bool rgba=desc.Format==DXGI_FORMAT_R8G8B8A8_UNORM||desc.Format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        const bool bgra=desc.Format==DXGI_FORMAT_B8G8R8A8_UNORM||desc.Format==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        if((!rgba&&!bgra)||slice>=desc.ArraySize||desc.SampleDesc.Count!=1)return;
        if(!slots[0].texture){
            width=desc.Width&~1u;height=desc.Height&~1u;format=desc.Format;
            if(!width||!height||width>4096||height>4096)return;
            desc.Width=width;desc.Height=height;desc.ArraySize=desc.MipLevels=1;desc.Usage=D3D11_USAGE_STAGING;
            desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.BindFlags=desc.MiscFlags=0;
            for(auto& slot:slots)checkHr(device->CreateTexture2D(&desc,nullptr,&slot.texture),"Native video staging");
            {std::lock_guard lock(mutex);stopping=false;dropped=0;queue.clear();}
            started=budgetChecked=steadyMilliseconds();
            finished.store(false);encoder=std::thread([this]{encode();});
        }
        if(finished.load())return;
        {std::lock_guard lock(mutex);if(stopping)return;}
        if((desc.Width&~1u)!=width||(desc.Height&~1u)!=height||desc.Format!=format){
            log("Native video texture changed; finishing current take");stop();return;
        }
        for(auto& slot:slots)if(slot.clock){
            D3D11_MAPPED_SUBRESOURCE mapped{};
            const auto result=context->Map(slot.texture.Get(),0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&mapped);
            if(result==DXGI_ERROR_WAS_STILL_DRAWING)continue;
            checkHr(result,"Read native video frame");
            VideoFrame copy;copy.clock=slot.clock;copy.bytes.resize(static_cast<size_t>(width)*height*4);
            for(UINT y=0;y<height;++y){
                auto* dst=copy.bytes.data()+static_cast<size_t>(y)*width*4;
                const auto* src=static_cast<const unsigned char*>(mapped.pData)+static_cast<size_t>(y)*mapped.RowPitch;
                std::memcpy(dst,src,static_cast<size_t>(width)*4);
                if(rgba)for(UINT x=0;x<width;++x)std::swap(dst[x*4],dst[x*4+2]);
            }
            context->Unmap(slot.texture.Get(),0);slot.clock=0;
            {std::lock_guard lock(mutex);if(queue.size()<4)queue.push_back(std::move(copy));else ++dropped;}
            wake.notify_one();
        }
        const auto clock=captureClock();if(clock<scheduled)return;
        scheduled=scheduled&&clock-scheduled<10000000?scheduled+10000000/30:clock+10000000/30;
        for(auto& slot:slots)if(!slot.clock){
            const D3D11_BOX box{0,0,0,width,height,1};
            context->CopySubresourceRegion(slot.texture.Get(),0,0,0,0,source,slice,&box);slot.clock=clock;return;
        }
        {std::lock_guard lock(mutex);++dropped;}
    }
};
NativeVideoRecorder::NativeVideoRecorder():state_(std::make_unique<State>()){}
NativeVideoRecorder::~NativeVideoRecorder()=default;
void NativeVideoRecorder::frame(ID3D11Device* device,ID3D11DeviceContext* context,ID3D11Texture2D* source,uint32_t slice) noexcept {
    try{state_->frame(device,context,source,slice);}catch(const std::exception& e){state_->stop();log(std::string("Native capture unavailable: ")+e.what());}
}
}
