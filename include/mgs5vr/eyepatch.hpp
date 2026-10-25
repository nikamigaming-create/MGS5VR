#pragma once
#include "stereo.hpp"
#include <cmath>

namespace mgs5vr {
// Exact owned face0 patch vertices retain native scale, shape and UVs. Convert
// the FMDL reader's +X-right / native +Z-forward basis to head-local -Z.
// The translation puts the near-eye surface at a usable relief distance;
// transparency preserves sight through it rather than replacing it with a mask.
inline Vec3 eyepatchHeadPoint(Vec3 imported) noexcept {
    return {imported.x-.012f,imported.y-.470f,-(imported.z-.045f)};
}
inline std::optional<std::array<float,4>> projectEyepatch(
    Pose head,Pose rightEye,const EyeView& eye,Vec3 surface) noexcept {
    if(!valid(head)||!valid(rightEye)||!valid(eye.pose))return {};
    const auto pupil=compose(inverse(head),rightEye).position;
    const auto p=compose(compose(inverse(eye.pose),head),Pose{{},pupil+surface}).position;
    const float left=std::tan(eye.fov.left),right=std::tan(eye.fov.right);
    const float down=std::tan(eye.fov.down),up=std::tan(eye.fov.up),depth=-p.z;
    if(!std::isfinite(left+right+down+up)||right-left<=0||up-down<=0)return {};
    // Keep homogeneous W: the real straps extend behind the eyes. D3D clips
    // those triangles at 5 mm instead of reflecting them across the screen.
    const std::array<float,4> projected{(2*p.x-(right+left)*depth)/(right-left),
        (2*p.y-(up+down)*depth)/(up-down),depth-.005f,depth};
    for(float value:projected)if(!std::isfinite(value))return {};
    return projected;
}
}
