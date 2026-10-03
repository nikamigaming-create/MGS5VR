#include "mgs5vr/idroid_rig.hpp"
#include "mgs5vr/stereo.hpp"
#include <cmath>

namespace mgs5vr {
std::optional<Pose> idroidBodyFromConnector(Pose connector) noexcept {
    if(!valid(connector))return {};
    const auto body=compose(connector,inverse(idroidConnectorInBody));
    return valid(body)?std::optional<Pose>{body}:std::nullopt;
}

std::optional<Pose> idroidGripContact(Pose grip,Pose aim,Pose fit) noexcept {
    if(!valid(grip)||!valid(aim)||!valid(fit))return {};
    const auto position=compose(grip,fit).position;
    const auto orientation=compose(Pose{aim.orientation,{}},Pose{fit.orientation,{}}).orientation;
    const Pose contact{orientation,position};
    return valid(contact)?std::optional<Pose>{contact}:std::nullopt;
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
    const float width=frame.controllers.idroidScreenWidth;
    if(!std::isfinite(width)||width<.20f||width>.60f)return {};
    // The native introductory hologram rises ABOVE the projector. Its lower
    // edge, not its centre, anchors relative to the emitter. Keeping the centre at socket
    // height put half the menu through the hand and left the light above it.
    // Size/angle pivot at that lower edge; the emitter remains on the housing.
    // Never substitute a newer raw grip or an anatomical palm for this mount.
    const auto bottom=compose(mount,Pose{frame.controllers.idroidScreenRotation,
        {offset.x,offset.y,frame.controllers.idroidScreenDepth}});
    const auto screen=compose(bottom,Pose{{},{0,width*9.f/32.f,0}});
    if(!valid(screen))return {};
    return IdroidPose{body,screen};
}

std::optional<IdroidRayHit> trackedIdroidRay(const HeadCameraSample& frame) noexcept{
    const auto idroid=trackedIdroidPose(frame);
    const auto& hand=frame.controllers.hands[1];
    if(!idroid||!hand.gripTracked||!valid(hand.grip)||!hand.aimTracked||!valid(hand.aim))return {};
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
