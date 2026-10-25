#include "mgs5vr/arm_ik.hpp"
#include <algorithm>
#include <cmath>

namespace mgs5vr {
std::optional<std::array<size_t,14>> armHelperIndices(std::span<const uint32_t> names,
    std::span<const int32_t> parents){
    if(names.empty()||names.size()!=parents.size())return {};
    std::array<size_t,14> result{};
    for(size_t helper=0;helper<armHelperNames.size();++helper){
        bool found=false;
        for(size_t i=0;i<names.size();++i)if(names[i]==armHelperNames[helper]){
            if(found||parents[i]!=armHelperParents[helper])return {};
            result[helper]=i;found=true;
        }
        if(!found)return {};
    }
    return result;
}
Quat ArmTwistState::update(Quat wrist,uint64_t time,RigContinuityKey key){
    if(!valid(Pose{wrist,{}})){*this={};return {};}
    const float n=std::hypot(wrist.x,wrist.w);
    float angle=n>.0001f?2.f*std::atan2(wrist.x,wrist.w):angle_;
    angle=std::remainder(angle,6.283185307f);
    if(sampled_&&key==key_&&time>=time_&&time-time_<=250)
        angle=angle_+std::remainder(angle-angle_,6.283185307f);
    angle_=angle;key_=key;time_=time;sampled_=true;
    // Both arms share the authored local X axis. Never let controller roll
    // turn the skin through a full revolution or jump at the quaternion seam.
    const float bounded=std::clamp(angle,-1.745329252f,1.745329252f);
    Quat twist{n>.0001f?wrist.x/n:0,0,0,n>.0001f?wrist.w/n:1};
    auto swing=compose(Pose{wrist,{}},inverse(Pose{twist,{}})).orientation;
    if(swing.w<0)swing={-swing.x,-swing.y,-swing.z,-swing.w};
    const float sine=std::sqrt(swing.x*swing.x+swing.y*swing.y+swing.z*swing.z);
    const float half=std::min(std::atan2(sine,swing.w),.698131701f);
    if(sine>.0001f){const float gain=std::sin(half)/sine;swing={swing.x*gain,swing.y*gain,swing.z*gain,std::cos(half)};}
    return compose(Pose{swing,{}},Pose{Quat{std::sin(bounded*.5f),0,0,std::cos(bounded*.5f)},{}}).orientation;
}
std::optional<HandPresentationPose> HandPresentationCache::update(HandPresentationPose pose,bool tracked,
    bool focused,uint64_t time,RigContinuityKey key){
    if(!focused||!key.owner||!key.model||!key.activation||!key.epoch||!key.presentationEpoch||!time
       ||(tracked&&(!valid(pose.grip)||!valid(pose.aim)))){reset();return {};}
    if(key!=key_||time<sampleAt_){reset();key_=key;}
    sampleAt_=time;
    if(tracked){
        if(!tracked_&&valid_){recoveryFrom_=last_;recoveryAt_=time;}
        if(recoveryAt_){
            const float t=std::min(1.f,float(time-recoveryAt_)/100.f);
            const auto blend=[t](Pose from,Pose to){
                auto q=to.orientation;const auto a=from.orientation;
                if(a.x*q.x+a.y*q.y+a.z*q.z+a.w*q.w<0)q={-q.x,-q.y,-q.z,-q.w};
                q={a.x+(q.x-a.x)*t,a.y+(q.y-a.y)*t,a.z+(q.z-a.z)*t,a.w+(q.w-a.w)*t};
                const float n=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
                return Pose{Quat{q.x/n,q.y/n,q.z/n,q.w/n},from.position+(to.position-from.position)*t};
            };
            pose={blend(recoveryFrom_.grip,pose.grip),blend(recoveryFrom_.aim,pose.aim)};
            if(t==1)recoveryAt_=0;
        }
        last_=pose;valid_=tracked_=true;return last_;
    }
    tracked_=false;recoveryAt_=0;
    // Idle controllers may remain asleep indefinitely. A continuous stream
    // of current owner/focus/epoch publications keeps only their visual pose.
    // A scheduling stall does not change that space or grant input authority;
    // fresh head/eye admission still owns every subsequent rig publication.
    return valid_?std::optional<HandPresentationPose>{last_}:std::nullopt;
}
bool StationaryBodyState::update(bool eligible,Vec3 root,uint64_t time,RigContinuityKey key){
    if(!eligible||!valid(Pose{{},root})||!key.owner||!key.model||!key.activation||!key.epoch){reset();return false;}
    if(key!=key_||time<time_||time-time_>250){reset();key_=key;}
    if(valid_&&time>time_){
        const float dt=float(time-time_)*.001f;
        const float speed=std::hypot(root.x-root_.x,root.z-root_.z)/dt;
        if(speed>.30f){moved_=true;active_=false;stoppedAt_=0;}
        else if(speed<.10f){
            if(!stoppedAt_)stoppedAt_=time;
            if(moved_&&time-stoppedAt_>=100)active_=true;
        }else stoppedAt_=0;
    }
    root_=root;time_=time;valid_=true;return active_;
}
std::optional<Pose> stationaryLowerBodyDelta(const StationaryBodyPose& b,Pose torsoDelta){
    for(const auto p:{b.pelvis,b.leftHip,b.rightHip,b.leftShoulder,b.rightShoulder})if(!valid(Pose{{},p}))return {};
    if(!valid(b.chest)||!valid(torsoDelta))return {};
    const auto turned=rotate(torsoDelta.orientation,{0,0,1});
    const auto hips=b.leftHip-b.rightHip,shoulders=b.leftShoulder-b.rightShoulder;
    if(turned.x*turned.x+turned.z*turned.z<.0001f||hips.x*hips.x+hips.z*hips.z<.0025f
        ||shoulders.x*shoulders.x+shoulders.z*shoulders.z<.0025f)return {};
    const auto wrap=[](float angle){return std::remainder(angle,6.283185307f);};
    const float pelvisYaw=std::atan2(-hips.z,hips.x),shoulderYaw=std::atan2(-shoulders.z,shoulders.x);
    const float torsoTurn=std::atan2(turned.x,turned.z);
    const float stance=std::clamp(wrap(pelvisYaw-shoulderYaw),-.1745f,.1745f);
    const float delta=wrap(shoulderYaw+torsoTurn+stance-pelvisYaw);
    const Quat yaw{0,std::sin(delta*.5f),0,std::cos(delta*.5f)};
    const Vec3 pivot{b.pelvis.x,0,b.pelvis.z};
    const auto chestAfter=compose(torsoDelta,b.chest);
    const Quat torsoYaw{0,std::sin(torsoTurn*.5f),0,std::cos(torsoTurn*.5f)};
    const auto offset=rotate(torsoYaw,Vec3{b.chest.position.x-b.pelvis.x,0,b.chest.position.z-b.pelvis.z});
    Vec3 shift{chestAfter.position.x-offset.x-b.pelvis.x,0,chestAfter.position.z-offset.z-b.pelvis.z};
    const float distance=std::hypot(shift.x,shift.z);
    if(distance>.50f)shift=shift*(.50f/distance);
    return Pose{yaw,pivot-rotate(yaw,pivot)+shift};
}
namespace {
Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
float length(Vec3 v){return std::sqrt(dot(v,v));}
Vec3 unit(Vec3 v){return v*(1/length(v));}
Quat normalize(Quat q){const float n=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);return {q.x/n,q.y/n,q.z/n,q.w/n};}
Quat swing(Vec3 a,Vec3 b){
    a=unit(a);b=unit(b);const float d=std::clamp(dot(a,b),-1.0f,1.0f);
    if(d<-0.9999f){const auto axis=unit(cross(a,std::abs(a.x)<0.8f?Vec3{1,0,0}:Vec3{0,1,0}));return {axis.x,axis.y,axis.z,0};}
    const auto axis=cross(a,b);return normalize({axis.x,axis.y,axis.z,1+d});
}
Quat turn(Quat delta,Quat q){return normalize(compose(Pose{delta,{}},Pose{q,{}}).orientation);}
std::optional<Quat> basisRotation(Vec3 localAxis,Vec3 localUp,Vec3 axis,Vec3 up){
    const auto frame=[](Vec3 x,Vec3 y)->std::optional<Pose>{
        if(length(x)<0.001f)return {};
        x=unit(x);y=y-x*dot(x,y);
        if(length(y)<0.001f)return {};
        y=unit(y);const auto z=cross(x,y);
        return nativeAffinePose({x.x,x.y,x.z,0,y.x,y.y,y.z,0,z.x,z.y,z.z,0,0,0,0,1});
    };
    const auto local=frame(localAxis,localUp),world=frame(axis,up);
    if(!local||!world)return {};
    return compose(*world,inverse(*local)).orientation;
}
}
std::optional<Vec3> outsideArmSurface(Vec3 point,const ArmSurface& surface){
    if(!valid(Pose{{},point})||!valid(Pose{{},surface.point})||!valid(Pose{{},surface.normal})
       ||std::abs(dot(surface.normal,surface.normal)-1.f)>.003f
       ||!std::isfinite(surface.clearance)||surface.clearance<0||surface.clearance>.15f)return {};
    return point+surface.normal*std::max(0.f,surface.clearance-dot(point-surface.point,surface.normal));
}
std::optional<Pose> weaponAimWithPitch(Pose aim,float pitchDegrees){
    if(!valid(aim)||!std::isfinite(pitchDegrees)||std::abs(pitchDegrees)>90.f)return {};
    const float half=pitchDegrees*3.14159265358979323846f/360.f;
    return compose(aim,Pose{{std::sin(half),0,0,std::cos(half)}, {}});
}
std::optional<Pose> aimedWeaponGrip(Pose primary,Pose aim,Vec3 forwardInPrimary){
    if(!valid(primary)||!valid(aim)||!valid(Pose{{},forwardInPrimary}))return {};
    const float axisLength=length(forwardInPrimary);
    if(axisLength<.001f||axisLength>2.f)return {};
    const auto current=rotate(primary.orientation,forwardInPrimary);
    const auto requested=rotate(aim.orientation,{0,0,-1});
    // Reject a reversed/unrelated muzzle frame instead of flipping the wrist.
    if(dot(unit(current),unit(requested))<-.8f)return {};
    return Pose{turn(swing(current,requested),primary.orientation),primary.position};
}
std::optional<Pose> twoHandGrip(Pose primary,Pose support,Vec3 forwardInPrimary,float influence){
    if(!valid(primary)||!valid(support)||!valid(Pose{{},forwardInPrimary})
       ||!std::isfinite(influence)||influence<0||influence>1)return {};
    const auto requested=support.position-primary.position;
    const float distance=length(requested),axisLength=length(forwardInPrimary);
    // Coincident/crossed controllers must not flip the sights or produce a
    // singular solve. The caller retains one-handed aim in these cases.
    if(distance<.12f||distance>1.1f||axisLength<.001f||axisLength>2.f)return {};
    const auto current=rotate(primary.orientation,forwardInPrimary);
    if(dot(unit(current),unit(requested))<-.8f)return {};
    auto delta=swing(current,requested);
    if(delta.w<0)delta={-delta.x,-delta.y,-delta.z,-delta.w};
    delta=normalize({delta.x*influence,delta.y*influence,delta.z*influence,1+(delta.w-1)*influence});
    return Pose{turn(delta,primary.orientation),primary.position};
}
std::optional<Pose> twoHandSupportGrip(Pose primary,Pose support,Pose contactInPrimary,float influence){
    if(!valid(contactInPrimary))return {};
    return twoHandGrip(primary,support,contactInPrimary.position,influence);
}
float freeFingerCurl(unsigned finger,float trigger,float squeeze,bool triggerTouched,bool thumbTouched,float resting,float touched){
    const float contact=finger==0?(thumbTouched?touched:resting):finger==1?(triggerTouched?touched:resting):resting;
    return std::clamp(std::max(contact,finger==1?trigger:squeeze),0.f,1.f);
}
std::optional<Quat> fingerJointRotation(bool right,unsigned finger,unsigned joint,float curl){
    if(finger>=5||joint>=3||!std::isfinite(curl)||curl<0||curl>1)return {};
    const float side=right?1.f:-1.f;
    if(finger==0){
        // The thumb's authored chain already slopes toward the palm. Its
        // hinge closes across the palm about Y, unlike the fingers' Z curl.
        // Applying finger flexion here folds the thumb backward at the wrist.
        constexpr float across[3]{35,30,35};
        const float angle=-side*across[joint]*curl*.00872664626f;
        return Quat{0,std::sin(angle),0,std::cos(angle)};
    }
    constexpr float flexion[5][3]={{35,55,60},{70,85,50},{80,95,60},{80,95,60},{85,95,60}};
    const float angle=side*flexion[finger][joint]*curl*.00872664626f;
    return Quat{0,0,std::sin(angle),std::cos(angle)};
}
std::optional<Pose> upperBodyPlacement(Pose chest,Vec3 shoulderCenter,Pose uprightHead){
    if(!valid(chest)||!valid(uprightHead)||!valid(Pose{{},shoulderCenter}))return {};
    // Coco's fitted shoulder line: lower, with less backward displacement.
    const auto target=compose(uprightHead,Pose{{},{0,-.23f,-.04f}});
    return compose(target,inverse(Pose{chest.orientation,shoulderCenter}));
}
std::optional<Quat> anatomicalTorsoHeading(Quat locomotionHeading,Quat view,bool prone){
    if(!valid(Pose{locomotionHeading,{}})||!valid(Pose{view,{}}))return {};
    const auto yaw=[](Quat q){
        const auto forward=rotate(q,{0,0,1}),right=rotate(q,{1,0,0});
        return std::hypot(forward.x,forward.z)>.001f
            ?std::atan2(forward.x,forward.z):std::atan2(-right.z,right.x);
    };
    float heading=yaw(locomotionHeading);
    if(!prone){
        constexpr float neckYaw=.872664626f; // 50 degrees; the torso owns the excess.
        const float look=std::remainder(yaw(view)-heading,6.283185307f);
        heading+=look-std::clamp(look,-neckYaw,neckYaw);
    }
    return Quat{0,std::sin(heading*.5f),0,std::cos(heading*.5f)};
}
std::optional<Quat> trackedTorsoHeading(Pose viewer,Vec3 handsMidpoint,Quat primaryAim){
    if(!valid(viewer)||!valid(Pose{primaryAim,handsMidpoint}))return {};
    const auto placement=handsMidpoint-viewer.position;
    const auto look=rotate(viewer.orientation,{0,0,1});
    const auto direction=rotate(primaryAim,{0,0,-1});
    const float distance=std::hypot(placement.x,placement.z);
    const float lookDistance=std::hypot(look.x,look.z),aimDistance=std::hypot(direction.x,direction.z);
    if(distance<=.2f||lookDistance<=.1f||aimDistance<=.1f
       ||placement.x*look.x+placement.z*look.z<=distance*lookDistance*.5f
       ||direction.x*look.x+direction.z*look.z<=aimDistance*lookDistance*.5f)return {};
    const float yaw=std::atan2(direction.x,direction.z);
    return Quat{0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)};
}
std::optional<Quat> BodyHeadingState::update(Quat locomotion,Quat view,bool prone,uint64_t time,RigContinuityKey key,
                                          std::optional<Quat> handHeading){
    if(!valid(Pose{locomotion,{}})||!valid(Pose{view,{}})||!time){reset();return {};}
    const auto yaw=[](Quat q){const auto f=rotate(q,{0,0,1}),r=rotate(q,{1,0,0});
        return std::hypot(f.x,f.z)>.001f?std::atan2(f.x,f.z):std::atan2(-r.z,r.x);};
    const auto wrap=[](float a){return std::remainder(a,6.283185307f);};
    const float base=yaw(locomotion),look=yaw(view);
    if(!valid_||key!=key_||time<time_){heading_=base;locomotion_=base;valid_=true;key_=key;}
    heading_=wrap(heading_+wrap(base-locomotion_));
    if(prone)heading_=base;
    else{
        // Both hands moving around the viewer identify a body turn. Keep
        // shoulder reach close to that direction while retaining independent
        // neck look when hands stay in their existing world direction.
        if(handHeading&&valid(Pose{*handHeading,{}})){
            const float delta=wrap(yaw(*handHeading)-heading_);
            heading_=wrap(heading_+delta);
        }
        constexpr float neckYaw=.872664626f;
        const float delta=wrap(look-heading_);
        heading_=wrap(heading_+delta-std::clamp(delta,-neckYaw,neckYaw));
    }
    locomotion_=base;time_=time;
    return Quat{0,std::sin(heading_*.5f),0,std::cos(heading_*.5f)};
}
std::optional<Pose> fullBodyPlacement(Pose chest,Pose nativeHead,Pose uprightHead){
    if(!valid(chest)||!valid(uprightHead)||!valid(nativeHead))return {};
    const auto nativeHeading=nativeTorsoHeading(chest);if(!nativeHeading)return {};
    const auto forward=rotate(*nativeHeading,{0,0,1});
    const auto heading=rotate(uprightHead.orientation,{0,0,1});
    if(std::hypot(heading.x,heading.z)<.001f)return {};
    const float oldYaw=std::atan2(forward.x,forward.z);
    const float yaw=std::remainder(std::atan2(heading.x,heading.z)-oldYaw,6.283185307f);
    const Quat turn{0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)};
    // Anchor the measured eyes, including the authored prone head pitch.
    // Looking down must not use a yaw-only offset to move the head joint.
    const auto nativeEye=compose(nativeHead,Pose{{},nativeEyeFromHead}).position;
    return Pose{turn,uprightHead.position-rotate(turn,nativeEye)};
}
std::optional<Quat> nativeTorsoHeading(Pose chest){
    if(!valid(chest))return {};
    const auto right=rotate(chest.orientation,{1,0,0}),forward=rotate(chest.orientation,{0,0,1});
    const float yaw=std::hypot(right.x,right.z)>.1f?std::atan2(-right.z,right.x)
        :std::atan2(forward.x,forward.z);
    return Quat{0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)};
}
std::optional<ArmSurface> headClearanceSurface(Vec3 point,Vec3 eye,float radius){
    if(!valid(Pose{{},point})||!valid(Pose{{},eye})||!std::isfinite(radius)||radius<=0||radius>.2f)return {};
    const auto delta=point-eye;const float distance=length(delta);
    if(distance>=radius)return {};
    const auto normal=distance>.001f?delta*(1/distance):Vec3{0,0,1};
    // A tangent plane at the camera envelope keeps the solved elbow circle
    // outside the eye without changing either bone length or the HMD pose.
    return ArmSurface{eye+normal*radius,normal,0};
}
std::optional<ArmSurface> armHeadClearanceSurface(const ArmPose& arm,Vec3 eye,float radius){
    const auto nearest=[&](Vec3 a,Vec3 b){
        const auto line=b-a;const float squared=dot(line,line);
        return a+line*(squared>1e-8f?std::clamp(dot(eye-a,line)/squared,0.f,1.f):0.f);
    };
    const auto upper=nearest(arm.shoulder.position,arm.elbow.position);
    const auto lower=nearest(arm.elbow.position,arm.wrist.position);
    return headClearanceSurface(dot(upper-eye,upper-eye)<dot(lower-eye,lower-eye)?upper:lower,eye,radius);
}
std::optional<ArmSurface> headClearanceSurface(Vec3 point,Vec3 leftEye,Vec3 rightEye,float radius){
    if(!valid(Pose{{},point})||!valid(Pose{{},leftEye})||!valid(Pose{{},rightEye}))return {};
    // The capsule joining the actual eyes preserves the existing clearance
    // at either eye, including asymmetric runtime eye offsets. Its tangent
    // plane supports the complete capsule, so a constrained shoulder cannot
    // clear one eye by moving into the other.
    const auto axis=rightEye-leftEye;const float squared=dot(axis,axis);
    if(squared<1e-8f)return headClearanceSurface(point,leftEye,radius);
    const auto closest=leftEye+axis*std::clamp(dot(point-leftEye,axis)/squared,0.f,1.f);
    auto surface=headClearanceSurface(point,closest,radius);
    if(surface&&length(point-closest)<=.001f){
        const auto direction=unit(axis);
        const auto normal=unit(cross(direction,std::abs(direction.y)<.8f?Vec3{0,1,0}:Vec3{1,0,0}));
        surface=ArmSurface{closest+normal*radius,normal,0};
    }
    return surface;
}
std::optional<ArmSurface> armHeadClearanceSurface(const ArmPose& arm,Vec3 leftEye,Vec3 rightEye,float radius,float sleeveRadius){
    // Closest points of each bone and the eye segment, including endpoint
    // minima. The resulting plane supports the entire stereo capsule.
    const auto nearest=[&](Vec3 a,Vec3 b){
        const auto u=b-a,v=rightEye-leftEye,w=a-leftEye;
        const float aa=dot(u,u),bb=dot(u,v),cc=dot(v,v),dd=dot(u,w),ee=dot(v,w);
        if(aa<1e-8f)return a;
        if(cc<1e-8f)return a+u*std::clamp(-dd/aa,0.f,1.f);
        const float determinant=aa*cc-bb*bb;
        float s=determinant>1e-8f?std::clamp((bb*ee-cc*dd)/determinant,0.f,1.f):0.f;
        const float t=(bb*s+ee)/cc;
        if(t<0)s=std::clamp(-dd/aa,0.f,1.f);
        else if(t>1)s=std::clamp((bb-dd)/aa,0.f,1.f);
        return a+u*s;
    };
    const auto upper=nearest(arm.shoulder.position,arm.elbow.position);
    const auto lower=nearest(arm.elbow.position,arm.wrist.position);
    const auto upperSurface=headClearanceSurface(upper,leftEye,rightEye,std::max(radius,sleeveRadius));
    const auto lowerSurface=headClearanceSurface(lower,leftEye,rightEye,radius);
    if(!upperSurface)return lowerSurface;
    if(!lowerSurface)return upperSurface;
    return dot(upper-upperSurface->point,upperSurface->normal)<dot(lower-lowerSurface->point,lowerSurface->normal)
        ?upperSurface:lowerSurface;
}
std::optional<ArmSolution> solveSpine(const ArmPose& native,Pose neckTarget,const ArmSurface* chestSurface){
    const auto hint=native.elbow.position-native.shoulder.position;
    // Lumbar -> chest -> neck is the anatomical chain. WAIST -> SPINE is
    // an authored 3 mm pivot, not an independently solvable torso segment.
    // The caller retains the pelvis and the lumbar anchor's position.
    auto result=solveArm(native,neckTarget,hint,nullptr,chestSurface);
    if(!result)return {};
    const auto requested=compose(Pose{neckTarget.orientation,{}},inverse(Pose{native.wrist.orientation,{}})).orientation;
    const auto fit=[&](Pose joint,Vec3 nativeAxis,Vec3 solvedAxis){
        const auto localAxis=rotate(inverse(Pose{joint.orientation,{}}).orientation,nativeAxis);
        const auto requestedOrientation=turn(requested,joint.orientation);
        return basisRotation(localAxis,{0,0,1},solvedAxis,rotate(requestedOrientation,{0,0,1}));
    };
    const auto lumbar=fit(native.shoulder,native.elbow.position-native.shoulder.position,
                         result->pose.elbow.position-result->pose.shoulder.position);
    const auto chest=fit(native.elbow,native.wrist.position-native.elbow.position,
                        result->pose.wrist.position-result->pose.elbow.position);
    if(!lumbar||!chest)return {};
    result->pose.shoulder.orientation=*lumbar;result->pose.elbow.orientation=*chest;
    return result;
}
namespace {
std::optional<ArmSolution> finishArm(const ArmPose& a,Pose target,Vec3 elbow,Vec3 wrist,const ArmBasis* basis,bool clamped){
    const auto upper=a.elbow.position-a.shoulder.position,lower=a.wrist.position-a.elbow.position;
    ArmSolution out{a,clamped};
    out.pose.shoulder.orientation=turn(swing(upper,elbow-a.shoulder.position),a.shoulder.orientation);
    auto forearmRotation=turn(swing(lower,wrist-elbow),a.elbow.orientation);
    if(basis){
        // The native elbow is a hinge. Pronation belongs to its downstream
        // twist helpers; rotating the elbow itself knots the upper sleeve.
        const auto upperRotation=basisRotation(basis->upperAxis,basis->elbowBend,elbow-a.shoulder.position,wrist-elbow);
        if(!upperRotation)return {};
        out.pose.shoulder.orientation=*upperRotation;
        const auto axis=unit(wrist-elbow);
        const auto up=rotate(*upperRotation,basis->wristUp);
        const auto lowerRotation=basisRotation(basis->forearmAxis,basis->wristUp,axis,up);
        if(!lowerRotation)return {};
        forearmRotation=*lowerRotation;
    }
    out.pose.elbow={forearmRotation,elbow};
    out.pose.wrist={normalize(target.orientation),wrist};
    return out;
}
}
std::optional<ArmSolution> solveArm(const ArmPose& a,Pose target,Vec3 hint,const ArmBasis* basis,const ArmSurface* surface){
    if(!valid(a.shoulder)||!valid(a.elbow)||!valid(a.wrist)||!valid(target)||!valid(Pose{{},hint}))return {};
    const auto upper=a.elbow.position-a.shoulder.position,lower=a.wrist.position-a.elbow.position;
    const float u=basis?length(basis->upperAxis):length(upper);
    const float l=basis?length(basis->forearmAxis):length(lower);
    if(u<0.05f||l<0.05f||u>0.7f||l>0.7f)return {};
    auto ray=target.position-a.shoulder.position;float requested=length(ray);
    if(requested<0.0001f)ray=a.wrist.position-a.shoulder.position;
    if(length(ray)<0.0001f)return {};
    const auto forward=unit(ray);
    const float reach=std::clamp(requested,std::abs(u-l)+0.001f,u+l-0.001f);
    const float along=(u*u-l*l+reach*reach)/(2*reach);
    const float away=std::sqrt(std::max(0.0f,u*u-along*along));
    auto bend=hint-forward*dot(hint,forward);
    if(length(bend)<0.001f)bend=upper-forward*dot(upper,forward);
    if(length(bend)<0.001f)bend=cross(forward,std::abs(forward.y)<0.8f?Vec3{0,1,0}:Vec3{1,0,0});
    const auto center=a.shoulder.position+forward*along;
    bend=unit(bend);
    if(surface){
        if(!outsideArmSurface(center,*surface))return {};
        // Keep both bone lengths by choosing a feasible point on the elbow
        // circle. Translating a solved elbow would stretch the arm instead.
        const auto inCircle=surface->normal-forward*dot(surface->normal,forward);
        const float span=length(inCircle)*away;
        const float required=surface->clearance-dot(center-surface->point,surface->normal);
        if(required>span+.0001f)return {};
        if(span>.0001f&&required>dot(bend,surface->normal)*away){
            const auto towardSurface=unit(inCircle);
            const float cosine=std::clamp(required/span,-1.f,1.f);
            auto tangent=bend-towardSurface*dot(bend,towardSurface);
            if(length(tangent)<.0001f)tangent=cross(forward,towardSurface);
            bend=towardSurface*cosine+unit(tangent)*std::sqrt(std::max(0.f,1.f-cosine*cosine));
        }
    }
    const auto elbow=center+bend*away;
    const auto wrist=a.shoulder.position+forward*reach;
    return finishArm(a,target,elbow,wrist,basis,std::abs(requested-reach)>0.0001f);
}

std::optional<ArmSolution> solveArmAroundHead(const ArmPose& a,Pose target,Vec3 hint,
    const ArmBasis* basis,Vec3 eye,float elbowRadius,float wristRadius,float sleeveRadius){
    sleeveRadius=std::max(elbowRadius,sleeveRadius);
    const auto unconstrained=solveArm(a,target,hint,basis);
    if(!unconstrained||!valid(Pose{{},eye})||!std::isfinite(elbowRadius)||!std::isfinite(wristRadius)
       ||elbowRadius<=0||elbowRadius>.3f||wristRadius<=0||wristRadius>.3f
       ||!std::isfinite(sleeveRadius)||sleeveRadius>.3f)return {};
    // An extended arm can put its entire elbow circle inside the head.
    // In that case no bend can preserve the requested wrist. Clamp the reach
    // at the two sphere intersections instead of discarding the whole frame.
    const auto endpoint=[&](Vec3 center,float radius,Vec3 wanted,float clearance,float segmentClearance)->std::optional<Vec3>{
        auto direction=wanted-center;
        if(length(direction)<.00001f)return {};
        direction=unit(direction);
        const auto head=eye-center;
        const float distance=length(head);
        if(distance<.00001f)return radius>=clearance?std::optional<Vec3>{center+direction*radius}:std::nullopt;
        const auto axis=head*(1.f/distance);
        float limit=(radius*radius+distance*distance-clearance*clearance)/(2.f*radius*distance);
        // Clear the whole bone, not only its endpoint. Past the sphere's
        // tangent point, the ray cone is stricter than endpoint separation.
        // A source anchor already inside the envelope retains the endpoint
        // fallback; it cannot be repaired by rotating this bone alone.
        const float tangentSquared=distance*distance-segmentClearance*segmentClearance;
        if(distance>=segmentClearance-1e-5f&&radius*radius>=tangentSquared)
            limit=std::min(limit,std::sqrt(std::max(0.f,tangentSquared))/distance);
        if(limit< -1.f)return {};
        if(limit<1.f&&dot(direction,axis)>limit){
            auto tangent=direction-axis*dot(direction,axis);
            if(length(tangent)<.00001f)tangent=hint-axis*dot(hint,axis);
            if(length(tangent)<.00001f)tangent=cross(axis,std::abs(axis.y)<.8f?Vec3{0,1,0}:Vec3{1,0,0});
            direction=axis*limit+unit(tangent)*std::sqrt(std::max(0.f,1.f-limit*limit));
        }
        return center+direction*radius;
    };
    const auto elbow=endpoint(a.shoulder.position,basis?length(basis->upperAxis):length(a.elbow.position-a.shoulder.position),
        unconstrained->pose.elbow.position,sleeveRadius,sleeveRadius);
    if(!elbow)return {};
    const auto wrist=endpoint(*elbow,basis?length(basis->forearmAxis):length(a.wrist.position-a.elbow.position),target.position,wristRadius,elbowRadius);
    if(!wrist)return {};
    return finishArm(a,target,*elbow,*wrist,basis,true);
}
std::optional<Pose> reachingClavicle(Pose clavicle,const ArmPose& arm,Pose target,const ArmSurface* shoulderSurface){
    if(!valid(clavicle)||!valid(arm.shoulder)||!valid(arm.elbow)||!valid(arm.wrist)||!valid(target))return {};
    const float reach=length(arm.elbow.position-arm.shoulder.position)
        +length(arm.wrist.position-arm.elbow.position);
    const auto ray=target.position-arm.shoulder.position;
    const float distance=length(ray);
    if(reach<.1f||reach>.7f)return {};
    if(distance<=reach-.001f&&!shoulderSurface)return clavicle;
    if(distance<.001f)return {};
    // A virtual straight arm provides the remaining reach for the clavicle
    // solve. Its elbow is the real shoulder. The ordinary two-bone arm solve
    // subsequently restores the elbow bend and wrist orientation.
    auto straight=target;straight.position=arm.shoulder.position+ray*(reach/distance);
    const auto fit=solveArm({clavicle,arm.shoulder,straight},target,
        arm.shoulder.position-clavicle.position,nullptr,shoulderSurface);
    return fit?std::optional<Pose>{fit->pose.shoulder}:std::nullopt;
}
std::optional<GroundedLegSolution> solveGroundedLeg(const ArmPose& native,Pose contact){
    const auto leg=solveArm(native,contact,native.elbow.position-native.shoulder.position);
    return leg?std::optional<GroundedLegSolution>{{*leg,1.f}}:std::nullopt;
}
std::optional<Vec3> groundedPelvisShift(const ArmPose& spine,Pose neck,
    const std::array<ArmPose,2>& legs,const std::array<Pose,2>& feet){
    const std::array<ArmPose,3> chains{spine,legs[0],legs[1]};
    const std::array<Pose,3> targets{neck,feet[0],feet[1]};
    std::array<Vec3,3> centers{},residuals{};std::array<float,3> radii{};
    for(size_t i=0;i<3;++i){
        const auto& c=chains[i];
        if(!valid(c.shoulder)||!valid(c.elbow)||!valid(c.wrist)||!valid(targets[i]))return {};
        radii[i]=length(c.elbow.position-c.shoulder.position)+length(c.wrist.position-c.elbow.position)-.002f;
        if(radii[i]<.05f||radii[i]>1.5f)return {};
        centers[i]=targets[i].position-c.shoulder.position;
    }
    Vec3 shift{};
    // Dykstra projections retain the closest feasible pelvis position, rather
    // than moving each hip independently or stretching either leg.
    for(unsigned iteration=0;iteration<64;++iteration)for(size_t i=0;i<3;++i){
        const auto request=shift+residuals[i],delta=request-centers[i];
        const float distance=length(delta);
        const auto projected=distance>radii[i]?centers[i]+delta*(radii[i]/distance):request;
        residuals[i]=request-projected;shift=projected;
    }
    for(size_t i=0;i<3;++i)if(length(shift-centers[i])>radii[i]+.0005f)return {};
    return shift;
}
std::array<Quat,7> armCorrectiveRotations(Quat clavicle,Quat upper,Quat elbow,Quat wrist,bool right){
    const auto twist=[](Quat q,unsigned axis,float weight){
        if(q.w<0)q={-q.x,-q.y,-q.z,-q.w};
        const float component=axis==0?q.x:axis==1?q.y:q.z;
        const float angle=std::atan2(component,q.w)*weight;
        const float s=std::sin(angle);
        return Quat{axis==0?s:0,axis==1?s:0,axis==2?s:0,std::cos(angle)};
    };
    // These coefficients reproduce native corrective channels, including
    // different shoulder twist weights on the prosthetic and ordinary arms.
    return {twist(clavicle,2,-1),twist(upper,0,right?-.75f:-.70f),twist(upper,0,-.45f),
        twist(elbow,1,-.55f),twist(wrist,0,.35f),twist(wrist,0,.75f),twist(wrist,1,-.55f)};
}
std::array<float,2> armCorrectiveTranslations(Quat upper,Quat wrist,bool right){
    // The owned outfit's native helper driver projects the rotated authored
    // axis onto its measurement axis, scales/clamps it, then converts decimetres
    // to metres. Keeping the pre-IK translation stretches the sleeve whenever
    // the tracked arm differs from the native weapon/idle animation.
    const float shoulder=2.f*rotate(upper,{-1,0,0}).y;
    const float wristBulge=-.35f*rotate(wrist,{0,1,0}).x*(right?-1.f:1.f);
    return {std::clamp(shoulder,right?-2.f:.03f,right?-.03f:2.f)*.1f,
            std::clamp(wristBulge,0.f,99999.f)*.1f};
}
std::optional<Pose> forearmPanel(Pose elbow,Pose wrist,Vec3 dorsal,float surfaceLift){
    if(!valid(elbow)||!valid(wrist)||!valid(Pose{{},dorsal})
       ||!std::isfinite(surfaceLift)||surfaceLift<0||surfaceLift>.1f)return {};
    const auto segment=wrist.position-elbow.position;
    if(length(segment)<0.05f||length(segment)>0.7f)return {};
    const auto x=unit(segment);
    auto z=dorsal-x*dot(dorsal,x);
    if(length(z)<0.1f)return {};
    z=unit(z);const auto y=cross(z,x);
    // The mount sits immediately behind the hand, not partway up the sleeve.
    // A fixed setback keeps the wrist location consistent across arm lengths.
    constexpr float wristSetback=.015f,baseSurfaceClearance=.005f;
    const auto p=wrist.position-x*wristSetback+z*(baseSurfaceClearance+surfaceLift);
    return nativeAffinePose({x.x,x.y,x.z,0,y.x,y.y,y.z,0,z.x,z.y,z.z,0,p.x,p.y,p.z,1});
}
bool withinSupportCone(Vec3 separation,Vec3 forward){
    const float distance2=dot(separation,separation),forward2=dot(forward,forward);
    if(!std::isfinite(distance2)||!std::isfinite(forward2)||distance2<.0001f||forward2<.0001f)return false;
    // Allow 60 degrees of two-hand guidance. Moving alongside or behind the
    // firing hand is a release, even if the guided weapon follows that motion.
    return dot(separation,forward)>=.5f*std::sqrt(distance2*forward2);
}
bool closeSupportContact(Vec3 separation){
    const float distance2=dot(separation,separation);
    return std::isfinite(distance2)&&distance2>=.000025f&&distance2<=.18f*.18f;
}
bool SupportContact::update(bool ready,bool tracked,float distance,uint64_t time,float acquireRadius,float detachRadius){
    if(!ready||!tracked||!std::isfinite(distance)||distance<0||time<lastTime_
       ||!std::isfinite(acquireRadius)||!std::isfinite(detachRadius)||acquireRadius<.03f||detachRadius<acquireRadius+.019f||detachRadius>.60f){reset();return false;}
    lastTime_=time;
    if(attached_){if(distance<detachRadius)return true;reset();return false;}
    if(distance>=acquireRadius){candidate_=false;return false;}
    if(!candidate_){candidate_=true;since_=time;}
    if(time-since_>=150){attached_=true;candidate_=false;}
    return attached_;
}
std::optional<Pose> SupportPose::contact(std::optional<Pose> weaponInWrist) const{
    if(!attached_||(weaponInWrist&&!valid(*weaponInWrist)))return {};
    return compose(weaponAnchored_&&weaponInWrist?*weaponInWrist:weaponInWrist_,acquired_);
}
std::optional<Pose> SupportPose::update(Pose nativeOffset,bool attached,bool manipulating,std::optional<Pose> weaponInWrist){
    if(!valid(nativeOffset)||(weaponInWrist&&!valid(*weaponInWrist))){reset();return {};}
    if(attached){
        if(!attached_){
            weaponAnchored_=weaponInWrist.has_value();weaponInWrist_=weaponInWrist.value_or(Pose{});
            acquired_=compose(inverse(weaponInWrist_),nativeOffset);
        }else if(weaponAnchored_&&weaponInWrist)weaponInWrist_=*weaponInWrist;
        presented_=manipulating?nativeOffset:compose(weaponInWrist_,acquired_);
    }
    attached_=attached;
    return presented_;
}
std::optional<Pose> anatomicalGrip(Pose wrist,Vec3 indexKnuckle,Vec3 littleKnuckle){
    if(!valid(wrist)||!valid(Pose{{},indexKnuckle})||!valid(Pose{{},littleKnuckle}))return {};
    const auto across=littleKnuckle-indexKnuckle;
    const auto along=(indexKnuckle+littleKnuckle)*0.5f-wrist.position;
    const float width=length(across),palmLength=length(along);
    if(width<0.02f||width>0.13f||palmLength<0.035f||palmLength>0.16f)return {};
    // -Z runs from little finger to index finger. The winding of the native
    // mirrored hands gives +X into the right palm and away from the left palm.
    const auto z=unit(across),normal=cross(z,along);
    if(length(normal)<0.01f)return {};
    const auto x=unit(normal),y=cross(z,x);
    const auto center=wrist.position+along*0.55f;
    return nativeAffinePose({x.x,x.y,x.z,0,y.x,y.y,y.z,0,z.x,z.y,z.z,0,center.x,center.y,center.z,1});
}
std::optional<PointThrow> pointThrow(Pose renderedPalm,Pose gripFromAim,Vec3 nativeVelocity){
    if(!valid(renderedPalm)||!valid(gripFromAim)||!valid(Pose{{},nativeVelocity}))return {};
    const float speed=std::sqrt(dot(nativeVelocity,nativeVelocity));
    if(speed<.1f||speed>100.f)return {};
    auto direction=rotate(compose(renderedPalm,gripFromAim).orientation,{0,0,-1});
    const float length=std::sqrt(dot(direction,direction));
    if(!std::isfinite(length)||length<.9f||length>1.1f)return {};
    return PointThrow{renderedPalm.position,direction*(speed/length)};
}
std::optional<Pose> nativeAffinePose(const std::array<float,16>& m){
    for(float f:m)if(!std::isfinite(f))return {};
    if(std::abs(m[3])+std::abs(m[7])+std::abs(m[11])+std::abs(m[15]-1)>0.001f)return {};
    const Vec3 x{m[0],m[1],m[2]},y{m[4],m[5],m[6]},z{m[8],m[9],m[10]};
    if(std::abs(dot(x,x)-1)>0.003f||std::abs(dot(y,y)-1)>0.003f||std::abs(dot(z,z)-1)>0.003f
       ||std::abs(dot(x,y))>0.003f||std::abs(dot(x,z))>0.003f||std::abs(dot(y,z))>0.003f||dot(cross(x,y),z)<0.997f)return {};
    Quat q;const float trace=m[0]+m[5]+m[10];
    if(trace>0){const float s=std::sqrt(trace+1)*2;q={(m[6]-m[9])/s,(m[8]-m[2])/s,(m[1]-m[4])/s,s/4};}
    else if(m[0]>m[5]&&m[0]>m[10]){const float s=std::sqrt(1+m[0]-m[5]-m[10])*2;q={s/4,(m[4]+m[1])/s,(m[8]+m[2])/s,(m[6]-m[9])/s};}
    else if(m[5]>m[10]){const float s=std::sqrt(1+m[5]-m[0]-m[10])*2;q={(m[4]+m[1])/s,s/4,(m[9]+m[6])/s,(m[8]-m[2])/s};}
    else {const float s=std::sqrt(1+m[10]-m[0]-m[5])*2;q={(m[8]+m[2])/s,(m[9]+m[6])/s,s/4,(m[1]-m[4])/s};}
    return Pose{normalize(q),{m[12],m[13],m[14]}};
}
}
