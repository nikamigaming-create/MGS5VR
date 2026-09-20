#include "mgs5vr/idroid_rig.hpp"
#include "mgs5vr/arm_ik.hpp"
#include "mgs5vr/stereo.hpp"
#include <cmath>

namespace mgs5vr {
namespace {
Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
std::optional<Vec3> unit(Vec3 value){
    const float squared=dot(value,value);
    if(!std::isfinite(squared)||squared<.000001f)return {};
    const float scale=1.f/std::sqrt(squared);
    return value*scale;
}
}

std::optional<IdroidPose> trackedIdroidPose(const HeadCameraSample& frame) noexcept{
    if(!frame.applied||!frame.stereoTracked||!frame.activation||!frame.controllers.hands[1].gripTracked
       ||!valid(frame.nativePose)||!valid(frame.headPose)||!valid(frame.controllers.hands[1].grip))return {};
    const auto head=nativeTrackedPose(frame.nativePose,frame.headPose,frame.headPose);
    const auto grip=nativeTrackedPose(frame.nativePose,frame.headPose,frame.controllers.hands[1].grip);
    if(!valid(head)||!valid(grip))return {};
    const bool palmTracked=frame.renderedPalmTracked[1]&&valid(frame.renderedPalms[1]);
    const auto attachment=palmTracked?frame.renderedPalms[1]:grip;
    if(!valid(attachment))return {};

    // The screen is on the anatomical palm side: the verified right-hand
    // palm frame points into the palm on +X, so its outward display normal is
    // -X. Anatomical +Y points toward the wrist, so screen up is -Y
    // (toward the fingers) and screen right is -Z. Keep these device axes
    // in the same frame instead of replacing the
    // hand's palm side with a head-facing billboard. The raw-grip path remains
    // a safe readable fallback before the final skin publication is available.
    std::optional<Vec3> normal;
    Vec3 xDirection{};
    if(palmTracked){
        normal=unit(rotate(attachment.orientation,{-1,0,0}));
        xDirection=rotate(attachment.orientation,{0,0,-1});
    }else{
        normal=unit(head.position-attachment.position);
        xDirection=rotate(attachment.orientation,{1,0,0});
    }
    if(!normal)return {};
    xDirection=xDirection-*normal*dot(xDirection,*normal);
    auto x=unit(xDirection);
    if(!x){
        xDirection=rotate(palmTracked?attachment.orientation:head.orientation,{0,1,0});
        xDirection=xDirection-*normal*dot(xDirection,*normal);
        x=unit(xDirection);
    }
    if(!x)return {};
    const auto y=unit(cross(*normal,*x));
    if(!y)return {};
    const auto z=cross(*x,*y);
    const std::array<float,16> axes{
        x->x,x->y,x->z,0,
        y->x,y->y,y->z,0,
        z.x,z.y,z.z,0,
        0,0,0,1};
    const auto orientation=nativeAffinePose(axes);
    if(!orientation)return {};
    const auto attachmentOrientation=orientation->orientation;
    // With the fingers up and palm facing the viewer, these axes give an
    // upright display without adding a camera-dependent rotation.
    const auto displayOrientation=attachmentOrientation;
    const auto displayNormal=rotate(displayOrientation,{0,0,1});
    const auto attachmentUp=rotate(attachmentOrientation,{0,1,0});
    const auto body=Pose{displayOrientation,attachment.position+displayNormal*(palmTracked?.012f:.035f)};
    const auto screen=Pose{body.orientation,body.position+displayNormal*(palmTracked?.012f:.022f)
        +(palmTracked?attachmentUp*.045f:Vec3{})};
    if(!valid(body)||!valid(screen))return {};
    return IdroidPose{body,screen};
}

std::optional<IdroidRayHit> trackedIdroidRay(const HeadCameraSample& frame) noexcept{
    const auto idroid=trackedIdroidPose(frame);
    const auto& hand=frame.controllers.hands[1];
    if(!idroid||!hand.aimTracked||!valid(hand.aim))return {};
    const auto aim=nativeTrackedPose(frame.nativePose,frame.headPose,hand.aim);
    if(!valid(aim))return {};
    const Panel display{idroid->screen,idroidScreenWidth,idroidScreenHeight,
        idroidScreenPixelWidth,idroidScreenPixelHeight};
    if(const auto hit=intersectPanel(display,aim,idroidRayMaxDistance))return IdroidRayHit{*hit,aim};
    return {};
}
}
