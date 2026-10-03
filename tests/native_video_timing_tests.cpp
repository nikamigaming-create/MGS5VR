#include "mgs5vr/native_video_timing.hpp"
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {
uint64_t checks{};
void require(bool condition,const char* message){
    ++checks;
    if(!condition)throw std::runtime_error(message);
}
// Capture samples follow elapsed rational deadlines; encoded PTS are floored.
// A nonintegral deadline becomes eligible at the next 100 ns clock tick.
constexpr int64_t deadline(uint64_t slot){
    return static_cast<int64_t>(slot/30*10000000+(slot%30*10000000+29)/30);
}
void sourcePixelConversion(){
    using namespace mgs5vr;
    std::array<unsigned char,12> rgba{255,17,3,91, 2,190,230,255, 0,0,0,0};
    const std::array<unsigned char,12> expected{3,17,255,91, 230,190,2,255, 0,0,0,0};
    require(normalizeNativeVideoPixels(rgba,true)&&rgba==expected,
        "worker converts every RGBA pixel to RGB32 BGRA while retaining green and alpha");
    const auto unchanged=rgba;
    require(normalizeNativeVideoPixels(rgba,false)&&rgba==unchanged,
        "a native BGRA eye is preserved byte for byte without double conversion");
    std::array<unsigned char,3> partial{40,50,60};const auto before=partial;
    require(!normalizeNativeVideoPixels(partial,true)&&partial==before,
        "an incomplete pixel cannot be partially normalized");
    require(normalizeNativeVideoPixels({},true),"empty source bytes are safe for pure conversion");
}
void rationalTimestamps(){
    using namespace mgs5vr;
    require(nativeVideoSlotTime(0)==0&&nativeVideoSlotTime(1)==333333
        &&nativeVideoSlotTime(2)==666666&&nativeVideoSlotTime(3)==1000000,
        "encoded timestamps retain exact rational 30 Hz boundaries");
    int64_t total{};
    for(uint64_t slot=0;slot<nativeVideoMaxSlots;++slot){
        const auto duration=nativeVideoSlotTime(slot+1)-nativeVideoSlotTime(slot);
        require(duration==333333||duration==333334,"each encoded frame has a positive rational duration");
        total+=duration;
        if((slot+1)%30==0)
            require(total==static_cast<int64_t>((slot+1)/30*10000000),"thirty encoded frames are exactly one second without cumulative drift");
    }
    require(total==1250000000,"bounded recorder timeline covers 125 seconds exactly");
}
void cadenceDeadlines(){
    using namespace mgs5vr;
    NativeVideoCadence cadence;
    require(!cadence.due(0)&&!cadence.due(-1)&&cadence.origin()==0&&cadence.slots()==0,
        "invalid initial clocks cannot start a capture timeline");
    constexpr int64_t origin=120000000;
    const auto first=cadence.due(origin);
    require(first&&first->slot==0&&first->missed==0&&cadence.origin()==origin&&cadence.slots()==1,
        "first positive source observation starts exactly one intended capture slot");
    require(!cadence.due(origin)&&!cadence.due(origin-1),"same or backward clocks do not duplicate a capture");
    for(uint64_t slot=1;slot<nativeVideoMaxSlots;++slot){
        const auto at=origin+deadline(slot);
        require(!cadence.due(at-1),"a source call just before the rational deadline is not another frame");
        const auto due=cadence.due(at);
        require(due&&due->slot==slot&&due->missed==0&&cadence.slots()==slot+1,
            "every elapsed rational deadline advances one slot without drift or invented misses");
        require(!cadence.due(at)&&!cadence.due(at+1),"same-slot jitter cannot capture a duplicate source");
    }
}
void cadenceRenderGap(){
    using namespace mgs5vr;
    NativeVideoCadence cadence;
    constexpr int64_t origin=40000000;
    require(cadence.due(origin).has_value(),"render-gap fixture starts");
    const auto late=cadence.due(origin+2000000);
    require(late&&late->slot==6&&late->missed==5&&cadence.slots()==7,
        "a 200 ms render gap accounts for all five missed intermediate deadlines");
    require(!cadence.due(origin+2000001)&&!cadence.due(origin+1000000),
        "no catch-up burst or backwards-clock acceptance follows a render gap");
    const auto next=cadence.due(origin+deadline(7));
    require(next&&next->slot==7&&next->missed==0,"normal cadence resumes at the next actual deadline");
    const auto secondGap=cadence.due(origin+deadline(30));
    require(secondGap&&secondGap->slot==30&&secondGap->missed==22&&cadence.slots()==31,
        "each later gap accounts only for its newly missed slots");
}
void oldestStagingOrder(){
    using namespace mgs5vr;
    require(!oldestNativeVideoSlot(std::span<const int64_t>{}),"empty staging rings have no frame");
    std::array<int64_t,3> slots{300,100,200};
    auto next=oldestNativeVideoSlot(slots);
    require(next&&*next==1,"wrapped physical staging indices drain by oldest source clock");
    slots[*next]=0;next=oldestNativeVideoSlot(slots);
    require(next&&*next==2,"second-oldest source is selected after oldest drains");
    slots[*next]=0;next=oldestNativeVideoSlot(slots);
    require(next&&*next==0,"newest wrapped frame drains last");
    slots[*next]=0;
    require(!oldestNativeVideoSlot(slots),"drained staging ring cannot invent a frame");
    slots={0,-10,40};next=oldestNativeVideoSlot(slots);
    require(next&&*next==2,"unoccupied and invalid staging clocks are ignored");
    slots={90,90,100};next=oldestNativeVideoSlot(slots);
    require(next&&*next==0,"equal staging clocks have deterministic oldest index selection");
}
void timelinePlacement(){
    using namespace mgs5vr;
    NativeVideoTimeline timeline;
    require(!timeline.accept(0,0)&&!timeline.accept(-1,0)&&timeline.frames()==0,
        "invalid source clocks cannot create encoded frames");
    const auto first=timeline.accept(10000000,42);
    require(first&&first->encodedSlot==0&&first->repeatsBefore==0
        &&timeline.firstClock()==10000000&&timeline.firstCaptureSlot()==42,
        "a late first accepted source starts encoded slot zero without fabricated leading pixels");
    const auto next=timeline.accept(10400000,43);
    require(next&&next->encodedSlot==1&&next->repeatsBefore==0,"adjacent capture slots encode without a repeat");
    const auto gap=timeline.accept(12000000,48);
    require(gap&&gap->encodedSlot==6&&gap->repeatsBefore==4,
        "a capture gap creates exactly its missing CFR slots using preceding accepted pixels");
    require(timeline.frames()==3&&timeline.encodedFrames()==7&&timeline.repeats()==4
        &&timeline.lastClock()==12000000&&timeline.encodedFrames()==timeline.frames()+timeline.repeats(),
        "source, encoded and repeated counts remain independently auditable");
    const auto last=timeline.accept(15000000,51);
    require(last&&last->encodedSlot==9&&last->repeatsBefore==2
        &&timeline.frames()==4&&timeline.encodedFrames()==10&&timeline.repeats()==6,
        "multiple gaps accumulate only their exact missing encoded slots");
}
void rejectedSourcesDoNotChangeLedger(){
    using namespace mgs5vr;
    NativeVideoTimeline timeline;
    require(timeline.accept(100,10).has_value()&&timeline.accept(200,12).has_value(),"ordering fixture starts with two sources");
    for(const auto& [clock,slot]:std::array<std::pair<int64_t,uint64_t>,6>{{{200,13},{199,13},{201,12},{201,11},{-1,13},{0,13}}}){
        require(!timeline.accept(clock,slot),"reordered/equal source timestamps or capture slots are rejected");
        require(timeline.firstClock()==100&&timeline.lastClock()==200&&timeline.firstCaptureSlot()==10
            &&timeline.frames()==2&&timeline.encodedFrames()==3&&timeline.repeats()==1,
            "rejection cannot rewrite source provenance or inflate encoded/repeat counts");
    }
    const auto recovery=timeline.accept(300,13);
    require(recovery&&recovery->encodedSlot==3&&recovery->repeatsBefore==0,
        "a valid next source follows rejected data without an artificial gap");
}
void boundedTimeline(){
    using namespace mgs5vr;
    NativeVideoTimeline timeline;
    require(timeline.accept(100,80).has_value(),"bounded timeline starts");
    const auto edge=timeline.accept(200,80+nativeVideoMaxSlots-1);
    require(edge&&edge->encodedSlot==nativeVideoMaxSlots-1&&edge->repeatsBefore==nativeVideoMaxSlots-2,
        "largest admitted timeline has an exact finite repeat count");
    require(!timeline.accept(300,80+nativeVideoMaxSlots)&&timeline.frames()==2
        &&timeline.encodedFrames()==nativeVideoMaxSlots&&timeline.repeats()==nativeVideoMaxSlots-2,
        "a source beyond the take bound is rejected without extending encoding");
    NativeVideoTimeline huge;
    constexpr auto maximum=std::numeric_limits<uint64_t>::max();
    require(huge.accept(100,maximum-1).has_value(),"late first capture slot does not allocate a leading timeline");
    const auto final=huge.accept(200,maximum);
    require(final&&final->encodedSlot==1&&final->repeatsBefore==0&&!huge.accept(300,0),
        "capture slot rollover is rejected rather than underflowing into a huge repeat allocation");
}
void cadenceAndEncodingAccounting(){
    using namespace mgs5vr;
    NativeVideoCadence cadence;NativeVideoTimeline timeline;
    constexpr int64_t origin=90000000;
    uint64_t missed{},busy{},queueLoss{};
    for(const uint64_t expected:{uint64_t{0},uint64_t{1},uint64_t{7},uint64_t{8},uint64_t{10}}){
        const auto clock=origin+deadline(expected);const auto due=cadence.due(clock);
        require(due&&due->slot==expected,"source scheduling retains its actual capture slot across render gaps");
        missed+=due->missed;
        if(expected==1){++busy;continue;}
        if(expected==8){++queueLoss;continue;}
        require(timeline.accept(clock,due->slot).has_value(),"accepted cadence sample retains its real source clock");
    }
    require(missed==6&&busy==1&&queueLoss==1&&timeline.frames()==3&&cadence.slots()==11,
        "missed render deadlines, staging pressure and queue loss stay separate from accepted samples");
    require(cadence.slots()==timeline.frames()+missed+busy+queueLoss
        &&timeline.encodedFrames()==11&&timeline.repeats()==8,
        "scheduled slots reconcile with accepted/loss counts while CFR repeats reconcile independently");
}
}
int main(){try{
    sourcePixelConversion();rationalTimestamps();cadenceDeadlines();cadenceRenderGap();oldestStagingOrder();
    timelinePlacement();rejectedSourcesDoNotChangeLedger();boundedTimeline();cadenceAndEncodingAccounting();
    std::cout<<checks<<" native video timing/order contracts passed; no native capture or runtime FPS claim.\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}}
