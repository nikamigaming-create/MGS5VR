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

std::optional<EyeView> weaponScopeSceneView(const WeaponScopeSample& scope){
    if(!scope.tracked||!scope.weaponIdentity||!finitePose(scope.ocular)||!finitePose(scope.objective)
       ||!std::isfinite(scope.radius)||scope.radius<.003f||scope.radius>.06f
       ||!std::isfinite(scope.eyeRelief)||scope.eyeRelief<.02f||scope.eyeRelief>.3f
       ||!std::isfinite(scope.magnification)||scope.magnification<1.f||scope.magnification>16.f)return {};
    const auto objective=compose(inverse(scope.ocular),scope.objective);
    // A reversed, detached or misidentified sight must not become a camera.
    if(objective.position.z>=-.005f||objective.position.z<-.6f
       ||std::abs(objective.position.x)>.005f||std::abs(objective.position.y)>.005f
       ||facing(scope.ocular,scope.objective)<.999f)return {};
    const float angle=std::atan(scope.radius/(scope.eyeRelief*scope.magnification));
    return EyeView{scope.objective,{-angle,angle,angle,-angle}};
}

bool weaponScopeEyeVisible(const WeaponScopeSample& scope,Pose eye){
    if(!weaponScopeSceneView(scope)||!finitePose(eye))return false;
    const auto local=compose(inverse(scope.ocular),eye).position;
    // Eye relief belongs to the physical scope. Lowering/rolling the rifle
    // never expands the image over the world or reveals it through the back.
    if(local.z<.005f||local.z>scope.eyeRelief*2.f)return false;
    const float lateral=local.x*local.x+local.y*local.y;
    return lateral<=scope.radius*scope.radius&&facing(scope.ocular,eye)>.75f;
}

std::optional<WeaponScopeGeometry> nativeWeaponScopeGeometry(Pose rear,Pose front,
    const std::array<uint8_t,4>& optical){
    if(!finitePose(rear)||!finitePose(front)||optical[3]!=1||facing(rear,front)<.999f)return {};
    const auto delta=compose(inverse(rear),front).position;
    if(std::abs(delta.x)>.001f||std::abs(delta.y)>.001f||delta.z<=0)return {};
    // Measurements from the owned sight FMDL aperture and named FCNP pair.
    // Length identifies the assembled sight, not the weapon's name/grade.
    // Insets place the portal at the glass, 0.2 mm toward the eye to avoid
    // coincident native depth. This table contains no retail mesh or texture.
    struct Profile{uint32_t sight;float length,radius,inset;std::array<uint8_t,3> powers;};
    static constexpr std::array<Profile,18> profiles{{
        {6,.124000f,.00706f,-.0002f,{2,0,0}},
        {7,.193248f,.01334f,.00758f,{3,0,0}},
        {9,.224005f,.01691f,-.0002f,{3,0,0}},
        {10,.260614f,.01764f,.00940f,{3,0,0}},
        {11,.194981f,.01701f,-.0002f,{2,6,0}},
        {12,.346509f,.01764f,.01763f,{3,0,0}},
        {13,.219000f,.01200f,.00780f,{4,0,0}},
        {14,.422982f,.01800f,-.00622f,{2,4,8}},
        {15,.289000f,.01498f,.00180f,{4,0,0}},
        {16,.291000f,.01699f,-.0002f,{2,4,8}},
        {17,.281990f,.01341f,.00416f,{4,0,0}},
        {18,.308000f,.02000f,-.0002f,{4,0,0}},
        {19,.378000f,.01432f,.000737f,{2,6,0}},
        {20,.403998f,.02174f,-.0002f,{4,6,8}},
        // Launcher sights live in the receiver, not a separate sight part.
        // Their circles fit INSIDE the authored low-poly opening (minimum
        // edge distance), rather than painting over its polygonal eyecup.
        {21,.168000f,.00926f,.007614f,{2,4,6}},
        // Receiver package order is ms00, ms02, ms03, ms01, not filename
        // order: RC/ST_80103 uses ms02 and RC/ST_80303 uses ms01.
        {22,.259995f,.01567f,.005989f,{2,4,6}},
        {23,.431995f,.01478f,.005795f,{2,4,6}},
        {24,.323995f,.01328f,.007445f,{2,4,6}}
    }};
    const std::array<uint8_t,3> powers{optical[0],optical[1],optical[2]};
    const auto found=std::find_if(profiles.begin(),profiles.end(),[&](const auto& p){
        return std::abs(p.length-delta.z)<.0005f&&p.powers==powers;
    });
    if(found==profiles.end())return {};
    const Pose basis{{0,1,0,0},{}};
    const auto ocular=compose(rear,compose(Pose{{},{0,0,found->inset}},basis));
    return WeaponScopeGeometry{ocular,compose(front,basis),found->radius,found->sight,powers};
}
float WeaponScopeZoom::update(uint64_t identity,uint64_t sequence,const std::array<uint8_t,3>& powers){
    unsigned count=0;for(const auto power:powers){if(!power)break;if(power>16)return 1; ++count;}
    if(!identity||!count)return 1;
    if(identity!=identity_||powers!=powers_||sequence<sequence_){
        identity_=identity;powers_=powers;step_=0;
    }else if(sequence!=sequence_)step_=(step_+static_cast<unsigned>((sequence-sequence_)%count))%count;
    sequence_=sequence;return static_cast<float>(powers[step_]);
}

uint64_t WeaponScopeZoomInput::update(bool requested,bool available){
    if(requested&&!requested_&&available)++sequence_;
    requested_=requested;return sequence_;
}

Quat binocularGripRotation(float pitchDegrees,float yawDegrees,float rollDegrees){
    constexpr float halfRadians=.00872664626f;
    const float pitch=pitchDegrees*halfRadians,yaw=yawDegrees*halfRadians,roll=rollDegrees*halfRadians;
    return compose(Pose{{0,std::sin(yaw),0,std::cos(yaw)},{}},
        compose(Pose{{std::sin(pitch),0,0,std::cos(pitch)},{}},Pose{{0,0,std::sin(roll),std::cos(roll)},{} })).orientation;
}
std::optional<OpticPose> solveBinocularPose(Pose leftGrip,Pose rightGrip,
    Pose leftAim,Pose rightAim,bool leftTracked,bool rightTracked,
    bool leftAimTracked,bool rightAimTracked,bool leftHeld,bool rightHeld,Quat gripRotation){
    if(!rightHeld||!rightTracked||!rightAimTracked
       ||!finitePose(rightGrip)||!finitePose(rightAim)||!valid(Pose{gripRotation,{}}))return {};
    // The primary palm owns the attachment position. Calibrate orientation
    // from this runtime's same-frame grip/aim pair; do not use aim POSITION
    // as the palm socket or assume every controller has the simulator basis.
    const auto aimFromGrip=compose(inverse(rightGrip),rightAim);
    if(!valid(Pose{aimFromGrip.orientation,{}}))return {};
    // Use THIS controller's grip-to-aim frame, not a simulator's neutral
    // wrist quaternion. The optic starts at controller aim -Z, then applies
    // the configured device fit about the stationary tracked palm socket.
    const Pose gripToBody=compose(Pose{aimFromGrip.orientation,{}},Pose{gripRotation,{}});
    // Fit rotates the DEVICE around the palm, not the wrist along with it.
    // Cancel that fit in the anatomical contacts or the IK hands follow the
    // housing and a 90-degree adjustment changes nothing about the actual grip.
    const auto palmInBody=compose(inverse(Pose{gripRotation,{}}),Pose{binocularPalmOrientation,{}}).orientation;
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
    const auto supportSocket=compose(body,Pose{palmInBody,binocularSupportSocket});
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
    result.primaryGrip=compose(body,Pose{palmInBody,binocularPrimarySocket});
    result.stability=supportHeld?1.f:.35f;
    result.ray=ray;
    return result;
}

std::optional<OpticPose> attachBinocularToPalm(const OpticPose& optic,Pose primaryPalm){
    if(!optic.tracked||optic.kind!=OpticKind::binocular||!finitePose(primaryPalm)
       ||!finitePose(optic.primaryGrip)||!finitePose(optic.body)||!finitePose(optic.renderBody)
       ||!finitePose(optic.leftEyepiece)||!finitePose(optic.rightEyepiece))return {};
    const auto delta=compose(primaryPalm,inverse(optic.primaryGrip));
    auto result=optic;
    result.body=compose(delta,optic.body);
    result.renderBody=compose(delta,optic.renderBody);
    result.leftEyepiece=compose(delta,optic.leftEyepiece);
    result.rightEyepiece=compose(delta,optic.rightEyepiece);
    result.primaryGrip=primaryPalm;
    if(optic.supportHeld)result.supportGrip=compose(delta,optic.supportGrip);
    result.ray.origin=compose(delta,Pose{{},optic.ray.origin}).position;
    result.ray.direction=rotate(delta.orientation,optic.ray.direction);
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
    bool leftHeld,bool rightHeld,bool available,uint64_t time,uint64_t epoch,Quat gripRotation){
    OpticSample result;
    if(!available||!epoch||time<time_||(epoch_&&epoch!=epoch_)){
        const bool wasActive=active_;
        reset();
        result.closed=wasActive;
        return result;
    }
    time_=time;epoch_=epoch;
    const auto pose=solveBinocularPose(leftGrip,rightGrip,leftAim,rightAim,
        leftTracked,rightTracked,leftAimTracked,rightAimTracked,leftHeld,rightHeld,gripRotation);
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
