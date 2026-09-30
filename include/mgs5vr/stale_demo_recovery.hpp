#pragma once

#include <cstdint>

namespace mgs5vr {

// Requires a continuous stream of new, fresh player-camera publications for
// the same native owner and Lua candidate. Any candidate, owner, camera, or
// publication discontinuity restarts the dwell.
class StaleDemoRecoveryDwell {
public:
    static constexpr uint64_t requiredDwellMs = 2000;
    static constexpr uint64_t maximumPublicationGapMs = 150;
    static constexpr uint64_t maximumCandidateAgeMs = 300;

    bool observe(bool candidate,uint64_t key,uint64_t generation,
                 uintptr_t camera,uintptr_t owner,uint64_t timeMs,uint64_t candidatePublishedAtMs,
                 bool menuOpen) noexcept {
        if(!candidate||!key||!generation||!camera||!owner||menuOpen
           ||candidatePublishedAtMs>timeMs||timeMs-candidatePublishedAtMs>maximumCandidateAgeMs){reset();return false;}
        if(!valid_||key!=key_||generation!=generation_||camera!=camera_||owner!=owner_
           ||timeMs<=lastTimeMs_||timeMs-lastTimeMs_>maximumPublicationGapMs){
            valid_=true;key_=key;generation_=generation;camera_=camera;owner_=owner;
            startedAtMs_=lastTimeMs_=timeMs;return false;
        }
        lastTimeMs_=timeMs;
        return timeMs>=startedAtMs_&&timeMs-startedAtMs_>=requiredDwellMs;
    }

    bool ready(uint64_t nowMs,uint64_t key,uint64_t generation,uint64_t candidatePublishedAtMs,
               bool menuOpen) const noexcept {
        return valid_&&!menuOpen&&key==key_&&generation==generation_
            &&candidatePublishedAtMs<=nowMs&&nowMs-candidatePublishedAtMs<=maximumCandidateAgeMs
            &&nowMs>=lastTimeMs_&&nowMs-lastTimeMs_<=maximumPublicationGapMs
            &&lastTimeMs_>=startedAtMs_&&lastTimeMs_-startedAtMs_>=requiredDwellMs;
    }

    void reset() noexcept {
        valid_=false;key_=generation_=camera_=owner_=startedAtMs_=lastTimeMs_=0;
    }

    bool matchesCamera(uintptr_t camera,uintptr_t acceptedOwner) const noexcept {
        return valid_&&camera&&camera==camera_&&owner_
            &&(!acceptedOwner||acceptedOwner==owner_);
    }

private:
    bool valid_{};
    uint64_t key_{},generation_{},startedAtMs_{},lastTimeMs_{};
    uintptr_t camera_{},owner_{};
};

} // namespace mgs5vr
