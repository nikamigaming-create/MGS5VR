#pragma once
#include <atomic>
#include <cstdint>

namespace mgs5vr {
// Shared optics lifecycle metadata has no dependency on either renderer or
// diagnostic hooks. Probe-only consumers must not pull in the full renderer.
namespace opticMaterialLifecycle {
inline thread_local bool drawing{};
inline std::atomic_uint64_t generation{1};
}
inline bool nativeBinocularMaterialDrawing() noexcept {
    return opticMaterialLifecycle::drawing;
}
inline uint64_t nativeBinocularMaterialGeneration() noexcept {
    return opticMaterialLifecycle::generation.load();
}
inline void invalidateNativeBinocularMaterial() noexcept {
    ++opticMaterialLifecycle::generation;
}
class OpticNativeMaterialDrawScope {
public:
    OpticNativeMaterialDrawScope() noexcept:previous_(opticMaterialLifecycle::drawing) {
        opticMaterialLifecycle::drawing=true;
    }
    ~OpticNativeMaterialDrawScope() {opticMaterialLifecycle::drawing=previous_;}
    OpticNativeMaterialDrawScope(const OpticNativeMaterialDrawScope&)=delete;
    OpticNativeMaterialDrawScope& operator=(const OpticNativeMaterialDrawScope&)=delete;
private:
    bool previous_{};
};
}
