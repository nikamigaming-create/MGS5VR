#pragma once
#include "core.hpp"
#include <algorithm>
#include <cmath>

namespace mgs5vr {
// Retail radio volumes carry a world center, half extents and orientation.
// Intersect the physical sight with that volume; proximity to its center is
// insufficient, particularly for long village and building volumes.
inline std::optional<float> opticRadioBoxHit(Pose ray,Pose box,Vec3 half,
                                            float minDistance,float maxDistance){
    if(!valid(ray)||!valid(box)||!std::isfinite(half.x)||!std::isfinite(half.y)
       ||!std::isfinite(half.z)||half.x<=0||half.y<=0||half.z<=0
       ||!std::isfinite(minDistance)||!std::isfinite(maxDistance)
       ||minDistance<0||maxDistance<0||(maxDistance>0&&maxDistance<minDistance))return {};
    const auto local=compose(inverse(box),ray);
    const auto direction=rotate(local.orientation,{0,0,-1});
    const float origins[]{local.position.x,local.position.y,local.position.z};
    const float directions[]{direction.x,direction.y,direction.z};
    const float extents[]{half.x,half.y,half.z};
    float near=0,far=maxDistance>0?std::min(maxDistance,1500.f):1500.f;
    for(int axis=0;axis<3;++axis){
        if(std::abs(directions[axis])<1e-6f){
            if(std::abs(origins[axis])>extents[axis])return {};
            continue;
        }
        auto a=(-extents[axis]-origins[axis])/directions[axis];
        auto b=(extents[axis]-origins[axis])/directions[axis];
        if(a>b)std::swap(a,b);
        near=std::max(near,a);far=std::min(far,b);
        if(near>far)return {};
    }
    if(far<minDistance)return {};
    return std::max(near,minDistance);
}
}
