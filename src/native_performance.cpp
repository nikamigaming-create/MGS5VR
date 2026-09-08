#include "mgs5vr/native_performance.hpp"
#include "mgs5vr/log.hpp"
#include <windows.h>
#include <mmsystem.h>
#include <MinHook.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <sstream>

extern "C" {
void* MgsPerformanceTrampoline{};
void MgsPerformanceIntercept();
}
namespace mgs5vr {
namespace {
std::atomic_bool enabled{};
std::atomic_bool fineTimer{};
using TimerResolutionFn=LONG(WINAPI*)(ULONG,BOOLEAN,PULONG);
TimerResolutionFn setTimerResolution{};
std::atomic_bool nativeTimer{};
void* sleepSite{};
template<size_t N> bool matches(uintptr_t address,const std::array<unsigned char,N>& expected){
    std::array<unsigned char,N> found{};SIZE_T read{};
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),found.data(),N,&read)&&read==N&&found==expected;
}
template<size_t N> bool write(uintptr_t address,const std::array<unsigned char,N>& bytes){
    DWORD old{};auto* target=reinterpret_cast<void*>(address);
    if(!VirtualProtect(target,N,PAGE_EXECUTE_READWRITE,&old))return false;
    std::memcpy(target,bytes.data(),N);FlushInstructionCache(GetCurrentProcess(),target,N);
    DWORD ignored{};VirtualProtect(target,N,old,&ignored);return true;
}
}
bool enableNativeFrameRate(uintptr_t base) noexcept {
    // TPP 1.0.15.4 graphics-option selection, independently checked against the
    // owned executable. The variable-rate approach is documented by MGSVFix
    // (Lyall, MIT); see docs/PERFORMANCE.md for provenance and limits.
    constexpr std::array<unsigned char,13> target{0x49,0x85,0xcc,0x75,0x1d,0xf2,0x0f,0x10,0x0d,0xe3,0xcf,0xeb,0x01};
    constexpr std::array<unsigned char,18> selection{0x48,0x33,0x05,0x10,0xa0,0x79,0x02,0x49,0x85,0xc4,0x48,0x0f,0x44,0x1d,0x15,0xa0,0x79,0x02};
    constexpr std::array<unsigned char,19> sleep{0x48,0x8b,0xf8,0x48,0x85,0xc0,0x75,0x12,0x8d,0x50,0x01,0x48,0x8d,0x8c,0x24,0x90,0,0,0};
    if(!base||enabled.load()||!matches(base+0x24be88,target)||!matches(base+0x24bef1,selection)||!matches(base+0x32c89,sleep))return false;
    const auto init=MH_Initialize();if(init!=MH_OK&&init!=MH_ERROR_ALREADY_INITIALIZED)return false;
    auto* site=reinterpret_cast<void*>(base+0x32c94);
    if(MH_CreateHook(site,reinterpret_cast<void*>(&MgsPerformanceIntercept),&MgsPerformanceTrampoline)!=MH_OK)return false;
    constexpr std::array<unsigned char,7> variable{0x48,0x31,0xc0,0x90,0x90,0x90,0x90};
    if(!write(base+0x24be8b,std::array<unsigned char,1>{0xeb})){MH_RemoveHook(site);return false;}
    if(!write(base+0x24bef1,variable)){write(base+0x24be8b,std::array<unsigned char,1>{0x75});MH_RemoveHook(site);return false;}
    if(MH_EnableHook(site)!=MH_OK){
        write(base+0x24be8b,std::array<unsigned char,1>{0x75});
        write(base+0x24bef1,std::array<unsigned char,7>{0x48,0x33,0x05,0x10,0xa0,0x79,0x02});
        MH_RemoveHook(site);return false;
    }
    sleepSite=site;
    fineTimer.store(timeBeginPeriod(1)==TIMERR_NOERROR);
    setTimerResolution=reinterpret_cast<TimerResolutionFn>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtSetTimerResolution"));
    ULONG resolution{};
    if(setTimerResolution&&setTimerResolution(5000,TRUE,&resolution)==0){
        nativeTimer.store(true);log("Native worker timer resolution in 100 ns units="+std::to_string(resolution));
    }
    // The headset remains visible when its desktop mirror is covered. Keep
    // Windows 11 from ignoring this process's timer request in that state.
    PROCESS_POWER_THROTTLING_STATE power{PROCESS_POWER_THROTTLING_CURRENT_VERSION,PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION,0};
    SetProcessInformation(GetCurrentProcess(),ProcessPowerThrottling,&power,sizeof(power));
    enabled.store(true);return true;
}
bool nativeFrameRateEnabled() noexcept {return enabled.load();}
void stopNativePerformance() noexcept {
    enabled.store(false);
    // A worker may already be inside the bridge. Keep its trampoline alive
    // until process exit even after preventing new entries.
    if(sleepSite){MH_DisableHook(sleepSite);sleepSite=nullptr;}
    if(nativeTimer.exchange(false)){ULONG resolution{};setTimerResolution(5000,FALSE,&resolution);}
    if(fineTimer.exchange(false))timeEndPeriod(1);
}
void paceNativePresent() noexcept {
    if(!enabled.load())return;
    // A 120 FPS producer leaves scheduling margin for the 90 Hz consumer.
    // Cap title/loading too: unlimited loading rates are unsafe in this engine.
    using Clock=std::chrono::steady_clock;
    static std::mutex mutex;std::lock_guard lock(mutex);
    static HANDLE timer=CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,TIMER_ALL_ACCESS);
    static auto next=Clock::now();
    const auto now=Clock::now();
    if(timer&&now<next){
        LARGE_INTEGER due{};due.QuadPart=-std::chrono::duration_cast<std::chrono::nanoseconds>(next-now).count()/100;
        if(due.QuadPart<0&&SetWaitableTimer(timer,&due,0,nullptr,nullptr,FALSE))WaitForSingleObject(timer,50);
    }
    next+=std::chrono::nanoseconds(8333333);
    if(next<Clock::now())next=Clock::now();
}
void recordNativePresent(double captureMs,double pacingMs,double presentMs) noexcept {try{
    if(!enabled.load())return;
    static std::mutex reportMutex;std::lock_guard lock(reportMutex);
    using Clock=std::chrono::steady_clock;
    static auto since=Clock::now();static uint64_t count{};
    static std::array<double,3> total{},maximum{};
    const std::array<double,3> sample{captureMs,pacingMs,presentMs};
    for(size_t i=0;i<sample.size();++i){total[i]+=sample[i];maximum[i]=sample[i]>maximum[i]?sample[i]:maximum[i];}
    ++count;const auto now=Clock::now();if(now-since<std::chrono::seconds(5))return;
    std::ostringstream line;line<<"Native present ms capture_mean_max="<<total[0]/count<<','<<maximum[0]
        <<" pacing_mean_max="<<total[1]/count<<','<<maximum[1]<<" present_mean_max="<<total[2]/count<<','<<maximum[2];
    log(line.str());since=now;count=0;total={};maximum={};
}catch(...){} }
}
