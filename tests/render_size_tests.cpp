#include "mgs5vr/render_size.hpp"
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <MinHook.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(){
    HWND probeWindow{},gameWindow{};std::filesystem::path path;int result=1;
    try {
        const auto suffix=std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
        path=std::filesystem::temp_directory_path()/(L"mgs5vr-display-fixture-"+suffix+L".ini");
        require(!std::filesystem::exists(path),"Fixture path already exists");
        {std::ofstream out(path);out<<"[display]\nenabled=1\nrender_width=4160\nrender_height=4160\nmirror_width=960\nmirror_height=540\n";}
        // Both windows remain hidden. No game, headset, or monitor mode changes.
        probeWindow=CreateWindowExW(0,L"STATIC",L"MGS5VR display test probe",WS_OVERLAPPEDWINDOW,0,0,32,32,nullptr,nullptr,nullptr,nullptr);
        gameWindow=CreateWindowExW(0,L"STATIC",L"METAL GEAR SOLID V: THE PHANTOM PAIN",WS_OVERLAPPEDWINDOW,0,0,100,100,nullptr,nullptr,nullptr,nullptr);
        require(probeWindow&&gameWindow,"Create hidden test windows");
        ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;ComPtr<IDXGISwapChain> probe;
        DXGI_SWAP_CHAIN_DESC desc{};desc.BufferDesc.Width=32;desc.BufferDesc.Height=32;desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=1;desc.OutputWindow=probeWindow;desc.Windowed=TRUE;
        require(SUCCEEDED(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&probe,&device,nullptr,&context)),"Create test D3D device");
        require(MH_Initialize()==MH_OK,"Initialize test hooks");
        mgs5vr::installRenderSizeHooks(probe.Get(),path);
        ComPtr<IDXGIDevice> dxgi;ComPtr<IDXGIAdapter> adapter;ComPtr<IDXGIFactory> factory;ComPtr<IDXGISwapChain> native;
        require(SUCCEEDED(device.As(&dxgi))&&SUCCEEDED(dxgi->GetAdapter(&adapter))&&SUCCEEDED(adapter->GetParent(IID_PPV_ARGS(&factory))),"Find DXGI factory");
        desc.OutputWindow=gameWindow;desc.BufferDesc.Width=1920;desc.BufferDesc.Height=1080;
        require(SUCCEEDED(factory->CreateSwapChain(device.Get(),&desc,&native)),"Create game-like swapchain");
        require(SUCCEEDED(native->GetDesc(&desc))&&desc.BufferDesc.Width==1920&&desc.BufferDesc.Height==1080&&desc.Windowed,"Display adapter must never override engine-owned render dimensions");
        mgs5vr::observeRenderWindow(native.Get());
        RECT bootClient{};GetClientRect(gameWindow,&bootClient);
        require(bootClient.right!=960,"Do not shrink the bootstrap window before native resources have the requested size");
        require(SUCCEEDED(native->ResizeBuffers(1,4160,4160,DXGI_FORMAT_R8G8B8A8_UNORM,0)),"Native engine may change render size above 4K");
        mgs5vr::observeRenderWindow(native.Get());
        MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
        for(int attempt=0;attempt<3;++attempt){
            SetWindowLongPtrW(gameWindow,GWL_STYLE,WS_POPUP|WS_MAXIMIZE);
            SetWindowPos(gameWindow,HWND_TOPMOST,0,0,3840,2160,SWP_NOACTIVATE|SWP_FRAMECHANGED);
            RECT client{};GetClientRect(gameWindow,&client);
            require(client.right==960&&client.bottom==540,"Native startup must not enlarge PC preview");
            require(!(GetWindowLongPtrW(gameWindow,GWL_STYLE)&(WS_POPUP|WS_MAXIMIZE)),"Full-screen window style must not return");
        }
        require(!IsWindowVisible(gameWindow),"Test window must stay hidden");
        require(SUCCEEDED(native->GetDesc(&desc))&&desc.BufferDesc.Width==4160&&desc.BufferDesc.Height==4160,"Engine-selected size must be retained");
        require(SUCCEEDED(native->SetFullscreenState(TRUE,nullptr)),"Exclusive request must be converted to windowed");
        BOOL fullscreen{};require(SUCCEEDED(native->GetFullscreenState(&fullscreen,nullptr))&&!fullscreen,"No monitor mode switch");
        native.Reset();probe.Reset();context.Reset();device.Reset();
        std::cout<<"Native 4160x4160 buffer, fixed 960x540 hidden preview, repeated startup resize and no exclusive fullscreen: passed\n";result=0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';}
    if(gameWindow)DestroyWindow(gameWindow);if(probeWindow)DestroyWindow(probeWindow);
    MH_Uninitialize();if(!path.empty())std::filesystem::remove(path);return result;
}
