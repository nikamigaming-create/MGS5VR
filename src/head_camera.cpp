#include "mgs5vr/head_camera.hpp"
#include "mgs5vr/input_bridge.hpp"
#include <cmath>
#include <stdexcept>

namespace mgs5vr {
void HeadCamera::configure(bool enabled,float units){
    if(!std::isfinite(units)||units<=0)throw std::invalid_argument("Camera scale must be finite and positive");
    std::lock_guard lock(mutex_);enabled_=enabled;units_=units;active_=pending_=false;camera_=0;reason_=HeadCameraStop::none;
}
void HeadCamera::track(Pose head,bool tracked,uint64_t time){
    std::lock_guard lock(mutex_);
    stereoTracking_=false;
    tracking_=tracked&&valid(head)&&(!time_||time>=time_);
    if(tracking_){head_=head;time_=time;++sequence_;}
    else cancelLocked(HeadCameraStop::trackingLost);
}
void HeadCamera::trackStereo(Pose head,const std::array<EyeView,2>& views,bool tracked,uint64_t time){
    std::lock_guard lock(mutex_);
    tracking_=tracked&&valid(head)&&(!time_||time>=time_);
    for(const auto& eye:views)tracking_=tracking_&&valid(eye.pose)&&valid(eye.fov);
    const auto separation=views[1].pose.position-views[0].pose.position;
    tracking_=tracking_&&dot(separation,separation)>0.0001f&&dot(separation,separation)<0.04f;
    stereoTracking_=tracking_;
    if(tracking_){head_=head;views_=views;time_=time;++sequence_;}
    else cancelLocked(HeadCameraStop::trackingLost);
}
void HeadCamera::toggle(){
    std::lock_guard lock(mutex_);
    if(!enabled_)return;
    if(active_||pending_)cancelLocked(HeadCameraStop::manual);
    else {pending_=true;reason_=HeadCameraStop::none;}
}
void HeadCamera::cancelLocked(HeadCameraStop reason){
    if(active_||pending_){reason_=reason;++cancellations_;}
    active_=pending_=false;camera_=0;
}
void HeadCamera::cancel(HeadCameraStop reason){std::lock_guard lock(mutex_);cancelLocked(reason);}
bool HeadCamera::available() const {std::lock_guard lock(mutex_);return enabled_;}
bool HeadCamera::active() const {std::lock_guard lock(mutex_);return active_;}
HeadCameraStatus HeadCamera::status() const {std::lock_guard lock(mutex_);return {enabled_,active_,pending_,reason_,cancellations_,activation_};}
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
    if(!tracking_){cancelLocked(HeadCameraStop::trackingLost);return result;}
    if(time<time_){cancelLocked(HeadCameraStop::clockMismatch);return result;}
    if(time-time_>150){cancelLocked(HeadCameraStop::staleTracking);return result;}
    if(pending_){camera_=camera;origin_=head_;pending_=false;active_=true;++activation_;}
    if(!active_)return result;
    if(camera_!=camera){cancelLocked(HeadCameraStop::cameraChanged);return result;}
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
