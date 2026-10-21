#pragma once

#include <cstdint>
#include <mutex>

namespace mgs5vr {

enum class NativeDemoMode { none, cinematic, interactiveLook, staleCandidate };

struct NativeDemoSnapshot {
    NativeDemoMode mode{NativeDemoMode::none};
    uint64_t candidateKey{};
    uint64_t generation{};
    uint64_t publishedAtMs{};
    uint64_t lookLessonKey{};
};

// Header-only state is shared by core and runtime without introducing a
// runtime-library dependency from HeadCamera back into the renderer.
inline std::mutex& nativeDemoStateMutex() noexcept {
    static std::mutex mutex;
    return mutex;
}
inline NativeDemoSnapshot& nativeDemoStateStorage() noexcept {
    static NativeDemoSnapshot state;
    return state;
}
inline NativeDemoSnapshot nativeDemoSnapshot() noexcept {
    std::lock_guard lock(nativeDemoStateMutex());
    return nativeDemoStateStorage();
}
inline void publishNativeDemoSnapshot(NativeDemoMode mode,uint64_t key,
                                      uint64_t publishedAtMs,uint64_t lookLessonKey=0) noexcept {
    std::lock_guard lock(nativeDemoStateMutex());
    auto& state=nativeDemoStateStorage();
    if(mode!=NativeDemoMode::staleCandidate)key=0;
    if(mode!=NativeDemoMode::interactiveLook)lookLessonKey=0;
    if(state.mode!=mode||state.candidateKey!=key||state.lookLessonKey!=lookLessonKey)++state.generation;
    state.mode=mode;
    state.candidateKey=key;
    state.lookLessonKey=lookLessonKey;
    state.publishedAtMs=(mode==NativeDemoMode::staleCandidate||mode==NativeDemoMode::interactiveLook)?publishedAtMs:0;
}

} // namespace mgs5vr
