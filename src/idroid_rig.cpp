#include "mgs5vr/idroid_rig.hpp"
#include "mgs5vr/stereo.hpp"
#include <cmath>

namespace mgs5vr {
std::optional<Pose> idroidBodyFromConnector(Pose connector) noexcept {
    if(!valid(connector))return {};
    const auto body=compose(connector,inverse(idroidConnectorInBody));
    return valid(body)?std::optional<Pose>{body}:std::nullopt;
}

std::optional<IdroidPose> trackedIdroidPose(const HeadCameraSample& frame) noexcept{
    if(!frame.applied||!frame.stereoTracked||!frame.activation
       ||!frame.idroidDeviceTracked||!valid(frame.idroidDevice)
       ||!valid(frame.nativePose)||!valid(frame.headPose))return {};
    const auto body=frame.idroidDevice;
    const auto mount=compose(body,idroidHologramInBody);
    if(!std::isfinite(frame.controllers.idroidScreenDepth)
       ||frame.controllers.idroidScreenDepth<0||frame.controllers.idroidScreenDepth>.20f)return {};
    const auto offset=frame.controllers.idroidScreenOffset;
    if(!valid(Pose{frame.controllers.idroidScreenRotation,offset})
       ||std::abs(offset.x)>.20f||std::abs(offset.y)>.20f||offset.z!=0)return {};
    // Native device axes define right, up and the readable face. Fit controls
    // pivot at the projected center; the hand, body and emitter stay attached.
    // Never substitute a newer raw grip or an anatomical palm for this mount.
    const auto screen=compose(mount,Pose{frame.controllers.idroidScreenRotation,
        {offset.x,offset.y,frame.controllers.idroidScreenDepth}});
    if(!valid(screen))return {};
    return IdroidPose{body,screen};
}

std::optional<IdroidRayHit> trackedIdroidRay(const HeadCameraSample& frame) noexcept{
    const auto idroid=trackedIdroidPose(frame);
    const auto& hand=frame.controllers.hands[1];
    if(!idroid||!hand.aimTracked||!valid(hand.aim))return {};
    const auto aim=nativeTrackedPose(frame.nativePose,frame.headPose,hand.aim);
    if(!valid(aim))return {};
    const float width=frame.controllers.idroidScreenWidth>0?frame.controllers.idroidScreenWidth:idroidScreenWidth;
    const float height=width*9.f/16.f;
    const Panel display{idroid->screen,width,height,
        idroidScreenPixelWidth,idroidScreenPixelHeight};
    if(const auto hit=intersectPanel(display,aim,idroidRayMaxDistance))return IdroidRayHit{*hit,aim};
    return {};
}
}
