#include "mgs5vr/idroid_rig.hpp"
#include "mgs5vr/stereo.hpp"
#include "mgs5vr/idroid_attachment.hpp"
#include "mgs5vr/idroid_native_hold.hpp"
#include "mgs5vr/arm_ik.hpp"
#include <cmath>

namespace mgs5vr {
std::optional<Pose> nativeIdroidLeftWristFromGrip() noexcept {
    const auto palm=anatomicalGrip(Pose{},Vec3{.087f,.018f,.029f},Vec3{.078f,.012f,-.037f});
    return palm?std::optional<Pose>{inverse(*palm)}:std::nullopt;
}
std::optional<Pose> nativeIdroidLeftHandheldWrist(Pose grip,Pose fit) noexcept {
    if(!valid(grip)||!valid(fit))return {};
    const auto wristFromGrip=nativeIdroidLeftWristFromGrip();
    if(!wristFromGrip)return {};
    // The tracked palm owns orientation as well as position. Orienting the
    // handset from aim and solving backwards into the wrist twists the hand
    // whenever grip and aim differ. Retain the authored device-in-wrist mount.
    const auto wrist=compose(compose(grip,fit),*wristFromGrip);
    return valid(wrist)?std::optional<Pose>{wrist}:std::nullopt;
}
std::optional<std::array<float,16>> nativeIdroidAttachmentDelta(const std::array<float,16>& rootBone,
        const std::array<float,16>& bindingRoot,uint8_t bindingFlags,Pose desiredWorld) noexcept {
    const auto current=nativeAffinePose(rootBone);
    if(!current||!valid(desiredWorld))return {};
    Pose desired=desiredWorld;
    if(bindingFlags&1u){
        const auto root=nativeAffinePose(bindingRoot);
        if(!root)return {};
        desired=compose(inverse(*root),desiredWorld);
    }
    const auto delta=compose(desired,inverse(*current));
    if(!valid(delta))return {};
    const auto x=rotate(delta.orientation,{1,0,0}),y=rotate(delta.orientation,{0,1,0}),z=rotate(delta.orientation,{0,0,1});
    return std::array<float,16>{x.x,x.y,x.z,0,y.x,y.y,y.z,0,z.x,z.y,z.z,0,
                               delta.position.x,delta.position.y,delta.position.z,1};
}
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
    if(frame.controllers.handheldIdroidStowed||!frame.applied||!frame.stereoTracked||!frame.activation
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
