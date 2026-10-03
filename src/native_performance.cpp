#include "mgs5vr/native_performance.hpp"
#include "mgs5vr/log.hpp"
#include <windows.h>
#include <mmsystem.h>
#include <MinHook.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <limits>
#include <mutex>
#include <sstream>

extern "C" {
void* MgsPerformanceTrampoline{};
void MgsPerformanceIntercept();
}
namespace mgs5vr {
bool FrameTimingHistogram::add(int64_t durationNs,int64_t displayPeriodNs) noexcept {
    if(durationNs<0)return false;
    const double milliseconds=static_cast<double>(durationNs)/1e6;
    const auto index=static_cast<size_t>(std::min(milliseconds/binMilliseconds,
        static_cast<double>(bins.size()-1)));
    ++bins[index];++samples;totalMs+=milliseconds;maximumMs=std::max(maximumMs,milliseconds);
    if(displayPeriodNs>0&&durationNs>displayPeriodNs)++overBudget;
    return true;
}
double FrameTimingHistogram::percentileUpperBound(double fraction) const noexcept {
    if(!samples||!std::isfinite(fraction)||fraction<=0||fraction>1)return 0;
    const auto rank=static_cast<uint64_t>(std::ceil(static_cast<double>(samples)*fraction));
    uint64_t cumulative{};
    for(size_t index=0;index<bins.size();++index){
        cumulative+=bins[index];
        if(cumulative>=rank)return index+1==bins.size()?maximumMs
            :std::min(maximumMs,static_cast<double>(index+1)*binMilliseconds);
    }
    return maximumMs;
}
bool XrSubmissionTiming::record(int64_t layerPrepareNs,int64_t endFrameNs,int64_t period,
    uint32_t layerCount,bool projection,bool quad,bool retained) noexcept {
    // Validate both durations before updating either distribution, so a
    // reversed clock cannot create differently sized stage populations.
    if(layerPrepareNs<0||endFrameNs<0)return false;
    layerPrepare.add(layerPrepareNs,period);endFrame.add(endFrameNs,period);++samples;
    if(period>0){
        if(!budgetSamples)minimumBudgetNs=maximumBudgetNs=period;
        else {minimumBudgetNs=std::min(minimumBudgetNs,period);maximumBudgetNs=std::max(maximumBudgetNs,period);}
        ++budgetSamples;
    }
    if(!layerCount&&!projection&&!quad)++noLayers;
    else if(layerCount==1&&projection&&!quad)++projectionOnly;
    else if(layerCount==1&&!projection&&quad)++quadOnly;
    else if(layerCount==2&&projection&&quad)++projectionAndQuad;
    else ++otherLayers;
    if(layerCount&&projection&&retained)++retainedProjection;
    return true;
}
std::string XrSubmissionTiming::summary() const {
    std::ostringstream line;line.setf(std::ios::fixed);line.precision(3);
    line<<"XR submission stages samples="<<samples<<" budget_samples="<<budgetSamples
        <<" budget_ms_min_max="<<static_cast<double>(minimumBudgetNs)/1e6<<','<<static_cast<double>(maximumBudgetNs)/1e6
        <<" bin_ms="<<FrameTimingHistogram::binMilliseconds<<" percentiles=upper_bounds";
    const auto append=[&](const char* name,const FrameTimingHistogram& stage){
        line<<' '<<name<<"_ms_mean_p50_p95_max="
            <<(stage.samples?stage.totalMs/static_cast<double>(stage.samples):0)<<','
            <<stage.percentileUpperBound(.5)<<','<<stage.percentileUpperBound(.95)<<','<<stage.maximumMs
            <<' '<<name<<"_over_budget="<<stage.overBudget<<' '<<name<<"_overflow="<<stage.bins.back();
    };
    append("layer_prepare",layerPrepare);append("end_frame",endFrame);
    line<<" no_layers="<<noLayers<<" projection_only="<<projectionOnly<<" quad_only="<<quadOnly
        <<" projection_and_quad="<<projectionAndQuad<<" other_layers="<<otherLayers
        <<" retained_projection="<<retainedProjection;
    return line.str();
}
namespace {
std::atomic_bool enabled{};
std::atomic_bool fineTimer{};
std::atomic_int64_t consumerPeriodNs{};
std::atomic_uint64_t pacingDiagnosticExpiryMs{};
using TimerResolutionFn=LONG(WINAPI*)(ULONG,BOOLEAN,PULONG);
TimerResolutionFn setTimerResolution{};
std::atomic_bool nativeTimer{};
void* sleepSite{};
NativeProducerPacing currentPacing(uint64_t now) noexcept {
    return nativeProducerDiagnosticActive(pacingDiagnosticExpiryMs.load(std::memory_order_relaxed),now)
        ?NativeProducerPacing::displayPeriod:NativeProducerPacing::adaptiveMargin;
}
void readPacingDiagnostic() noexcept {try{
    pacingDiagnosticExpiryMs.store(0,std::memory_order_relaxed);
    std::array<wchar_t,32768> executable{};
    const auto size=GetModuleFileNameW(nullptr,executable.data(),static_cast<DWORD>(executable.size()));
    if(!size||size>=executable.size())return;
    const auto file=std::filesystem::path(executable.data()).parent_path()/L"mgs5vr-runtime.ini";
    const auto value=[&](const wchar_t* key){
        std::array<wchar_t,128> data{};
        const auto count=GetPrivateProfileStringW(L"runtime",key,L"",data.data(),static_cast<DWORD>(data.size()),file.c_str());
        return count<data.size()-1?std::wstring(data.data(),count):std::wstring{};
    };
    const auto mode=value(L"producer_pacing_test");
    if(mode.empty()||mode==L"baseline")return;
    const auto port=value(L"operator_port");
    std::array<wchar_t,16> activePort{};
    const auto activeSize=GetEnvironmentVariableW(L"AGENTICXR_MCP_PORT",activePort.data(),static_cast<DWORD>(activePort.size()));
    const bool owned=GetPrivateProfileIntW(L"runtime",L"enabled",0,file.c_str())==1
        &&value(L"api_layers")==L"XR_APILAYER_METAX_operator"
        &&!port.empty()&&port.size()<=5&&port.find_first_not_of(L"0123456789")==std::wstring::npos
        &&std::stoul(port)>=1024&&std::stoul(port)<=65535
        &&activeSize>0&&activeSize<activePort.size()&&port==activePort.data();
    const auto expiry=nativeProducerDiagnosticExpiryMs(mode,value(L"producer_pacing_test_seconds"),owned,GetTickCount64());
    pacingDiagnosticExpiryMs.store(expiry,std::memory_order_relaxed);
    if(expiry)log("Native producer pacing diagnostic mode=display expires_tick_ms="+std::to_string(expiry)
        +"; process-only test, normal margin restored at expiry");
    else log("Native producer pacing diagnostic refused; normal producer margin retained");
}catch(...){pacingDiagnosticExpiryMs.store(0,std::memory_order_relaxed);} }
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
    readPacingDiagnostic();
    enabled.store(true);return true;
}
bool nativeFrameRateEnabled() noexcept {return enabled.load();}
void stopNativePerformance() noexcept {
    enabled.store(false);
    consumerPeriodNs.store(0,std::memory_order_relaxed);
    pacingDiagnosticExpiryMs.store(0,std::memory_order_relaxed);
    // A worker may already be inside the bridge. Keep its trampoline alive
    // until process exit even after preventing new entries.
    if(sleepSite){MH_DisableHook(sleepSite);sleepSite=nullptr;}
    if(nativeTimer.exchange(false)){ULONG resolution{};setTimerResolution(5000,FALSE,&resolution);}
    if(fineTimer.exchange(false))timeEndPeriod(1);
}
int64_t nativeProducerIntervalNs(int64_t displayPeriodNs,NativeProducerPacing mode) noexcept {
    // Adaptive pacing contributed by s-ilent. Validate the runtime period and
    // bound the producer to 60..180 Hz, including title/loading transitions.
    if(displayPeriodNs<1000000||displayPeriodNs>50000000)return 8333333;
    const auto interval=mode==NativeProducerPacing::displayPeriod?displayPeriodNs:displayPeriodNs-displayPeriodNs/4;
    return std::clamp(interval,int64_t{5555556},int64_t{16666667});
}
uint64_t nativeProducerDiagnosticExpiryMs(std::wstring_view mode,std::wstring_view seconds,
    bool ownedRuntime,uint64_t nowMs) noexcept {
    if(!ownedRuntime||mode!=L"display"||seconds.empty()||seconds.size()>4)return 0;
    uint64_t duration{};
    for(const auto c:seconds){if(c<L'0'||c>L'9')return 0;duration=duration*10+static_cast<uint64_t>(c-L'0');}
    if(duration<30||duration>1200)return 0;
    duration*=1000;
    if(nowMs>std::numeric_limits<uint64_t>::max()-duration)return 0;
    return nowMs+duration;
}
bool nativeProducerDiagnosticActive(uint64_t expiryMs,uint64_t nowMs) noexcept {
    return expiryMs&&nowMs<expiryMs;
}
void reportConsumerDisplayPeriod(int64_t displayPeriodNs) noexcept {
    if(displayPeriodNs<1000000||displayPeriodNs>50000000)displayPeriodNs=0;
    const auto current=consumerPeriodNs.load(std::memory_order_relaxed);
    // Some runtimes report tiny period jitter. It must not reset the producer
    // deadline every frame or spam the log; follow actual refresh changes.
    if(current&&displayPeriodNs&&std::abs(current-displayPeriodNs)<=current/100)return;
    const auto prior=consumerPeriodNs.exchange(displayPeriodNs,std::memory_order_relaxed);
    if(prior==displayPeriodNs)return;
    try{log("Native producer pacing display_period_ns="+std::to_string(displayPeriodNs)
        +" target_interval_ns="+std::to_string(nativeProducerIntervalNs(displayPeriodNs,currentPacing(GetTickCount64()))));}catch(...){}
}
void paceNativePresent() noexcept {
    if(!enabled.load())return;
    // Keep a scheduling margin ahead of the consumer without an unbounded
    // engine rate. The original 90 Hz runtime still has a 120 Hz producer cap.
    const auto tick=GetTickCount64();
    auto expiry=pacingDiagnosticExpiryMs.load(std::memory_order_relaxed);
    if(expiry&&!nativeProducerDiagnosticActive(expiry,tick)
        &&pacingDiagnosticExpiryMs.compare_exchange_strong(expiry,0,std::memory_order_relaxed))
        try{log("Native producer pacing diagnostic expired; normal producer margin restored");}catch(...){}
    const auto interval=nativeProducerIntervalNs(consumerPeriodNs.load(std::memory_order_relaxed),currentPacing(tick));
    using Clock=std::chrono::steady_clock;
    static std::mutex mutex;std::lock_guard lock(mutex);
    static HANDLE timer=CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,TIMER_ALL_ACCESS);
    static auto next=Clock::now();
    static int64_t priorInterval{};
    const auto now=Clock::now();
    if(priorInterval!=interval){next=now;priorInterval=interval;}
    if(timer&&now<next){
        LARGE_INTEGER due{};due.QuadPart=-std::chrono::duration_cast<std::chrono::nanoseconds>(next-now).count()/100;
        if(due.QuadPart<0&&SetWaitableTimer(timer,&due,0,nullptr,nullptr,FALSE))WaitForSingleObject(timer,50);
    }
    next+=std::chrono::nanoseconds(interval);
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
