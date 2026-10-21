#include "mgs5vr/head_camera.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/arm_ik.hpp"
#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <unordered_map>
#include <mutex>

namespace mgs5vr {
std::optional<float> physicalFloorHeightOffset(float headY,float floorY,float units,float standingEyeHeight){
    const float height=(headY-floorY)*units;
    if(!std::isfinite(height)||!std::isfinite(standingEyeHeight)||!std::isfinite(units)||units<=0
       ||height<=.35f||height>=2.4f||standingEyeHeight<=1.f||standingEyeHeight>=2.f)return {};
    return height-standingEyeHeight;
}
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
    Vec3 eyePosition{};
    uintptr_t eyeModel{}; // Original owned skin, never the cached camera HEAD basis.
    Quat anatomicalHeading{};
    bool bodyFramesTracked{};
};
struct CameraStabilizationState {
    Vec3 headLocalOffset{};
    float standingEyeHeight{};
    bool valid{};
    uint64_t time{};
    bool followingPosture{};
    Quat heading{};
    PlayerPosture posture{PlayerPosture::unknown};
    uint64_t postureUntil{};
    Vec3 postureSum{};
    float postureWeight{};
    bool anatomical{};
    uintptr_t anatomicalModel{};
};
std::mutex playerRootMutex;
std::unordered_map<uintptr_t,PlayerRootSample> playerRoots;
std::mutex stabilizationMutex;
std::unordered_map<const HeadCamera*,CameraStabilizationState> stabilization;
// Coco's tested head/shoulder fit and bob suppression settings.
constexpr float headStabilizationTau=.20f;
constexpr float headBobDeadbandY=.15f;
constexpr float cameraBackwardOffset=.35f;
constexpr float cameraHeightOffset=.25f;
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
    controllers_={};rig_={};nativeMenuOpen_=false;nativeIdroidOpen_=false;nativeIdroidPaused_=false;awaitingScene_=false;lastView_={};cameraRenderTime_=0;menuAnchored_=false;trackingEpoch_=0;
    cameraFreezeAnchored_=false;
    seatedHeadY_=0;seatedMode_=false;fieldPostureUntil_=0;
    menuGeneration_=0;
    staleDemoRecoveryDwell_.reset();
    lastTitleSource_={};avatarEditorBackdrop_={};lastTitleSourceValid_=avatarEditorBackdropValid_=false;
    scriptedDemoActive_=false;cinematicFollow_={};sceneTransitionActivation_=0;
    headLookLessonKey_=0;
    snapYaw_=0;snapTranslation_={};recenterPending_=false;driverView_={};vehicleHeading_.reset();
    vehicleHeadingCamera_=vehicleHeadingOwner_=vehicleHeadingEpoch_=0;
    clearPlayerRootSamples();
    resetStabilization(this);
}
Pose HeadCamera::followCinematicLocked(Pose nativePose,uint64_t time,bool advance){
    auto& s=cinematicFollow_;
    const float rawYaw=horizontalYaw(nativePose.orientation);
    if(!s.valid||s.camera!=camera_||s.activation!=activation_||time<s.time||time-s.time>500){
        s={uprightOrigin(nativePose),nativePose.position,rawYaw,camera_,time,activation_,true};
    }else if(advance&&time>s.time){
        const float dt=std::min(.1f,float(time-s.time)*.001f);
        const auto jump=nativePose.position-s.rawPosition;
        const auto wrap=[](float a){return std::remainder(a,6.283185307f);};
        float yaw=horizontalYaw(s.pose.orientation);
        if(dot(jump,jump)>16.f||std::abs(wrap(rawYaw-s.rawYaw))>.9f){
            s.pose.position=nativePose.position;yaw=rawYaw;s.freeOffset={};
        }else{
            s.pose.position=s.pose.position+(nativePose.position-s.pose.position)*smoothingAlpha(dt,.25f);
            const float requested=wrap(rawYaw-yaw)*smoothingAlpha(dt,.45f);
            yaw=wrap(yaw+std::clamp(requested,-1.2f*dt,1.2f*dt));
        }
        s.pose.orientation=yawRotation(yaw);s.rawPosition=nativePose.position;s.rawYaw=rawYaw;s.time=time;
        if(controllers_.cinematicFreeMove){
            auto move=controllers_.cinematicMove;
            for(auto& axis:move)axis=std::isfinite(axis)&&std::abs(axis)>.18f?std::clamp(axis,-1.f,1.f):0.f;
            const float magnitude=std::hypot(move[0],move[1]);if(magnitude>1)for(auto& axis:move)axis/=magnitude;
            const auto facing=yawRotation(yaw+controllers_.snapYaw+horizontalYaw(head_.orientation)-horizontalYaw(origin_.orientation));
            s.freeOffset=s.freeOffset+rotate(facing,{-move[0],0,move[1]})*(dt*.85f);
        }else s.freeOffset={};
    }
    auto result=s.pose;result.position=result.position+s.freeOffset;return result;
}
bool HeadCamera::publishPlayerHead(uintptr_t camera,uintptr_t owner,Pose sourceCamera,
                                  const std::array<float,16>& root,const std::array<float,16>& head,uint64_t time,
                                  bool verifiedRocketArmGuidance){
    const auto reject=[&]{
        std::lock_guard lock(mutex_);
        if(camera==camera_&&(!playerOwner_||owner==playerOwner_))staleDemoRecoveryDwell_.reset();
        return false;
    };
    const auto position=playerHeadPosition(root,head);
    if(!camera||!owner||!valid(sourceCamera)||!position)return reject();
    auto eyeHead=head;
    eyeHead[12]+=nativeEyeFromHead.y*head[4]+nativeEyeFromHead.z*head[8];
    eyeHead[13]+=nativeEyeFromHead.y*head[5]+nativeEyeFromHead.z*head[9];
    eyeHead[14]+=nativeEyeFromHead.y*head[6]+nativeEyeFromHead.z*head[10];
    const auto eyePosition=playerHeadPosition(root,eyeHead);
    if(!eyePosition)return reject();
    const Vec3 rootPosition{root[12],root[13],root[14]};
    if(!std::isfinite(rootPosition.x)||!std::isfinite(rootPosition.y)||!std::isfinite(rootPosition.z))return reject();
    const Vec3 rootForward{root[8],root[9],root[10]};
    if(!std::isfinite(rootForward.x)||!std::isfinite(rootForward.y)||!std::isfinite(rootForward.z)
       ||rootForward.x*rootForward.x+rootForward.z*rootForward.z<.0001f)return reject();
    const auto rootYaw=yawRotation(std::atan2(rootForward.x,rootForward.z));
    const auto boom=sourceCamera.position-*position;
    if(dot(boom,boom)>144&&!verifiedRocketArmGuidance)return reject();
    std::lock_guard lock(mutex_);
    if(!enabled_||!requirePlayerHead_)return false;
    if(camera==camera_&&owner==playerOwner_&&time>=ownerHeadTime_)ownerHeadTime_=time;
    auto found=std::find_if(playerHeads_.begin(),playerHeads_.end(),[&](const auto& p){return p.camera==camera;});
    if(found==playerHeads_.end())found=std::find_if(playerHeads_.begin(),playerHeads_.end(),[](const auto& p){return !p.camera;});
    if(found==playerHeads_.end())found=std::min_element(playerHeads_.begin(),playerHeads_.end(),[](const auto& a,const auto& b){return a.time<b.time;});
    if(found->camera==camera&&time<found->time){
        if(camera==camera_)staleDemoRecoveryDwell_.reset();
        return false;
    }
    const auto sequence=++playerSequence_;
    *found={camera,owner,sourceCamera,*position,time,sequence,verifiedRocketArmGuidance,rootYaw};
    {
        std::lock_guard rootLock(playerRootMutex);
        playerRoots[camera]={owner,rootPosition,rootYaw,time,sequence,*eyePosition};
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
        if(controllers.seatedMode&&!seatedMode_)seatedHeadY_=head.position.y;
        else if(controllers.seatedMode&&trackingEpoch_&&controllers.referenceEpoch!=trackingEpoch_)
            seatedHeadY_+=head.position.y-head_.position.y;
        seatedMode_=controllers.seatedMode;
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
        scriptedDemoActive_=controllers.scriptedDemo;
        if(active_&&(authoredCameraChanged||scriptedDemoChanged||controllers.avatarEditor!=controllers_.avatarEditor)){
            headLookLessonKey_=0;
            // A cinematic/gameplay handoff cannot reuse the preceding rig or
            // eye images even when FOX keeps the same camera object. Preserve
            // the last source view across AvatarEdit->demo so a new authored
            // shot camera can be adopted after its old source retires.
            rig_={};if(authoredCameraChanged)lastView_={};++activation_;activationCause_=1;resetStabilization(this);
            if(authoredCameraChanged||scriptedDemoChanged){sceneTransitionActivation_=activation_;sceneTransitionAt_=time;}
        }
        if(trackingEpoch_&&controllers.referenceEpoch&&controllers.referenceEpoch!=trackingEpoch_&&(active_||pending_||awaitingPlayer_)){
            headLookLessonKey_=0;
            if(active_&&lastView_.applied){
                // Rebase the physical tracking origin without moving the native
                // viewpoint or the panel already anchored in this world.
                // Gameplay resumes in this new reference space too: carrying
                // the old origin through menu close would teleport the view.
                origin_=uprightOrigin(compose(compose(head,inverse(head_)),origin_));
                if(nativeMenuOpen_&&menuAnchored_){menuNative_=lastView_.nativePose;menuHead_=head;}
                // LOCAL rebasing changes tracking coordinates, not the native
                // actor's posture or measured anatomical standing height.
                rig_={};++activation_;activationCause_=2;
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
        const bool freezeAnchor=controllers.cameraWorldFrozen&&active_&&lastView_.applied&&playerOwner_
            &&controllers.presentationFocused&&controllers.referenceEpoch==trackingEpoch_
            &&!controllers.frontEnd&&!controllers.loading&&!controllers.avatarEditor&&!controllers.authoredCamera
            &&!nativeMenuOpen_;
        if(freezeAnchor&&!cameraFreezeAnchored_){
            cameraFreezeNative_=lastView_.nativePose;cameraFreezeHead_=lastView_.headPose;cameraFreezeAnchored_=true;rig_={};
        }else if(!freezeAnchor&&cameraFreezeAnchored_){
            cameraFreezeAnchored_=false;rig_={};resetStabilization(this);
        }
        if(controllers_.cabinTransit&&!controllers.cabinTransit)
            fieldPostureUntil_=time+900;
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
    if(enabled_&&(active_||pending_||awaitingPlayer_)){
        recenterPending_=true;rig_={};
        // Recenter heading/position without turning a temporary crouch into
        // a permanent seated height offset. Mode entry calibrates seated height.
        // Do not reseed the native eye anchor from a transient crouch/gait
        // frame merely because the player recentered their headset.
    }
}
void HeadCamera::setNativeMenuOpen(bool open,bool idroid,bool pause,bool idroidPaused){
    std::lock_guard lock(mutex_);
    // Native Help can cover an iDroid terminal without closing it. Its pause
    // stops player skin jobs, so it must own the spatial menu/camera policy.
    // Treating that overlap as a moving handheld menu waits forever for the
    // very player-head publication the guide has paused.
    const auto nextIdroid=open&&idroid&&!pause;
    const auto nextPaused=nextIdroid&&idroidPaused;
    if(open==nativeMenuOpen_&&nextIdroid==nativeIdroidOpen_&&nextPaused==nativeIdroidPaused_)return;
    const bool retainMenuAnchor=open&&nativeMenuOpen_&&menuAnchored_;
    nativeMenuOpen_=open;nativeIdroidOpen_=nextIdroid;nativeIdroidPaused_=nextPaused;++menuGeneration_;rig_={};
    if(open)staleDemoRecoveryDwell_.reset();
    if(open){
        sceneTransitionActivation_=0;
        menuAnchored_=retainMenuAnchor||(active_&&!suspended_&&lastView_.applied&&lastView_.activation==activation_);
        if(menuAnchored_&&!retainMenuAnchor){
            menuNative_=lastView_.nativePose;menuHead_=lastView_.headPose;
            const auto trackedHead=nativeTrackedPose(menuNative_,menuHead_,menuHead_);
            const auto head=nextIdroid?uprightOrigin(trackedHead):trackedHead;
            const float tilt=controllers_.menuQuadTilt*.00872664626f;
            menuPanel_=compose(head,Pose{{std::sin(tilt),0,0,std::cos(tilt)},
                {0,nextIdroid?-.20f:0.f,-controllers_.menuQuadDistance}});
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
    headLookLessonKey_=0;
    active_=pending_=suspended_=awaitingPlayer_=awaitingScene_=false;camera_=playerOwner_=0;rig_={};lastView_={};cameraRenderTime_=0;menuAnchored_=false;
    cameraFreezeAnchored_=false;
    avatarEditorBackdropValid_=false;avatarEditorBackdrop_={};
    scriptedDemoActive_=false;cinematicFollow_={};sceneTransitionActivation_=0;
    staleDemoRecoveryDwell_.reset();
    snapYaw_=0;snapTranslation_={};recenterPending_=false;
    resetStabilization(this);
    clearPlayerRootSample(oldCamera);
}
void HeadCamera::awaitPlayerLocked(uint8_t cause){
    // A native menu/cinematic has no matching player-head publication. Keep
    // native pixels and normal menu input available, but remember the user's
    // VR request. Resume only the exact camera/owner previously accepted.
    if(active_||pending_||awaitingPlayer_){
        if(!awaitingPlayer_){awaitCause_=cause;awaitTime_=time_;}
        awaitingPlayer_=true;active_=pending_=suspended_=false;rig_={};
        staleDemoRecoveryDwell_.reset();
        resetStabilization(this);
        reason_=HeadCameraStop::playerHeadUnavailable;
    }
}
void HeadCamera::suspendLocked(HeadCameraStop reason){
    if(active_||pending_){suspended_=true;reason_=reason;staleDemoRecoveryDwell_.reset();}
}
void HeadCamera::cancel(HeadCameraStop reason){std::lock_guard lock(mutex_);cancelLocked(reason);}
void HeadCamera::awaitScene(bool nativeLoading){
    if(!nativeLoading)return;
    std::lock_guard lock(mutex_);
    // The native camera can retire before XR observes the loading terminal.
    // Preserve the planned scene boundary even after awaitPlayerLocked, so
    // loading cannot remain behind a retained Pause image without its panel.
    const bool retiredPlayer=awaitingPlayer_&&reason_==HeadCameraStop::playerHeadUnavailable;
    if((active_||retiredPlayer)&&!awaitingScene_){awaitingScene_=true;rig_={};sceneTransitionActivation_=0;staleDemoRecoveryDwell_.reset();resetStabilization(this);}
}
bool HeadCamera::available() const {std::lock_guard lock(mutex_);return enabled_;}
bool HeadCamera::active() const {std::lock_guard lock(mutex_);return active_&&!awaitingScene_;}
HeadCameraStatus HeadCamera::status() const {std::lock_guard lock(mutex_);return {enabled_,active_&&!awaitingScene_,pending_,awaitingScene_?HeadCameraStop::sceneUnavailable:reason_,cancellations_,activation_,suspended_,awaitingPlayer_||awaitingScene_,nativeMenuOpen_,nativeIdroidOpen_,rigRejectFlags_,rigAgeMs_,sceneTransitionActivation_,activationCause_,camera_,playerOwner_,ownerHeadTime_,awaitCause_,awaitJoinFlags_,awaitTime_,awaitOwnerAge_};}
bool HeadCamera::staleDemoRecoveryReady() const {
    const auto candidate=nativeDemoSnapshot();
    if(candidate.mode!=NativeDemoMode::staleCandidate||!candidate.candidateKey)return false;
    std::lock_guard lock(mutex_);
    if(!enabled_||!active_||suspended_||awaitingScene_||!camera_
       ||!staleDemoRecoveryDwell_.matchesCamera(camera_,playerOwner_))return false;
    return staleDemoRecoveryDwell_.ready(steadyMilliseconds(),candidate.candidateKey,candidate.generation,
                                         candidate.publishedAtMs,
                                         nativeMenuOpen_||nativeIdroidOpen_);
}
std::optional<HeadCameraSample> HeadCamera::publishedView() const {
    std::lock_guard lock(mutex_);
    if(!active_||!lastView_.applied||lastView_.activation!=activation_)return {};
    return lastView_;
}
std::optional<HeadCameraSample> HeadCamera::publishedRigFrame(uint64_t now) const {
    std::lock_guard lock(mutex_);
    const auto& frame=rig_.sample;
    if(!active_||suspended_||awaitingScene_||!rig_.camera||rig_.camera!=camera_||!rig_.owner||rig_.owner!=playerOwner_
        ||!frame.applied||!frame.rigSequence||frame.playerOwner!=rig_.owner
        ||frame.activation!=activation_||frame.trackingSequence>sequence_
        ||frame.controllers.referenceEpoch!=controllers_.referenceEpoch
        ||frame.controllers.presentationEpoch!=controllers_.presentationEpoch||frame.menuGeneration!=menuGeneration_
        ||now<frame.sampleTime||now-frame.sampleTime>150)return {};
    // A completed paused skin solve has current tracking and a current rig
    // generation even though native animation has stopped publishing heads.
    // Device attachment must consume that same solve; rejecting it here lets
    // the hand move while the native handset keeps its preceding transform.
    const bool anchoredMenu=frame.menuOpen&&nativeMenuOpen_&&menuAnchored_
        &&(!nativeIdroidOpen_||nativeIdroidPaused_||!frame.controllers.handheldMenus||frame.controllers.cabinPlay);
    const bool anchoredFreeze=frame.controllers.cameraWorldFrozen&&controllers_.cameraWorldFrozen&&cameraFreezeAnchored_;
    if(requirePlayerHead_&&!anchoredMenu&&!anchoredFreeze){
        const auto current=std::find_if(playerHeads_.begin(),playerHeads_.end(),[&](const auto& p){
            return p.camera==rig_.camera&&p.owner==rig_.owner&&p.sequence;
        });
        const auto& p=rig_.sourceCamera;
        if(current==playerHeads_.end()||now<current->time||now-current->time>150
            ||current->sourceCamera.position.x!=p.position.x||current->sourceCamera.position.y!=p.position.y
            ||current->sourceCamera.position.z!=p.position.z||current->sourceCamera.orientation.x!=p.orientation.x
            ||current->sourceCamera.orientation.y!=p.orientation.y||current->sourceCamera.orientation.z!=p.orientation.z
            ||current->sourceCamera.orientation.w!=p.orientation.w)return {};
    }
    return frame;
}
bool HeadCamera::joinNativeBodyHeadLocked(uintptr_t camera,Pose sourceCamera,
                                        const NativeBodyHead& nativeHead,uint64_t time){
    if(!enabled_||!requirePlayerHead_||!nativeHead.owner||!nativeHead.model||!valid(nativeHead.worldFromHead))return false;
    const auto eye=compose(nativeHead.worldFromHead,Pose{{},nativeEyeFromHead}).position;
    if(!valid(Pose{{},eye}))return false;
    const auto heading=nativeHead.bodyFramesTracked?nativeTorsoHeading(nativeHead.worldFromChest):std::optional<Quat>{};
    if(nativeHead.bodyFramesTracked&&(!valid(nativeHead.worldFromRoot)||!heading))return false;
    const auto same=[](Pose a,Pose b){
        return a.position.x==b.position.x&&a.position.y==b.position.y&&a.position.z==b.position.z
            &&a.orientation.x==b.orientation.x&&a.orientation.y==b.orientation.y
            &&a.orientation.z==b.orientation.z&&a.orientation.w==b.orientation.w;
    };
    const auto found=std::find_if(playerHeads_.begin(),playerHeads_.end(),[&](const auto& head){
        return head.camera==camera&&head.owner==nativeHead.owner&&head.sequence
            &&time>=head.time&&time-head.time<=150&&same(head.sourceCamera,sourceCamera);
    });
    if(found==playerHeads_.end())return false;
    std::lock_guard rootLock(playerRootMutex);
    const auto current=playerRoots.find(camera);
    if(current==playerRoots.end()||current->second.owner!=found->owner||current->second.sequence!=found->sequence)return false;
    // Retain the camera owner's HEAD publication identity. Anatomical eyes,
    // root and chest heading come from one validated original skin, avoiding
    // the camera cache's different root placement during a prone animation.
    current->second.eyePosition=eye;current->second.eyeModel=nativeHead.model;
    if(nativeHead.bodyFramesTracked){
        current->second.position=nativeHead.worldFromRoot.position;
        current->second.anatomicalHeading=*heading;
        current->second.bodyFramesTracked=true;
    }
    return true;
}
HeadCameraSample HeadCamera::resolve(uintptr_t camera,Pose nativePose,uint64_t time,
                                    std::optional<NativeBodyHead> nativeHead){
    std::lock_guard lock(mutex_);
    const bool joined=nativeHead&&joinNativeBodyHeadLocked(camera,nativePose,*nativeHead,time);
    return resolveLocked(camera,nativePose,time,!nativeHead.has_value(),joined);
}
HeadCameraSample HeadCamera::resolveCurrent(uintptr_t camera,Pose nativePose){
    std::lock_guard lock(mutex_);
    return resolveLocked(camera,nativePose,steadyMilliseconds());
}
HeadCameraSample HeadCamera::resolveCurrentForRig(uintptr_t camera,Pose nativePose,
                                                 std::optional<NativeBodyHead> nativeHead){
    std::lock_guard lock(mutex_);
    const auto time=steadyMilliseconds();
    const bool joined=nativeHead&&joinNativeBodyHeadLocked(camera,nativePose,*nativeHead,time);
    return resolveLocked(camera,nativePose,time,false,joined);
}
bool HeadCamera::publishRigFrame(uintptr_t camera,uintptr_t owner,Pose sourceCamera,HeadCameraSample frame){
    if(!owner||!frame.applied||!valid(sourceCamera)||!valid(frame.nativePose))return false;
    std::lock_guard lock(mutex_);
    if(!active_||camera_!=camera||frame.activation!=activation_||frame.trackingSequence>sequence_
       ||frame.controllers.referenceEpoch!=controllers_.referenceEpoch
       ||frame.controllers.presentationEpoch!=controllers_.presentationEpoch
       ||frame.controllers.presentationFocused!=controllers_.presentationFocused
       ||frame.menuGeneration!=menuGeneration_)return false;
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
HeadCameraSample HeadCamera::resolveLocked(uintptr_t camera,Pose nativePose,uint64_t time,bool useRig,bool nativeEyeJoined){
    const auto sourceCamera=nativePose;
    HeadCameraSample result{nativePose,head_,sequence_,activation_,false};
    result.menuGeneration=menuGeneration_;
    result.views=views_;result.sampleTime=time_;result.stereoTracked=stereoTracking_;
    if(!enabled_||!camera||!valid(nativePose))return result;
    if(controllers_.avatarEditor||controllers_.nativeSceneCanvas){
        // The editor owns a separate native preview stage, not the hospital
        // scene. Preserve its camera and full canvas for the compositor quad.
        // The compositor retains the last accepted hospital stereo image.
        result.controllers=controllers_;
        if(active_&&!awaitingScene_){awaitingScene_=true;rig_={};sceneTransitionActivation_=0;
            staleDemoRecoveryDwell_.reset();resetStabilization(this);}
        return result;
    }
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
            camera_=camera;playerOwner_=0;rig_={};lastView_={};++activation_;activationCause_=3;
        }else{
            // Entering an authored scene invalidates lastView_'s gameplay
            // pixels. Its source retirement time must remain available or a
            // different movie camera is rejected for the entire cutscene.
            if(!useRig||!cameraRenderTime_||time<cameraRenderTime_||time-cameraRenderTime_<=500)return result;
            camera_=camera;playerOwner_=0;rig_={};lastView_={};++activation_;activationCause_=4;
            sceneTransitionActivation_=activation_;sceneTransitionAt_=time;
        }
    }
    // Loading can still publish its decorative character/camera. It must not
    // immediately reactivate stereo and cover the native confirmation screen.
    if(controllers_.loading){awaitPlayerLocked(1);return result;}
    // A field tutorial can retain its terminal flag across an authored shot.
    // Its old menu anchor must not require the retired player's skin while
    // the shot owns the camera, or prevent the exact player join on return.
    const bool menuSceneHandoff=nativeMenuOpen_&&nativeIdroidOpen_&&!nativeIdroidPaused_
        &&controllers_.handheldMenus&&!controllers_.cabinPlay
        &&(authoredScene||!playerOwner_||!menuAnchored_);
    const bool spatialMenu=nativeMenuOpen_&&menuAnchored_&&active_&&!menuSceneHandoff;
    const bool frozenCamera=controllers_.cameraWorldFrozen&&cameraFreezeAnchored_&&active_&&!nativeMenuOpen_;
    const bool liveHandheldMenu=spatialMenu&&nativeIdroidOpen_&&!nativeIdroidPaused_&&controllers_.handheldMenus
        &&!controllers_.cabinPlay&&!controllers_.frontEnd&&!controllers_.avatarEditor;
    if(nativeMenuOpen_&&!spatialMenu&&!menuSceneHandoff){awaitPlayerLocked(2);return result;}
    if(spatialMenu){
        if(camera!=camera_)return result;
        // Field handheld iDroid follows live locomotion. In the ACC, the
        // native phone camera switches to the seated player's facing; keep
        // the accepted cabin view and apply fresh HMD motion below instead.
        // A guide can retain its own gameplay pause after its Help closes.
        // Keep the physical handset and fresh tracked rig, using the accepted
        // world anchor until that exact native tutorial releases its pause.
        if(!nativeIdroidOpen_||nativeIdroidPaused_||!controllers_.handheldMenus||controllers_.cabinPlay)nativePose=menuNative_;
        result.menuOpen=true;result.menuIdroid=nativeIdroidOpen_;result.menuPanel=menuPanel_;
        result.menuWorldQuad=!nativeIdroidOpen_||!controllers_.handheldMenus;
        result.playerOwner=playerOwner_;result.playerHead=lastView_.playerHead;
        result.playerSequence=lastView_.playerSequence;
        result.anatomicalEyeModel=lastView_.anatomicalEyeModel;
        // Paused native menus keep the last skin on screen. Keep its palm
        // attachment too; a raw controller fallback would detach the display
        // from that still-visible hand. A fresh skin replaces these below.
        result.renderedPalms=lastView_.renderedPalms;
        result.renderedPalmTracked=lastView_.renderedPalmTracked;
        result.wristPanel=lastView_.wristPanel;
        result.wristPanelTracked=lastView_.wristPanelTracked;
        if(liveHandheldMenu&&requirePlayerHead_){
            // A handheld terminal keeps the field and vehicle moving. The
            // native vehicle camera is a chase boom, not the driver's eyes.
            // Join the live handset camera to its exact current player skin,
            // just as ordinary first-person rendering does below.
            const auto found=std::find_if(playerHeads_.begin(),playerHeads_.end(),[&](const auto& p){
                const auto& source=p.sourceCamera;
                return p.camera==camera&&p.owner==playerOwner_&&p.sequence
                    &&time>=p.time&&time-p.time<=150
                    &&source.position.x==sourceCamera.position.x&&source.position.y==sourceCamera.position.y
                    &&source.position.z==sourceCamera.position.z
                    &&source.orientation.x==sourceCamera.orientation.x&&source.orientation.y==sourceCamera.orientation.y
                    &&source.orientation.z==sourceCamera.orientation.z&&source.orientation.w==sourceCamera.orientation.w;
            });
            if(found==playerHeads_.end()){
                rig_={};suspendLocked(HeadCameraStop::playerHeadUnavailable);return result;
            }
            nativePose.position=found->position;
            result.playerSequence=found->sequence;result.playerHead=found->position;
            ownerHeadTime_=found->time;
        }
    }else if(frozenCamera){
        if(camera!=camera_||!playerOwner_)return result;
        // This anchor was accepted before OUR named world pause. It retains
        // the exact player/camera ownership, while each new eye/skin solve
        // below uses current tracked poses. No native animation is advanced.
        nativePose=cameraFreezeNative_;result.playerOwner=playerOwner_;
        result.playerHead=lastView_.playerHead;result.playerSequence=lastView_.playerSequence;
        result.anatomicalEyeModel=lastView_.anatomicalEyeModel;
    }else if(authoredScene&&awaitingPlayer_&&(camera_||controllers_.scriptedDemo)){
        // This is the verified primary render publication of a native scripted
        // shot. Its camera remains valid when the demo stops publishing the
        // player's skeleton. Do not attach that shot to a decorative actor.
        if(camera_&&camera_!=camera)return result;
        if(!camera_){
            camera_=camera;origin_=uprightOrigin(head_);frontEndOrigin_=uprightOrigin(sourceCamera);
            frontEndPanel_=compose(nativeTrackedPose(frontEndOrigin_,head_,head_),Pose{{},{0,-.05f,-1.3f}});
        }
        playerOwner_=0;rig_={};awaitingPlayer_=pending_=false;active_=true;++activation_;activationCause_=5;
        sceneTransitionActivation_=activation_;sceneTransitionAt_=time;
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
                        &&time>=p.time&&((time-p.time<=150)
                            ||(time-p.time<=1000&&same(p.sourceCamera,nativePose)));
                });
            if(interrupted){
                // Reject this unmatched/stale skin without changing activation/input
                // context. Only an already accepted stereo pair may cover
                // the gap; the next fresh skin resumes this same session.
                // During native camera motion the fresh player skin can
                // precede this camera pose. That failed exact-pose join is
                // not a menu/scene retirement. It has no new-pixel authority.
                rig_={};suspendLocked(HeadCameraStop::playerHeadUnavailable);
            }else{
                if(!awaitingPlayer_){
                    awaitJoinFlags_=active_?8:0;
                    awaitOwnerAge_=~uint64_t{0};
                    for(const auto& p:playerHeads_){
                        if(p.camera!=camera||p.owner!=playerOwner_||!p.sequence||time<p.time)continue;
                        const auto age=time-p.time;awaitOwnerAge_=std::min(awaitOwnerAge_,age);
                        awaitJoinFlags_|=1;
                        if(age<=150)awaitJoinFlags_|=2;
                        if(age<=1000&&same(p.sourceCamera,nativePose))awaitJoinFlags_|=4;
                    }
                }
                awaitPlayerLocked(3);
            }
            return result;
        }
        if(camera_==camera&&!playerOwner_){
            // The scripted shot had no player owner. A fresh matching player
            // publication can take over immediately when control returns.
            playerOwner_=found->owner;rig_={};lastView_={};++activation_;activationCause_=6;
            sceneTransitionActivation_=activation_;sceneTransitionAt_=time;
            resetStabilization(this);
        }
        if(camera_&&(camera_!=camera||playerOwner_!=found->owner)){
            // ACC -> field / mission restart creates a new verified player.
            // Never borrow another live owner's camera. After the previous
            // owner has stopped publishing for a full second, adopt only a
            // fresh player-head publication matching this rendered camera.
            // Retained ACC eyes, rig and menu anchors must not survive travel.
            if(time<ownerHeadTime_||time-ownerHeadTime_<=1000)return result;
            // A movie can retire its shot camera and create a different
            // player camera. This fresh native join completes that explicit
            // handoff; ordinary travel still has no retained-image authority.
            const bool returningFromShot=sceneTransitionActivation_==activation_
                &&time>=sceneTransitionAt_&&time-sceneTransitionAt_<=250;
            camera_=playerOwner_=0;rig_={};lastView_={};menuAnchored_=false;
            active_=pending_=false;awaitingPlayer_=true;awaitingScene_=false;
            if(!returningFromShot)sceneTransitionActivation_=0;
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
            const bool returningFromShot=sceneTransitionActivation_==activation_
                &&time>=sceneTransitionAt_&&time-sceneTransitionAt_<=250;
            ++activation_;activationCause_=7; // No eye image from before the menu may be reused.
            if(returningFromShot)sceneTransitionActivation_=activation_;
        }
        // A scripted character can be scenery rather than the player's
        // viewpoint. In the hospital-bed setup flow, keep the authored camera
        // so the patient and room remain visible behind the native UI quad.
        if(!controllers_.frontEnd&&!controllers_.avatarEditor&&!controllers_.cabinPlay)nativePose.position=found->position;
        ownerHeadTime_=found->time;
        result.playerSequence=found->sequence;result.playerOwner=found->owner;result.playerHead=found->position;
        if(found->rocketArmGuidance){
            result.rocketArmGuidance=true;result.rocketArmCamera=sourceCamera;
            // Projectile steering has no authority to rotate Snake's tracking
            // space. Both eyes and the skin retain the live player root yaw.
            nativePose.orientation=found->rootHeading;
        }
    }
    if(pending_){
        camera_=camera;playerOwner_=result.playerOwner;origin_=uprightOrigin(head_);
        frontEndOrigin_=uprightOrigin(nativePose);
        frontEndPanel_=compose(nativeTrackedPose(frontEndOrigin_,head_,head_),Pose{{},{0,-.05f,-1.3f}});
        pending_=false;active_=true;++activation_;activationCause_=8;snapTranslation_={};snapYaw_=controllers_.snapYaw;
    }
    if(!active_)return result;
    if(camera_!=camera){cancelLocked(HeadCameraStop::cameraChanged);return result;}
    if(menuSceneHandoff&&!authoredScene&&result.playerOwner&&result.playerSequence){
        menuNative_=nativePose;menuHead_=head_;menuAnchored_=true;
        const auto trackedHead=uprightOrigin(nativeTrackedPose(menuNative_,menuHead_,menuHead_));
        const float tilt=controllers_.menuQuadTilt*.00872664626f;
        menuPanel_=compose(trackedHead,Pose{{std::sin(tilt),0,0,std::cos(tilt)},
            {0,-.20f,-controllers_.menuQuadDistance}});
        result.menuOpen=true;result.menuIdroid=true;result.menuPanel=menuPanel_;
    }
    // A scripted shot can have no player owner. Observe recovery only when a
    // fresh, verified player publication joins this exact accepted render
    // camera and source pose; assigning a gameplay owner before this join
    // would either strand an ownerless shot or accept a decorative actor.
    const auto candidate=nativeDemoSnapshot();
    if(candidate.mode==NativeDemoMode::staleCandidate&&!spatialMenu&&!controllers_.loading
       &&!controllers_.frontEnd&&!controllers_.avatarEditor){
        const auto same=[](Pose a,Pose b){
            return a.position.x==b.position.x&&a.position.y==b.position.y&&a.position.z==b.position.z
                &&a.orientation.x==b.orientation.x&&a.orientation.y==b.orientation.y
                &&a.orientation.z==b.orientation.z&&a.orientation.w==b.orientation.w;
        };
        const auto found=std::find_if(playerHeads_.begin(),playerHeads_.end(),[&](const auto& p){
            return p.camera==camera&&p.owner&&(!playerOwner_||p.owner==playerOwner_)
                &&p.sequence&&time>=p.time&&time-p.time<=150&&same(p.sourceCamera,sourceCamera);
        });
        if(found==playerHeads_.end()){staleDemoRecoveryDwell_.reset();recoveryPlayerSequence_=0;}
        else if(found->sequence!=recoveryPlayerSequence_){
            staleDemoRecoveryDwell_.observe(true,candidate.candidateKey,candidate.generation,
                camera,found->owner,found->time,candidate.publishedAtMs,false);
            recoveryPlayerSequence_=found->sequence;
        }
    }else{staleDemoRecoveryDwell_.reset();recoveryPlayerSequence_=0;}
    if(awaitingScene_){awaitingScene_=false;++activation_;activationCause_=9;}
    if(controllers_.frontEnd)result.menuPanel=controllers_.authoredCamera
        ?compose(nativeTrackedPose(nativePose,head_,head_),Pose{{},{0,-.05f,-1.3f}}):frontEndPanel_;
    else if(controllers_.avatarEditor)
        result.menuPanel=compose(nativeTrackedPose(nativePose,head_,head_),Pose{{},{0,-.05f,-1.3f}});
    // Coco's cinematic follow keeps current shot motion while filtering its
    // shake and roll. Cuts snap to the new source; physical head motion is
    // applied afterwards without filtering. Pause owns its separate anchor.
    if(!spatialMenu&&controllers_.scriptedDemo&&!controllers_.scriptedLook){
        nativePose=followCinematicLocked(nativePose,time,useRig);
        result.cinematicBasePose=cinematicFollow_.pose;
        result.cinematicFreeOffset=cinematicFollow_.freeOffset;
        result.cinematicFollowValid=cinematicFollow_.valid;
    }
    else if(useRig&&(!controllers_.scriptedDemo||controllers_.scriptedLook))cinematicFollow_={};
    if(!spatialMenu&&controllers_.scriptedLook&&controllers_.headLookLessonKey){
        // The game consumes HMD-derived look for its real tutorial history.
        // Retain this authored eye anchor so its resulting stock camera turn
        // does not apply the same physical head motion a second time.
        if(headLookLessonKey_!=controllers_.headLookLessonKey){
            headLookLessonAnchor_=nativePose;headLookLessonKey_=controllers_.headLookLessonKey;
        }
        nativePose=headLookLessonAnchor_;
    }else headLookLessonKey_=0;
    if((!spatialMenu||liveHandheldMenu)&&!controllers_.authoredCamera){
        // Horses and other native mounts keep their established camera path.
        // This adapter resolves the actual TppVehicle2 carrier only.
        const bool otherCarrier=controllers_.vehicleView.valid&&controllers_.vehicleView.object!=65535
            &&(controllers_.vehicleView.object>>9)!=50;
        if((controllers_.cabinTransit||(controllers_.vehicleMounted&&otherCarrier))&&!controllers_.nativeGamepad){
            PlayerRootSample root;
            if(!getPlayerRootSample(camera,result.playerOwner,result.playerSequence,time,root)){
                rig_={};suspendLocked(HeadCameraStop::playerHeadUnavailable);return result;
            }
            const float nativeYaw=horizontalYaw(sourceCamera.orientation);
            const float rootYaw=horizontalYaw(root.orientation);
            auto& view=driverView_;
            const bool same=view.valid&&view.camera==camera&&view.owner==result.playerOwner
                &&view.activation==activation_&&view.referenceEpoch==controllers_.referenceEpoch
                &&view.passenger==controllers_.cabinTransit
                &&time>=view.time&&time-view.time<=500;
            if(!same){
                view={nativeYaw,rootYaw,0,camera,result.playerOwner,time,activation_,
                    controllers_.referenceEpoch,0,true};
                view.passenger=controllers_.cabinTransit;
                if(view.passenger){
                    view.lookYaw=std::remainder(nativeYaw-rootYaw,6.283185307f);
                    view.headLocalOffset=rotate(inverseQuat(root.orientation),result.playerHead-root.position);
                }
            }
            const bool looking=!view.passenger&&(std::abs(controllers_.vehicleViewInput[0])>.18f
                ||std::abs(controllers_.vehicleViewInput[1])>.18f);
            if(looking)view.lookUntil=time+250;
            if(same&&time>view.time&&time<=view.lookUntil){
                // Preserve deliberate native right-stick look, including its
                // brief settling motion. Body/vehicle turns supply the base
                // heading; unrelated chase-camera auto-orbit does not.
                const float delta=std::remainder(nativeYaw-view.nativeYaw,6.283185307f)
                    -std::remainder(rootYaw-view.rootYaw,6.283185307f);
                if(std::abs(delta)<.9f)view.lookYaw=std::remainder(view.lookYaw+delta,6.283185307f);
            }
            view.nativeYaw=nativeYaw;view.rootYaw=rootYaw;view.time=time;
            nativePose.orientation=yawRotation(rootYaw+view.lookYaw);
            // A passenger's authored seated fit rides the native player root.
            // Native camera orbit and idle head animation do not move the VR
            // seat; HMD turning/leaning and cabin walking are applied below.
            if(view.passenger)nativePose.position=root.position+rotate(root.orientation,view.headLocalOffset);
        }else driverView_={};
        if(!otherCarrier&&!controllers_.cabinTransit&&!controllers_.cabinPlay&&!controllers_.frontEnd
           &&!controllers_.scriptedDemo&&!controllers_.nativeGamepad){
            if(vehicleHeadingCamera_!=camera||vehicleHeadingOwner_!=result.playerOwner
               ||vehicleHeadingEpoch_!=controllers_.referenceEpoch){
                vehicleHeading_.reset();vehicleHeadingCamera_=camera;vehicleHeadingOwner_=result.playerOwner;
                vehicleHeadingEpoch_=controllers_.referenceEpoch;
            }
            const bool looking=std::abs(controllers_.vehicleViewInput[0])>.18f
                ||std::abs(controllers_.vehicleViewInput[1])>.18f;
            nativePose.orientation=yawRotation(vehicleHeading_.update(horizontalYaw(nativePose.orientation),
                controllers_.snapYaw,controllers_.vehicleView,looking,time,controllers_.contextInteract));
        }else vehicleHeading_.reset();
    }
    if(controllers_.authoredCamera||controllers_.nativeGamepad)vehicleHeading_.reset();
    // The native camera can look down, lean, recoil or bank. Its yaw supplies
    // gameplay heading; gravity and physical HMD pitch/roll supply the VR view.
    // A full native/activation rotation tilts the tracking space when turning.
    if(!spatialMenu||liveHandheldMenu){
        if(!controllers_.authoredCamera)
            nativePose=(controllers_.frontEnd||controllers_.cabinPlay)?frontEndOrigin_:uprightOrigin(nativePose);
        if(recenterPending_){
            const float baseYaw=horizontalYaw(nativePose.orientation)+controllers_.snapYaw;
            const float facing=lastView_.applied?horizontalYaw(lastView_.nativePose.orientation):baseYaw;
            origin_={yawRotation(horizontalYaw(head_.orientation)-facing+baseYaw),head_.position};
            snapTranslation_={};snapYaw_=controllers_.snapYaw;recenterPending_=false;rig_={};++activation_;activationCause_=10;
        }
    }
    if(useRig&&!authoredScene&&rig_.camera){
        const auto& s=rig_.sample;const auto& p=rig_.sourceCamera;
        const bool same=p.position.x==sourceCamera.position.x&&p.position.y==sourceCamera.position.y&&p.position.z==sourceCamera.position.z
            &&p.orientation.x==sourceCamera.orientation.x&&p.orientation.y==sourceCamera.orientation.y
            &&p.orientation.z==sourceCamera.orientation.z&&p.orientation.w==sourceCamera.orientation.w;
        const bool sameOwner=rig_.camera==camera&&rig_.owner==result.playerOwner&&s.activation==activation_;
        const bool clockValid=time>=s.sampleTime;
        rigAgeMs_=clockValid?time-s.sampleTime:0;
        rigRejectFlags_=(!sameOwner?1u:0u)|(!same?2u:0u)|(!clockValid?4u:0u)
            |(clockValid&&rigAgeMs_>150?8u:0u);
        const bool freshRig=rigRejectFlags_==0;
        if(freshRig){
            const auto playerPublication=result.playerSequence;
            result=s;result.playerSequence=playerPublication;suspended_=false;reason_=HeadCameraStop::none;lastView_=result;cameraRenderTime_=time;return result;
        }
        // Native Pause/Help stops skin updates while its camera and UI keep
        // drawing. A cached rig must not poison every subsequent menu frame
        // after 150 ms. Resolve fresh tracked eyes from this accepted menu
        // anchor without relabeling the old hand publication as current.
        if((!spatialMenu&&!frozenCamera)||!sameOwner){
            // An aged rig from the same camera/owner is a producer delay,
            // not a broken pose join. Do not publish it as a new frame; let
            // the compositor retain only its previously accepted image.
            suspendLocked(rigRejectFlags_==8?HeadCameraStop::staleRig:HeadCameraStop::rigFrameMismatch);
            return result;
        }
        rig_={};
    }
    suspended_=false;reason_=HeadCameraStop::none;
    if(controllers_.cabinPlay&&!controllers_.frontEnd&&!controllers_.authoredCamera
       &&nativeEyeJoined&&result.playerSequence){
        // The cabin keeps its room anchor rather than field head stabilization,
        // but body admission still needs this exact original-skin eye identity.
        PlayerRootSample cabinRoot;
        if(getPlayerRootSample(camera,result.playerOwner,result.playerSequence,time,cabinRoot))
            result.anatomicalEyeModel=cabinRoot.eyeModel;
    }
    if(!frozenCamera&&(!spatialMenu||liveHandheldMenu)&&!controllers_.frontEnd&&!controllers_.avatarEditor&&!controllers_.cabinPlay
       &&!controllers_.cabinTransit
       &&requirePlayerHead_&&result.playerSequence){
        PlayerRootSample rootSample;
        if(getPlayerRootSample(camera,result.playerOwner,result.playerSequence,time,rootSample)){
            // A previous owned join can still occupy this root publication.
            // Live body admission requires this invocation's original skin;
            // anchored Pause/photo frames retain their admitted tag above.
            result.anatomicalEyeModel=nativeEyeJoined?rootSample.eyeModel:0;
            auto& state=stabilizationState(this);
            const bool anatomical=controllers_.fullBody&&!controllers_.vehicleMounted;
            if(state.valid&&(state.anatomical!=anatomical
                ||(anatomical&&rootSample.eyeModel&&state.anatomicalModel!=rootSample.eyeModel)))state={};
            float dt=0;
            if(state.valid&&time>=state.time)dt=static_cast<float>(time-state.time)/1000.f;
            const auto headPoint=anatomical?rootSample.eyePosition:result.playerHead;
            const Vec3 worldHeadOffset{
                headPoint.x-rootSample.position.x,
                headPoint.y-rootSample.position.y,
                headPoint.z-rootSample.position.z
            };
            // Crawling chest yaw oscillates on every stride. Measure and
            // transport its eye offset in the actor root frame instead.
            const auto anatomicalHeading=rootSample.bodyFramesTracked&&controllers_.playerPosture!=PlayerPosture::prone
                ?rootSample.anatomicalHeading:rootSample.orientation;
            const auto targetHeadLocalOffset=rotate(inverseQuat(anatomical?anatomicalHeading:rootSample.orientation),worldHeadOffset);
            const bool advanceOffset=!anatomical||!state.anatomicalModel||(nativeEyeJoined&&rootSample.eyeModel);
            // Helicopter exit retains the crouched posture flag while the
            // authored head descends. Do not freeze its first airborne sample.
            // Limit settling to this handoff; ordinary gait remains anchored.
            if(time<fieldPostureUntil_&&state.postureUntil<fieldPostureUntil_){
                state.postureUntil=fieldPostureUntil_;state.postureSum={};state.postureWeight=0;
            }
            if(!state.valid){
                state.headLocalOffset=targetHeadLocalOffset;
                state.heading=rootSample.orientation;
                state.posture=controllers_.playerPosture;
                state.anatomical=anatomical;
                state.anatomicalModel=anatomical?rootSample.eyeModel:0;
                state.valid=true;
            }else if(dt>0&&advanceOffset){
                // A new camera publication can precede its owned skin. Its
                // cached HEAD must not advance a latch admitted from bone 4.
                dt=std::min(dt,.05f);
                const auto posture=controllers_.playerPosture;
                if(posture!=PlayerPosture::unknown){
                    if(posture!=state.posture){
                        state.postureUntil=time+900;state.postureSum={};state.postureWeight=0;
                    }
                    state.posture=posture;
                    if(state.postureUntil){
                        const auto alpha=smoothingAlpha(dt,headStabilizationTau);
                        state.headLocalOffset=state.headLocalOffset+(targetHeadLocalOffset-state.headLocalOffset)*alpha;
                        if(time+200>=state.postureUntil){state.postureSum=state.postureSum+targetHeadLocalOffset*dt;state.postureWeight+=dt;}
                        if(time>=state.postureUntil){
                            if(state.postureWeight>0)state.headLocalOffset=state.postureSum*(1/state.postureWeight);
                            state.postureUntil=0;
                        }
                    }
                    state.followingPosture=false;
                }else{
                const float deltaY=targetHeadLocalOffset.y-state.headLocalOffset.y;
                if(std::abs(deltaY)>headBobDeadbandY)
                    state.headLocalOffset.y+=deltaY*smoothingAlpha(dt,headStabilizationTau);
                // Lock Coco's horizontal anchor through ordinary gait bob.
                // A real posture/mount change still needs a new anchor; ease
                // that transition rather than retaining the previous stance.
                const auto delta=targetHeadLocalOffset-state.headLocalOffset;
                const float horizontal=std::hypot(delta.x,delta.z);
                // A heading change is an animation choice, not a new posture.
                // Repeated rig/camera resolves in one millisecond must retain
                // the filter state instead of reseeding it from gait motion.
                if(horizontal>.25f||std::abs(deltaY)>headBobDeadbandY)state.followingPosture=true;
                if(state.followingPosture){
                    const float alpha=smoothingAlpha(dt,headStabilizationTau);
                    state.headLocalOffset.x+=delta.x*alpha;
                    state.headLocalOffset.z+=delta.z*alpha;
                    if(horizontal<.015f&&std::abs(deltaY)<=headBobDeadbandY){
                        state.headLocalOffset.x=targetHeadLocalOffset.x;state.headLocalOffset.z=targetHeadLocalOffset.z;
                        state.followingPosture=false;
                    }
                }
                }
            }
            // Calibrate from the SAME settled anchor used by the viewpoint.
            // Native STAND becomes true before the rise animation finishes;
            // latching its first low eye permanently over-elevates standing VR.
            if(anatomical&&advanceOffset&&controllers_.playerPosture==PlayerPosture::standing
               &&!state.postureUntil&&state.headLocalOffset.y>1.f&&state.headLocalOffset.y<2.f)
                state.standingEyeHeight=state.headLocalOffset.y;
            // Preserve the owned update clock when the camera beats its skin
            // to this millisecond; that skin must still receive the full dt.
            if(advanceOffset)state.time=time;
            if(controllers_.vehicleMounted&&!controllers_.nativeGamepad){
                // The standing shoulder fit moves eyes .35 m behind and
                // .25 m above the native head. In a fixed cab that puts the
                // wheel outside arm reach and the shoulder mesh at the eyes.
                // A seated driver uses this exact authored head publication;
                // physical HMD motion remains relative to it below.
                nativePose.position=controllers_.fullBody&&nativeEyeJoined
                    ?rootSample.eyePosition:result.playerHead;
            }else{
                // Retain the eye's horizontal frame when Snake turns to run
                // sideways/backwards. Native camera yaw still owns view turn.
                // The anatomical offset was measured in the authored chest
                // heading. A prone head extends well beyond the root; rotating
                // that offset into an independent camera yaw moved the eyes
                // 77 cm sideways into the slope in the retained M1 fixture.
                // Preserve that native prone footprint. View yaw and HMD neck
                // look still own orientation, not the anchored eye position.
                const auto heading=anatomical&&rootSample.bodyFramesTracked
                    ?(controllers_.playerPosture==PlayerPosture::prone?anatomicalHeading
                        :yawRotation(horizontalYaw(nativePose.orientation))):state.heading;
                auto cameraAnchor=rootSample.position+rotate(heading,state.headLocalOffset);
                if(!anatomical){
                    cameraAnchor=cameraAnchor+rotate(state.heading,{0,0,-cameraBackwardOffset});
                    cameraAnchor.y+=cameraHeightOffset;
                }
                nativePose.position=cameraAnchor;
            }
        }
    }
    // FOX's camera-local forward/right candidates are +Z/-X. The explicit
    // 180-degree basis rotation keeps handedness intact; validate in live motion.
    if(!frozenCamera&&(!spatialMenu||liveHandheldMenu)&&!controllers_.authoredCamera)nativePose.position.y+=controllers_.playerHeightOffset;
    if(controllers_.cabinTransit&&driverView_.valid&&driverView_.passenger
       &&(!spatialMenu||liveHandheldMenu)){
        result.cabinBasePose=nativePose;result.cabinBaseTracked=true;
    }
    const Pose basis{{0,1,0,0},{}};
    // Keep a body heading from this exact camera transaction. HMD yaw belongs
    // to neck look; using the final viewer yaw for every bone makes prone feet
    // orbit the eye and lets an ordinary sideways glance rotate the pelvis.
    if(!controllers_.authoredCamera){
        result.locomotionHeading=frozenCamera&&lastView_.locomotionHeadingTracked
            ?lastView_.locomotionHeading
            :yawRotation(horizontalYaw(nativePose.orientation)
                +(std::isfinite(controllers_.snapYaw)?controllers_.snapYaw:0));
        result.locomotionHeadingTracked=valid(Pose{result.locomotionHeading,{}});
    }
    // A live field handset shares the gameplay tracking origin. Using the
    // head pose captured at menu entry zeroes the current pitch/snap heading
    // and tilts the world when the player looks back up from the handset.
    const bool anchoredMenu=spatialMenu&&!liveHandheldMenu;
    auto relative=compose(inverse(frozenCamera?cameraFreezeHead_:anchoredMenu?menuHead_:origin_),head_);
    relative.position=relative.position*units_;
    if(!anchoredMenu&&!frozenCamera&&controllers_.seatedMode&&!controllers_.authoredCamera)
        relative.position.y=(head_.position.y-seatedHeadY_)*units_;
    if(!anchoredMenu&&!frozenCamera&&!controllers_.seatedMode&&controllers_.physicalFloorTracked&&controllers_.fullBody
       &&!controllers_.frontEnd&&!controllers_.authoredCamera&&!controllers_.scriptedDemo
       &&!controllers_.cabinPlay&&!controllers_.cabinTransit&&!controllers_.vehicleMounted
       &&controllers_.playerPosture!=PlayerPosture::prone){
        const auto& body=stabilizationState(this);
        if(body.valid)if(const auto offset=physicalFloorHeightOffset(head_.position.y,
            controllers_.physicalFloorY,units_,body.standingEyeHeight))relative.position.y=*offset;
    }
    const auto nativeDelta=compose(compose(basis,relative),inverse(basis));
    result.nativePose=compose(nativePose,nativeDelta);
    if(!anchoredMenu&&!frozenCamera){
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
        if(result.menuOpen&&!(result.menuIdroid&&result.controllers.handheldMenus
                &&result.controllers.handheldMenuCoexist))result.controllers.optic={};
        // Skin preparation can keep evaluating a retired player camera while
        // a demo renders from another camera. Only the render publication may
        // advance the accepted view used for camera liveness and handoffs.
        if(useRig){
            lastView_=result;cameraRenderTime_=time;
            if(controllers_.frontEnd&&!controllers_.loading){lastTitleSource_=sourceCamera;lastTitleSourceValid_=true;}
        }
    }
    return result;
}
HeadCamera& headCamera(){static auto* instance=new HeadCamera;return *instance;}
}
