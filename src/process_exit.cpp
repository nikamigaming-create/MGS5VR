#include "mgs5vr/process_exit.hpp"
#include "mgs5vr/log.hpp"
#include <windows.h>
#include <MinHook.h>
#include <stdexcept>
#include <string>
#include <atomic>
#include <array>
#include <sstream>

namespace mgs5vr {
namespace {
using ExitFunction=void(WINAPI*)(UINT);
ExitFunction originalExit{};
ExitCleanup callback{};
std::atomic_flag invoked=ATOMIC_FLAG_INIT;
LONG CALLBACK observeLoadFault(EXCEPTION_POINTERS* e){
    static std::atomic_uint reports{};
    if(!e||!e->ExceptionRecord||!e->ContextRecord
        ||e->ExceptionRecord->ExceptionCode!=EXCEPTION_ACCESS_VIOLATION
        ||reports.fetch_add(1)>=8)return EXCEPTION_CONTINUE_SEARCH;
    const auto& c=*e->ContextRecord;
    std::ostringstream s;s<<"Native load AV rip="<<std::hex<<c.Rip<<" rcx="<<c.Rcx
        <<" rdx="<<c.Rdx<<" r8="<<c.R8<<" r9="<<c.R9<<" rax="<<c.Rax<<" stack=";
    std::array<uintptr_t,48> stack{};SIZE_T read{};
    if(ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(c.Rsp),stack.data(),sizeof(stack),&read))
        for(auto address:stack)s<<address<<',';
    log(s.str());
    return EXCEPTION_CONTINUE_SEARCH; // Evidence only: never swallow/recover a game fault.
}
void WINAPI exitProcess(UINT code){
    log("Native ExitProcess code="+std::to_string(code));
    if(!invoked.test_and_set()&&callback)callback();
    originalExit(code);
}
}
void installProcessExitHook(ExitCleanup cleanup){
    const auto initialized=MH_Initialize();
    if(initialized!=MH_OK&&initialized!=MH_ERROR_ALREADY_INITIALIZED)
        throw std::runtime_error("Cannot initialize process-exit interception");
    const auto kernel=GetModuleHandleW(L"kernel32.dll");
    auto* address=reinterpret_cast<void*>(kernel?GetProcAddress(kernel,"ExitProcess"):nullptr);
    if(!address||!cleanup)throw std::runtime_error("Process-exit cleanup endpoint unavailable");
    callback=cleanup;
    std::array<wchar_t,32768> exe{};
    if(GetModuleFileNameW(nullptr,exe.data(),static_cast<DWORD>(exe.size()))){
        const auto ini=std::filesystem::path(exe.data()).parent_path()/L"mgs5vr.ini";
        if(GetPrivateProfileIntW(L"diagnostics",L"native_load_trace",0,ini.c_str())==1)
            AddVectoredExceptionHandler(0,&observeLoadFault);
    }
    const auto created=MH_CreateHook(address,reinterpret_cast<void*>(&exitProcess),reinterpret_cast<void**>(&originalExit));
    if(created!=MH_OK)throw std::runtime_error(std::string("Create process-exit hook: ")+MH_StatusToString(created));
    const auto enabled=MH_EnableHook(address);
    if(enabled!=MH_OK){MH_RemoveHook(address);throw std::runtime_error(std::string("Enable process-exit hook: ")+MH_StatusToString(enabled));}
}
}
