#pragma once
#include <cstdint>
#include <cmath>

namespace mgs5vr {
// Temporarily lend the LEFT hand while retaining the same native terminal and
// handset allocation. Closing the terminal cancels this presentation lease.
enum class IdroidHandUse : uint8_t { device, support, reload };
// Native skin and player-head publications can cross between two XR polls.
// This bounded history is for input ownership only, never for rendering poses.
constexpr bool idroidHandUseSourceFresh(uintptr_t player,uint64_t activation,uint64_t epoch,
        uint64_t sampled,uint64_t currentActivation,uint64_t currentEpoch,uint64_t now) noexcept {
    return player&&activation&&epoch&&sampled&&activation==currentActivation&&epoch==currentEpoch
        &&now>=sampled&&now-sampled<=150;
}
struct IdroidHandUseInput {
    bool eligible{},open{},ready{},supportHeld{},contactTracked{},reloadChord{},manipulating{};
    float contactDistance{},acquireRadius{.10f},detachRadius{.30f};
    uintptr_t player{};
    uint64_t activation{},referenceEpoch{};
};
class IdroidHandUseController {
public:
    IdroidHandUse update(const IdroidHandUseInput& input,uint64_t now) noexcept {
        if(!input.eligible||!input.open||!input.player||!input.activation||!input.referenceEpoch
            ||(player_&&(player_!=input.player||activation_!=input.activation||epoch_!=input.referenceEpoch))
            ||(lastTime_&&now<lastTime_)){
            reset();priorReload_=input.reloadChord;lastTime_=now;return use_;
        }
        player_=input.player;activation_=input.activation;epoch_=input.referenceEpoch;lastTime_=now;
        const bool reloadEdge=input.reloadChord&&!priorReload_;priorReload_=input.reloadChord;
        if(use_==IdroidHandUse::reload){
            if(requested_){
                if(input.manipulating)acknowledged_=true;
                if((acknowledged_&&!input.manipulating&&!input.reloadChord)
                    ||(!acknowledged_&&now-requestedAt_>750)||now-requestedAt_>10000)
                    clearReload();
            }else if((!input.reloadChord&&now-lastChordAt_>180)||!input.ready||now-startedAt_>5000)
                clearReload();
            if(use_==IdroidHandUse::reload){
                if(input.reloadChord)lastChordAt_=now;
                return use_;
            }
        }
        if(reloadEdge&&input.ready){
            use_=IdroidHandUse::reload;startedAt_=lastChordAt_=now;return use_;
        }
        const float radius=use_==IdroidHandUse::support?input.detachRadius:input.acquireRadius;
        const bool contact=input.ready&&input.supportHeld&&input.contactTracked
            &&std::isfinite(input.contactDistance)&&input.contactDistance>=0
            &&std::isfinite(radius)&&radius>0&&input.contactDistance<=radius;
        use_=contact?IdroidHandUse::support:IdroidHandUse::device;
        return use_;
    }
    void reloadRequested(uint64_t now) noexcept {
        if(use_==IdroidHandUse::reload&&!requested_){requested_=true;requestedAt_=now;}
    }
    IdroidHandUse use() const noexcept {return use_;}
    void reset() noexcept {*this={};}
private:
    void clearReload() noexcept {use_=IdroidHandUse::device;requested_=acknowledged_=false;requestedAt_=0;}
    uintptr_t player_{};
    uint64_t activation_{},epoch_{},lastTime_{},startedAt_{},lastChordAt_{},requestedAt_{};
    IdroidHandUse use_{IdroidHandUse::device};
    bool priorReload_{},requested_{},acknowledged_{};
};
}
