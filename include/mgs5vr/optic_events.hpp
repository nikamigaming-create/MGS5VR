#pragma once
#include "core.hpp"
#include <cstdint>
#include <optional>
#include <deque>

namespace mgs5vr {
enum class OpticEventKind { raised, waypoint };
struct OpticEvent {
    OpticEventKind kind{};
    Vec3 position{};
    uint64_t sampleTime{},activation{};
};

// The caller serializes producer and consumer access. Keep the same bounded
// queue and freshness/activation checks on both sides of the Lua handoff.
class OpticEventQueue {
public:
    void push(OpticEvent event) {
        if(events_.size()>=16)events_.pop_front();
        events_.push_back(event);
    }
    std::optional<OpticEvent> take(uint64_t now,uint64_t activation) {
        while(!events_.empty()){
            const auto event=events_.front();
            if(event.activation!=activation){events_.pop_front();continue;}
            // A producer may publish after the consumer samples its clock.
            // Leave that event queued until the next update instead of losing
            // the only physical raise edge for this use of the binoculars.
            if(now<event.sampleTime)return {};
            events_.pop_front();
            if(now-event.sampleTime<=250)return event;
        }
        return {};
    }
private:
    std::deque<OpticEvent> events_;
};

// A physical ocular entering eye relief is the VR equivalent of entering
// binocular mode. Replayed images and lost tracking cannot create that edge.
class OpticUseEdge {
public:
    bool update(bool atEye,uint64_t sampleTime,uint64_t activation) noexcept {
        if(!sampleTime||!activation){reset();return false;}
        if(activation!=activation_||sampleTime<last_||sampleTime-last_>150)active_=false;
        const bool raised=atEye&&!active_;
        activation_=activation;last_=sampleTime;active_=atEye;
        return raised;
    }
    void reset() noexcept {active_=false;last_=activation_=0;}
private:
    uint64_t last_{},activation_{};
    bool active_{};
};

// Produced only by accepted physical optic samples and successful native
// waypoint insertion, consumed on the retail Lua update thread.
std::optional<OpticEvent> takeOpticEvent(uint64_t now,uint64_t activation);
void acknowledgeOpticUse(const OpticEvent& event);
}
