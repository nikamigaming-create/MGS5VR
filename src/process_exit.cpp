#include "mgs5vr/process_exit.hpp"
#include <windows.h>
#include <MinHook.h>
#include <stdexcept>
#include <string>
#include <atomic>

namespace mgs5vr {
namespace {
using ExitFunction=void(WINAPI*)(UINT);
ExitFunction originalExit{};
ExitCleanup callback{};
std::atomic_flag invoked=ATOMIC_FLAG_INIT;
void WINAPI exitProcess(UINT code){
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
    const auto created=MH_CreateHook(address,reinterpret_cast<void*>(&exitProcess),reinterpret_cast<void**>(&originalExit));
    if(created!=MH_OK)throw std::runtime_error(std::string("Create process-exit hook: ")+MH_StatusToString(created));
    const auto enabled=MH_EnableHook(address);
    if(enabled!=MH_OK){MH_RemoveHook(address);throw std::runtime_error(std::string("Enable process-exit hook: ")+MH_StatusToString(enabled));}
}
}
