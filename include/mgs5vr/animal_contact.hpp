#pragma once
#include "core.hpp"
#include <algorithm>
#include <cmath>
#include <optional>

namespace mgs5vr {
// A conservative head contact proxy attached to the native animated bones.
// This constrains the rendered hand; it never moves or replaces the animal.
struct AnimalContactCapsule { Vec3 a{},b{};float radius{}; };
inline Vec3 animalContactAxisPoint(Vec3 p,const AnimalContactCapsule& c) noexcept {
    const auto axis=c.b-c.a;
    return c.a+axis*std::clamp(dot(p-c.a,axis)/std::max(dot(axis,axis),1e-8f),0.f,1.f);
}
inline std::optional<Vec3> animalContactPoint(Vec3 previous,Vec3 requested,
    const AnimalContactCapsule& c,Vec3 fallback) noexcept {
    if(!valid(Pose{{},previous})||!valid(Pose{{},requested})||!valid(Pose{{},c.a})
       ||!valid(Pose{{},c.b})||!std::isfinite(c.radius)||c.radius<=0||c.radius>1)return {};
    const auto normalAt=[&](Vec3 p){
        auto n=p-animalContactAxisPoint(p,c);float length=std::sqrt(dot(n,n));
        if(length<1e-5f){n=fallback-animalContactAxisPoint(fallback,c);length=std::sqrt(dot(n,n));}
        if(!std::isfinite(length)||length<1e-5f)return Vec3{0,1,0};
        return n*(1.f/length);
    };
    const auto distance=[&](Vec3 p){const auto n=p-animalContactAxisPoint(p,c);return std::sqrt(dot(n,n));};
    const auto project=[&](Vec3 p){return animalContactAxisPoint(p,c)+normalAt(p)*(c.radius+.001f);};
    // Recover gracefully when an animated surface moves into a resting hand.
    if(distance(previous)<c.radius)previous=project(previous);
    const auto delta=requested-previous;const float length=std::sqrt(dot(delta,delta));
    if(length<1e-6f)return distance(requested)<c.radius?project(requested):requested;
    const auto direction=delta*(1.f/length);
    // Conservative advancement over the capsule distance field prevents a fast
    // controller sample from tunnelling through to the far side of the head.
    float travel=0;
    for(unsigned i=0;i<80&&travel<length;++i){
        const auto p=previous+direction*travel;
        const float gap=distance(p)-c.radius;
        if(gap<.0002f){
            const auto normal=normalAt(p);const auto remaining=requested-p;
            if(dot(remaining,normal)>=0)return requested;
            const auto tangent=remaining-normal*dot(remaining,normal);
            return project(p+tangent);
        }
        travel+=std::max(gap*.95f,.00005f);
    }
    return distance(requested)<c.radius?project(requested):requested;
}
}
