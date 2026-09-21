#include "mgs5vr/render_size.hpp"
#include "mgs5vr/log.hpp"
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <MinHook.h>
#include <intrin.h>
#include <array>
#include <atomic>
#include <filesystem>
#include <string>
#include <vector>
#include <stdexcept>

namespace mgs5vr {
namespace {
using Microsoft::WRL::ComPtr;
UINT renderWidth{},renderHeight{},mirrorWidth{},mirrorHeight{},smallMessage{},canvasMessage{};
std::atomic<HWND> mirrorWindow{};
std::atomic_bool canvasReady{};
WNDPROC priorProcedure{};
bool sizing{};
using Fullscreen=HRESULT(WINAPI*)(IDXGISwapChain*,BOOL,IDXGIOutput*);
using ResizeTarget=HRESULT(WINAPI*)(IDXGISwapChain*,const DXGI_MODE_DESC*);
using CreateSwap=HRESULT(WINAPI*)(IDXGIFactory*,IUnknown*,DXGI_SWAP_CHAIN_DESC*,IDXGISwapChain**);
Fullscreen originalFullscreen{};ResizeTarget originalTarget{};
CreateSwap originalCreate{};
using ClientRect=BOOL(WINAPI*)(HWND,LPRECT);
ClientRect originalClientRect{};
uintptr_t nativeBegin{},nativeEnd{};
void attachWindow(HWND window);
bool gameWindow(HWND w){DWORD owner{};GetWindowThreadProcessId(w,&owner);return w&&owner==GetCurrentProcessId();}
bool mainWindow(HWND w){
    if(!gameWindow(w))return false;
    std::array<wchar_t,128> title{};GetWindowTextW(w,title.data(),static_cast<int>(title.size()));
    return !wcscmp(title.data(),L"METAL GEAR SOLID V: THE PHANTOM PAIN");
}
BOOL WINAPI clientRect(HWND window,LPRECT rect){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const auto result=originalClientRect(window,rect);
    // FlexibleWindowed creates ALL native render resources from its client size.
    // Only the game sees the render canvas. DXGI, Windows, XR and our preview
    // observer still see the real small client; no backbuffer-only override.
    if(result&&rect&&nativeBegin&&canvasReady.load()&&caller>=nativeBegin&&caller<nativeEnd&&mirrorWindow.load()==window){
        *rect={0,0,static_cast<LONG>(renderWidth),static_cast<LONG>(renderHeight)};
        static std::atomic_bool reported{};
        if(!reported.exchange(true))log("Native render canvas: "+std::to_string(renderWidth)+"x"+std::to_string(renderHeight)+"; desktop client remains independent");
    }
    return result;
}
bool managed(IDXGISwapChain* swap){
    DXGI_SWAP_CHAIN_DESC desc{};return mirrorWidth&&SUCCEEDED(swap->GetDesc(&desc))&&gameWindow(desc.OutputWindow)
        &&desc.BufferDesc.Width>=640&&desc.BufferDesc.Height>=360;
}
HRESULT WINAPI fullscreen(IDXGISwapChain* swap,BOOL value,IDXGIOutput* output){
    // A custom eye size must never become an exclusive monitor mode, including Alt+Enter.
    if(managed(swap))return originalFullscreen(swap,FALSE,nullptr);
    return originalFullscreen(swap,value,output);
}
HRESULT WINAPI target(IDXGISwapChain* swap,const DXGI_MODE_DESC* mode){
    if(managed(swap)&&canvasReady.load()&&mode){
        auto preview=*mode;preview.Width=mirrorWidth;preview.Height=mirrorHeight;
        preview.RefreshRate={0,0};return originalTarget(swap,&preview);
    }
    return originalTarget(swap,mode);
}
HRESULT WINAPI createSwap(IDXGIFactory* factory,IUnknown* device,DXGI_SWAP_CHAIN_DESC* desc,IDXGISwapChain** swap){
    if(!desc||!mainWindow(desc->OutputWindow))return originalCreate(factory,device,desc,swap);
    auto windowed=*desc;windowed.Windowed=TRUE;
    windowed.BufferDesc.RefreshRate={0,0};windowed.BufferDesc.Scaling=DXGI_MODE_SCALING_UNSPECIFIED;
    log("Keeping engine-owned render buffer "+std::to_string(windowed.BufferDesc.Width)+"x"+std::to_string(windowed.BufferDesc.Height));
    return originalCreate(factory,device,&windowed,swap);
}
RECT mirrorBounds(HWND window){
    RECT bounds{0,0,static_cast<LONG>(mirrorWidth),static_cast<LONG>(mirrorHeight)};
    AdjustWindowRectEx(&bounds,static_cast<DWORD>(GetWindowLongPtrW(window,GWL_STYLE)),FALSE,static_cast<DWORD>(GetWindowLongPtrW(window,GWL_EXSTYLE)));
    return bounds;
}
LRESULT CALLBACK procedure(HWND w,UINT message,WPARAM a,LPARAM b){
    const auto original=priorProcedure;
    if(message==smallMessage||message==canvasMessage){
        const bool preview=message==smallMessage;
        sizing=preview;
        auto style=GetWindowLongPtrW(w,GWL_STYLE);
        style=(style&~(WS_POPUP|WS_MAXIMIZE))|WS_OVERLAPPEDWINDOW;
        SetWindowLongPtrW(w,GWL_STYLE,style);
        const auto ex=GetWindowLongPtrW(w,GWL_EXSTYLE)&~WS_EX_TOPMOST;
        SetWindowLongPtrW(w,GWL_EXSTYLE,ex);
        RECT bounds{0,0,static_cast<LONG>(preview?mirrorWidth:renderWidth),static_cast<LONG>(preview?mirrorHeight:renderHeight)};
        AdjustWindowRectEx(&bounds,static_cast<DWORD>(style),FALSE,static_cast<DWORD>(ex));
        RECT work{};SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
        SetWindowPos(w,HWND_NOTOPMOST,work.left+24,work.top+24,bounds.right-bounds.left,bounds.bottom-bounds.top,SWP_NOACTIVATE|SWP_FRAMECHANGED);
        sizing=false;
        log(preview?"Small PC preview: "+std::to_string(mirrorWidth)+"x"+std::to_string(mirrorHeight)+"; native render buffer unchanged"
            :"Initializing the engine's native windowed render canvas");
        return 0;
    }
    if(message==WM_STYLECHANGING&&static_cast<int>(a)==GWL_STYLE){
        auto* style=reinterpret_cast<STYLESTRUCT*>(b);
        style->styleNew=(style->styleNew&~(WS_POPUP|WS_MAXIMIZE))|WS_OVERLAPPEDWINDOW;
        return 0;
    }
    if(message==WM_WINDOWPOSCHANGING){
        auto* position=reinterpret_cast<WINDOWPOS*>(b);
        if(canvasReady.load()&&!(position->flags&SWP_NOSIZE)&&!IsIconic(w)){
            const auto bounds=mirrorBounds(w);
            position->cx=bounds.right-bounds.left;position->cy=bounds.bottom-bounds.top;
        }
        if(position->hwndInsertAfter==HWND_TOPMOST)position->hwndInsertAfter=HWND_NOTOPMOST;
        return 0;
    }
    // Windowed DXGI presentation scales the fixed eye buffer into the client
    // area. Do not let a preview resize rebuild the engine at desktop-preview resolution.
    if(message==WM_SIZE&&a!=SIZE_MINIMIZED){
        // Our own preview-only resize must not recreate render resources. A
        // native mode change must still reach FOX so it resizes every target.
        if(sizing)return 0;
        return original?CallWindowProcW(original,w,message,a,canvasReady.load()?MAKELPARAM(renderWidth,renderHeight):b):0;
    }
    if(sizing&&message==WM_WINDOWPOSCHANGED)return DefWindowProcW(w,message,a,b);
    if(message==WM_NCDESTROY){mirrorWindow.store(nullptr);priorProcedure=nullptr;}
    return original?CallWindowProcW(original,w,message,a,b):DefWindowProcW(w,message,a,b);
}
void attachWindow(HWND window){
    HWND expected{};if(!mirrorWindow.compare_exchange_strong(expected,window))return;
    priorProcedure=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(window,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(&procedure)));
    if(!priorProcedure){mirrorWindow.store(nullptr);return;}
    PostMessageW(window,canvasReady.load()?smallMessage:canvasMessage,0,0);
}
void hook(void* address,void* replacement,void** original,std::vector<void*>& created){
    const auto result=MH_CreateHook(address,replacement,original);
    if(result!=MH_OK)throw std::runtime_error(std::string("Render-size hook: ")+MH_StatusToString(result));
    created.push_back(address);
    if(MH_QueueEnableHook(address)!=MH_OK)throw std::runtime_error("Cannot queue render-size hook");
}
}
void installRenderSizeHooks(IDXGISwapChain* probe,const std::filesystem::path& configuration){
    std::array<wchar_t,32768> exe{};GetModuleFileNameW(nullptr,exe.data(),static_cast<DWORD>(exe.size()));
    const auto path=configuration.empty()?std::filesystem::path(exe.data()).parent_path()/L"mgs5vr-display.ini":configuration;
    if(GetPrivateProfileIntW(L"display",L"enabled",0,path.c_str())!=1)return;
    renderWidth=GetPrivateProfileIntW(L"display",L"render_width",0,path.c_str());
    renderHeight=GetPrivateProfileIntW(L"display",L"render_height",0,path.c_str());
    mirrorWidth=GetPrivateProfileIntW(L"display",L"mirror_width",960,path.c_str());
    mirrorHeight=GetPrivateProfileIntW(L"display",L"mirror_height",540,path.c_str());
    if(renderWidth<640||renderWidth>8192||renderHeight<360||renderHeight>8192||renderWidth%2||renderHeight%2
        ||mirrorWidth<320||mirrorWidth>1920||mirrorHeight<240||mirrorHeight>1080){
        renderWidth=renderHeight=mirrorWidth=mirrorHeight=0;log("Invalid display configuration; native display handling retained");return;
    }
    smallMessage=RegisterWindowMessageW(L"MGS5VR.SmallPreview.1");
    canvasMessage=RegisterWindowMessageW(L"MGS5VR.NativeCanvas.1");
    ComPtr<ID3D11Device> device;ComPtr<IDXGIDevice> dxgi;ComPtr<IDXGIAdapter> adapter;ComPtr<IDXGIFactory> factory;
    if(FAILED(probe->GetDevice(IID_PPV_ARGS(&device)))||FAILED(device.As(&dxgi))||FAILED(dxgi->GetAdapter(&adapter))
       )throw std::runtime_error("No DXGI adapter for windowed preview");
    if(FAILED(adapter->GetParent(IID_PPV_ARGS(&factory))))throw std::runtime_error("No DXGI factory for windowed render-size adapter");
    std::vector<void*> created;
    try{
        if(!_wcsicmp(std::filesystem::path(exe.data()).filename().c_str(),L"mgsvtpp.exe")){
            nativeBegin=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
            const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(nativeBegin);
            const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS*>(nativeBegin+dos->e_lfanew);
            nativeEnd=nativeBegin+nt->OptionalHeader.SizeOfImage;
            hook(reinterpret_cast<void*>(&GetClientRect),reinterpret_cast<void*>(&clientRect),reinterpret_cast<void**>(&originalClientRect),created);
        }
        auto** v=*reinterpret_cast<void***>(factory.Get());hook(v[10],reinterpret_cast<void*>(&createSwap),reinterpret_cast<void**>(&originalCreate),created);
        v=*reinterpret_cast<void***>(probe);
        hook(v[10],reinterpret_cast<void*>(&fullscreen),reinterpret_cast<void**>(&originalFullscreen),created);
        hook(v[14],reinterpret_cast<void*>(&target),reinterpret_cast<void**>(&originalTarget),created);
        if(MH_ApplyQueued()!=MH_OK)throw std::runtime_error("Cannot enable render-size hooks");
    }catch(...){for(auto address:created){MH_DisableHook(address);MH_RemoveHook(address);}renderWidth=renderHeight=mirrorWidth=mirrorHeight=0;throw;}
    log("Independent windowed display requested: native "+std::to_string(renderWidth)+"x"+std::to_string(renderHeight)
        +", PC preview "+std::to_string(mirrorWidth)+"x"+std::to_string(mirrorHeight));
}
void observeRenderWindow(IDXGISwapChain* swap){
    if(!managed(swap))return;
    DXGI_SWAP_CHAIN_DESC desc{};if(FAILED(swap->GetDesc(&desc)))return;
    // Let the engine finish its bootstrap -> requested-size transition before
    // separating the desktop preview. Shrinking the 1024x576 boot window first
    // makes FlexibleWindowed treat that small preview as the requested canvas.
    if(!desc.Windowed){static bool reported{};if(!reported){reported=true;log("Small preview unavailable: game created an exclusive swapchain");}return;}
    if(desc.BufferDesc.Width==renderWidth&&desc.BufferDesc.Height==renderHeight&&!canvasReady.exchange(true)){
        if(mirrorWindow.load())PostMessageW(desc.OutputWindow,smallMessage,0,0);
    }
    if(mirrorWindow.load())return;
    attachWindow(desc.OutputWindow);
}
}
