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
std::optional<Pose> upperBodyPlacement(Pose chest,Vec3 shoulderCenter,Pose uprightHead){
    if(!valid(chest)||!valid(uprightHead)||!valid(Pose{{},shoulderCenter}))return {};
    // Place the shoulder line behind the eyes. The former 6 cm setback exposed
    // the open shoulder ends of the native shirt when the elbows were bent.
    const auto target=compose(uprightHead,Pose{{},{0,-.18f,-.16f}});
    return compose(target,inverse(Pose{chest.orientation,shoulderCenter}));
}
std::optional<ArmSolution> solveArm(const ArmPose& a,Pose target,Vec3 hint,const ArmBasis* basis){
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
    const auto elbow=a.shoulder.position+forward*along+unit(bend)*away;
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
bool SupportContact::update(bool ready,bool tracked,float distance){
    if(!tracked||!std::isfinite(distance)||distance<0)attached_=false;
    else if(ready)attached_=distance<(attached_?0.45f:0.30f);
    // Lowering for selection releases the rendered hand but retains contact
    // intent. The newly readied weapon still has to be within release range.
    return ready&&attached_;
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
