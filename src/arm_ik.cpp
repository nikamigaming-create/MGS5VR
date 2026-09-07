#include "mgs5vr/arm_ik.hpp"
#include <algorithm>
#include <cmath>

namespace mgs5vr {
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
    // Place the shoulder line behind the eyes. The former 6 cm setback exposed
    // the open shoulder ends of the native shirt when the elbows were bent.
    const auto target=compose(uprightHead,Pose{{},{0,-.18f,-.16f}});
    return compose(target,inverse(Pose{chest.orientation,shoulderCenter}));
}
std::optional<ArmSolution> solveArm(const ArmPose& a,Pose target,Vec3 hint,const ArmBasis* basis,const ArmSurface* surface){
    if(!valid(a.shoulder)||!valid(a.elbow)||!valid(a.wrist)||!valid(target)||!valid(Pose{{},hint}))return {};
    const auto upper=a.elbow.position-a.shoulder.position,lower=a.wrist.position-a.elbow.position;
    const float u=length(upper),l=length(lower);
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
    ArmSolution out{a,std::abs(requested-reach)>0.0001f};
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
std::optional<Pose> forearmPanel(Pose elbow,Pose wrist,Vec3 dorsal){
    if(!valid(elbow)||!valid(wrist)||!valid(Pose{{},dorsal}))return {};
    const auto segment=wrist.position-elbow.position;
    if(length(segment)<0.05f||length(segment)>0.7f)return {};
    const auto x=unit(segment);
    auto z=dorsal-x*dot(dorsal,x);
    if(length(z)<0.1f)return {};
    z=unit(z);const auto y=cross(z,x);
    const auto p=wrist.position-segment*0.35f+z*0.025f;
    return nativeAffinePose({x.x,x.y,x.z,0,y.x,y.y,y.z,0,z.x,z.y,z.z,0,p.x,p.y,p.z,1});
}
bool SupportContact::update(bool ready,bool tracked,float distance,uint64_t time){
    if(!ready||!tracked||!std::isfinite(distance)||distance<0||time<lastTime_){reset();return false;}
    lastTime_=time;
    if(attached_){if(distance<.20f)return true;reset();return false;}
    if(distance>=.10f){candidate_=false;return false;}
    if(!candidate_){candidate_=true;since_=time;}
    if(time-since_>=150){attached_=true;candidate_=false;}
    return attached_;
}
std::optional<Pose> SupportPose::update(Pose nativeOffset,bool attached,bool manipulating){
    if(!valid(nativeOffset)){reset();return {};}
    if(attached){
        if(!attached_)acquired_=nativeOffset;
        presented_=manipulating?nativeOffset:acquired_;
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
