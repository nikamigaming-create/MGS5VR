#include "mgs5vr/head_camera.hpp"
#include "mgs5vr/input_bridge.hpp"
#include <cmath>
#include <stdexcept>
#include <algorithm>

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
    requirePlayerHead_=requirePlayerHead;playerHeads_={};playerSequence_=0;suspended_=false;
    controllers_={};rig_={};nativeMenuOpen_=false;lastView_={};menuAnchored_=false;trackingEpoch_=0;
    snapYaw_=0;snapTranslation_={};recenterPending_=false;
}
bool HeadCamera::publishPlayerHead(uintptr_t camera,uintptr_t owner,Pose sourceCamera,
                                  const std::array<float,16>& root,const std::array<float,16>& head,uint64_t time){
    const auto position=playerHeadPosition(root,head);
    if(!camera||!owner||!valid(sourceCamera)||!position)return false;
    const auto boom=sourceCamera.position-*position;
    if(dot(boom,boom)>144)return false;
    std::lock_guard lock(mutex_);
    if(!enabled_||!requirePlayerHead_)return false;
    auto found=std::find_if(playerHeads_.begin(),playerHeads_.end(),[&](const auto& p){return p.camera==camera;});
    if(found==playerHeads_.end())found=std::find_if(playerHeads_.begin(),playerHeads_.end(),[](const auto& p){return !p.camera;});
    if(found==playerHeads_.end())found=std::min_element(playerHeads_.begin(),playerHeads_.end(),[](const auto& a,const auto& b){return a.time<b.time;});
    if(found->camera==camera&&time<found->time)return false;
    *found={camera,owner,sourceCamera,*position,time,++playerSequence_};return true;
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
        if(trackingEpoch_&&controllers.referenceEpoch&&controllers.referenceEpoch!=trackingEpoch_&&(active_||pending_||awaitingPlayer_)){
            if(active_&&lastView_.applied){
                // Rebase the physical tracking origin without moving the native
                // viewpoint or the panel already anchored in this world.
                // Gameplay resumes in this new reference space too: carrying
                // the old origin through menu close would teleport the view.
                origin_=uprightOrigin(compose(compose(head,inverse(head_)),origin_));
                if(nativeMenuOpen_&&menuAnchored_){menuNative_=lastView_.nativePose;menuHead_=head;}
                rig_={};++activation_;
            }else{
                if(!awaitingPlayer_){active_=false;pending_=true;}
                camera_=playerOwner_=0;
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
    if(enabled_&&(active_||pending_||awaitingPlayer_)){recenterPending_=true;rig_={};}
}
void HeadCamera::setNativeMenuOpen(bool open){
    std::lock_guard lock(mutex_);
    if(open==nativeMenuOpen_)return;
    nativeMenuOpen_=open;rig_={};
    if(open){
        menuAnchored_=active_&&!suspended_&&lastView_.applied&&lastView_.activation==activation_;
        if(menuAnchored_){
            menuNative_=lastView_.nativePose;menuHead_=lastView_.headPose;
            menuPanel_=compose(nativeTrackedPose(menuNative_,menuHead_,menuHead_),Pose{{},{0,-.05f,-1.3f}});
        }
    }else{
        menuAnchored_=false;
        // Keep VR active. resolveLocked still requires a fresh publication of
        // this same player before adopting the gameplay camera again.
    }
}
void HeadCamera::cancelLocked(HeadCameraStop reason){
    if(active_||pending_||awaitingPlayer_){reason_=reason;++cancellations_;}
    active_=pending_=suspended_=awaitingPlayer_=false;camera_=playerOwner_=0;rig_={};lastView_={};menuAnchored_=false;
    snapYaw_=0;snapTranslation_={};recenterPending_=false;
}
void HeadCamera::awaitPlayerLocked(){
    // A native menu/cinematic has no matching player-head publication. Keep
    // native pixels and normal menu input available, but remember the user's
    // VR request. Resume only the exact camera/owner previously accepted.
    if(active_||pending_||awaitingPlayer_){
        awaitingPlayer_=true;active_=pending_=suspended_=false;rig_={};
        reason_=HeadCameraStop::playerHeadUnavailable;
    }
}
void HeadCamera::suspendLocked(HeadCameraStop reason){
    if(active_||pending_){suspended_=true;reason_=reason;}
}
void HeadCamera::cancel(HeadCameraStop reason){std::lock_guard lock(mutex_);cancelLocked(reason);}
bool HeadCamera::available() const {std::lock_guard lock(mutex_);return enabled_;}
bool HeadCamera::active() const {std::lock_guard lock(mutex_);return active_;}
HeadCameraStatus HeadCamera::status() const {std::lock_guard lock(mutex_);return {enabled_,active_,pending_,reason_,cancellations_,activation_,suspended_,awaitingPlayer_,nativeMenuOpen_};}
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
    if(!owner||!frame.applied||!valid(sourceCamera)||!valid(frame.nativePose)||!frame.controllers.hands[1].gripTracked)return false;
    std::lock_guard lock(mutex_);
    if(!active_||camera_!=camera||frame.activation!=activation_||frame.trackingSequence>sequence_
       ||frame.controllers.referenceEpoch!=controllers_.referenceEpoch)return false;
    frame.rigSequence=++rigSequence_;rig_={camera,owner,sourceCamera,frame};return true;
}
HeadCameraSample HeadCamera::resolveLocked(uintptr_t camera,Pose nativePose,uint64_t time,bool useRig){
    const auto sourceCamera=nativePose;
    HeadCameraSample result{nativePose,head_,sequence_,activation_,false};
    result.views=views_;result.sampleTime=time_;result.stereoTracked=stereoTracking_;
    if(!enabled_||!camera||!valid(nativePose))return result;
    if(!tracking_){suspendLocked(HeadCameraStop::trackingLost);return result;}
    if(time<time_){cancelLocked(HeadCameraStop::clockMismatch);return result;}
    if(time-time_>150){suspendLocked(HeadCameraStop::staleTracking);return result;}
    const bool spatialMenu=nativeMenuOpen_&&menuAnchored_&&active_;
    if(nativeMenuOpen_&&!spatialMenu){awaitPlayerLocked();return result;}
    if(spatialMenu){
        if(camera!=camera_)return result;
        nativePose=menuNative_;
        result.menuOpen=true;result.menuPanel=menuPanel_;
        result.playerOwner=playerOwner_;result.playerHead=lastView_.playerHead;
        result.playerSequence=lastView_.playerSequence;
    }else if(requirePlayerHead_&&(pending_||active_||awaitingPlayer_)){
        const auto same=[](Pose a,Pose b){
            return a.position.x==b.position.x&&a.position.y==b.position.y&&a.position.z==b.position.z
                &&a.orientation.x==b.orientation.x&&a.orientation.y==b.orientation.y
                &&a.orientation.z==b.orientation.z&&a.orientation.w==b.orientation.w;
        };
        const auto found=std::find_if(playerHeads_.begin(),playerHeads_.end(),[&](const auto& p){
            return p.camera==camera&&p.sequence&&time>=p.time&&time-p.time<=150&&same(p.sourceCamera,nativePose);
        });
        if(found==playerHeads_.end()){awaitPlayerLocked();return result;}
        if(awaitingPlayer_){
            if((camera_&&camera_!=camera)||(playerOwner_&&playerOwner_!=found->owner))return result;
            if(!camera_)origin_=uprightOrigin(head_);
            camera_=camera;playerOwner_=found->owner;awaitingPlayer_=false;active_=true;
            ++activation_; // No eye image from before the menu may be reused.
        }
        nativePose.position=found->position;
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
    if(controllers_.frontEnd)result.menuPanel=frontEndPanel_;
    // The native camera can look down, lean, recoil or bank. Its yaw supplies
    // gameplay heading; gravity and physical HMD pitch/roll supply the VR view.
    // A full native/activation rotation tilts the tracking space when turning.
    if(!spatialMenu){
        nativePose=controllers_.frontEnd?frontEndOrigin_:uprightOrigin(nativePose);
        if(recenterPending_){
            const float baseYaw=horizontalYaw(nativePose.orientation)+controllers_.snapYaw;
            const float facing=lastView_.applied?horizontalYaw(lastView_.nativePose.orientation):baseYaw;
            origin_={yawRotation(horizontalYaw(head_.orientation)-facing+baseYaw),head_.position};
            snapTranslation_={};snapYaw_=controllers_.snapYaw;recenterPending_=false;rig_={};++activation_;
        }
    }
    if(useRig&&rig_.camera){
        const auto& s=rig_.sample;const auto& p=rig_.sourceCamera;
        const bool same=p.position.x==sourceCamera.position.x&&p.position.y==sourceCamera.position.y&&p.position.z==sourceCamera.position.z
            &&p.orientation.x==sourceCamera.orientation.x&&p.orientation.y==sourceCamera.orientation.y
            &&p.orientation.z==sourceCamera.orientation.z&&p.orientation.w==sourceCamera.orientation.w;
        if(rig_.camera!=camera||rig_.owner!=result.playerOwner||s.activation!=activation_||!same
           ||time<s.sampleTime||time-s.sampleTime>150){suspendLocked(HeadCameraStop::rigFrameMismatch);return result;}
        const auto playerPublication=result.playerSequence;
        result=s;result.playerSequence=playerPublication;suspended_=false;reason_=HeadCameraStop::none;lastView_=result;return result;
    }
    suspended_=false;reason_=HeadCameraStop::none;
    // FOX's camera-local forward/right candidates are +Z/-X. The explicit
    // 180-degree basis rotation keeps handedness intact; validate in live motion.
    const Pose basis{{0,1,0,0},{}};
    auto relative=compose(inverse(spatialMenu?menuHead_:origin_),head_);
    relative.position=relative.position*units_;
    const auto nativeDelta=compose(compose(basis,relative),inverse(basis));
    result.nativePose=compose(nativePose,nativeDelta);
    if(!spatialMenu){
        const float turn=std::isfinite(controllers_.snapYaw)?controllers_.snapYaw:0;
        const Quat rotation{0,std::sin(turn*.5f),0,std::cos(turn*.5f)};
        const auto offset=result.nativePose.position-nativePose.position;
        if(turn!=snapYaw_){
            const Quat previous{0,std::sin(snapYaw_*.5f),0,std::cos(snapYaw_*.5f)};
            // Turn around the current physical head, including a roomscale
            // lean. The snap must not orbit the head around the tracking origin.
            snapTranslation_=snapTranslation_+rotate(previous,offset)-rotate(rotation,offset);
            snapYaw_=turn;
        }
        result.nativePose.position=nativePose.position+rotate(rotation,offset)+snapTranslation_;
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
    if(result.applied){result.controllers=controllers_;lastView_=result;}
    return result;
}
HeadCamera& headCamera(){static auto* instance=new HeadCamera;return *instance;}
}
