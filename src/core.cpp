#include "mgs5vr/core.hpp"
#include <algorithm>
#include <cmath>

namespace mgs5vr {
Vec3 operator+(Vec3 a, Vec3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vec3 operator-(Vec3 a, Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vec3 operator*(Vec3 a, float b) { return {a.x*b,a.y*b,a.z*b}; }
float dot(Vec3 a, Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
static Vec3 cross(Vec3 a, Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
Vec3 rotate(Quat q, Vec3 v) {
    const Vec3 u{q.x,q.y,q.z};
    return v + cross(u, cross(u,v) + v*q.w)*2;
}
static Quat multiply(Quat a, Quat b) {
    return {a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
            a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
            a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
            a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};
}
Pose compose(Pose a, Pose b) { return {multiply(a.orientation,b.orientation),a.position+rotate(a.orientation,b.position)}; }
Pose inverse(Pose p) {
    const Quat q{-p.orientation.x,-p.orientation.y,-p.orientation.z,p.orientation.w};
    return {q,rotate(q,p.position*-1)};
}
bool valid(Pose p) {
    const auto q=p.orientation;
    const float n=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
    return std::isfinite(p.position.x)&&std::isfinite(p.position.y)&&std::isfinite(p.position.z)
        &&std::isfinite(n)&&std::abs(n-1)<0.002f;
}
Pose recenteredScreen(Pose head,float distance) {
    const auto f=rotate(head.orientation,{0,0,-1});
    const float yaw=std::atan2(-f.x,-f.z);
    Pose forward{{0,std::sin(yaw/2),0,std::cos(yaw/2)},head.position};
    return compose(forward,Pose{{},{0,0,-distance}});
}
void PoseHistory::reset(uint64_t epoch) { epoch_=epoch; newest_=0; entries_={}; }
bool PoseHistory::put(const PoseFrame& f) {
    if (!f.id.epoch || f.id.epoch!=epoch_ || !f.id.sequence || f.id.sequence<=newest_) return false;
    if (!valid(f.head)||!valid(f.leftGrip)||!valid(f.rightGrip)) return false;
    newest_=f.id.sequence;
    entries_[f.id.sequence%entries_.size()]=f;
    return true;
}
std::optional<PoseFrame> PoseHistory::find(FrameId id) const {
    const auto& f=entries_[id.sequence%entries_.size()];
    if (id.epoch!=epoch_ || !f || f->id!=id) return {};
    return f;
}
std::optional<Hit> intersectPanel(const Panel& p,Pose aim,float maxDistance) {
    if (!valid(p.pose)||!valid(aim)||!(p.widthMeters>0)||!(p.heightMeters>0)
        ||!std::isfinite(p.widthMeters)||!std::isfinite(p.heightMeters)
        ||!p.pixelWidth||!p.pixelHeight||!(maxDistance>0)||!std::isfinite(maxDistance)) return {};
    const Pose local=compose(inverse(p.pose),aim);
    const Vec3 direction=rotate(local.orientation,{0,0,-1});
    // Front of the panel is +Z; a ray must approach its front face.
    if (local.position.z<=0 || direction.z>=-0.00001f) return {};
    const float distance=-local.position.z/direction.z;
    if (distance>maxDistance) return {};
    const auto point=local.position+direction*distance;
    const float u=point.x/p.widthMeters+0.5f, v=0.5f-point.y/p.heightMeters;
    if (u<0||u>1||v<0||v>1) return {};
    return Hit{u,v,distance,
        std::min(static_cast<uint32_t>(u*static_cast<float>(p.pixelWidth)),p.pixelWidth-1),
        std::min(static_cast<uint32_t>(v*static_cast<float>(p.pixelHeight)),p.pixelHeight-1)};
}
WristState WristFocus::update(double t,bool tracking,bool ready,float facing,float distance) {
    if (!std::isfinite(t)||t<lastTime_) { state_=WristState::hidden; since_=0; lastTime_=-1; return state_; }
    lastTime_=t;
    const bool usable=tracking&&ready&&std::isfinite(facing)&&std::isfinite(distance)&&distance>=0;
    const bool enter=usable&&facing>=0.72f&&distance<=0.65f;
    const bool stay=usable&&facing>=0.50f&&distance<=0.85f;
    if (!usable) { state_=WristState::hidden; since_=t; return state_; }
    switch(state_) {
    case WristState::hidden: if(enter) { state_=WristState::candidate; since_=t; } break;
    case WristState::candidate:
        if(!enter) state_=WristState::hidden;
        else if(t-since_>=0.20) {state_=WristState::visible; since_=t;} break;
    case WristState::visible:
        if(!stay && t-since_>=0.15) {state_=WristState::cooldown; since_=t;} break;
    case WristState::cooldown: if(t-since_>=0.25) state_=WristState::hidden; break;
    }
    return state_;
}
bool SkipGate::update(Scene scene,uint64_t generation,bool canSkip,bool focused,bool pressed) {
    const bool transitioned=generation!=generation_;
    generation_=generation;
    const bool fire=pressed&&!previous_&&!transitioned&&focused&&scene==Scene::cinematic&&canSkip;
    previous_=pressed;
    return fire;
}
std::optional<WeaponPose> solveWeapon(Pose grip,const WeaponSockets& s,bool tracked) {
    if(!tracked||!s.calibrated||!valid(grip)||!valid(s.weaponFromGrip)||!valid(s.weaponFromMuzzle))return {};
    const auto weapon=compose(grip,inverse(s.weaponFromGrip));
    return WeaponPose{weapon,compose(weapon,s.weaponFromMuzzle)};
}
}
