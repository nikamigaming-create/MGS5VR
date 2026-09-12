#include "mgs5vr/capture_hook.hpp"
#include "mgs5vr/head_camera.hpp"
#include "mgs5vr/log.hpp"
#include "mgs5vr/render_camera.hpp"
#include "mgs5vr/scene_capture.hpp"
#include "mgs5vr/native_performance.hpp"
#include "mgs5vr/menu_surface.hpp"
#include "mgs5vr/ui_renderer.hpp"
#include <MinHook.h>
#include <atomic>
#include <stdexcept>
#include <mutex>
#include <chrono>

namespace mgs5vr {
namespace {
using PresentFn=HRESULT(WINAPI*)(IDXGISwapChain*,UINT,UINT);
using ResizeFn=HRESULT(WINAPI*)(IDXGISwapChain*,UINT,UINT,UINT,DXGI_FORMAT,UINT);
PresentFn originalPresent{};
ResizeFn originalResize{};
TextureMailbox* destination{};
std::mutex captureMutex;
IDXGISwapChain* selected{}; // Identity only. Never hold a backbuffer or swapchain reference across Present.
HWND selectedWindow{};
bool failed{};
uint64_t presented{},copied{};
HRESULT WINAPI present(IDXGISwapChain* swap,UINT interval,UINT flags){
    using Clock=std::chrono::steady_clock;const auto entered=Clock::now();
    bool pace=false;
    if(!(flags&DXGI_PRESENT_TEST)) {
        std::lock_guard lock(captureMutex);
        if(!failed)try {
            DXGI_SWAP_CHAIN_DESC desc{};checkHr(swap->GetDesc(&desc),"Get game swapchain");
            DWORD owner=0;GetWindowThreadProcessId(desc.OutputWindow,&owner);
            if(owner==GetCurrentProcessId()&&desc.BufferDesc.Width>=640&&desc.BufferDesc.Height>=360
                &&(!selectedWindow||!IsWindow(selectedWindow)||selectedWindow==desc.OutputWindow)) {
                selected=swap;selectedWindow=desc.OutputWindow;
                pace=nativeFrameRateEnabled();
                observeSceneSwapchain(swap);
                ComPtr<ID3D11Texture2D> source;
                checkHr(swap->GetBuffer(0,IID_PPV_ARGS(&source)),"Get game backbuffer");
                ComPtr<ID3D11Device> device;source->GetDevice(&device);
                ComPtr<ID3D11DeviceContext> context;device->GetImmediateContext(&context);
                const auto eye=observeRenderPresent(swap);
                if(headCamera().active()&&nativeTitleMenuOpen())
                    captureNativeMenuSurface(context.Get(),presented+1);
                // Keep the complete stereo family in the mailbox between native
                // scene completions. A mono fallback would replace its array and
                // force the XR consumer to rebuild both eye swapchains.
                const bool published=headCamera().active()
                    ? publishSceneEyes(swap,context.Get(),*destination)
                    : destination->publish(source.Get(),context.Get(),eye);
                if(published)++copied;
                if(++presented%600==1)log("Game Present="+std::to_string(presented)+" published="+std::to_string(copied)
                    +" size="+std::to_string(desc.BufferDesc.Width)+"x"+std::to_string(desc.BufferDesc.Height)+" interval="+std::to_string(interval));
            }
        }catch(const std::exception& e){failed=true;log(std::string("Capture suspended: ")+e.what());}
        catch(...){failed=true;log("Capture suspended: unexpected exception");}
    }
    const auto captured=Clock::now();if(pace)paceNativePresent();const auto paced=Clock::now();
    const auto result=originalPresent(swap,pace?0:interval,flags);
    if(pace){const auto elapsed=[](auto begin,auto end){return std::chrono::duration<double,std::milli>(end-begin).count();};
        recordNativePresent(elapsed(entered,captured),elapsed(captured,paced),elapsed(paced,Clock::now()));}
    if(result==DXGI_ERROR_DEVICE_REMOVED||result==DXGI_ERROR_DEVICE_RESET){
        std::lock_guard lock(captureMutex);selected=nullptr;selectedWindow=nullptr;failed=false;destination->invalidate();
        log("Game device lost; waiting for replacement swapchain");
    }
    return result;
}
HRESULT WINAPI resize(IDXGISwapChain* swap,UINT count,UINT width,UINT height,DXGI_FORMAT format,UINT flags){
    // Serialize capture with resize. Mailbox owns copies, never backbuffer references.
    std::lock_guard lock(captureMutex);
    if(selected==swap){destination->invalidate();invalidateSceneCapture();stopNativeMenuSurface();selected=nullptr;failed=false;log("Game swapchain resize");}
    return originalResize(swap,count,width,height,format,flags);
}
void mh(MH_STATUS status,const char* operation){
    if(status!=MH_OK)throw std::runtime_error(std::string(operation)+": "+MH_StatusToString(status));
}
struct Dummy {
    HWND window{};
    ComPtr<IDXGISwapChain> swap;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ~Dummy(){context.Reset();device.Reset();swap.Reset();if(window)DestroyWindow(window);}
};
}
void installCaptureHook(TextureMailbox& mailbox){
    Dummy dummy;
    dummy.window=CreateWindowExW(0,L"STATIC",L"MGS5VR capture initialization",WS_OVERLAPPED,0,0,32,32,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!dummy.window)throw std::runtime_error("Create hidden DXGI probe window failed");
    DXGI_SWAP_CHAIN_DESC desc{};desc.BufferDesc.Width=32;desc.BufferDesc.Height=32;
    desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount=1;desc.OutputWindow=dummy.window;desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    const D3D_FEATURE_LEVEL level=D3D_FEATURE_LEVEL_11_0;
    checkHr(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,&level,1,D3D11_SDK_VERSION,
        &desc,&dummy.swap,&dummy.device,nullptr,&dummy.context),"Create capture probe device");
    auto** table=*reinterpret_cast<void***>(dummy.swap.Get());
    void* presentAddress=table[8];void* resizeAddress=table[13];
    const auto init=MH_Initialize();if(init!=MH_OK&&init!=MH_ERROR_ALREADY_INITIALIZED)mh(init,"Initialize MinHook");
    destination=&mailbox;
    mh(MH_CreateHook(presentAddress,reinterpret_cast<void*>(&present),reinterpret_cast<void**>(&originalPresent)),"Create Present hook");
    try {
        mh(MH_CreateHook(resizeAddress,reinterpret_cast<void*>(&resize),reinterpret_cast<void**>(&originalResize)),"Create ResizeBuffers hook");
        mh(MH_QueueEnableHook(presentAddress),"Queue Present hook");
        mh(MH_QueueEnableHook(resizeAddress),"Queue ResizeBuffers hook");
        mh(MH_ApplyQueued(),"Enable capture hooks");
    }catch(...){MH_DisableHook(presentAddress);MH_DisableHook(resizeAddress);MH_RemoveHook(presentAddress);MH_RemoveHook(resizeAddress);throw;}
    installSceneCapture(dummy.device.Get());
    log("D3D11 capture hooks installed; native stereo awaits a complete scene draw pair");
}
void stopCapture() noexcept {
    try {
        std::lock_guard lock(captureMutex);
        failed=true;selected=nullptr;selectedWindow=nullptr;
        if(destination)destination->invalidate();
        invalidateSceneCapture();
        stopNativeMenuSurface();
    }catch(...){}
}
}
