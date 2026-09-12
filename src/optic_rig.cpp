#include "mgs5vr/optic_rig.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace mgs5vr {
namespace {
float distance(Vec3 a,Vec3 b){return std::sqrt(dot(a-b,a-b));}
float facing(Pose a,Pose b){
    return dot(rotate(a.orientation,{0,0,-1}),rotate(b.orientation,{0,0,-1}));
}
bool finitePose(Pose p){return valid(p)&&std::isfinite(p.position.x)&&std::isfinite(p.position.y)&&std::isfinite(p.position.z);}
}

std::optional<OpticPose> solveBinocularPose(Pose leftGrip,Pose rightGrip,
    Pose leftAim,Pose rightAim,bool leftTracked,bool rightTracked,
    bool leftAimTracked,bool rightAimTracked,bool leftHeld,bool rightHeld){
    if(!rightHeld||!rightTracked||!rightAimTracked
       ||!finitePose(rightGrip)||!finitePose(rightAim))return {};
    // The right hand owns the device.  The body is an authored grip socket,
    // exactly like a firearm's weaponFromGrip socket: its translation and
    // orientation come from the right grip pose.  The aim pose is validated
    // separately because it is the optical alignment probe, never the body
    // attachment owner.  This prevents the binoculars from floating when the
    // controller's aim pose and anatomical palm pose differ.
    const auto aimFromGrip=compose(inverse(rightGrip),rightAim);
    if(!valid(Pose{aimFromGrip.orientation,{}}))return {};
    // The actual retail telescope mesh has its large ocular recess on the
    // broad +Z face.  Its authored grip frame is not the simulator's weapon-ready
    // palm frame, so the mesh needs one calibrated attachment transform.  At
    // the measured weapon grip quaternion this makes the ocular face point at
    // the user and keeps the device upright; it is a single transform carried
    // by the hand, not a screen-space or guessed quarter-turn.
    const Pose referenceGrip{{.61595203f,-.00760909f,.07982154f,.78369236f},{}};
    const Pose referenceBody{}; // +Z ocular face points back toward the user's eye.
    const auto gripToBody=compose(inverse(referenceGrip),referenceBody);
    // Seat the housing against the inside of the right palm. The rear ocular
    // remains behind the fingers, so bringing it to the eye does not bring
    // the wrist through the near plane.
    const auto bodyOffset=rotate(gripToBody.orientation,binocularPrimarySocket*-1.f);
    const Pose bodyFromRightGrip{gripToBody.orientation,bodyOffset};
    const Pose body=compose(rightGrip,bodyFromRightGrip);
    const Pose renderBody=body;
    // Geometry inspection of the retail FMDL places the real large ocular
    // recess at approximately (-.0328, -.0006, +.0551) in the imported mesh.
    // Keep the relief point just outside that face.  This asset is a
    // monocular, so both eye-relief probes stay on that one real aperture;
    // the two native eye origins themselves are never collapsed or moved.
    constexpr auto ocularCenter=binocularOcularCenter;
    // This retail device has one aperture. Either eye can look through that
    // same physical pupil; no artificial second eyepiece is created.
    const auto rightPosition=compose(body,Pose{{0,0,0,1},{ocularCenter.x,ocularCenter.y,ocularCenter.z}}).position;
    // Look into the housing from its +Z ocular face, along body -Z. The lens
    // normal points toward the eye; the optical ray points the opposite way.
    const Pose viewFrame=body;
    const Pose left{viewFrame.orientation,rightPosition};
    const Pose right{viewFrame.orientation,rightPosition};
    // The left palm can cup the opposite side wall. Primary ownership stays
    // with the right palm when support is acquired or released.
    // Merely squeezing the left controller must not teleport that hand onto
    // the binoculars: it has to be tracked, aimed, held, and physically close
    // to this authored socket.
    const auto supportSocket=compose(body,Pose{referenceGrip.orientation,binocularSupportSocket});
    const float supportDistance=finitePose(leftGrip)&&finitePose(supportSocket)
        ?distance(leftGrip.position,supportSocket.position):std::numeric_limits<float>::infinity();
    const bool supportHeld=leftHeld&&leftTracked&&leftAimTracked
        &&finitePose(leftGrip)&&finitePose(leftAim)&&supportDistance<=.12f;
    // Keep target projection independent from eye relief. The housing stays
    // grip-owned, and its ocular frame supplies the optical axis
    // for a continuous carry-state ray. The ray is deliberately published
    // even when the binoculars are not yet at the face.
    const auto direction=rotate(viewFrame.orientation,{0,0,-1});
    const float directionLength=dot(direction,direction);
    if(!std::isfinite(directionLength)||directionLength<.98f||directionLength>1.02f)return {};
    const OpticRay ray{rightPosition,direction,true};
    OpticPose result{};
    result.kind=OpticKind::binocular;
    result.body=body;
    result.renderBody=renderBody;
    result.leftEyepiece=left;
    result.rightEyepiece=right;
    result.tracked=true;
    result.primaryRight=true;
    result.supportHeld=supportHeld;
    result.supportGrip=supportHeld?supportSocket:leftGrip;
    result.stability=supportHeld?1.f:.35f;
    result.ray=ray;
    return result;
}

Pose binocularFaceSafeGrip(Pose head,Pose primary,const OpticPose& optic){
    if(!finitePose(head)||!finitePose(primary)||!optic.tracked)return primary;
    const auto body=compose(inverse(head),optic.renderBody);
    Vec3 low{1e6f,1e6f,1e6f},high{-1e6f,-1e6f,-1e6f};
    // Bounds of the imported retail housing, including its front fittings.
    for(float x:{-.071133f,.077854f})for(float y:{-.026208f,.038920f})for(float z:{-.050424f,.055070f}){
        const auto p=compose(body,Pose{{},{x,y,z}}).position;
        low={std::min(low.x,p.x),std::min(low.y,p.y),std::min(low.z,p.z)};
        high={std::max(high.x,p.x),std::max(high.y,p.y),std::max(high.z,p.z)};
    }
    if(low.x<.075f&&high.x>-.075f&&low.y<.075f&&high.y>-.06f&&low.z<.10f&&high.z>-.045f)
        primary.position=primary.position+rotate(head.orientation,{0,0,-.045f-high.z});
    return primary;
}

Pose OpticStabilizer::update(Pose head,Pose grip,const OpticPose& optic,bool available,uint64_t time,uint64_t epoch){
    if(!available||!epoch||!finitePose(head)||!finitePose(grip)||!optic.tracked){reset();return grip;}
    const auto local=compose(inverse(head),grip);
    const bool close=distance(head.position,optic.rightEyepiece.position)<.18f
        &&facing(head,optic.rightEyepiece)>.6f;
    if(!ready_||epoch!=epoch_||time<=time_||time-time_>150||!close){
        filtered_=local;ready_=true;time_=time;epoch_=epoch;return grip;
    }
    const float dt=static_cast<float>(time-time_)*.001f;time_=time;
    auto target=local.orientation;
    auto prior=filtered_.orientation;
    float alignment=prior.x*target.x+prior.y*target.y+prior.z*target.z+prior.w*target.w;
    if(alignment<0){target={-target.x,-target.y,-target.z,-target.w};alignment=-alignment;}
    const float angle=2.f*std::acos(std::clamp(alignment,0.f,1.f));
    const float travel=distance(filtered_.position,local.position);
    // Small involuntary movement is damped; a deliberate adjustment catches
    // up quickly. The optional support hand strengthens only the steady hold.
    const float tau=travel>.02f||angle>.05236f?.012f:(optic.supportHeld?.10f:.055f);
    const float alpha=1.f-std::exp(-dt/tau);
    if(travel>.001f)filtered_.position=filtered_.position+(local.position-filtered_.position)*alpha;
    if(angle>.002618f){
        Quat q{prior.x+(target.x-prior.x)*alpha,prior.y+(target.y-prior.y)*alpha,
            prior.z+(target.z-prior.z)*alpha,prior.w+(target.w-prior.w)*alpha};
        const float length=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
        filtered_.orientation={q.x/length,q.y/length,q.z/length,q.w/length};
    }
    return compose(head,filtered_);
}

std::optional<EyeView> binocularSceneView(const OpticPose& optic,float magnification){
    if(optic.kind!=OpticKind::binocular||!optic.tracked||!optic.ray.tracked
       ||!finitePose(optic.rightEyepiece)||!std::isfinite(magnification)
       ||magnification<1.f||magnification>4.f)return {};
    // The fixed exit pupil defines the angular size at normal eye relief.
    // Rendering this narrow frustum gives the lens its own scene detail;
    // enlarging pixels from an HMD eye cannot do that.
    const float halfAngle=std::atan(binocularOcularRadius/(binocularEyeRelief*magnification));
    // The scene enters through the front glass, not from inside the housing.
    // Starting at the rear ocular lets the cupped hand appear inside the
    // magnified scene even though it is behind the objective. The exit pupil
    // is still drawn at the rear rim and depth-tested against both hands.
    const auto objective=compose(optic.rightEyepiece,
        Pose{{},binocularObjectiveCenter-binocularOcularCenter});
    return EyeView{objective,{-halfAngle,halfAngle,halfAngle,-halfAngle}};
}

bool OpticSelection::update(bool available,bool chord,bool close){
    if(!available||close)selected_=false;
    else if(chord&&!priorChord_)selected_=!selected_;
    priorChord_=chord;
    return selected_;
}

bool validateBinocularViews(const OpticSample& optic,const Pose& head,
    const std::array<EyeView,2>& views){
    if(!optic.active||optic.pose.kind!=OpticKind::binocular||!optic.pose.tracked
       ||!finitePose(optic.pose.body)||!finitePose(optic.pose.leftEyepiece)
       ||!finitePose(optic.pose.rightEyepiece)||!finitePose(head))return false;
    for(const auto& eye:views)if(!valid(eye.pose)||!valid(eye.fov))return false;
    return true;
}

OpticSample OpticGate::update(Pose head,const std::array<EyeView,2>& eyes,
    Pose leftGrip,Pose rightGrip,Pose leftAim,Pose rightAim,
    bool leftTracked,bool rightTracked,bool leftAimTracked,bool rightAimTracked,
    bool leftHeld,bool rightHeld,bool available,uint64_t time,uint64_t epoch){
    OpticSample result;
    if(!available||!epoch||time<time_||(epoch_&&epoch!=epoch_)){
        const bool wasActive=active_;
        reset();
        result.closed=wasActive;
        return result;
    }
    time_=time;epoch_=epoch;
    const auto pose=solveBinocularPose(leftGrip,rightGrip,leftAim,rightAim,
        leftTracked,rightTracked,leftAimTracked,rightAimTracked,leftHeld,rightHeld);
    if(!pose||!finitePose(head)||!finitePose(eyes[0].pose)||!finitePose(eyes[1].pose)){
        const bool wasActive=active_;
        active_=false;++sequence_;
        result.closed=wasActive;result.sequence=sequence_;
        return result;
    }
    result.pose=*pose;
    const float leftDistance=distance(pose->leftEyepiece.position,eyes[0].pose.position);
    const float rightDistance=distance(pose->rightEyepiece.position,eyes[1].pose.position);
    const float leftFacing=facing(pose->leftEyepiece,eyes[0].pose);
    const float rightFacing=facing(pose->rightEyepiece,eyes[1].pose);
    // Enter conservatively, then keep the optic alive through normal hand
    // tremor and eye-relief motion.  The hysteresis is important in VR: a
    // single noisy pose must not flicker the native binocular state or reset
    // the marker/intel acquisition transaction.
    // The native scene keeps a roughly 10 cm near clip.  The real retail
    // ocular therefore has to sit just beyond it while still being inside
    // eye relief; the measured 14 cm gate covers the near-clip-safe pose and
    // the two-eye lateral offset without allowing activation at arm's length.
    const float radius=active_?.17f:.14f;
    const float cosine=active_?.68f:.82f;
    result.aligned=(leftDistance<=radius&&leftFacing>=cosine)
        ||(rightDistance<=radius&&rightFacing>=cosine);
    const bool wasActive=active_;
    if(result.aligned&&!active_){active_=true;result.opened=true;}
    else if(!result.aligned&&active_){active_=false;result.closed=true;}
    result.active=active_;result.pose.tracked=true;result.sequence=++sequence_;
    if(!active_&&wasActive&&!result.closed)result.closed=true;
    return result;
}
}
