#pragma once
#include <cstdint>
#include <optional>

namespace mgs5vr {
// One acquisition per uninterrupted look. Source sample time, not render
// calls or wall time spent looking at a stale image, advances the dwell.
class ReconDwell {
public:
    bool update(std::optional<uint16_t> target,uint64_t sampleTime,uint64_t activation,uint64_t dwellMs) noexcept {
        if(!target||!activation||!sampleTime||!dwellMs){reset();return false;}
        if(target!=target_||activation!=activation_||sampleTime<last_||sampleTime-last_>150){
            target_=target;activation_=activation;began_=sampleTime;fired_=false;
        }
        last_=sampleTime;
        if(!fired_&&sampleTime-began_>=dwellMs){fired_=true;return true;}
        return false;
    }
    // A deliberate clear must remain cleared until the player looks away.
    void suppress() noexcept {fired_=true;}
    void reset() noexcept {target_.reset();activation_=began_=last_=0;fired_=false;}
private:
    std::optional<uint16_t> target_;
    uint64_t activation_{},began_{},last_{};
    bool fired_{};
};
}
