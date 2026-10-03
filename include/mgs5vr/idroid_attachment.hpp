#pragma once
#include "core.hpp"
#include <cstddef>

namespace mgs5vr {
struct HeadCameraSample;
// This stamp describes a completed owned body publication, not a pose request.
struct IdroidBodyStamp {
    uintptr_t player{},model{},driver{},palette{};
    uint64_t activation{},referenceEpoch{},presentationEpoch{},tracking{},rig{},menuGeneration{};
    int64_t predictedXrTime{};
    bool focused{};
};
constexpr bool sameIdroidBodyPublication(const IdroidBodyStamp& completed,
                                         const IdroidBodyStamp& current) noexcept {
    return completed.player&&completed.model&&completed.driver&&completed.palette
        &&completed.activation&&completed.referenceEpoch&&completed.presentationEpoch
        &&completed.tracking&&completed.rig&&completed.predictedXrTime>0&&completed.focused
        &&current.focused&&completed.player==current.player&&completed.model==current.model
        &&completed.driver==current.driver&&completed.palette==current.palette
        &&completed.activation==current.activation&&completed.referenceEpoch==current.referenceEpoch
        &&completed.presentationEpoch==current.presentationEpoch&&completed.tracking==current.tracking
        &&completed.rig==current.rig&&completed.menuGeneration==current.menuGeneration
        &&completed.predictedXrTime==current.predictedXrTime;
}
constexpr bool idroidBodyCompletedBefore(const IdroidBodyStamp& completed,uint64_t completedQpc,
                                         const IdroidBodyStamp& current,uint64_t eventQpc) noexcept {
    return completedQpc&&eventQpc>=completedQpc&&sameIdroidBodyPublication(completed,current);
}
struct NativeIdroidBinding {
    uintptr_t character{},service{},component{},row{},nativeInterface{},model{},palette{},point{};
    uint32_t playerIndex{},localIndex{},handle{};
    bool operator==(const NativeIdroidBinding&) const=default;
};
constexpr bool sameNativeIdroidBinding(const NativeIdroidBinding& a,const NativeIdroidBinding& b) noexcept {
    return a.character&&a.service&&a.component&&a.row&&a.nativeInterface&&a.model&&a.palette&&a.point
        &&(a.handle&31u)==17u&&((a.handle>>5)&0x7ffu)==499u&&(a.handle>>16)<=255u
        &&a.localIndex<256u&&a==b;
}
// Native named-bone getter 216050: a local palette is postmultiplied by its
// binding root. A world palette is already complete and must not be moved twice.
constexpr std::array<float,16> nativeIdroidWorldMatrix(const std::array<float,16>& bone,
        const std::array<float,16>& bindingRoot,uint8_t bindingFlags) noexcept {
    if(!(bindingFlags&1u))return bone;
    std::array<float,16> world{};
    for(std::size_t r=0;r<4;++r)for(std::size_t c=0;c<4;++c)
        for(std::size_t k=0;k<4;++k)world[r*4+c]+=bone[r*4+k]*bindingRoot[k*4+c];
    return world;
}
struct NativeIdroidDisplay {
    Pose body{}; // Authored SKL_000_ROOT in WORLD space, after the native A8 call.
    IdroidBodyStamp stamp{};
    NativeIdroidBinding binding{};
    uint64_t bodyCompleteQpc{},publishedQpc{};
};
// A pose publication, not native visibility or interactive input authority.
// No earlier rig/menu/focus generation is eligible, including paused refreshes
// which completed the body without a matching native device publication.
std::optional<NativeIdroidDisplay> resolveNativeIdroidDisplay(const HeadCameraSample&) noexcept;
// The native A8 call always receives its original pointer and runs exactly once.
// Read-only publication remains active; bounded logging is opt-in through
// MGS5VR_IDROID_ATTACHMENT_TRACE=1 or MGS5VR_IDROID_UI_BOUNDARY_TRACE=1.
void installIdroidAttachmentDiagnostics(uintptr_t moduleBase) noexcept;
bool idroidAttachmentDiagnosticsActive() noexcept;
// Caller captures completedFrame immediately after this solve's rig publication,
// then calls here only after the same owned original skin publisher returns.
void noteIdroidBodyComplete(uintptr_t driver,uintptr_t binding,
                           const HeadCameraSample& completedFrame,bool pausedRefresh) noexcept;
void stopIdroidAttachmentDiagnostics() noexcept;
}
