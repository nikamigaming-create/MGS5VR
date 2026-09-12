// Private capture helper. Uses the game's process audio endpoint, independent
// of desktop focus and unrelated applications. QPC timestamps join native video.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <audioclientactivationparams.h>
#include <wrl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <array>
#include <vector>
#include <cstring>
#include <stdexcept>

using Microsoft::WRL::ComPtr;
void check(HRESULT result,const char* operation){if(FAILED(result))throw std::runtime_error(std::string(operation)+" HRESULT="+std::to_string(static_cast<unsigned long>(result)));}
struct Handle {HANDLE value{};~Handle(){if(value)CloseHandle(value);}};
class Activation final:public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IActivateAudioInterfaceCompletionHandler,Microsoft::WRL::FtmBase> {
public:
    Handle ready{CreateEventW(nullptr,TRUE,FALSE,nullptr)};
    HRESULT result{E_PENDING};ComPtr<IAudioClient> client;
    HRESULT STDMETHODCALLTYPE ActivateCompleted(IActivateAudioInterfaceAsyncOperation* operation) override {
        ComPtr<IUnknown> object;HRESULT status{};
        result=operation->GetActivateResult(&status,&object);
        if(SUCCEEDED(result))result=status;
        if(SUCCEEDED(result))result=object.As(&client);
        SetEvent(ready.value);return S_OK;
    }
};
int wmain(int argc,wchar_t** argv){
    if(argc!=4){std::cerr<<"capture-game-audio.exe GAME_PID OUTPUT.wav STOP_FILE\n";return 2;}
    try {
        const auto pid=static_cast<DWORD>(std::stoul(argv[1]));
        const std::filesystem::path output=argv[2],stop=argv[3];
        if(!output.is_absolute()||std::filesystem::exists(output))throw std::runtime_error("Use a new absolute WAV path");
        Handle process{OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid)};
        std::array<wchar_t,32768> name{};DWORD count=static_cast<DWORD>(name.size());
        if(!process.value||!QueryFullProcessImageNameW(process.value,0,name.data(),&count)
            ||_wcsicmp(std::filesystem::path(name.data()).filename().c_str(),L"mgsvtpp.exe"))throw std::runtime_error("PID is not the MGSV game");
        check(CoInitializeEx(nullptr,COINIT_MULTITHREADED),"Initialize capture COM");
        auto activation=Microsoft::WRL::Make<Activation>();
        AUDIOCLIENT_ACTIVATION_PARAMS params{};params.ActivationType=AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
        params.ProcessLoopbackParams={pid,PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE};
        PROPVARIANT prop{};prop.vt=VT_BLOB;prop.blob.cbSize=sizeof(params);prop.blob.pBlobData=reinterpret_cast<BYTE*>(&params);
        ComPtr<IActivateAudioInterfaceAsyncOperation> operation;
        check(ActivateAudioInterfaceAsync(VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK,__uuidof(IAudioClient),&prop,activation.Get(),&operation),"Activate game audio");
        if(WaitForSingleObject(activation->ready.value,10000)!=WAIT_OBJECT_0)throw std::runtime_error("Audio activation timed out");
        check(activation->result,"Game audio activation");
        WAVEFORMATEX format{};format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=2;format.nSamplesPerSec=48000;
        format.wBitsPerSample=16;format.nBlockAlign=4;format.nAvgBytesPerSec=192000;
        check(activation->client->Initialize(AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_LOOPBACK|AUDCLNT_STREAMFLAGS_EVENTCALLBACK|AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM|AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
            0,0,&format,nullptr),"Initialize game audio");
        Handle samples{CreateEventW(nullptr,FALSE,FALSE,nullptr)};
        check(activation->client->SetEventHandle(samples.value),"Audio event");
        ComPtr<IAudioCaptureClient> capture;check(activation->client->GetService(IID_PPV_ARGS(&capture)),"Audio capture service");
        std::filesystem::create_directories(output.parent_path());
        std::ofstream wav(output,std::ios::binary);std::array<char,44> header{};wav.write(header.data(),header.size());
        if(!wav)throw std::runtime_error("Cannot create WAV");
        uint64_t firstQpc{},lastQpc{},firstDevice{},totalFrames{},gaps{},flagsSeen{};
        check(activation->client->Start(),"Start audio");
        std::cout<<"READY game="<<pid<<" PCM 48000 Hz stereo\n"<<std::flush;
        const auto began=GetTickCount64();
        while(!std::filesystem::exists(stop)&&GetTickCount64()-began<1800000
            &&WaitForSingleObject(process.value,0)==WAIT_TIMEOUT){
            WaitForSingleObject(samples.value,100);
            UINT32 next{};check(capture->GetNextPacketSize(&next),"Audio packet");
            while(next){
                BYTE* data{};UINT32 frames{};DWORD flags{};UINT64 device{},qpc{};
                check(capture->GetBuffer(&data,&frames,&flags,&device,&qpc),"Read game audio");
                if(!firstQpc){firstQpc=qpc;firstDevice=device;}
                flagsSeen|=flags;lastQpc=qpc;
                const auto expected=device-firstDevice;
                if(expected>totalFrames&&expected-totalFrames<48000*5){
                    const auto missing=expected-totalFrames;std::vector<char> silence(static_cast<size_t>(missing)*4);
                    wav.write(silence.data(),static_cast<std::streamsize>(silence.size()));totalFrames+=missing;++gaps;
                }
                const auto bytes=static_cast<std::streamsize>(frames)*4;
                if(flags&AUDCLNT_BUFFERFLAGS_SILENT){std::vector<char> silence(static_cast<size_t>(bytes));wav.write(silence.data(),bytes);}
                else wav.write(reinterpret_cast<char*>(data),bytes);
                totalFrames+=frames;check(capture->ReleaseBuffer(frames),"Release audio");
                check(capture->GetNextPacketSize(&next),"Audio packet");
            }
        }
        check(activation->client->Stop(),"Stop audio");
        const auto dataBytes=static_cast<uint32_t>(totalFrames*4),riffBytes=dataBytes+36;
        const uint32_t fmtSize=16;
        std::memcpy(header.data(),"RIFF",4);std::memcpy(header.data()+4,&riffBytes,4);std::memcpy(header.data()+8,"WAVEfmt ",8);
        std::memcpy(header.data()+16,&fmtSize,4);std::memcpy(header.data()+20,&format,16);
        std::memcpy(header.data()+36,"data",4);std::memcpy(header.data()+40,&dataBytes,4);
        wav.seekp(0);wav.write(header.data(),header.size());wav.close();
        std::ofstream meta(output.string()+".json");meta<<"{\"game_pid\":"<<pid<<",\"first_qpc_100ns\":"<<firstQpc
            <<",\"last_qpc_100ns\":"<<lastQpc<<",\"frames\":"<<totalFrames<<",\"sample_rate\":48000,\"gaps\":"<<gaps<<",\"flags\":"<<flagsSeen<<"}\n";
        std::cout<<"FINISHED frames="<<totalFrames<<" gaps="<<gaps<<'\n';
        return totalFrames?0:1;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
