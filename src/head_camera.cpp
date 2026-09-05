#include "mgs5vr/head_camera.hpp"
#include "mgs5vr/input_bridge.hpp"
#include <cmath>
#include <stdexcept>
#include <algorithm>

namespace mgs5vr {
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
    std::lock_guard lock(mutex_);enabled_=enabled;units_=units;active_=pending_=false;camera_=0;reason_=HeadCameraStop::none;
    requirePlayerHead_=requirePlayerHead;playerHeads_={};playerSequence_=0;suspended_=false;
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
    tracking_=tracked&&valid(head)&&(!time_||time>=time_);
    if(tracking_){head_=head;time_=time;++sequence_;}
    else suspendLocked(HeadCameraStop::trackingLost);
}
void HeadCamera::trackStereo(Pose head,const std::array<EyeView,2>& views,bool tracked,uint64_t time){
    std::lock_guard lock(mutex_);
    tracking_=tracked&&valid(head)&&(!time_||time>=time_);
    for(const auto& eye:views)tracking_=tracking_&&valid(eye.pose)&&valid(eye.fov);
    const auto separation=views[1].pose.position-views[0].pose.position;
    tracking_=tracking_&&dot(separation,separation)>0.0001f&&dot(separation,separation)<0.04f;
    stereoTracking_=tracking_;
    if(tracking_){head_=head;views_=views;time_=time;++sequence_;}
    else suspendLocked(HeadCameraStop::trackingLost);
}
void HeadCamera::toggle(){
    std::lock_guard lock(mutex_);
    if(!enabled_)return;
    if(active_||pending_)cancelLocked(HeadCameraStop::manual);
    else {pending_=true;reason_=HeadCameraStop::none;}
}
void HeadCamera::cancelLocked(HeadCameraStop reason){
    if(active_||pending_){reason_=reason;++cancellations_;}
    active_=pending_=suspended_=false;camera_=0;
}
void HeadCamera::suspendLocked(HeadCameraStop reason){
    if(active_||pending_){suspended_=true;reason_=reason;}
}
void HeadCamera::cancel(HeadCameraStop reason){std::lock_guard lock(mutex_);cancelLocked(reason);}
bool HeadCamera::available() const {std::lock_guard lock(mutex_);return enabled_;}
bool HeadCamera::active() const {std::lock_guard lock(mutex_);return active_;}
HeadCameraStatus HeadCamera::status() const {std::lock_guard lock(mutex_);return {enabled_,active_,pending_,reason_,cancellations_,activation_,suspended_};}
HeadCameraSample HeadCamera::resolve(uintptr_t camera,Pose nativePose,uint64_t time){
    std::lock_guard lock(mutex_);
    return resolveLocked(camera,nativePose,time);
}
HeadCameraSample HeadCamera::resolveCurrent(uintptr_t camera,Pose nativePose){
    std::lock_guard lock(mutex_);
    return resolveLocked(camera,nativePose,steadyMilliseconds());
}
HeadCameraSample HeadCamera::resolveLocked(uintptr_t camera,Pose nativePose,uint64_t time){
    HeadCameraSample result{nativePose,head_,sequence_,activation_,false};
    result.views=views_;result.sampleTime=time_;result.stereoTracked=stereoTracking_;
    if(!enabled_||!camera||!valid(nativePose))return result;
    if(!tracking_){suspendLocked(HeadCameraStop::trackingLost);return result;}
    if(time<time_){cancelLocked(HeadCameraStop::clockMismatch);return result;}
    if(time-time_>150){suspendLocked(HeadCameraStop::staleTracking);return result;}
    if(requirePlayerHead_&&(pending_||active_)){
        const auto same=[](Pose a,Pose b){
            return a.position.x==b.position.x&&a.position.y==b.position.y&&a.position.z==b.position.z
                &&a.orientation.x==b.orientation.x&&a.orientation.y==b.orientation.y
                &&a.orientation.z==b.orientation.z&&a.orientation.w==b.orientation.w;
        };
        const auto found=std::find_if(playerHeads_.begin(),playerHeads_.end(),[&](const auto& p){
            return p.camera==camera&&p.sequence&&time>=p.time&&time-p.time<=150&&same(p.sourceCamera,nativePose);
        });
        if(found==playerHeads_.end()){cancelLocked(HeadCameraStop::playerHeadUnavailable);return result;}
        nativePose.position=found->position;
        result.playerSequence=found->sequence;result.playerOwner=found->owner;result.playerHead=found->position;
    }
    if(pending_){camera_=camera;origin_=head_;pending_=false;active_=true;++activation_;}
    if(!active_)return result;
    if(camera_!=camera){cancelLocked(HeadCameraStop::cameraChanged);return result;}
    suspended_=false;reason_=HeadCameraStop::none;
    // FOX's camera-local forward/right candidates are +Z/-X. The explicit
    // 180-degree basis rotation keeps handedness intact; validate in live motion.
    const Pose basis{{0,1,0,0},{}};
    auto relative=compose(inverse(origin_),head_);
    relative.position=relative.position*units_;
    const auto nativeDelta=compose(compose(basis,relative),inverse(basis));
    result.nativePose=compose(nativePose,nativeDelta);
    // Native and runtime quaternions are accepted with a small norm tolerance.
    // Normalize after composition: the native inverse builder assumes a rigid
    // rotation, and even tiny norm errors are amplified by kilometer coordinates.
    auto& q=result.nativePose.orientation;
    const double length=std::sqrt(static_cast<double>(q.x)*q.x+static_cast<double>(q.y)*q.y
        +static_cast<double>(q.z)*q.z+static_cast<double>(q.w)*q.w);
    if(!std::isfinite(length)||length<0.5)return result;
    q={static_cast<float>(q.x/length),static_cast<float>(q.y/length),static_cast<float>(q.z/length),static_cast<float>(q.w/length)};
    result.activation=activation_;result.applied=valid(result.nativePose);
    return result;
}
HeadCamera& headCamera(){static auto* instance=new HeadCamera;return *instance;}
}
