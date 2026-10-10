#pragma once
#include <atomic>
#include <cstdint>

namespace mgs5vr {
// Explicit local simulator negative fixture. It can only invalidate raw
// observations and expires independently of the native runner/transport.
class TrackingFaultProbe {
public:
    explicit TrackingFaultProbe(unsigned supportedMask=15) noexcept :supportedMask_(supportedMask){}
    void configure(bool enabled) noexcept {enabled_=enabled;lease_=0;focused_=false;}
    void focused(bool value) noexcept {focused_=value;if(!value)lease_=0;}
    bool arm(unsigned mask,uint64_t duration,uint64_t now) noexcept {
        if(!enabled_||!focused_||(mask&~supportedMask_)||mask>15||!now||duration<100||duration>8000
           ||now>(UINT64_MAX>>4)-duration)return false;
        lease_=((now+duration)<<4)|mask;
        return true;
    }
    void clear() noexcept {lease_=0;}
    unsigned mask(uint64_t now) const noexcept {
        if(!enabled_||!focused_)return 0;
        const auto sample=lease_.load();const auto until=sample>>4;
        return now<until&&until-now<=8000?unsigned(sample&15):0;
    }
    bool enabled() const noexcept {return enabled_;}
private:
    const unsigned supportedMask_;
    std::atomic_bool enabled_{},focused_{};
    std::atomic_uint64_t lease_{};
};
using ControllerTrackingFaultProbe=TrackingFaultProbe;
inline ControllerTrackingFaultProbe& controllerTrackingFaultProbe(){
    static ControllerTrackingFaultProbe probe;return probe;
}
// Separate head/view validity fixture. Bits 1 and 2 invalidate only the
// corresponding raw observation; controller leases and actions are untouched.
inline TrackingFaultProbe& headTrackingFaultProbe(){
    static TrackingFaultProbe probe{3};return probe;
}
}
