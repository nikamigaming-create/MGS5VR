#include "mgs5vr/native_performance.hpp"
#include <windows.h>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>

extern "C" uint64_t MgsTestWorkerDelay(void* site,const void* worker,uint64_t* flags);
int main(){try{
    struct Fixture {
        unsigned char* bytes=static_cast<unsigned char*>(VirtualAlloc(nullptr,0x250000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        ~Fixture(){if(bytes)VirtualFree(bytes,0,MEM_RELEASE);}
    } fixture;
    if(!fixture.bytes)throw std::runtime_error("allocate graphics-option fixture");
    const auto require=[](bool condition,const char* reason){if(!condition)throw std::runtime_error(reason);};
    using mgs5vr::nativeProducerIntervalNs;
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
        require(nativeProducerIntervalNs(period)>=5555556&&nativeProducerIntervalNs(period)<=16666667,
            "all valid runtime periods preserve the engine rate bounds");
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
