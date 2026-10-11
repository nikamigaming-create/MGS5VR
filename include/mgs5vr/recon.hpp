#pragma once
#include <cstdint>
#include <bitset>
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
    // Suppress repeated dwell pulses during this uninterrupted look.
    void suppress() noexcept {fired_=true;}
    void reset() noexcept {target_.reset();activation_=began_=last_=0;fired_=false;}
private:
    std::optional<uint16_t> target_;
    uint64_t activation_{},began_{},last_{};
    bool fired_{};
};

// Explicit unmarking wins over automatic acquisition for this native scene.
// Looking away, tracking gaps, optic stow and menu focus do not undo it.
class ReconClearPolicy {
public:
    void scene(uint64_t player,uint64_t records) noexcept {
        if(!player||!records||player!=player_||records!=records_)reset();
        player_=player;records_=records;
    }
    void clear(uint16_t id) noexcept {if(player_&&records_&&id!=0xffff)cleared_.set(id);}
    void explicitMark(uint16_t id) noexcept {if(id!=0xffff)cleared_.reset(id);}
    bool automaticAllowed(uint16_t id) const noexcept {
        return player_&&records_&&id!=0xffff&&!cleared_.test(id);
    }
    void reset() noexcept {cleared_.reset();player_=records_=0;}
private:
    std::bitset<65536> cleared_;
    uint64_t player_{},records_{};
};
}
