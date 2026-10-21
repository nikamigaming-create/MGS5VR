#include "mgs5vr/cabin_studio_store.hpp"
#include "mgs5vr/log.hpp"
#include "mgs5vr/mailbox.hpp"
#include <windows.h>
#include <wincodec.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <atomic>
#include <algorithm>
#include <condition_variable>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <thread>

namespace mgs5vr {
namespace {
using Microsoft::WRL::ComPtr;
std::filesystem::path studioDirectory(){
    wchar_t path[32768]{};const auto n=GetEnvironmentVariableW(L"LOCALAPPDATA",path,32768);
    return n&&n<32768?std::filesystem::path(path)/L"MGS5VR"/L"CabinStudio":std::filesystem::path{};
}
const auto directory=studioDirectory();
template<class T>void write(std::ostream& stream,const T& value){stream.write(reinterpret_cast<const char*>(&value),sizeof(value));}
template<class T>bool read(std::istream& stream,T& value){return bool(stream.read(reinterpret_cast<char*>(&value),sizeof(value)));}
std::filesystem::path photoPath(uint64_t photo){return directory/(std::to_wstring(photo)+L".png");}
void persist(const CabinStudioFrame& frame){
    if(directory.empty())return;std::filesystem::create_directories(directory);
    const auto temporary=directory/L"board.tmp",target=directory/L"board.dat";
    std::ofstream stream(temporary,std::ios::binary|std::ios::trunc);const uint32_t magic=0x32545343;
    write(stream,magic);const auto count=static_cast<uint32_t>(frame.papers.size());write(stream,count);
    for(const auto& paper:frame.papers){
        write(stream,paper.id);write(stream,paper.photo);write(stream,paper.pose);const uint8_t pinned=paper.pinned?1:0;write(stream,pinned);
        const auto& ink=*paper.ink;const auto text=static_cast<uint32_t>(ink.text.size());write(stream,text);stream.write(ink.text.data(),text);
        const auto strokes=static_cast<uint32_t>(ink.strokes.size());write(stream,strokes);
        for(const auto& stroke:ink.strokes){const auto points=static_cast<uint32_t>(stroke.points.size());write(stream,points);
            stream.write(reinterpret_cast<const char*>(stroke.points.data()),static_cast<std::streamsize>(points*sizeof(StudioPoint)));}
    }
    const auto photos=static_cast<uint32_t>(frame.photos.size());write(stream,photos);
    for(const auto photo:frame.photos)write(stream,photo);
    write(stream,frame.selectedPhoto);
    stream.flush();if(!stream)throw std::runtime_error("Studio board write incomplete");stream.close();
    if(!MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Studio board atomic replace failed");
    uint64_t pending{};std::ifstream receipt(directory/L"pending-photo.dat",std::ios::binary);
    if(read(receipt,pending)&&(std::find(frame.photos.begin(),frame.photos.end(),pending)!=frame.photos.end()
        ||std::any_of(frame.papers.begin(),frame.papers.end(),[&](const auto& p){return p.photo==pending;}))){
        receipt.close();std::filesystem::remove(directory/L"pending-photo.dat");}
}
void encodePhoto(const StudioPhotoPixels& pixels,uint64_t photo){
    std::filesystem::create_directories(directory);const auto target=photoPath(photo),temporary=directory/(std::to_wstring(photo)+L".tmp");
    ComPtr<IWICImagingFactory> factory;checkHr(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)),"Studio WIC");
    ComPtr<IWICStream> stream;checkHr(factory->CreateStream(&stream),"Studio PNG stream");
    checkHr(stream->InitializeFromFilename(temporary.c_str(),GENERIC_WRITE),"Studio PNG path");
    ComPtr<IWICBitmapEncoder> encoder;checkHr(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder),"Studio PNG encoder");
    checkHr(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache),"Studio PNG initialize");
    ComPtr<IWICBitmapFrameEncode> frame;checkHr(encoder->CreateNewFrame(&frame,nullptr),"Studio PNG frame");
    checkHr(frame->Initialize(nullptr),"Studio PNG frame initialize");checkHr(frame->SetSize(pixels.width,pixels.height),"Studio PNG size");
    auto format=GUID_WICPixelFormat32bppBGRA;checkHr(frame->SetPixelFormat(&format),"Studio PNG format");
    checkHr(frame->WritePixels(pixels.height,pixels.width*4,static_cast<UINT>(pixels.bgra.size()*4),
        reinterpret_cast<BYTE*>(const_cast<uint32_t*>(pixels.bgra.data()))),"Studio PNG pixels");
    checkHr(frame->Commit(),"Studio PNG frame commit");checkHr(encoder->Commit(),"Studio PNG commit");
    frame.Reset();encoder.Reset();stream.Reset();
    if(!MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Studio photo publish failed");
    // A print survives quitting between the PNG commit and the next XR packet.
    const auto pending=directory/L"pending-photo.tmp";std::ofstream receipt(pending,std::ios::binary|std::ios::trunc);
    write(receipt,photo);receipt.flush();if(!receipt)throw std::runtime_error("Studio print receipt failed");receipt.close();
    if(!MoveFileExW(pending.c_str(),(directory/L"pending-photo.dat").c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Studio print receipt publish failed");
}
struct Store {
    std::mutex mutex;std::condition_variable wake;std::thread worker;bool stopping{};
    std::shared_ptr<const CabinStudioFrame> save;
    std::shared_ptr<StudioPhotoPixels> encode;
    uint64_t encodeId{},savedRevision{};
    std::map<uint64_t,std::shared_ptr<const StudioPhotoPixels>> images;
    std::set<uint64_t> retainedImages;
    std::atomic_uint64_t completed{},accepted{};
    ComPtr<ID3D11Texture2D> pending;ComPtr<ID3D11Query> query;ComPtr<ID3D11Device> device;
    D3D11_TEXTURE2D_DESC desc{};uint64_t shutter{},queued{};bool armed{};
    Store():worker([this]{
        const auto com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
        for(;;){
            std::shared_ptr<const CabinStudioFrame> state;std::shared_ptr<StudioPhotoPixels> photo;uint64_t id{};
            {std::unique_lock lock(mutex);wake.wait(lock,[&]{return stopping||save||encode;});
                if(stopping&&!save&&!encode)break;state=std::move(save);photo=std::move(encode);id=encodeId;}
            try{if(photo){encodePhoto(*photo,id);{std::lock_guard lock(mutex);images[id]=photo;retainedImages.insert(id);}completed.store(id);}
                if(state){persist(*state);std::lock_guard lock(mutex);
                    std::set<uint64_t> retained(state->photos.begin(),state->photos.end());
                    for(const auto& paper:state->papers)if(paper.photo)retained.insert(paper.photo);
                    uint64_t pendingPhoto{};std::ifstream receipt(directory/L"pending-photo.dat",std::ios::binary);
                    if(read(receipt,pendingPhoto)&&pendingPhoto)retained.insert(pendingPhoto);
                    // Only resources previously owned by this store can be
                    // collected. Printed copies keep their immutable pixels
                    // even after the digital library entry is deleted.
                    for(const auto old:retainedImages)if(!retained.contains(old)){
                        std::filesystem::remove(photoPath(old));images.erase(old);
                    }
                    retainedImages=std::move(retained);
                    // A library can be much larger than the CPU image cache.
                    for(auto it=images.begin();it!=images.end();)if(images.size()>12&&it->first!=state->selectedPhoto
                        &&it->first!=completed.load())it=images.erase(it);else ++it;
                }
            }catch(const std::exception& e){log(std::string("Cabin studio save failed: ")+e.what());}
        }
        if(SUCCEEDED(com))CoUninitialize();
    }){}
    ~Store(){stop();}
    void stop(){
        {std::lock_guard lock(mutex);stopping=true;}wake.notify_one();if(worker.joinable())worker.join();
    }
};
Store& store(){static Store value;return value;}
}
StudioCollection loadCabinStudio(){
    if(directory.empty())return {};
    const auto readBoard=[&]() -> StudioCollection {StudioCollection collection;auto& papers=collection.papers;
    std::ifstream stream(directory/L"board.dat",std::ios::binary);uint32_t magic{},count{};
    if(!read(stream,magic)||(magic!=0x31545343&&magic!=0x32545343)||!read(stream,count)||count>studioPaperLimit)return {};
    for(uint32_t i=0;i<count;++i){
        StudioPaper paper;uint8_t pinned{};uint32_t text{},strokes{};auto ink=std::make_shared<StudioInk>();
        if(!read(stream,paper.id)||!read(stream,paper.photo)||!read(stream,paper.pose)||!valid(paper.pose)
            ||!read(stream,pinned)||!read(stream,text)||text>studioTextLimit)return {};
        ink->text.resize(text);if(!stream.read(ink->text.data(),text)||!read(stream,strokes)||strokes>128)return {};
        size_t total{};
        for(uint32_t s=0;s<strokes;++s){uint32_t points{};if(!read(stream,points)||points>1024||(total+=points)>8192)return StudioCollection{};
            StudioStroke stroke;stroke.points.resize(points);
            if(!stream.read(reinterpret_cast<char*>(stroke.points.data()),static_cast<std::streamsize>(points*sizeof(StudioPoint))))return StudioCollection{};
            for(const auto p:stroke.points)if(!std::isfinite(p.u)||!std::isfinite(p.v)||p.u<0||p.u>1||p.v<0||p.v>1)return StudioCollection{};
            ink->strokes.push_back(std::move(stroke));}
        paper.pinned=pinned!=0;paper.ink=ink;papers.push_back(std::move(paper));
    }
    if(magic==0x32545343){
        uint32_t photos{};if(!read(stream,photos)||photos>studioPhotoLimit)return {};
        for(uint32_t i=0;i<photos;++i){uint64_t photo{};
            if(!read(stream,photo)||!photo||std::find(collection.photos.begin(),collection.photos.end(),photo)!=collection.photos.end())return StudioCollection{};
            collection.photos.push_back(photo);
        }
        if(!read(stream,collection.selectedPhoto))return {};
    }else{
        // Upgrade the former automatic-print collection once. New-format
        // loads never reconstruct deleted digital entries from wall prints.
        for(const auto& paper:papers)if(paper.photo&&std::find(collection.photos.begin(),collection.photos.end(),paper.photo)==collection.photos.end())
            collection.photos.push_back(paper.photo);
        if(!collection.photos.empty())collection.selectedPhoto=collection.photos.back();
    }
    return collection;};
    auto collection=readBoard();auto& papers=collection.papers;
    uint64_t pending{};std::ifstream receipt(directory/L"pending-photo.dat",std::ios::binary);
    if(read(receipt,pending)&&pending&&std::filesystem::is_regular_file(photoPath(pending))
        &&std::find(collection.photos.begin(),collection.photos.end(),pending)==collection.photos.end()){
        if(collection.photos.size()<studioPhotoLimit){collection.photos.push_back(pending);collection.selectedPhoto=pending;}
        if(papers.size()<studioPaperLimit&&std::none_of(papers.begin(),papers.end(),[&](const auto& p){return p.photo==pending;})){
            StudioPaper paper{};paper.id=1;for(const auto& p:papers)paper.id=std::max(paper.id,p.id+1);
            paper.photo=pending;paper.pose={{-.70710678f,0,0,.70710678f},studioPrintHome};paper.ink=std::make_shared<const StudioInk>();
            papers.push_back(std::move(paper));
        }
        receipt.close();CabinStudioFrame restored;restored.papers=papers;restored.photos=collection.photos;
        restored.selectedPhoto=collection.selectedPhoto;persist(restored);
    }
    auto& s=store();{std::lock_guard lock(s.mutex);
        s.retainedImages.insert(collection.photos.begin(),collection.photos.end());
        for(const auto& paper:papers)if(paper.photo)s.retainedImages.insert(paper.photo);
    }
    if(collection.selectedPhoto)studioPhotoPixels(collection.selectedPhoto);
    return collection;
}
void saveCabinStudio(std::shared_ptr<const CabinStudioFrame> frame) noexcept{
    try{auto& s=store();std::lock_guard lock(s.mutex);if(s.stopping||!frame||!frame->revision||frame->revision==s.savedRevision)return;
        s.savedRevision=frame->revision;s.save=std::move(frame);s.wake.notify_one();}catch(...){}
}
uint64_t completedStudioPhoto() noexcept{return store().completed.load();}
uint64_t acceptedStudioShutter() noexcept{return store().accepted.load();}
bool captureStudioPhoto(ID3D11DeviceContext* context,ID3D11Texture2D* source,uint64_t shutter) noexcept{
    if(!context||!source||!shutter||directory.empty())return false;
    try{auto& s=store();std::lock_guard lock(s.mutex);if(s.stopping||s.pending||s.encode||shutter<=s.accepted.load())return false;
        D3D11_TEXTURE2D_DESC desc{};source->GetDesc(&desc);
        if(!desc.Width||!desc.Height||desc.Width>8192||desc.Height>8192||desc.ArraySize!=1||desc.SampleDesc.Count!=1
            ||(desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM&&desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
                &&desc.Format!=DXGI_FORMAT_B8G8R8A8_UNORM&&desc.Format!=DXGI_FORMAT_B8G8R8A8_UNORM_SRGB))return false;
        context->GetDevice(&s.device);s.desc=desc;desc.BindFlags=desc.MiscFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        desc.MipLevels=1;checkHr(s.device->CreateTexture2D(&desc,nullptr,&s.pending),"Studio photo readback");
        const D3D11_QUERY_DESC query{D3D11_QUERY_EVENT,0};checkHr(s.device->CreateQuery(&query,&s.query),"Studio photo completion");
        context->CopySubresourceRegion(s.pending.Get(),0,0,0,0,source,0,nullptr);
        s.shutter=shutter;s.queued=GetTickCount64();s.armed=false;s.accepted.store(shutter);return true;
    }catch(...){return false;}
}
void pollStudioPhoto(ID3D11DeviceContext* context) noexcept{
    if(!context||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return;
    try{auto& s=store();std::lock_guard lock(s.mutex);if(!s.pending)return;
        if(GetTickCount64()-s.queued>5000){s.pending.Reset();s.query.Reset();log("Camera photo GPU completion timed out");return;}
        ComPtr<ID3D11Device> device;context->GetDevice(&device);if(device.Get()!=s.device.Get())return;
        if(!s.armed){context->End(s.query.Get());s.armed=true;return;}
        BOOL ready{};if(context->GetData(s.query.Get(),&ready,sizeof(ready),D3D11_ASYNC_GETDATA_DONOTFLUSH)!=S_OK||!ready)return;
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if(FAILED(context->Map(s.pending.Get(),0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&mapped)))return;
        auto photo=std::make_shared<StudioPhotoPixels>();photo->width=photo->height=1024;photo->bgra.resize(1024*1024);
        const bool rgba=s.desc.Format==DXGI_FORMAT_R8G8B8A8_UNORM||s.desc.Format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        for(uint32_t y=0;y<1024;++y){const auto* row=reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(mapped.pData)
            +static_cast<size_t>(y)*s.desc.Height/1024*mapped.RowPitch);
            for(uint32_t x=0;x<1024;++x){const auto p=row[static_cast<size_t>(x)*s.desc.Width/1024];
                photo->bgra[static_cast<size_t>(y)*1024+x]=rgba?(p&0xff00ff00)|((p&0xff)<<16)|((p>>16)&0xff)|0xff000000:p|0xff000000;}}
        context->Unmap(s.pending.Get(),0);s.pending.Reset();s.query.Reset();
        FILETIME stamp{};GetSystemTimeAsFileTime(&stamp);s.encodeId=((uint64_t(stamp.dwHighDateTime)<<32)|stamp.dwLowDateTime)/10000;
        s.encode=std::move(photo);s.wake.notify_one();
    }catch(...){}
}
std::shared_ptr<const StudioPhotoPixels> studioPhotoPixels(uint64_t photo){
    if(!photo||directory.empty())return {};auto& s=store();{std::lock_guard lock(s.mutex);if(const auto it=s.images.find(photo);it!=s.images.end())return it->second;}
    const auto com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);std::shared_ptr<StudioPhotoPixels> pixels;
    try{ComPtr<IWICImagingFactory> factory;checkHr(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)),"Studio photo load");
        ComPtr<IWICBitmapDecoder> decoder;checkHr(factory->CreateDecoderFromFilename(photoPath(photo).c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder),"Studio PNG load");
        ComPtr<IWICBitmapFrameDecode> frame;checkHr(decoder->GetFrame(0,&frame),"Studio PNG read");
        pixels=std::make_shared<StudioPhotoPixels>();checkHr(frame->GetSize(&pixels->width,&pixels->height),"Studio PNG dimensions");
        if(pixels->width>2048||pixels->height>2048)throw std::runtime_error("Studio photo bounds");pixels->bgra.resize(static_cast<size_t>(pixels->width)*pixels->height);
        ComPtr<IWICFormatConverter> converted;checkHr(factory->CreateFormatConverter(&converted),"Studio PNG conversion");
        checkHr(converted->Initialize(frame.Get(),GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom),"Studio PNG BGRA");
        checkHr(converted->CopyPixels(nullptr,pixels->width*4,static_cast<UINT>(pixels->bgra.size()*4),reinterpret_cast<BYTE*>(pixels->bgra.data())),"Studio PNG pixels");
    }catch(...){pixels.reset();}
    if(SUCCEEDED(com))CoUninitialize();{std::lock_guard lock(s.mutex);s.images[photo]=pixels;}return pixels;
}
void stopCabinStudioStore() noexcept{try{store().stop();}catch(...){}}
}
