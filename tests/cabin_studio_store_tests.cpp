#include "mgs5vr/cabin_studio_store.hpp"
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <thread>

namespace {
std::atomic_uint saveFailures{};
int failures{};
void expect(bool value,const char* message){if(!value){++failures;std::cerr<<"FAIL "<<message<<'\n';}}
template<class Predicate>bool waitFor(Predicate predicate){
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    do{if(predicate())return true;std::this_thread::sleep_for(std::chrono::milliseconds(20));}
    while(std::chrono::steady_clock::now()<until);
    return predicate();
}
}
namespace mgs5vr {
// Exercise the real storage implementation without starting the renderer or a
// game session. The logger also provides a deterministic failed-I/O checkpoint.
void log(std::string_view message) noexcept{
    if(message.starts_with("Cabin studio save failed:"))++saveFailures;
}
void checkHr(HRESULT result,const char* operation){if(FAILED(result))throw std::runtime_error(operation);}
}
namespace {
void writePhoto(const std::filesystem::path& path){
    using Microsoft::WRL::ComPtr;using mgs5vr::checkHr;
    ComPtr<IWICImagingFactory> factory;
    checkHr(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)),"test WIC");
    ComPtr<IWICStream> stream;checkHr(factory->CreateStream(&stream),"test stream");
    checkHr(stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE),"test path");
    ComPtr<IWICBitmapEncoder> encoder;checkHr(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder),"test encoder");
    checkHr(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache),"test encode init");
    ComPtr<IWICBitmapFrameEncode> frame;checkHr(encoder->CreateNewFrame(&frame,nullptr),"test frame");
    checkHr(frame->Initialize(nullptr),"test frame init");checkHr(frame->SetSize(1,1),"test size");
    auto format=GUID_WICPixelFormat32bppBGRA;checkHr(frame->SetPixelFormat(&format),"test format");
    uint32_t pixel=0xff123456;checkHr(frame->WritePixels(1,4,4,reinterpret_cast<BYTE*>(&pixel)),"test pixels");
    checkHr(frame->Commit(),"test frame commit");checkHr(encoder->Commit(),"test commit");
}
}
int main(){
    using namespace mgs5vr;namespace fs=std::filesystem;
    wchar_t appData[32768]{};GetEnvironmentVariableW(L"LOCALAPPDATA",appData,32768);
    const fs::path root(appData);
    // CTest must supply this dedicated root before the production storage
    // module initializes. Refuse to touch a real user's LOCALAPPDATA.
    if(root.filename()!=L"studio-store-test-data"||!root.is_absolute()){
        std::cerr<<"Refusing storage test outside its isolated CTest directory\n";return 2;
    }
    const auto directory=root/L"MGS5VR"/L"CabinStudio";
    if(fs::exists(directory)&&!fs::is_empty(directory)){
        std::cerr<<"Refusing to overwrite preexisting storage test data\n";return 2;
    }
    fs::create_directories(directory);
    const auto com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    try{
        // A temporary-path obstruction deterministically rejects the first
        // write without modifying a personal collection or using permissions.
        fs::create_directory(directory/L"board.tmp");
        auto frame=std::make_shared<CabinStudioFrame>();frame->revision=1;
        StudioPaper paper{};paper.id=1;auto ink=std::make_shared<StudioInk>();ink->text="RECOVERED";
        paper.ink=ink;frame->papers.push_back(paper);
        saveCabinStudio(frame);
        expect(waitFor([]{return saveFailures.load()>0;}),"blocked board write is reported");
        expect(!fs::exists(directory/L"board.dat"),"failed write does not publish a board");
        for(unsigned i=0;i<20;++i)saveCabinStudio(frame);
        expect(saveFailures.load()==1,"render-frame submissions do not hammer unavailable storage");
        fs::remove(directory/L"board.tmp");
        expect(waitFor([&]{saveCabinStudio(frame);return fs::is_regular_file(directory/L"board.dat");}),
            "the same unchanged revision retries after storage recovers");
        const auto restored=loadCabinStudio();
        expect(restored.papers.size()==1&&restored.papers[0].id==1&&restored.papers[0].ink->text=="RECOVERED",
            "retry preserves the actual note content and identity");

        constexpr uint64_t photo=711;
        expect(!studioPhotoPixels(photo),"missing photo returns unavailable pixels");
        writePhoto(directory/L"711.png");
        expect(waitFor([&]{const auto pixels=studioPhotoPixels(photo);
            return pixels&&pixels->width==1&&pixels->height==1&&pixels->bgra.size()==1&&pixels->bgra[0]==0xff123456;}),
            "photo decode recovers after a missing file appears");
    }catch(const std::exception& e){++failures;std::cerr<<"FAIL test exception: "<<e.what()<<'\n';}
    stopCabinStudioStore();
    if(SUCCEEDED(com))CoUninitialize();
    // Remove only the explicitly test-owned names, never a recursive tree.
    for(const auto* name:{L"board.dat",L"board.tmp",L"711.png"})fs::remove(directory/name);
    fs::remove(directory);fs::remove(root/L"MGS5VR");fs::remove(root);
    std::cout<<"Studio storage recovery checks: "<<failures<<" failures. No game or headset acceptance.\n";
    return failures?1:0;
}
