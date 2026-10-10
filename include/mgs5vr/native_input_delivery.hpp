#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <ostream>

namespace mgs5vr {
enum class NativeInputDeliverySource:uint8_t { unavailable,nativeXInput,xrMailbox };
inline const char* nativeInputDeliverySourceName(NativeInputDeliverySource source) noexcept {
    switch(source){
    case NativeInputDeliverySource::nativeXInput:return "native_xinput";
    case NativeInputDeliverySource::xrMailbox:return "xr_mailbox";
    default:return "unavailable";
    }
}
struct NativeInputCaller {uint64_t rva{};bool inGame{};};
inline NativeInputCaller nativeInputCaller(uintptr_t caller,uintptr_t base,uint32_t imageSize) noexcept {
    if(base&&imageSize&&caller>=base&&caller-base<imageSize)return {caller-base,true};
    return {};
}
// A witness of the packet immediately before this adapter returns from the
// game's imported XInputGetState. It does not certify player movement, a
// simulation transaction, or physical controller/headset acceptance.
struct NativeInputDelivery {
    uint64_t deliveryCounter{},timeMs{};
    uint32_t index{},resultCode{},packetNumber{},nativeSlot{};
    NativeInputCaller caller{};
    NativeInputDeliverySource source{NativeInputDeliverySource::unavailable};
    uint16_t buttons{};
    std::array<int16_t,4> axes{};
    std::array<uint8_t,2> triggers{};
    bool packetValid{},nativeSlotValid{},xrMailboxFresh{};
};
struct NativeInputDeliverySnapshot {
    NativeInputDelivery last{},index0{};
    uint64_t attemptedDeliveries{},publicationDropped{};
    bool available{};
};
class NativeInputDeliveryAudit {
public:
    // One try-acquire, fixed-size copies, no waits or allocations. Contention
    // loses telemetry only; callers always retain the original input result.
    bool publish(NativeInputDelivery value) noexcept {
        value.deliveryCounter=attempted_.fetch_add(1,std::memory_order_relaxed)+1;
        if(guard_.test_and_set(std::memory_order_acquire)){
            dropped_.fetch_add(1,std::memory_order_relaxed);return false;
        }
        value.packetValid=value.packetValid&&value.resultCode==0;
        if(!value.packetValid){
            value.packetNumber=0;value.buttons=0;value.axes={};value.triggers={};
            value.nativeSlot=0;value.nativeSlotValid=false;
        }
        // Concurrent return witnesses can acquire the guard out of order.
        // Keep the highest counter for each independently retained slot.
        if(value.deliveryCounter>last_.deliveryCounter)last_=value;
        if(value.index==0&&value.deliveryCounter>index0_.deliveryCounter)index0_=value;
        guard_.clear(std::memory_order_release);return true;
    }
    NativeInputDeliverySnapshot snapshot() const noexcept {
        NativeInputDeliverySnapshot value;
        if(!guard_.test_and_set(std::memory_order_acquire)){
            value.last=last_;value.index0=index0_;value.available=true;
            guard_.clear(std::memory_order_release);
        }
        // Counters are independently sampled, after the coherent record copy.
        value.attemptedDeliveries=attempted_.load(std::memory_order_relaxed);
        value.publicationDropped=dropped_.load(std::memory_order_relaxed);
        return value;
    }
private:
    mutable std::atomic_flag guard_=ATOMIC_FLAG_INIT;
    std::atomic_uint64_t attempted_{},dropped_{};
    NativeInputDelivery last_{},index0_{};
};
inline constinit NativeInputDeliveryAudit nativeInputDeliveryAuditInstance;
inline NativeInputDeliveryAudit& nativeInputDeliveryAudit() noexcept {return nativeInputDeliveryAuditInstance;}
inline void writeNativeInputDeliveryJson(std::ostream& out,const NativeInputDeliverySnapshot& value,uint64_t now){
    const auto flags=out.flags();out<<std::dec;
    const auto boolean=[](bool flag){return flag?"true":"false";};
    out<<"{\"schema\":1,\"source\":\"game_xinput_getstate_return\",\"snapshot_available\":"<<boolean(value.available)
       <<",\"record_copy_coherent\":"<<boolean(value.available)<<",\"counters_independently_sampled\":true"
       <<",\"attempted_deliveries\":"<<value.attemptedDeliveries<<",\"publication_dropped\":"<<value.publicationDropped;
    const auto record=[&](const char* name,const NativeInputDelivery& item){
        out<<",\""<<name<<"\":";
        if(!value.available||!item.deliveryCounter){out<<"null";return;}
        out<<"{\"delivery_counter\":"<<item.deliveryCounter<<",\"sample_ms\":"<<item.timeMs<<",\"age_ms\":";
        if(now>=item.timeMs)out<<now-item.timeMs;else out<<"null";
        out<<",\"index\":"<<item.index<<",\"caller_in_game\":"<<boolean(item.caller.inGame)<<",\"caller_rva\":";
        if(item.caller.inGame)out<<item.caller.rva;else out<<"null";
        out<<",\"source\":\""<<nativeInputDeliverySourceName(item.source)<<"\",\"result_code\":"<<item.resultCode
           <<",\"xr_mailbox_fresh\":"<<boolean(item.xrMailboxFresh)<<",\"packet_valid\":"<<boolean(item.packetValid)
           <<",\"native_slot\":";
        if(item.packetValid&&item.nativeSlotValid)out<<item.nativeSlot;else out<<"null";
        out<<",\"packet_number\":";
        if(item.packetValid)out<<item.packetNumber;else out<<"null";
        out<<",\"buttons\":";
        if(item.packetValid)out<<item.buttons;else out<<"null";
        out<<",\"axes\":";
        if(item.packetValid)out<<'['<<item.axes[0]<<','<<item.axes[1]<<','<<item.axes[2]<<','<<item.axes[3]<<']';else out<<"null";
        out<<",\"triggers\":";
        if(item.packetValid)out<<'['<<unsigned(item.triggers[0])<<','<<unsigned(item.triggers[1])<<']';else out<<"null";
        out<<'}';
    };
    record("last",value.last);record("index0",value.index0);out<<'}';out.flags(flags);
}
}
