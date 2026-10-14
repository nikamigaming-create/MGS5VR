#include "mgs5vr/cabin_props.hpp"
#include <algorithm>
#include <cmath>

namespace mgs5vr {
namespace {
constexpr Quat flat{-.70710678f,0,0,.70710678f};
Vec3 extent(Pose pose){
    const auto x=rotate(pose.orientation,{1,0,0}),y=rotate(pose.orientation,{0,1,0}),z=rotate(pose.orientation,{0,0,1});
    return {std::abs(x.x)*cabinTapeHalf.x+std::abs(y.x)*cabinTapeHalf.y+std::abs(z.x)*cabinTapeHalf.z,
            std::abs(x.y)*cabinTapeHalf.x+std::abs(y.y)*cabinTapeHalf.y+std::abs(z.y)*cabinTapeHalf.z,
            std::abs(x.z)*cabinTapeHalf.x+std::abs(y.z)*cabinTapeHalf.y+std::abs(z.z)*cabinTapeHalf.z};
}
float distanceBox(Vec3 point,const CabinTape& tape){
    const auto p=compose(inverse(tape.pose),Pose{{},point}).position;
    const Vec3 delta{std::max(0.f,std::abs(p.x)-cabinTapeHalf.x),
        std::max(0.f,std::abs(p.y)-cabinTapeHalf.y),std::max(0.f,std::abs(p.z)-cabinTapeHalf.z)};
    return dot(delta,delta);
}
float length(Vec3 p){return std::sqrt(dot(p,p));}
Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
Vec3 angularVelocity(Quat before,Quat after,float dt){
    auto q=compose(Pose{after,{}},inverse(Pose{before,{}})).orientation;
    if(q.w<0)q={-q.x,-q.y,-q.z,-q.w};
    const float s=std::sqrt(std::max(0.f,1-q.w*q.w));
    return s<.0001f||dt<=0?Vec3{}:Vec3{q.x,q.y,q.z}*(2*std::acos(std::clamp(q.w,-1.f,1.f))/(s*dt));
}
Vec3 limit(Vec3 p,float maximum){const float n=length(p);return n>maximum?p*(maximum/n):p;}
Vec3 inverseInertia(Pose pose,Vec3 impulse){
    const auto local=rotate(inverse(pose).orientation,impulse);const auto h=cabinTapeHalf;
    const Vec3 response{local.x/(cabinTapeMass*(h.y*h.y+h.z*h.z)/3),
        local.y/(cabinTapeMass*(h.x*h.x+h.z*h.z)/3),local.z/(cabinTapeMass*(h.x*h.x+h.y*h.y)/3)};
    return rotate(pose.orientation,response);
}
Quat turn(Quat q,Vec3 angular,float dt){
    const float speed=length(angular);if(speed<.001f)return q;
    const float angle=speed*dt*.5f,s=std::sin(angle)/speed;
    return compose(Pose{{angular.x*s,angular.y*s,angular.z*s,std::cos(angle)},{}},Pose{q,{}}).orientation;
}
// Separating-axis contact for real tape OBBs. Keep the minimum-penetration
// normal, including edge/edge axes, so tilted or stacked tapes cannot overlap.
bool contact(const CabinTape& a,const CabinTape& b,Vec3& normal,float& penetration,Vec3 bHalf=cabinTapeHalf){
    const std::array<Vec3,3> aa{rotate(a.pose.orientation,{1,0,0}),rotate(a.pose.orientation,{0,1,0}),rotate(a.pose.orientation,{0,0,1})};
    const std::array<Vec3,3> bb{rotate(b.pose.orientation,{1,0,0}),rotate(b.pose.orientation,{0,1,0}),rotate(b.pose.orientation,{0,0,1})};
    const std::array<float,3> h{cabinTapeHalf.x,cabinTapeHalf.y,cabinTapeHalf.z},bh{bHalf.x,bHalf.y,bHalf.z};
    const auto delta=b.pose.position-a.pose.position;penetration=100;
    const auto axis=[&](Vec3 n){
        const float len=length(n);if(len<.0001f)return true;n=n*(1/len);
        float radius{};for(size_t i=0;i<3;++i)radius+=h[i]*std::abs(dot(aa[i],n))+bh[i]*std::abs(dot(bb[i],n));
        const float d=dot(delta,n),overlap=radius-std::abs(d);if(overlap<=0)return false;
        if(overlap<penetration){penetration=overlap;normal=n*(d<0?-1.f:1.f);}return true;
    };
    for(const auto n:aa)if(!axis(n))return false;
    for(const auto n:bb)if(!axis(n))return false;
    for(const auto aAxis:aa)for(const auto bAxis:bb){
        const Vec3 n{aAxis.y*bAxis.z-aAxis.z*bAxis.y,aAxis.z*bAxis.x-aAxis.x*bAxis.z,aAxis.x*bAxis.y-aAxis.y*bAxis.x};
        if(!axis(n))return false;
    }
    return true;
}
bool dynamic(const CabinTape& tape){return tape.hand<0&&tape.state!=CabinTapeState::seated&&tape.state!=CabinTapeState::ejecting;}
void playerContact(CabinTape& tape,const CabinLayout& layout){
    CabinTape player;player.pose=layout.player;Vec3 normal{};float penetration{};
    if(contact(tape,player,normal,penetration,{.0555f,.047f,.0162f})){
        tape.pose.position=tape.pose.position-normal*(penetration+.0001f);
        const float into=dot(tape.velocity,normal);if(into>0)tape.velocity=tape.velocity-normal*(1.12f*into);
    }
}
void supportContacts(CabinTape& tape,const CabinLayout& layout){
        CabinTape support;support.pose=layout.seat;Vec3 normal{};float penetration{};
        if(contact(tape,support,normal,penetration,layout.seatHalf)){
            tape.pose.position=tape.pose.position-normal*(penetration+.0001f);
            const float into=dot(tape.velocity,normal);if(into>0)tape.velocity=tape.velocity-normal*(1.12f*into);
        }
}
}
CabinLayout nativeCabinLayout(Pose origin,uint64_t generation) noexcept{
    CabinLayout layout{};if(!valid(origin)||!generation)return layout;
    const auto fromWorld=inverse(origin);
    const auto local=[&](Vec3 p,Quat q=Quat{}){return compose(fromWorld,Pose{q,p+Vec3{0,1200,0}});};
    layout.room=local({});
    // The rear brown seat beside the existing iDroid/player case. Measured
    // uth0_intr0 cushion triangles span X +/- .259, Z [-1.257,-.878].
    layout.seat=local(cabinNativeSeat.position);
    // Existing mesh 10/11 player on the case: no second player is rendered.
    layout.player=local({.5755f,-2.198f,-1.111f},flat);
    layout.slot=local({.5755f,-2.1765f,-1.111f},flat);
    layout.eject=local({.619f,-2.178f,-1.087f}).position;
    layout.board=local(cabinNativeBoard.position,cabinNativeBoard.orientation);
    layout.floor=1197.30f-origin.position.y;
    layout.generation=generation;layout.verified=true;return layout;
}
CabinLayout cabinDefaultLayout() noexcept{
    // Recorded native title camera, converted to the OpenXR prop basis.
    return nativeCabinLayout(Pose{{0,.371772f,0,.928324f},{.213f,1198.17f,.106f}},1);
}
CabinBox cabinBench() noexcept{const auto l=cabinDefaultLayout();return {l.seat.position,l.seatHalf};}
Pose cabinPlayerPose() noexcept{return cabinDefaultLayout().player;}
Pose cabinSlotPose() noexcept{return cabinDefaultLayout().slot;}
Vec3 cabinEjectButton() noexcept{return cabinDefaultLayout().eject;}
std::array<Pose,6> tapeHomes(const CabinLayout& layout) noexcept{
    std::array<Pose,6> result{};
    for(size_t i=0;i<6;++i){
        const Vec3 p{(static_cast<float>(i%3)-1)*.13f,0,-1.0675f+(i<3?-.085f:.085f)};
        const float height=cabinNativeSupportHeight(p);
        result[i]=compose(layout.room,Pose{flat,{p.x,height+cabinTapeHalf.z+.004f,p.z}});
    }
    return result;
}
float cabinNativeSupportHeight(Vec3 p) noexcept{
    float height=-2.70f;
    // Four measured cushion facets, approximated by their two slopes. The
    // case on the neighbouring seat and the folded side seat remain supports.
    if(std::abs(p.x)<=.259f&&p.z>=-1.257f&&p.z<=-.878f)
        height=-2.338f+std::max(std::abs(p.x)*.097f,
            (p.z>=-1.0675f?(p.z+1.0675f)*.154f:(-1.0675f-p.z)*.132f));
    if(p.x>=.343f&&p.x<=.857f&&p.z>=-1.243f&&p.z<=-.840f)height=std::max(height,-2.2122f);
    if(p.x>=-.925f&&p.x<=-.523f&&p.z>=-.814f&&p.z<=.747f)height=std::max(height,-2.3957f);
    return height;
}
std::array<Pose,6> cabinTapeHomes() noexcept{
    return tapeHomes(cabinDefaultLayout());
}
void CabinProps::reset() noexcept{*this=CabinProps{};}
void CabinProps::step(float dt){
    const auto& layout=frame_.layout;
    for(auto& tape:frame_.tapes){
        if(!dynamic(tape))continue;
        const auto before=tape.pose.position;const auto priorExtent=extent(tape.pose);
        tape.velocity.y-=9.81f*dt;
        tape.pose.position=tape.pose.position+tape.velocity*dt;
        tape.pose.orientation=turn(tape.pose.orientation,tape.angularVelocity,dt);
        const auto e=extent(tape.pose);const auto p=tape.pose.position;
        const auto roomPoint=compose(inverse(layout.room),Pose{{},p}).position;
        const float surface=cabinNativeSupportHeight(roomPoint)+layout.room.position.y;
        const float floor=before.y+priorExtent.y>surface?surface:layout.floor;
        if(p.y-e.y<floor){
            tape.pose.position.y=floor+e.y;
            // The support force acts at the lowest corner/edge. Its moment
            // tips a released tilted cassette until a broad face settles.
            Vec3 support{};const std::array<float,3> h{cabinTapeHalf.x,cabinTapeHalf.y,cabinTapeHalf.z};
            const std::array<Vec3,3> axes{rotate(tape.pose.orientation,{1,0,0}),rotate(tape.pose.orientation,{0,1,0}),rotate(tape.pose.orientation,{0,0,1})};
            for(size_t i=0;i<3;++i)if(std::abs(axes[i].y)>.0001f)support=support-axes[i]*(h[i]*(axes[i].y<0?-1.f:1.f));
            const Vec3 up{0,1,0};const float closing=(tape.velocity+cross(tape.angularVelocity,support)).y;
            if(closing<0){
                const float response=1/cabinTapeMass+cross(inverseInertia(tape.pose,cross(support,up)),support).y;
                const float impulse=-1.12f*closing/response;
                tape.velocity.y+=impulse/cabinTapeMass;
                tape.angularVelocity=limit(tape.angularVelocity+inverseInertia(tape.pose,cross(support,up*impulse)),12.f);
            }
            const float friction=std::max(0.f,1.f-9.f*dt);
            tape.velocity.x*=friction;tape.velocity.z*=friction;tape.angularVelocity=tape.angularVelocity*friction;
            if(length(tape.velocity)<.025f&&length(tape.angularVelocity)<.08f){tape.velocity={};tape.angularVelocity={};tape.state=CabinTapeState::resting;}
        }else tape.state=CabinTapeState::falling;
        playerContact(tape,layout);
        supportContacts(tape,layout);
        // Cabin-contained props remain recoverable on the native floor.
        auto roomPose=compose(inverse(layout.room),tape.pose);const auto roomExtent=extent(roomPose);
        roomPose.position.x=std::clamp(roomPose.position.x,-.95f+roomExtent.x,.95f-roomExtent.x);
        roomPose.position.z=std::clamp(roomPose.position.z,-1.35f+roomExtent.z,2.40f-roomExtent.z);
        tape.pose=compose(layout.room,roomPose);
    }
    for(unsigned iteration=0;iteration<3;++iteration)for(size_t i=0;i<6;++i)for(size_t j=i+1;j<6;++j){
        auto& a=frame_.tapes[i];auto& b=frame_.tapes[j];const bool da=dynamic(a),db=dynamic(b);
        if(!da&&!db)continue;
        Vec3 normal{};float penetration{};if(!contact(a,b,normal,penetration))continue;
        const float wa=da?1.f/cabinTapeMass:0,wb=db?1.f/cabinTapeMass:0,total=wa+wb;
        if(da)a.pose.position=a.pose.position-normal*(penetration*wa/total);
        if(db)b.pose.position=b.pose.position+normal*(penetration*wb/total);
        const float closing=dot(b.velocity-a.velocity,normal);
        if(closing<0){const float impulse=-1.12f*closing/total;
            if(da)a.velocity=a.velocity-normal*(impulse*wa);
            if(db)b.velocity=b.velocity+normal*(impulse*wb);}
    }
}
CabinBenchFrame CabinProps::update(const std::array<CabinHand,2>& hands,uint64_t now,CabinLayout layout){
    if(initialized_&&layout.verified&&layout.generation!=frame_.layout.generation)reset();
    if(!initialized_){
        initialized_=true;frame_.layout=layout;frame_.player=layout.player;const auto homes=tapeHomes(layout);
        for(size_t i=0;i<6;++i)frame_.tapes[i].pose=homes[i];
        for(size_t side=0;side<2;++side){grabbed_[side]=hands[side].grip>.55f;triggered_[side]=hands[side].trigger>.68f;}
    }
    const float dt=lastTime_&&now>=lastTime_?std::min(.05f,static_cast<float>(now-lastTime_)*.001f):0;
    lastTime_=now;frame_.hovered=frame_.activated=-1;frame_.ejectPressed=false;frame_.blocked=false;
    frame_.hapticMask=0;
    const auto slot=frame_.layout.slot;
    for(size_t side=0;side<2;++side){
        const auto& hand=hands[side];const bool tracked=hand.tracked&&valid(hand.pose);
        const bool gripping=tracked&&(hand.grip>(grabbed_[side]?.25f:.55f));
        const bool trigger=tracked&&hand.trigger>.68f;
        int held=-1;for(size_t i=0;i<6;++i)if(frame_.tapes[i].hand==static_cast<int>(side))held=static_cast<int>(i);
        if(held>=0){
            auto& tape=frame_.tapes[static_cast<size_t>(held)];
            if(tracked){const auto next=compose(hand.pose,attachment_[side]);
                if(dt>0){tape.velocity=limit((next.position-tape.pose.position)*(1/dt),3.f);
                    tape.angularVelocity=limit(angularVelocity(tape.pose.orientation,next.orientation,dt),12.f);}
                tape.pose=next;
                if(frame_.inserted<0){
                    const auto relative=compose(inverse(slot),tape.pose);
                    const auto normal=rotate(relative.orientation,{0,0,1});
                    const bool aligned=std::abs(relative.position.x)<.026f&&std::abs(relative.position.y)<.035f
                        &&relative.position.z>=-.016f&&relative.position.z<.085f&&normal.z>.85f;
                    if(aligned){if(tape.state!=CabinTapeState::guiding)frame_.hapticMask|=uint8_t(1u<<side);
                        tape.state=CabinTapeState::guiding;frame_.hovered=held;
                        // Gentle slot guidance retains the player's insertion axis.
                        tape.pose=compose(slot,Pose{{},{relative.position.x*.25f,relative.position.y*.25f,std::max(0.f,relative.position.z)}});
                    }else tape.state=CabinTapeState::held;
                }
                if(tape.state!=CabinTapeState::guiding){
                    const auto e=extent(tape.pose);const auto p=compose(inverse(frame_.layout.seat),tape.pose).position;
                    if(std::abs(p.x)<frame_.layout.seatHalf.x&&std::abs(p.z)<frame_.layout.seatHalf.z
                       &&p.y+e.y>-frame_.layout.seatHalf.y)
                        tape.pose.position.y=std::max(tape.pose.position.y,frame_.layout.seat.position.y+frame_.layout.seatHalf.y+e.y);
                    playerContact(tape,frame_.layout);
                }
            }
            if(!gripping){
                tape.hand=-1;
                const auto relative=compose(inverse(slot),tape.pose);
                if(tracked&&tape.state==CabinTapeState::guiding&&relative.position.z<.025f&&frame_.inserted<0){
                    frame_.inserted=held;tape.state=CabinTapeState::seated;tape.pose=slot;tape.velocity={};
                    tape.angularVelocity={};
                    insertionTime_=0;activationSent_=false;frame_.lidOpen=1;
                    frame_.hapticMask|=uint8_t(1u<<side);
                }else {tape.state=CabinTapeState::falling;if(!tracked){tape.velocity={};tape.angularVelocity={};}}
            }
        }else if(gripping&&!grabbed_[side]){
            int nearest=-1;float distance=.04f*.04f;
            for(size_t i=0;i<6;++i){const auto& tape=frame_.tapes[i];if(tape.hand>=0||tape.state==CabinTapeState::seated)continue;
                const float d=distanceBox(hand.pose.position,tape);if(d<distance){distance=d;nearest=static_cast<int>(i);}}
            if(nearest>=0){auto& tape=frame_.tapes[static_cast<size_t>(nearest)];tape.hand=static_cast<int>(side);
                tape.state=CabinTapeState::held;attachment_[side]=compose(inverse(hand.pose),tape.pose);frame_.hovered=nearest;
                frame_.hapticMask|=uint8_t(1u<<side);}
        }
        if(tracked&&held<0&&!gripping){
            for(size_t i=0;i<6;++i)if(distanceBox(hand.pose.position,frame_.tapes[i])<.04f*.04f)frame_.hovered=static_cast<int>(i);
            const auto delta=hand.pose.position-frame_.layout.eject;
            if(trigger&&!triggered_[side]&&dot(delta,delta)<.05f*.05f&&frame_.inserted>=0){
                auto& tape=frame_.tapes[static_cast<size_t>(frame_.inserted)];tape.state=CabinTapeState::ejecting;
                frame_.inserted=-1;ejectionTime_=0;frame_.ejectPressed=true;frame_.lidOpen=1;
                frame_.hapticMask|=uint8_t(1u<<side);
            }
        }
        grabbed_[side]=gripping;triggered_[side]=trigger;
    }
    if(frame_.inserted>=0){
        insertionTime_+=dt;frame_.lidOpen=std::max(0.f,1-insertionTime_/.25f);
        if(insertionTime_>=.28f&&!activationSent_){frame_.activated=frame_.inserted;activationSent_=true;}
    }
    for(auto& tape:frame_.tapes)if(tape.state==CabinTapeState::ejecting){
        ejectionTime_+=dt;const float t=std::min(1.f,ejectionTime_/.25f);
        tape.pose=compose(slot,Pose{{},{0,0,.065f*t}});
        if(t>=1){tape.state=CabinTapeState::falling;tape.velocity={-.35f,.20f,.15f};}
    }
    // Fixed small steps keep release throws from tunnelling through the support.
    for(float remaining=dt;remaining>0;){const float h=std::min(remaining,1.f/120.f);step(h);remaining-=h;}
    return frame_;
}
}
