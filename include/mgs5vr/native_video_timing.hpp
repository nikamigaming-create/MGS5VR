#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace mgs5vr {
inline bool normalizeNativeVideoPixels(std::span<unsigned char> bytes,bool rgba) noexcept {
    if(bytes.size()%4)return false;
    if(rgba)for(size_t pixel=0;pixel<bytes.size();pixel+=4){
        const auto red=bytes[pixel];bytes[pixel]=bytes[pixel+2];bytes[pixel+2]=red;
    }
    return true;
}
constexpr uint64_t nativeVideoMaxSlots=30*125; // Two-minute take plus stop polling margin.
constexpr int64_t nativeVideoSlotTime(uint64_t slot) noexcept {
    return static_cast<int64_t>(slot/30*10000000+slot%30*10000000/30);
}
struct NativeVideoDue {uint64_t slot{},missed{};};
class NativeVideoCadence {
    int64_t origin_{};
    uint64_t next_{};
public:
    std::optional<NativeVideoDue> due(int64_t now) noexcept {
        if(now<=0)return {};
        if(!origin_)origin_=now;
        if(now<origin_)return {};
        const auto delta=static_cast<uint64_t>(now-origin_);
        const auto slot=delta/10000000*30+delta%10000000*30/10000000;
        if(slot<next_)return {};
        const NativeVideoDue result{slot,slot-next_};
        next_=slot+1;
        return result;
    }
    int64_t origin() const noexcept{return origin_;}
    uint64_t slots() const noexcept{return next_;}
};
inline std::optional<size_t> oldestNativeVideoSlot(std::span<const int64_t> clocks) noexcept {
    std::optional<size_t> oldest;
    for(size_t i=0;i<clocks.size();++i)
        if(clocks[i]>0&&(!oldest||clocks[i]<clocks[*oldest]))oldest=i;
    return oldest;
}
struct NativeVideoPlacement {uint64_t encodedSlot{},repeatsBefore{};};
class NativeVideoTimeline {
    int64_t first_{},last_{};
    uint64_t firstSlot_{},lastSlot_{},frames_{},encoded_{},repeats_{};
public:
    // Slots describe intended capture cadence; clock is the actual source-copy
    // QPC. Reject reordered frames rather than silently moving their timestamps.
    std::optional<NativeVideoPlacement> accept(int64_t clock,uint64_t captureSlot) noexcept {
        if(clock<=0||(frames_&&(clock<=last_||captureSlot<=lastSlot_)))return {};
        const auto slot=frames_?captureSlot-firstSlot_:0;
        if(slot>=nativeVideoMaxSlots)return {};
        const auto fill=frames_?slot-encoded_:0;
        if(!frames_){first_=clock;firstSlot_=captureSlot;}
        last_=clock;lastSlot_=captureSlot;++frames_;encoded_=slot+1;repeats_+=fill;
        return NativeVideoPlacement{slot,fill};
    }
    int64_t firstClock() const noexcept{return first_;}
    int64_t lastClock() const noexcept{return last_;}
    uint64_t firstCaptureSlot() const noexcept{return firstSlot_;}
    uint64_t frames() const noexcept{return frames_;}
    uint64_t encodedFrames() const noexcept{return encoded_;}
    uint64_t repeats() const noexcept{return repeats_;}
};
}
