#pragma once

#include "core.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>

namespace mgs5vr {

// A sampled envelope from the game's collision scene, in origin-local units.
// This is not a swept capsule query or proof of clearance between the samples.
// Never populate it from guessed dimensions or decorative props.
struct NativeRoomBounds {
    bool valid{};
    Pose origin{};
    Vec3 min{};
    Vec3 max{};
    uint64_t sceneGeneration{};
};

inline bool validNativeRoomBounds(const NativeRoomBounds& bounds) noexcept {
    if(!bounds.valid||!valid(bounds.origin)||!bounds.sceneGeneration)return false;
    const auto finite=[](Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);};
    return finite(bounds.min)&&finite(bounds.max)
        &&bounds.max.x>bounds.min.x&&bounds.max.y>bounds.min.y&&bounds.max.z>bounds.min.z
        &&bounds.max.x-bounds.min.x<20.f&&bounds.max.y-bounds.min.y<20.f
        &&bounds.max.z-bounds.min.z<20.f;
}

// Distances along +X, -X, +Y, -Y, +Z, -Z. Missing/invalid collision samples
// are unknown, never free space. Keep construction here so production and tests
// exercise the same sign, completeness and validity rules.
using NativeRoomSamples=std::array<std::optional<float>,6>;
inline NativeRoomBounds nativeRoomBoundsFromSamples(Pose origin,uint64_t generation,
    const NativeRoomSamples& samples,float reach) noexcept {
    NativeRoomBounds bounds;bounds.origin=origin;bounds.sceneGeneration=generation;
    if(!valid(origin)||!generation||!std::isfinite(reach)||reach<=.10f)return bounds;
    for(const auto& sample:samples)
        if(!sample||!std::isfinite(*sample)||*sample<=.08f||*sample>=reach-.02f)return bounds;
    bounds.min={-*samples[1],-*samples[3],-*samples[5]};
    bounds.max={*samples[0],*samples[2],*samples[4]};
    bounds.valid=true;
    bounds.valid=validNativeRoomBounds(bounds);
    return bounds;
}

inline Vec3 nativeRoomLocal(const NativeRoomBounds& bounds,Vec3 world) noexcept {
    return compose(inverse(bounds.origin),Pose{{},world}).position;
}

inline bool nativeRoomContains(const NativeRoomBounds& bounds,Vec3 local,float margin=0) noexcept {
    if(!validNativeRoomBounds(bounds)||!std::isfinite(margin)||margin<0)return false;
    return local.x>=bounds.min.x+margin&&local.x<=bounds.max.x-margin
        &&local.y>=bounds.min.y+margin&&local.y<=bounds.max.y-margin
        &&local.z>=bounds.min.z+margin&&local.z<=bounds.max.z-margin;
}

// Point constraint within the sampled envelope, NOT capsule collision.
// Callers must leave the pose unchanged when the samples are unavailable.
inline Vec3 nativeRoomClamp(const NativeRoomBounds& bounds,Vec3 world,Vec3 margin) noexcept {
    if(!validNativeRoomBounds(bounds)||!valid(Pose{{},margin})
       ||margin.x<0||margin.y<0||margin.z<0)return world;
    auto local=nativeRoomLocal(bounds,world);
    const auto low=bounds.min+margin;
    const auto high=bounds.max-margin;
    if(high.x<low.x||high.y<low.y||high.z<low.z)return world;
    local.x=std::clamp(local.x,low.x,high.x);
    local.y=std::clamp(local.y,low.y,high.y);
    local.z=std::clamp(local.z,low.z,high.z);
    return compose(bounds.origin,Pose{{},local}).position;
}
inline Vec3 nativeRoomClamp(const NativeRoomBounds& bounds,Vec3 world,float margin) noexcept {
    return nativeRoomClamp(bounds,world,Vec3{margin,margin,margin});
}

inline Vec3 nativeRoomDimensions(const NativeRoomBounds& bounds) noexcept {
    return bounds.max-bounds.min;
}

}
