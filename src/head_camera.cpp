#include "mgs5vr/head_camera.hpp"
#include "mgs5vr/input_bridge.hpp"
#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <unordered_map>
#include <mutex>

namespace mgs5vr {
namespace {
float horizontalYaw(Quat orientation){
    const auto forward=rotate(orientation,{0,0,1});
    if(forward.x*forward.x+forward.z*forward.z>.0001f)return std::atan2(forward.x,forward.z);
    const auto right=rotate(orientation,{1,0,0});
    return std::atan2(-right.z,right.x);
}
Quat yawRotation(float yaw){return {0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)};}
Pose uprightOrigin(Pose pose){pose.orientation=yawRotation(horizontalYaw(pose.orientation));return pose;}
Quat inverseQuat(Quat q){
    const float norm2=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
    if(!std::isfinite(norm2)||norm2<.000001f)return {0,0,0,1};
    return {-q.x/norm2,-q.y/norm2,-q.z/norm2,q.w/norm2};
}
struct PlayerRootSample {
    uintptr_t owner{};
    Vec3 position{};
    Quat orientation{};
    uint64_t time{};
    uint64_t sequence{};
};
struct CameraStabilizationState {
    Vec3 headLocalOffset{};
    bool valid{};
    uint64_t time{};
};
std::mutex playerRootMutex;
std::unordered_map<uintptr_t,PlayerRootSample> playerRoots;
std::mutex stabilizationMutex;
std::unordered_map<const HeadCamera*,CameraStabilizationState> stabilization;
constexpr float headStabilizationTau=.14f;
constexpr float headBobDeadbandY=.075f;
constexpr float cameraBackwardOffset=.16f;
constexpr float cameraHeightOffset=.05f;
float smoothingAlpha(float dt,float tau){
    if(!std::isfinite(dt)||dt<=0||!std::isfinite(tau)||tau<=0)return 1;
    return 1-std::exp(-dt/tau);
}
void clearPlayerRootSamples(){
    std::lock_guard lock(playerRootMutex);
    playerRoots.clear();
}
void clearPlayerRootSample(uintptr_t camera){
    if(!camera)return;
    std::lock_guard lock(playerRootMutex);
    playerRoots.erase(camera);
}
bool getPlayerRootSample(uintptr_t camera,uintptr_t owner,uint64_t sequence,
                         uint64_t now,PlayerRootSample& out){
    if(!camera||!owner||!sequence)return false;
    std::lock_guard lock(playerRootMutex);
    const auto found=playerRoots.find(camera);
    if(found==playerRoots.end())return false;
    const auto& sample=found->second;
    if(sample.owner!=owner||sample.sequence!=sequence||now<sample.time||now-sample.time>150)return false;
    out=sample;
    return true;
}
CameraStabilizationState& stabilizationState(const HeadCamera* camera){
    std::lock_guard lock(stabilizationMutex);
    return stabilization[camera];
}
void resetStabilization(const HeadCamera* camera){
    std::lock_guard lock(stabilizationMutex);
    stabilization[camera]={};
}
}
std::optional<Pose> trackedListenerPose(const HeadCameraSample& frame,HeadCameraStatus status,uint64_t now){
    if(!status.enabled||!status.active||status.pending||status.suspended||!frame.applied||!frame.stereoTracked
        ||!frame.trackingSequence||frame.activation!=status.activation||now<frame.sampleTime
        ||now-frame.sampleTime>150||!valid(frame.nativePose))return {};
    return frame.nativePose;
}
std::optional<Vec3> playerHeadPosition(const std::array<float,16>& root,const std::array<float,16>& head){
    const auto rigid=[](const std::array<float,16>& m){
        for(float f:m)if(!std::isfinite(f))return false;
        if(std::abs(m[3])+std::abs(m[7])+std::abs(m[11])>0.001f||std::abs(m[15]-1)>0.001f)return false;
        const Vec3 x{m[0],m[1],m[2]},y{m[4],m[5],m[6]},z{m[8],m[9],m[10]};
        return std::abs(dot(x,x)-1)<0.003f&&std::abs(dot(y,y)-1)<0.003f&&std::abs(dot(z,z)-1)<0.003f
            &&std::abs(dot(x,y))<0.003f&&std::abs(dot(x,z))<0.003f&&std::abs(dot(y,z))<0.003f
            &&dot(Vec3{x.y*y.z-x.z*y.y,x.z*y.x-x.x*y.z,x.x*y.y-x.y*y.x},z)>0.997f;
    };
    if(!rigid(root)||!rigid(head))return {};
    const Vec3 local{head[12],head[13],head[14]};
    if(dot(local,local)>9||dot(local,local)<0.0025f)return {};
    return Vec3{root[12]+local.x*root[0]+local.y*root[4]+local.z*root[8],
                root[13]+local.x*root[1]+local.y*root[5]+local.z*root[9],
                root[14]+local.x*root[2]+local.y*root[6]+local.z*root[10]};
}
void HeadCamera::configure(bool enabled,float units,bool requirePlayerHead){
    if(!std::isfinite(units)||units<=0)throw std::invalid_argument("Camera scale must be finite and positive");
    std::lock_guard lock(mutex_);enabled_=enabled;units_=units;active_=pending_=awaitingPlayer_=false;camera_=playerOwner_=0;reason_=HeadCameraStop::none;
    requirePlayerHead_=requirePlayerHead;playerHeads_={};playerSequence_=ownerHeadTime_=0;suspended_=false;
    controllers_={};rig_={};nativeMenuOpen_=false;nativeIdroidOpen_=false;awaitingScene_=false;lastView_={};menuAnchored_=false;trackingEpoch_=0;
    lastTitleSource_={};avatarEditorBackdrop_={};lastTitleSourceValid_=avatarEditorBackdropValid_=false;
    scriptedCameraAnchor_={};scriptedCameraAnchorValid_=scriptedDemoActive_=false;
    snapYaw_=0;snapTranslation_={};recenterPending_=false;
    clearPlayerRootSamples();
    resetStabilization(this);
}
bool HeadCamera::publishPlayerHead(uintptr_t camera,uintptr_t owner,Pose sourceCamera,
                                  const std::array<float,16>& root,const std::array<float,16>& head,uint64_t time){
    const auto position=playerHeadPosition(root,head);
    if(!camera||!owner||!valid(sourceCamera)||!position)return false;
    const Vec3 rootPosition{root[12],root[13],root[14]};
    if(!std::isfinite(rootPosition.x)||!std::isfinite(rootPosition.y)||!std::isfinite(rootPosition.z))return false;
    const Vec3 rootForward{root[8],root[9],root[10]};
    if(!std::isfinite(rootForward.x)||!std::isfinite(rootForward.y)||!std::isfinite(rootForward.z)
       ||rootForward.x*rootForward.x+rootForward.z*rootForward.z<.0001f)return false;
    const auto rootYaw=yawRotation(std::atan2(rootForward.x,rootForward.z));
    const auto boom=sourceCamera.position-*position;
    if(dot(boom,boom)>144)return false;
    std::lock_guard lock(mutex_);
    if(!enabled_||!requirePlayerHead_)return false;
    if(camera==camera_&&owner==playerOwner_&&time>=ownerHeadTime_)ownerHeadTime_=time;
    auto found=std::find_if(playerHeads_.begin(),playerHeads_.end(),[&](const auto& p){return p.camera==camera;});
    if(found==playerHeads_.end())found=std::find_if(playerHeads_.begin(),playerHeads_.end(),[](const auto& p){return !p.camera;});
    if(found==playerHeads_.end())found=std::min_element(playerHeads_.begin(),playerHeads_.end(),[](const auto& a,const auto& b){return a.time<b.time;});
    if(found->camera==camera&&time<found->time)return false;
    const auto sequence=++playerSequence_;
    *found={camera,owner,sourceCamera,*position,time,sequence};
    {
        std::lock_guard rootLock(playerRootMutex);
        playerRoots[camera]={owner,rootPosition,rootYaw,time,sequence};
    }
    return true;
}
void HeadCamera::track(Pose head,bool tracked,uint64_t time){
    std::lock_guard lock(mutex_);
    stereoTracking_=false;
    controllers_={};
    tracking_=tracked&&valid(head)&&(!time_||time>=time_);
    if(tracking_){head_=head;time_=time;++sequence_;}
    else suspendLocked(HeadCameraStop::trackingLost);
}
void HeadCamera::trackStereo(Pose head,const std::array<EyeView,2>& views,bool tracked,uint64_t time,ControllerFrame controllers){
    std::lock_guard lock(mutex_);
    tracking_=tracked&&valid(head)&&(!time_||time>=time_);
    for(const auto& eye:views)tracking_=tracking_&&valid(eye.pose)&&valid(eye.fov);
    const auto separation=views[1].pose.position-views[0].pose.position;
    tracking_=tracking_&&dot(separation,separation)>0.0001f&&dot(separation,separation)<0.04f;
    stereoTracking_=tracking_;
    if(tracking_){
        const bool enteringAvatarEditor=controllers.avatarEditor&&!controllers_.avatarEditor;
        const bool leavingAvatarEditor=!controllers.avatarEditor&&controllers_.avatarEditor;
        if(enteringAvatarEditor){
            // AvatarEdit moves FOX to its separate character-preview camera.
            // Keep the last actual title/hospital source camera as the scene
            // behind the native setup UI, then follow headset motion from it.
            avatarEditorBackdropValid_=lastTitleSourceValid_;
            if(avatarEditorBackdropValid_)avatarEditorBackdrop_=lastTitleSource_;
        }else if(leavingAvatarEditor){
            avatarEditorBackdropValid_=false;avatarEditorBackdrop_={};
        }
        const bool authoredCameraChanged=controllers.authoredCamera!=controllers_.authoredCamera;
        const bool scriptedDemoChanged=controllers.scriptedDemo!=scriptedDemoActive_;
        if(scriptedDemoChanged){
            scriptedDemoActive_=controllers.scriptedDemo;
            scriptedCameraAnchorValid_=false;
        }
        if(active_&&(authoredCameraChanged||scriptedDemoChanged||controllers.avatarEditor!=controllers_.avatarEditor)){
            // A cinematic/gameplay handoff cannot reuse the preceding rig or
            // eye images even when FOX keeps the same camera object. Preserve
            // the last source view across AvatarEdit->demo so a new authored
            // shot camera can be adopted after its old source retires.
            rig_={};if(authoredCameraChanged)lastView_={};++activation_;resetStabilization(this);
        }
        if(trackingEpoch_&&controllers.referenceEpoch&&controllers.referenceEpoch!=trackingEpoch_&&(active_||pending_||awaitingPlayer_)){
            if(active_&&lastView_.applied){
                // Rebase the physical tracking origin without moving the native
                // viewpoint or the panel already anchored in this world.
                // Gameplay resumes in this new reference space too: carrying
                // the old origin through menu close would teleport the view.
                origin_=uprightOrigin(compose(compose(head,inverse(head_)),origin_));
                if(nativeMenuOpen_&&menuAnchored_){menuNative_=lastView_.nativePose;menuHead_=head;}
                rig_={};++activation_;resetStabilization(this);
            }else{
                if(!awaitingPlayer_){active_=false;pending_=true;}
                camera_=playerOwner_=0;
                resetStabilization(this);
            }
        }
        for(auto& hand:controllers.hands){
            hand.gripTracked=controllers.predictedXrTime>0&&controllers.referenceEpoch&&hand.gripTracked&&valid(hand.grip);
            hand.aimTracked=controllers.predictedXrTime>0&&controllers.referenceEpoch&&hand.aimTracked&&valid(hand.aim);
            if(!hand.gripTracked)hand.grip={};
            if(!hand.aimTracked)hand.aim={};
        }
        head_=head;views_=views;controllers_=controllers;trackingEpoch_=controllers.referenceEpoch;time_=time;++sequence_;
    }else {controllers_={};suspendLocked(HeadCameraStop::trackingLost);}
}
void HeadCamera::toggle(){
    std::lock_guard lock(mutex_);
    if(!enabled_)return;
    if(active_||pending_||awaitingPlayer_)cancelLocked(HeadCameraStop::manual);
    else {pending_=true;reason_=HeadCameraStop::none;}
}
void HeadCamera::recenter(){
    std::lock_guard lock(mutex_);
    if(enabled_&&(active_||pending_||awaitingPlayer_)){recenterPending_=true;rig_={};resetStabilization(this);}
}
void HeadCamera::setNativeMenuOpen(bool open,bool idroid){
    std::lock_guard lock(mutex_);
    const auto nextIdroid=open&&idroid;
    if(open==nativeMenuOpen_&&nextIdroid==nativeIdroidOpen_)return;
    const bool retainMenuAnchor=open&&nativeMenuOpen_&&menuAnchored_;
    nativeMenuOpen_=open;nativeIdroidOpen_=nextIdroid;rig_={};
    if(open){
        menuAnchored_=retainMenuAnchor||(active_&&!suspended_&&lastView_.applied&&lastView_.activation==activation_);
        if(menuAnchored_&&!retainMenuAnchor){
            menuNative_=lastView_.nativePose;menuHead_=lastView_.headPose;
            const auto head=uprightOrigin(nativeTrackedPose(menuNative_,menuHead_,menuHead_));
            const float tilt=controllers_.menuQuadTilt*.00872664626f;
            menuPanel_=compose(head,Pose{{std::sin(tilt),0,0,std::cos(tilt)},
                {0,-.20f,-controllers_.menuQuadDistance}});
        }
    }else{
        menuAnchored_=false;
        resetStabilization(this);
        // Keep VR active. resolveLocked still requires a fresh publication of
        // this same player before adopting the gameplay camera again.
    }
}
void HeadCamera::cancelLocked(HeadCameraStop reason){
    if(active_||pending_||awaitingPlayer_){reason_=reason;++cancellations_;}
    const auto oldCamera=camera_;
    active_=pending_=suspended_=awaitingPlayer_=awaitingScene_=false;camera_=playerOwner_=0;rig_={};lastView_={};menuAnchored_=false;
    avatarEditorBackdropValid_=false;avatarEditorBackdrop_={};
    scriptedCameraAnchorValid_=false;scriptedDemoActive_=false;scriptedCameraAnchor_={};
    snapYaw_=0;snapTranslation_={};recenterPending_=false;
    resetStabilization(this);
    clearPlayerRootSample(oldCamera);
}
void HeadCamera::awaitPlayerLocked(){
    // A native menu/cinematic has no matching player-head publication. Keep
    // native pixels and normal menu input available, but remember the user's
    // VR request. Resume only the exact camera/owner previously accepted.
    if(active_||pending_||awaitingPlayer_){
        awaitingPlayer_=true;active_=pending_=suspended_=false;rig_={};
        resetStabilization(this);
        reason_=HeadCameraStop::playerHeadUnavailable;
    }
}
void HeadCamera::suspendLocked(HeadCameraStop reason){
    if(active_||pending_){suspended_=true;reason_=reason;}
}
void HeadCamera::cancel(HeadCameraStop reason){std::lock_guard lock(mutex_);cancelLocked(reason);}
void HeadCamera::awaitScene(){
    std::lock_guard lock(mutex_);
    if(active_&&!awaitingScene_){awaitingScene_=true;rig_={};resetStabilization(this);}
}
bool HeadCamera::available() const {std::lock_guard lock(mutex_);return enabled_;}
bool HeadCamera::active() const {std::lock_guard lock(mutex_);return active_&&!awaitingScene_;}
HeadCameraStatus HeadCamera::status() const {std::lock_guard lock(mutex_);return {enabled_,active_&&!awaitingScene_,pending_,awaitingScene_?HeadCameraStop::sceneUnavailable:reason_,cancellations_,activation_,suspended_,awaitingPlayer_||awaitingScene_,nativeMenuOpen_,nativeIdroidOpen_};}
HeadCameraSample HeadCamera::resolve(uintptr_t camera,Pose nativePose,uint64_t time){
    std::lock_guard lock(mutex_);
    return resolveLocked(camera,nativePose,time);
}
HeadCameraSample HeadCamera::resolveCurrent(uintptr_t camera,Pose nativePose){
    std::lock_guard lock(mutex_);
    return resolveLocked(camera,nativePose,steadyMilliseconds());
}
HeadCameraSample HeadCamera::resolveCurrentForRig(uintptr_t camera,Pose nativePose){
    std::lock_guard lock(mutex_);
    return resolveLocked(camera,nativePose,steadyMilliseconds(),false);
}
bool HeadCamera::publishRigFrame(uintptr_t camera,uintptr_t owner,Pose sourceCamera,HeadCameraSample frame){
    if(!owner||!frame.applied||!valid(sourceCamera)||!valid(frame.nativePose))return false;
    std::lock_guard lock(mutex_);
    if(!active_||camera_!=camera||frame.activation!=activation_||frame.trackingSequence>sequence_
       ||frame.controllers.referenceEpoch!=controllers_.referenceEpoch)return false;
    frame.rigSequence=++rigSequence_;rig_={camera,owner,sourceCamera,frame};return true;
}
std::optional<Pose> HeadCamera::openingTrackingOrigin(uint64_t now) const{
    std::lock_guard lock(mutex_);
    const auto& frame=rig_.sample;
    // The title camera is already anchored before Press Start. If the player
    // turns while entering the menu, the first rack must keep that cabin
    // anchor rather than adopting the newer head direction.
    if(active_&&controllers_.frontEnd&&!frame.controllers.openingWorldAnchored
       &&tracking_&&now>=time_&&now-time_<=150)return origin_;
    if(!active_||frame.activation!=activation_||!frame.applied
       ||!frame.controllers.openingWorldAnchored||now<frame.sampleTime||now-frame.sampleTime>150
       ||frame.controllers.referenceEpoch!=controllers_.referenceEpoch)return {};
    // Invert the SAME native/tracking join used by rendering. Collision moves
    // the virtual head, not the rack; its input volume must use that correction.
    const auto nativeFromTracking=compose(frame.nativePose,Pose{{0,1,0,0},{}});
    return compose(frame.headPose,compose(inverse(nativeFromTracking),frame.controllers.openingWorldOrigin));
}
HeadCameraSample HeadCamera::resolveLocked(uintptr_t camera,Pose nativePose,uint64_t time,bool useRig){
    const auto sourceCamera=nativePose;
    HeadCameraSample result{nativePose,head_,sequence_,activation_,false};
    result.views=views_;result.sampleTime=time_;result.stereoTracked=stereoTracking_;
    if(!enabled_||!camera||!valid(nativePose))return result;
    if(!tracking_){suspendLocked(HeadCameraStop::trackingLost);return result;}
    if(time<time_){cancelLocked(HeadCameraStop::clockMismatch);return result;}
    if(time-time_>150){suspendLocked(HeadCameraStop::staleTracking);return result;}
    const bool authoredScene=controllers_.authoredCamera;
    if(controllers_.avatarEditor&&avatarEditorBackdropValid_){
        nativePose=avatarEditorBackdrop_;
        result.nativePose=nativePose;
    }
    // Ignore secondary publications while the accepted camera is still live.
    // Native demos replace the player camera with a separate shot camera. Once
    // the old primary has stopped publishing, follow that verified render
    // publication with a new generation instead of freezing its last image.
    if(authoredScene&&camera_&&camera_!=camera){
        if(controllers_.avatarEditor&&avatarEditorBackdropValid_){
            if(!useRig)return result;
            camera_=camera;playerOwner_=0;rig_={};lastView_={};++activation_;
        }else{
            if(!useRig||!lastView_.applied||time<lastView_.sampleTime||time-lastView_.sampleTime<=500)return result;
            camera_=camera;playerOwner_=0;rig_={};lastView_={};++activation_;
        }
    }
    // Loading can still publish its decorative character/camera. It must not
    // immediately reactivate stereo and cover the native confirmation screen.
    if(controllers_.loading){awaitPlayerLocked();return result;}
    const bool spatialMenu=nativeMenuOpen_&&menuAnchored_&&active_;
    if(nativeMenuOpen_&&!spatialMenu){awaitPlayerLocked();return result;}
    if(spatialMenu){
        if(camera!=camera_)return result;
        // Default menus pause the native world but keep fresh stereo head
        // tracking. Only the opt-in handheld iDroid follows live locomotion.
        if(!nativeIdroidOpen_||!controllers_.handheldMenus)nativePose=menuNative_;
        result.menuOpen=true;result.menuIdroid=nativeIdroidOpen_;result.menuPanel=menuPanel_;
        result.playerOwner=playerOwner_;result.playerHead=lastView_.playerHead;
        result.playerSequence=lastView_.playerSequence;
        // Paused native menus keep the last skin on screen. Keep its palm
        // attachment too; a raw controller fallback would detach the display
        // from that still-visible hand. A fresh skin replaces these below.
        result.renderedPalms=lastView_.renderedPalms;
        result.renderedPalmTracked=lastView_.renderedPalmTracked;
        result.wristPanel=lastView_.wristPanel;
        result.wristPanelTracked=lastView_.wristPanelTracked;
    }else if(authoredScene&&awaitingPlayer_&&(camera_||controllers_.scriptedDemo)){
        // This is the verified primary render publication of a native scripted
        // shot. Its camera remains valid when the demo stops publishing the
        // player's skeleton. Do not attach that shot to a decorative actor.
        if(camera_&&camera_!=camera)return result;
        if(!camera_){
            camera_=camera;origin_=uprightOrigin(head_);frontEndOrigin_=uprightOrigin(sourceCamera);
            frontEndPanel_=compose(nativeTrackedPose(frontEndOrigin_,head_,head_),Pose{{},{0,-.05f,-1.3f}});
        }
        playerOwner_=0;rig_={};awaitingPlayer_=pending_=false;active_=true;++activation_;
    }else if(requirePlayerHead_&&(!authoredScene||(!camera_&&!controllers_.scriptedDemo))
       &&(pending_||active_||awaitingPlayer_)){
        const auto same=[](Pose a,Pose b){
            return a.position.x==b.position.x&&a.position.y==b.position.y&&a.position.z==b.position.z
                &&a.orientation.x==b.orientation.x&&a.orientation.y==b.orientation.y
                &&a.orientation.z==b.orientation.z&&a.orientation.w==b.orientation.w;
        };
        const auto found=std::find_if(playerHeads_.begin(),playerHeads_.end(),[&](const auto& p){
            return p.camera==camera&&p.sequence&&time>=p.time&&time-p.time<=150&&same(p.sourceCamera,nativePose);
        });
        if(found==playerHeads_.end()){
            // A decorative/retired camera has no authority to suspend the
            // accepted player, even when its own head publication is absent.
            if(camera_&&camera_!=camera)return result;
            const bool interrupted=active_&&camera_==camera&&std::any_of(
                playerHeads_.begin(),playerHeads_.end(),[&](const auto& p){
                    return p.camera==camera&&p.owner==playerOwner_&&p.sequence
                        &&time>=p.time&&time-p.time<=1000&&same(p.sourceCamera,nativePose);
                });
            if(interrupted){
                // Reject this stale skin without changing activation/input
                // context. Only an already accepted stereo pair may cover
                // the gap; the next fresh skin resumes this same session.
                rig_={};suspendLocked(HeadCameraStop::playerHeadUnavailable);
            }else awaitPlayerLocked();
            return result;
        }
        if(camera_==camera&&!playerOwner_){
            // The scripted shot had no player owner. A fresh matching player
            // publication can take over immediately when control returns.
            playerOwner_=found->owner;rig_={};lastView_={};++activation_;
            resetStabilization(this);
        }
        if(camera_&&(camera_!=camera||playerOwner_!=found->owner)){
            // ACC -> field / mission restart creates a new verified player.
            // Never borrow another live owner's camera. After the previous
            // owner has stopped publishing for a full second, adopt only a
            // fresh player-head publication matching this rendered camera.
            // Retained ACC eyes, rig and menu anchors must not survive travel.
            if(time<ownerHeadTime_||time-ownerHeadTime_<=1000)return result;
            camera_=playerOwner_=0;rig_={};lastView_={};menuAnchored_=false;
            active_=pending_=false;awaitingPlayer_=true;awaitingScene_=false;
            snapYaw_=controllers_.snapYaw;snapTranslation_={};recenterPending_=false;
            resetStabilization(this);
            clearPlayerRootSample(camera);
        }
        if(awaitingPlayer_){
            if((camera_&&camera_!=camera)||(playerOwner_&&playerOwner_!=found->owner))return result;
            if(!camera_){
                origin_=uprightOrigin(head_);
                frontEndOrigin_=uprightOrigin(sourceCamera);
                frontEndPanel_=compose(nativeTrackedPose(frontEndOrigin_,head_,head_),Pose{{},{0,-.05f,-1.3f}});
            }
            camera_=camera;playerOwner_=found->owner;awaitingPlayer_=false;active_=true;
            ++activation_; // No eye image from before the menu may be reused.
        }
        // A scripted character can be scenery rather than the player's
        // viewpoint. In the hospital-bed setup flow, keep the authored camera
        // so the patient and room remain visible behind the native UI quad.
        if(!controllers_.frontEnd&&!controllers_.avatarEditor&&!controllers_.cabinPlay)nativePose.position=found->position;
        ownerHeadTime_=found->time;
        result.playerSequence=found->sequence;result.playerOwner=found->owner;result.playerHead=found->position;
    }
    if(pending_){
        camera_=camera;playerOwner_=result.playerOwner;origin_=uprightOrigin(head_);
        frontEndOrigin_=uprightOrigin(nativePose);
        frontEndPanel_=compose(nativeTrackedPose(frontEndOrigin_,head_,head_),Pose{{},{0,-.05f,-1.3f}});
        pending_=false;active_=true;++activation_;snapTranslation_={};snapYaw_=controllers_.snapYaw;
    }
    if(!active_)return result;
    if(camera_!=camera){cancelLocked(HeadCameraStop::cameraChanged);return result;}
    if(awaitingScene_){awaitingScene_=false;++activation_;}
    if(controllers_.frontEnd)result.menuPanel=controllers_.authoredCamera
        ?compose(nativeTrackedPose(nativePose,head_,head_),Pose{{},{0,-.05f,-1.3f}}):frontEndPanel_;
    else if(controllers_.avatarEditor)
        result.menuPanel=compose(nativeTrackedPose(nativePose,head_,head_),Pose{{},{0,-.05f,-1.3f}});
    // A scripted demo animates its authored camera in world space. Keep the
    // first accepted pose as the viewer's world anchor for the full demo, even
    // across native camera cuts. The scene and actors remain native 3D, while
    // the tracked HMD still moves and turns relative to this fixed viewpoint.
    // Native camera motion resumes as soon as the demo state ends.
    if(controllers_.scriptedDemo){
        if(!scriptedCameraAnchorValid_){scriptedCameraAnchor_=nativePose;scriptedCameraAnchorValid_=true;}
        nativePose=scriptedCameraAnchor_;
    }
    // The native camera can look down, lean, recoil or bank. Its yaw supplies
    // gameplay heading; gravity and physical HMD pitch/roll supply the VR view.
    // A full native/activation rotation tilts the tracking space when turning.
    if(!spatialMenu){
        if(!controllers_.authoredCamera)
            nativePose=(controllers_.frontEnd||controllers_.cabinPlay)?frontEndOrigin_:uprightOrigin(nativePose);
        if(recenterPending_){
            const float baseYaw=horizontalYaw(nativePose.orientation)+controllers_.snapYaw;
            const float facing=lastView_.applied?horizontalYaw(lastView_.nativePose.orientation):baseYaw;
            origin_={yawRotation(horizontalYaw(head_.orientation)-facing+baseYaw),head_.position};
            snapTranslation_={};snapYaw_=controllers_.snapYaw;recenterPending_=false;rig_={};++activation_;
        }
    }
    if(useRig&&!authoredScene&&rig_.camera){
        const auto& s=rig_.sample;const auto& p=rig_.sourceCamera;
        const bool same=p.position.x==sourceCamera.position.x&&p.position.y==sourceCamera.position.y&&p.position.z==sourceCamera.position.z
            &&p.orientation.x==sourceCamera.orientation.x&&p.orientation.y==sourceCamera.orientation.y
            &&p.orientation.z==sourceCamera.orientation.z&&p.orientation.w==sourceCamera.orientation.w;
        const bool sameOwner=rig_.camera==camera&&rig_.owner==result.playerOwner&&s.activation==activation_;
        const bool freshRig=sameOwner&&same&&time>=s.sampleTime&&time-s.sampleTime<=150;
        if(freshRig){
            const auto playerPublication=result.playerSequence;
            result=s;result.playerSequence=playerPublication;suspended_=false;reason_=HeadCameraStop::none;lastView_=result;return result;
        }
        // Native Pause/Help stops skin updates while its camera and UI keep
        // drawing. A cached rig must not poison every subsequent menu frame
        // after 150 ms. Resolve fresh tracked eyes from this accepted menu
        // anchor without relabeling the old hand publication as current.
        if(!spatialMenu||!sameOwner){suspendLocked(HeadCameraStop::rigFrameMismatch);return result;}
        rig_={};
    }
    suspended_=false;reason_=HeadCameraStop::none;
    if(!spatialMenu&&!controllers_.frontEnd&&!controllers_.avatarEditor&&!controllers_.cabinPlay
       &&requirePlayerHead_&&result.playerSequence){
        PlayerRootSample rootSample;
        if(getPlayerRootSample(camera,result.playerOwner,result.playerSequence,time,rootSample)){
            auto& state=stabilizationState(this);
            float dt=0;
            if(state.valid&&time>=state.time)dt=static_cast<float>(time-state.time)/1000.f;
            const Vec3 worldHeadOffset{
                result.playerHead.x-rootSample.position.x,
                result.playerHead.y-rootSample.position.y,
                result.playerHead.z-rootSample.position.z
            };
            const auto targetHeadLocalOffset=rotate(inverseQuat(rootSample.orientation),worldHeadOffset);
            if(!state.valid||dt<=0||dt>.25f){
                state.headLocalOffset=targetHeadLocalOffset;
                state.valid=true;
            }else{
                const float deltaY=targetHeadLocalOffset.y-state.headLocalOffset.y;
                if(std::abs(deltaY)>headBobDeadbandY)
                    state.headLocalOffset.y+=deltaY*smoothingAlpha(dt,headStabilizationTau);
                // Filter animation bob without freezing the old stance/mount
                // offset for the rest of the session. The anchor must follow
                // a seated, prone or otherwise repositioned native head.
                const float alpha=smoothingAlpha(dt,headStabilizationTau);
                state.headLocalOffset.x+=(targetHeadLocalOffset.x-state.headLocalOffset.x)*alpha;
                state.headLocalOffset.z+=(targetHeadLocalOffset.z-state.headLocalOffset.z)*alpha;
            }
            state.time=time;
            auto cameraAnchor=rootSample.position+rotate(rootSample.orientation,state.headLocalOffset);
            const auto backward=rotate(rootSample.orientation,{0,0,-cameraBackwardOffset});
            cameraAnchor=cameraAnchor+backward;
            cameraAnchor.y+=cameraHeightOffset;
            nativePose.position=cameraAnchor;
        }
    }
    // FOX's camera-local forward/right candidates are +Z/-X. The explicit
    // 180-degree basis rotation keeps handedness intact; validate in live motion.
    if(!spatialMenu&&!controllers_.authoredCamera)nativePose.position.y+=controllers_.playerHeightOffset;
    const Pose basis{{0,1,0,0},{}};
    auto relative=compose(inverse(spatialMenu?menuHead_:origin_),head_);
    relative.position=relative.position*units_;
    const auto nativeDelta=compose(compose(basis,relative),inverse(basis));
    result.nativePose=compose(nativePose,nativeDelta);
    if(!spatialMenu){
        const float turn=std::isfinite(controllers_.snapYaw)?controllers_.snapYaw:0;
        const Quat rotation{0,std::sin(turn*.5f),0,std::cos(turn*.5f)};
        // Keep the turn-pivot translation in the native heading's frame.
        // A world-space accumulator survives native smooth/vehicle yaw in the
        // old direction and leaves the player center behind the viewpoint.
        const auto offset=nativeDelta.position;
        if(turn!=snapYaw_){
            const Quat previous{0,std::sin(snapYaw_*.5f),0,std::cos(snapYaw_*.5f)};
            // Turn around the current physical head, including a roomscale
            // lean. The snap must not orbit the head around the tracking origin.
            snapTranslation_=snapTranslation_+rotate(previous,offset)-rotate(rotation,offset);
            snapYaw_=turn;
        }
        result.nativePose.position=nativePose.position+rotate(nativePose.orientation,rotate(rotation,offset)+snapTranslation_);
        result.nativePose.orientation=compose(Pose{rotation,{}},Pose{result.nativePose.orientation,{}}).orientation;
    }
    // Native and runtime quaternions are accepted with a small norm tolerance.
    // Normalize after composition: the native inverse builder assumes a rigid
    // rotation, and even tiny norm errors are amplified by kilometer coordinates.
    auto& q=result.nativePose.orientation;
    const double length=std::sqrt(static_cast<double>(q.x)*q.x+static_cast<double>(q.y)*q.y
        +static_cast<double>(q.z)*q.z+static_cast<double>(q.w)*q.w);
    if(!std::isfinite(length)||length<0.5)return result;
    q={static_cast<float>(q.x/length),static_cast<float>(q.y/length),static_cast<float>(q.z/length),static_cast<float>(q.w/length)};
    result.activation=activation_;result.applied=valid(result.nativePose);
    if(result.applied){
        result.controllers=controllers_;
        // Menu state can arrive between input publications. The iDroid owns
        // the hand immediately, even if the last input still held an optic.
        if(result.menuOpen)result.controllers.optic={};
        // Skin preparation can keep evaluating a retired player camera while
        // a demo renders from another camera. Only the render publication may
        // advance the accepted view used for camera liveness and handoffs.
        if(useRig){
            lastView_=result;
            if(controllers_.frontEnd&&!controllers_.loading){lastTitleSource_=sourceCamera;lastTitleSourceValid_=true;}
        }
    }
    return result;
}
HeadCamera& headCamera(){static auto* instance=new HeadCamera;return *instance;}
}
