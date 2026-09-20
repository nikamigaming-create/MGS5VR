#include <windows.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <bcrypt.h>
#include "mgs5vr/capture_hook.hpp"
#include "mgs5vr/xr_runtime.hpp"
#include "mgs5vr/log.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/camera_observer.hpp"
#include "mgs5vr/render_camera.hpp"
#include "mgs5vr/head_camera.hpp"
#include "mgs5vr/native_performance.hpp"
#include "mgs5vr/controller_rig.hpp"
#include "mgs5vr/process_exit.hpp"
#include "mgs5vr/native_actions.hpp"
#include "mgs5vr/game_target.hpp"
#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <thread>

namespace {
HMODULE proxyModule{};
std::atomic_bool stopRequested{};
std::atomic<HANDLE> workerHandle{};
HANDLE wakeWorker{};

// GZ's original launcher remains attached to the game. XR runtimes can start
// their own short-lived helpers; those must not become debuggees of that
// launcher. Keep the existing game/debugger connection intact, changing only
// inheritance by subsequently-created children, and restore it after XR stops.
class RuntimeChildIsolation {
    using Query=LONG(NTAPI*)(HANDLE,ULONG,void*,ULONG,ULONG*);
    using Set=LONG(NTAPI*)(HANDLE,ULONG,void*,ULONG);
    Set set_{};
    ULONG previous_{};
    bool changed_{};
public:
    explicit RuntimeChildIsolation(bool required){
        if(!required)return;
        const auto module=GetModuleHandleW(L"ntdll.dll");
        const auto query=reinterpret_cast<Query>(GetProcAddress(module,"NtQueryInformationProcess"));
        set_=reinterpret_cast<Set>(GetProcAddress(module,"NtSetInformationProcess"));
        ULONG_PTR debugPort{};
        if(!query||!set_||query(GetCurrentProcess(),7,&debugPort,sizeof(debugPort),nullptr)<0
            ||query(GetCurrentProcess(),31,&previous_,sizeof(previous_),nullptr)<0)
            throw std::runtime_error("Cannot query GZ runtime-helper inheritance");
        mgs5vr::log("GZ runtime-helper inheritance="+std::to_string(previous_)+" launcher_attached="+std::to_string(debugPort!=0));
        if(!debugPort||!previous_)return;
        ULONG noInherit=0;
        if(set_(GetCurrentProcess(),31,&noInherit,sizeof(noInherit))<0)
            throw std::runtime_error("Cannot isolate XR helpers from GZ's launcher");
        changed_=true;
        mgs5vr::log("XR helpers isolated from GZ launcher; original game connection retained");
    }
    ~RuntimeChildIsolation(){
        if(changed_)set_(GetCurrentProcess(),31,&previous_,sizeof(previous_));
    }
    RuntimeChildIsolation(const RuntimeChildIsolation&)=delete;
    RuntimeChildIsolation& operator=(const RuntimeChildIsolation&)=delete;
};

void cleanupBeforeExit() noexcept {
    using namespace mgs5vr;
    log("MGS5VR process exit requested; stopping OpenXR worker");
    stopRequested.store(true);
    stopNativeActions();
    stopNativePerformance();
    if(wakeWorker)SetEvent(wakeWorker);
    try{gamepadMailbox().publish({},false,steadyMilliseconds());}catch(...){}
    bool finished=true;
    const auto worker=workerHandle.load();
    if(worker&&GetThreadId(worker)!=GetCurrentThreadId()){
        // Meta XR Operator's instance shutdown took about 18 seconds in an isolated probe.
        // Let its server threads finish before ExitProcess terminates the remaining threads.
        finished=WaitForSingleObject(worker,30000)==WAIT_OBJECT_0;
        if(!finished)log("OpenXR worker did not finish within shutdown deadline");
    }
    stopCameraObserver();
    stopRenderCamera();
    stopCapture();
    log(finished?"MGS5VR cleanup completed before process exit":"MGS5VR cleanup incomplete at process exit");
}
HMODULE systemDinput(){
    static HMODULE module=[](){
        std::array<wchar_t,32768> path{};
        const auto n=GetSystemDirectoryW(path.data(),static_cast<UINT>(path.size()));
        if(n==0||n>=path.size())return HMODULE{};
        return LoadLibraryExW((std::filesystem::path(path.data())/L"dinput8.dll").c_str(),nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    }();
    return module;
}
std::filesystem::path modulePath(HMODULE module){
    std::array<wchar_t,32768> path{};
    const auto count=GetModuleFileNameW(module,path.data(),static_cast<DWORD>(path.size()));
    if(!count||count>=path.size())throw std::runtime_error("Cannot resolve module path");
    return path.data();
}
std::string sha256(const std::filesystem::path& path){
    struct Provider {BCRYPT_ALG_HANDLE h{};~Provider(){if(h)BCryptCloseAlgorithmProvider(h,0);}} provider;
    struct Hash {BCRYPT_HASH_HANDLE h{};~Hash(){if(h)BCryptDestroyHash(h);}} hash;
    if(BCryptOpenAlgorithmProvider(&provider.h,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0
        ||BCryptCreateHash(provider.h,&hash.h,nullptr,0,nullptr,0,0)<0)throw std::runtime_error("Cannot initialize SHA256");
    std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("Cannot open executable for SHA256");
    std::array<unsigned char,65536> buffer{};
    while(input){input.read(reinterpret_cast<char*>(buffer.data()),buffer.size());
        const auto bytes=input.gcount();
        if(bytes&&BCryptHashData(hash.h,buffer.data(),static_cast<ULONG>(bytes),0)<0)throw std::runtime_error("Executable hash failed");}
    if(!input.eof())throw std::runtime_error("Executable hash read failed");
    std::array<unsigned char,32> digest{};
    if(BCryptFinishHash(hash.h,digest.data(),static_cast<ULONG>(digest.size()),0)<0)throw std::runtime_error("Hash finalization failed");
    std::ostringstream out;out<<std::hex<<std::setfill('0');for(auto byte:digest)out<<std::setw(2)<<static_cast<unsigned>(byte);return out.str();
}
DWORD WINAPI initialize(void*){
    using namespace mgs5vr;
    try {
        const auto folder=modulePath(proxyModule).parent_path();
        setLogPath(folder/L"mgs5vr.log");
        const auto ini=folder/L"mgs5vr.ini";
        if(GetPrivateProfileIntW(L"theatre",L"enabled",0,ini.c_str())!=1){log("Theatre preview disabled; DirectInput forwarding remains active");return 0;}
        const auto hash=sha256(modulePath(nullptr));
        log("MGS5VR 0.1.0 development theatre preview. Executable SHA256="+hash);
        const auto* target=gameTarget(hash);
        if(!target){
            log("Unrecognized executable. Capture disabled; no game patches applied.");return 0;
        }
        log("Game target: "+std::string(target->name)+" / "+std::string(target->id));
        if(!target->nativeAdapter)log("Ground Zeroes target: independent native scene experiment available; TPP player, weapon and UI hooks remain disabled.");
        // All code/hooks and the compositor have process lifetime. Never unload live detours.
        HMODULE pinned{};
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(&initialize),&pinned))throw std::runtime_error("Cannot pin capture module");
        auto& mailbox=*new TextureMailbox;
        if(target->nativeAdapter&&GetPrivateProfileIntW(L"diagnostics",L"head_camera_experiment",0,ini.c_str())==1)
            log(enableNativeFrameRate(reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)))
                ?"Native variable frame rate enabled; producer capped at 120 FPS for the 90 Hz target"
                :"Native frame-rate signatures differ; original limiter retained");
        installCaptureHook(mailbox);
        installProcessExitHook(&cleanupBeforeExit);
        if(target->nativeAdapter&&GetPrivateProfileIntW(L"diagnostics",L"camera_observer",0,ini.c_str())==1){
            try{
                std::array<wchar_t,32768> evidencePath{};
                GetPrivateProfileStringW(L"diagnostics",L"camera_evidence_dir",L"",evidencePath.data(),static_cast<DWORD>(evidencePath.size()),ini.c_str());
                installCameraObserver(evidencePath.data());
                if(GetPrivateProfileIntW(L"diagnostics",L"head_camera_experiment",0,ini.c_str())==1){
                    installRenderCamera(reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)),evidencePath.data());
                    headCamera().configure(true,1,true);
                    if(GetPrivateProfileIntW(L"diagnostics",L"controller_rig_experiment",0,ini.c_str())==1)
                        installControllerRig(reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)));
                    log("Native VR camera requires a matching player head-bone publication; ADS no longer supplies its position");
                }
            }catch(const std::exception& e){log(std::string("Camera observer unavailable: ")+e.what());}
        }
        try{installGamepadHook();}catch(const std::exception& e){log(std::string("XR gamepad unavailable: ")+e.what());}
        if(target->nativeAdapter)
            try{installNativeActions(reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)),folder,
                GetPrivateProfileIntW(L"diagnostics",L"native_actions",0,ini.c_str())==1);}
            catch(const std::exception& e){log(std::string("Native action queue unavailable: ")+e.what());}
        TheatreConfig config;
        config.controlsPath=folder/L"mgs5vr-controls.ini";
        config.widthMeters=static_cast<float>(GetPrivateProfileIntW(L"theatre",L"width_cm",800,ini.c_str()))/100;
        config.distanceMeters=static_cast<float>(GetPrivateProfileIntW(L"theatre",L"distance_cm",600,ini.c_str()))/100;
        // The game must finish its own graphics/bootstrap phase before an XR
        // runtime loads its graphics drivers and helper processes. In particular,
        // GZ performs a second-process startup. Do not race that initialization
        // merely because the first DirectInput call has happened.
        log("Waiting for the first game image before starting OpenXR");
        while(!stopRequested.load()&&!mailbox.latest()){
            if(wakeWorker)WaitForSingleObject(wakeWorker,100);
            else std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        // GZ unpacks its engine during normal startup. Qualify its independent
        // renderer only after the native game has produced a real image.
        if(target->id=="gz-1.0.0.5"&&!stopRequested.load()
            &&(GetPrivateProfileIntW(L"diagnostics",L"head_camera_experiment",0,ini.c_str())==1
               ||GetPrivateProfileIntW(L"diagnostics",L"camera_observer",0,ini.c_str())==1)){
            try{
                std::array<wchar_t,32768> evidencePath{};
                GetPrivateProfileStringW(L"diagnostics",L"camera_evidence_dir",L"",evidencePath.data(),static_cast<DWORD>(evidencePath.size()),ini.c_str());
                if(GetPrivateProfileIntW(L"diagnostics",L"head_camera_experiment",0,ini.c_str())==1){
                    installRenderCamera(reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)),evidencePath.data(),RenderBuild::groundZeroes_1_0_0_5);
                    headCamera().configure(true,1,false);
                    log("GZ scene-camera experiment: native stereo at the game camera, not yet a tracked first-person player rig");
                }
                if(GetPrivateProfileIntW(L"diagnostics",L"camera_observer",0,ini.c_str())==1)
                    installCameraObserver(evidencePath.data(),RenderBuild::groundZeroes_1_0_0_5);
            }catch(const std::exception& e){log(std::string("GZ native scene unavailable: ")+e.what());}
        }
        RuntimeChildIsolation runtimeChildren(target->id=="gz-1.0.0.5");
        while(!stopRequested.load()){
            try{runTheatre(mailbox,config,stopRequested);}
            catch(const std::exception& e){if(!stopRequested.load())log(std::string("OpenXR unavailable: ")+e.what());}
            if(wakeWorker)WaitForSingleObject(wakeWorker,3000);
            else for(int n=0;n<30&&!stopRequested.load();++n)std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }catch(const std::exception& e){mgs5vr::log(std::string("MGS5VR initialization stopped: ")+e.what());}
    catch(...){mgs5vr::log("MGS5VR initialization stopped: unexpected exception");}
    return 0;
}
void startMod() noexcept {
    try {
        static std::once_flag once;
        std::call_once(once,[]{
            const auto executable=modulePath(nullptr).filename();bool recognized=false;
            for(const auto& target:mgs5vr::gameTargets)
                if(_wcsicmp(executable.c_str(),target.executable.data())==0){recognized=true;break;}
            if(!recognized)return;
            HMODULE pinned{};
            if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                reinterpret_cast<LPCWSTR>(&initialize),&pinned))return;
            // The first game import call happens outside loader lock. Do no graphics/XR work in DllMain.
            wakeWorker=CreateEventW(nullptr,TRUE,FALSE,nullptr);
            workerHandle.store(CreateThread(nullptr,0,initialize,nullptr,0,nullptr));
        });
    }catch(...){}
}
template<class T>T original(const char* name)noexcept{
    try{const auto module=systemDinput();return module?reinterpret_cast<T>(GetProcAddress(module,name)):nullptr;}catch(...){return nullptr;}
}
}
extern "C" HRESULT WINAPI DirectInput8Create(HINSTANCE instance,DWORD version,REFIID iid,LPVOID* output,LPUNKNOWN outer){
    using Fn=HRESULT(WINAPI*)(HINSTANCE,DWORD,REFIID,LPVOID*,LPUNKNOWN);
    const auto fn=original<Fn>("DirectInput8Create");
    if(!fn){if(output)*output=nullptr;return E_FAIL;}
    const HRESULT result=fn(instance,version,iid,output,outer);startMod();return result;
}
extern "C" HRESULT WINAPI DllCanUnloadNow(){return S_FALSE;}
extern "C" HRESULT WINAPI DllGetClassObject(REFCLSID cls,REFIID iid,LPVOID* output){
    using Fn=HRESULT(WINAPI*)(REFCLSID,REFIID,LPVOID*);const auto fn=original<Fn>("DllGetClassObject");
    if(!fn){if(output)*output=nullptr;return CLASS_E_CLASSNOTAVAILABLE;}return fn(cls,iid,output);
}
extern "C" HRESULT WINAPI DllRegisterServer(){const auto fn=original<HRESULT(WINAPI*)()>("DllRegisterServer");return fn?fn():E_FAIL;}
extern "C" HRESULT WINAPI DllUnregisterServer(){const auto fn=original<HRESULT(WINAPI*)()>("DllUnregisterServer");return fn?fn():E_FAIL;}
extern "C" LPCDIDATAFORMAT WINAPI GetdfDIJoystick(){const auto fn=original<LPCDIDATAFORMAT(WINAPI*)()>("GetdfDIJoystick");return fn?fn():nullptr;}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID){
    if(reason==DLL_PROCESS_ATTACH){proxyModule=instance;DisableThreadLibraryCalls(instance);}return TRUE;
}
