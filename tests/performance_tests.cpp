#include "mgs5vr/native_performance.hpp"
#include <windows.h>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

extern "C" uint64_t MgsTestWorkerDelay(void* site,const void* worker,uint64_t* flags);
int main(){try{
    struct Fixture {
        unsigned char* bytes=static_cast<unsigned char*>(VirtualAlloc(nullptr,0x250000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        ~Fixture(){if(bytes)VirtualFree(bytes,0,MEM_RELEASE);}
    } fixture;
    if(!fixture.bytes)throw std::runtime_error("allocate graphics-option fixture");
    const auto require=[](bool condition,const char* reason){if(!condition)throw std::runtime_error(reason);};
    {
        mgs5vr::FrameTimingHistogram histogram;
        require(histogram.percentileUpperBound(.95)==0,"empty timing population has no invented percentile");
        require(!histogram.add(-1,11111111)&&!histogram.samples,"negative timing is refused before any aggregate changes");
        require(histogram.add(0,11111111)&&histogram.add(249999,11111111)
            &&histogram.add(250000,11111111)&&histogram.add(11111111,11111111)
            &&histogram.add(11111112,11111111)&&histogram.add(653330000,11111111),
            "timing includes zero, bin boundaries, exact display deadline and long stalls");
        require(histogram.bins[0]==2&&histogram.bins[1]==1&&histogram.bins.back()==1,
            "fixed bins preserve exact boundary and overflow population");
        require(histogram.samples==6&&histogram.overBudget==2&&histogram.maximumMs==653.33,
            "exact deadline is allowed while one-nanosecond excess and long stall are counted");
        require(histogram.percentileUpperBound(.5)==.5&&histogram.percentileUpperBound(.95)==653.33,
            "percentiles are bounded histogram ranks and overflow retains the measured maximum");
        require(histogram.percentileUpperBound(0)==0&&histogram.percentileUpperBound(1.1)==0
            &&histogram.percentileUpperBound(std::numeric_limits<double>::quiet_NaN())==0,
            "invalid quantiles cannot invent a timing result");
        mgs5vr::XrSubmissionTiming split;
        require(!split.record(1000,-1,11111111,1,true,false,false)
            &&!split.samples&&!split.layerPrepare.samples&&!split.endFrame.samples,
            "invalid end-frame duration cannot partially advance the preparation distribution");
        require(split.record(20000,653330000,11111111,1,true,false,true)
            &&split.record(17000000,100000,11111111,1,false,true,false),
            "preparation stall and runtime API stall retain independent distributions");
        require(split.layerPrepare.overBudget==1&&split.endFrame.overBudget==1
            &&split.layerPrepare.maximumMs==17&&split.endFrame.maximumMs==653.33,
            "a slow API call is not attributed to preparation or hidden by averaging");
        require(split.record(0,0,0,0,false,false,false)
            &&split.record(1000,1000,8333333,2,true,true,true)
            &&split.record(1000,1000,-1,3,true,true,false),
            "empty, combined and unknown layer submissions remain measured without a valid budget");
        require(split.samples==5&&split.budgetSamples==3&&split.minimumBudgetNs==8333333
            &&split.maximumBudgetNs==11111111&&split.noLayers==1&&split.projectionOnly==1
            &&split.quadOnly==1&&split.projectionAndQuad==1&&split.otherLayers==1
            &&split.retainedProjection==2,
            "actual layer categories are exclusive while retained projection is an overlapping subset");
        const auto report=split.summary();
        require(report.find("percentiles=upper_bounds")!=std::string::npos
            &&report.find("layer_prepare_ms_mean_p50_p95_max=")!=std::string::npos
            &&report.find("end_frame_ms_mean_p50_p95_max=")!=std::string::npos
            &&report.find("end_frame_overflow=1")!=std::string::npos
            &&report.find("projection_and_quad=1")!=std::string::npos,
            "bounded report preserves histogram semantics, API identity and layer attribution");
        split={};
        require(!split.samples&&!split.layerPrepare.samples&&!split.endFrame.samples
            &&!split.retainedProjection&&!split.budgetSamples,"a new window cannot retain old stalls or layer counts");
    }
    using mgs5vr::nativeProducerIntervalNs;
    using mgs5vr::NativeProducerPacing;
    require(nativeProducerIntervalNs(0)==8333333&&nativeProducerIntervalNs(-1)==8333333,
        "missing or lost runtime returns to bounded 120 Hz fallback");
    require(nativeProducerIntervalNs(999999)==8333333&&nativeProducerIntervalNs(50000001)==8333333,
        "invalid runtime periods cannot command extreme engine rates");
    require(nativeProducerIntervalNs(13888889)==10416667,"72 Hz runtime targets 96 Hz producer");
    require(nativeProducerIntervalNs(11111111)==8333334,"90 Hz retains historical 120 Hz pacing");
    require(nativeProducerIntervalNs(8333333)==6250000,"120 Hz runtime leaves producer scheduling margin");
    require(nativeProducerIntervalNs(1000000)==5555556&&nativeProducerIntervalNs(50000000)==16666667,
        "producer remains bounded at 60..180 Hz for every accepted period");
    for(int64_t period=1000000;period<=50000000;period+=10000)
        require(nativeProducerIntervalNs(period)>=5555556&&nativeProducerIntervalNs(period)<=16666667
            &&nativeProducerIntervalNs(period,NativeProducerPacing::displayPeriod)>=nativeProducerIntervalNs(period)
            &&nativeProducerIntervalNs(period,NativeProducerPacing::displayPeriod)<=16666667,
            "all valid runtime periods preserve the engine rate bounds");
    require(nativeProducerIntervalNs(11111111,NativeProducerPacing::displayPeriod)==11111111
        &&nativeProducerIntervalNs(13888889,NativeProducerPacing::displayPeriod)==13888889
        &&nativeProducerIntervalNs(8333333,NativeProducerPacing::displayPeriod)==8333333,
        "diagnostic follows actual 90/72/120 Hz display periods without the normal scheduling margin");
    require(nativeProducerIntervalNs(0,NativeProducerPacing::displayPeriod)==8333333
        &&nativeProducerIntervalNs(50000001,NativeProducerPacing::displayPeriod)==8333333,
        "diagnostic never invents a display cadence when the runtime period is invalid");
    using mgs5vr::nativeProducerDiagnosticExpiryMs;
    using mgs5vr::nativeProducerDiagnosticActive;
    require(nativeProducerDiagnosticExpiryMs(L"display",L"30",true,1000)==31000
        &&nativeProducerDiagnosticExpiryMs(L"display",L"1200",true,1000)==1201000,
        "explicit diagnostic duration stays between thirty seconds and twenty minutes");
    for(const auto seconds:{L"",L"0",L"29",L"1201",L"90000",L"-90",L"+90",L" 90",L"90x"})
        require(nativeProducerDiagnosticExpiryMs(L"display",seconds,true,1000)==0,
            "malformed or unbounded diagnostic duration cannot change pacing");
    require(!nativeProducerDiagnosticExpiryMs(L"display",L"900",false,1000)
        &&!nativeProducerDiagnosticExpiryMs(L"baseline",L"900",true,1000)
        &&!nativeProducerDiagnosticExpiryMs(L"Display",L"900",true,1000)
        &&!nativeProducerDiagnosticExpiryMs(L"display",L"900",true,std::numeric_limits<uint64_t>::max()-1),
        "unowned, unknown, baseline and overflowing leases retain normal pacing");
    require(nativeProducerDiagnosticActive(31000,30999)&&!nativeProducerDiagnosticActive(31000,31000)
        &&!nativeProducerDiagnosticActive(31000,31001)&&!nativeProducerDiagnosticActive(0,0),
        "diagnostic expires exactly without relying on runner cleanup");
    require(!mgs5vr::nativeFrameRateEnabled(),"native pacing must start disabled");
    require(!mgs5vr::enableNativeFrameRate(0),"null image refused");
    constexpr std::array<unsigned char,13> target{0x49,0x85,0xcc,0x75,0x1d,0xf2,0x0f,0x10,0x0d,0xe3,0xcf,0xeb,0x01};
    constexpr std::array<unsigned char,18> selection{0x48,0x33,0x05,0x10,0xa0,0x79,0x02,0x49,0x85,0xc4,0x48,0x0f,0x44,0x1d,0x15,0xa0,0x79,0x02};
    std::memcpy(fixture.bytes+0x24be88,target.data(),target.size());
    const auto base=reinterpret_cast<uintptr_t>(fixture.bytes);
    require(!mgs5vr::enableNativeFrameRate(base),"missing second signature refused");
    require(std::memcmp(fixture.bytes+0x24be88,target.data(),target.size())==0,"refusal must not partially change the first site");
    std::memcpy(fixture.bytes+0x24bef1,selection.data(),selection.size());
    require(!mgs5vr::enableNativeFrameRate(base),"missing worker signature refused before any writes");
    require(std::memcmp(fixture.bytes+0x24be88,target.data(),target.size())==0,"worker refusal preserves graphics options");
    constexpr std::array<unsigned char,19> sleep{0x48,0x8b,0xf8,0x48,0x85,0xc0,0x75,0x12,0x8d,0x50,0x01,0x48,0x8d,0x8c,0x24,0x90,0,0,0};
    std::memcpy(fixture.bytes+0x32c89,sleep.data(),sleep.size());
    constexpr std::array<unsigned char,4> returnDelay{0x48,0x8b,0xc2,0xc3};
    std::memcpy(fixture.bytes+0x32c9c,returnDelay.data(),returnDelay.size());
    DWORD old{};require(VirtualProtect(fixture.bytes,0x250000,PAGE_EXECUTE_READ,&old)!=FALSE,"protect graphics-option fixture");
    require(mgs5vr::enableNativeFrameRate(base)&&mgs5vr::nativeFrameRateEnabled(),"matching protected image accepts the adapter");
    require(fixture.bytes[0x24be8b]==0xeb&&fixture.bytes[0x24bef2]==0x31,"variable-rate selection is installed");
    require(std::memcmp(fixture.bytes+0x24bef8,selection.data()+7,selection.size()-7)==0,"adjacent native instructions remain intact");
    MEMORY_BASIC_INFORMATION page{};require(VirtualQuery(fixture.bytes+0x24bef1,&page,sizeof(page))==sizeof(page)&&page.Protect==PAGE_EXECUTE_READ,"original code-page protection restored");
    std::array<int32_t,4> worker{};std::array<uint64_t,2> flags{};
    for(const int32_t id:{0,1,4,5,10}){
        worker[2]=id;
        require(MgsTestWorkerDelay(fixture.bytes+0x32c94,worker.data(),flags.data())==(id<=4?0u:1u),"only critical worker delays change");
        require(flags[0]==flags[1],"worker adapter preserves incoming arithmetic flags");
    }
    mgs5vr::stopNativePerformance();
    require(!mgs5vr::nativeFrameRateEnabled(),"shutdown releases native pacing and timer request");
    worker[2]=0;
    require(MgsTestWorkerDelay(fixture.bytes+0x32c94,worker.data(),flags.data())==1,"shutdown removes the worker detour");
    std::cout<<"Native graphics-option signature refusal, atomic validation and page-protection checks passed. No headset FPS claim.\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}
