#pragma once
#include "core.hpp"
#include <span>

namespace mgs5vr {
struct RigContinuityKey {
    uintptr_t owner{},model{};
    uint64_t activation{},epoch{};
    bool operator==(const RigContinuityKey&) const=default;
};
// Coco's occlusion continuity, limited to presentation. Raw tracking still
// owns firing, throws, melee, animal contact and controller interaction.
class HandPresentationCache {
public:
    std::optional<Pose> update(Pose grip,bool tracked,uint64_t time,RigContinuityKey key);
    void reset(){*this={};}
private:
    RigContinuityKey key_{};
    Pose last_{},recoveryFrom_{};
    uint64_t sampleAt_{},trackedAt_{},recoveryAt_{};
    bool valid_{},tracked_{};
};
class StationaryBodyState {
public:
    bool update(bool eligible,Vec3 root,uint64_t time,RigContinuityKey key);
    void reset(){*this={};}
private:
    RigContinuityKey key_{};
    Vec3 root_{};
    uint64_t time_{},stoppedAt_{};
    bool valid_{},moved_{},active_{};
};
struct StationaryBodyPose {Vec3 pelvis,leftHip,rightHip,leftShoulder,rightShoulder;Pose chest;};
std::optional<Pose> stationaryLowerBodyDelta(const StationaryBodyPose& body,Pose torsoDelta);
struct ArmPose { Pose shoulder,elbow,wrist; };
struct ArmSolution { ArmPose pose; bool reachClamped{}; };
// A contact returned by native world collision, in the arm solve's space.
struct ArmSurface { Vec3 point,normal; float clearance{}; };
std::optional<Vec3> outsideArmSurface(Vec3 point,const ArmSurface& surface);
// Axes in the authored joint frames, independent of the current weapon animation.
struct ArmBasis { Vec3 upperAxis,forearmAxis; Vec3 elbowBend{0,0,1},wristUp{0,1,0}; };
// A shared model-space transform for the spine, clavicles and arm roots.
// uprightHead carries the camera's yaw, without tracked look pitch or roll.
std::optional<Pose> upperBodyPlacement(Pose chest,Vec3 shoulderCenter,Pose uprightHead);
std::optional<ArmSolution> solveArm(const ArmPose& animated,Pose wristTarget,Vec3 bendHint,
                                  const ArmBasis* basis=nullptr,const ArmSurface* elbowSurface=nullptr);
// Local corrective rotations observed on this profile's seven arm helpers.
std::array<Quat,7> armCorrectiveRotations(Quat clavicle,Quat upper,Quat elbow,Quat wrist,bool right);
inline constexpr std::array<uint32_t,14> armHelperNames{
    0x8cb42ff9,0x17c46537,0x668bcff7,0xf8ae9203,0x9ccbd1fd,0xc7a9a0c4,0x6cae37b1,
    0x82901b42,0x4b89fc94,0x18f26b1e,0x24ce95fb,0x0831f646,0x9bed7bf0,0xa4b3e85d};
inline constexpr std::array<int32_t,14> armHelperParents{5,6,6,7,7,7,8,9,10,10,11,11,11,12};
// Outfits insert different joints before the arm corrections. Identity and
// verified parentage own the binding; an index from another outfit does not.
std::optional<std::array<size_t,14>> armHelperIndices(std::span<const uint32_t> names,
    std::span<const int32_t> parents);
// Landscape display: +X runs elbow to wrist; +Z is the back of the forearm.
std::optional<Pose> forearmPanel(Pose elbow,Pose wrist,Vec3 dorsal,float surfaceLift=.02f);
// Support remains in front of calibrated primary aim, before two-hand guidance.
// guided weapon cannot keep a withdrawn hand attached by rotating after it.
bool withinSupportCone(Vec3 handSeparation,Vec3 primaryForward);
// Classify the native palm-to-palm contact, not the current controller gap.
// Close pistol-style cups brace the firing hand; they do not steer a barrel
// with the unstable direction between two almost-coincident controllers.
bool closeSupportContact(Vec3 authoredHandSeparation);
class SupportContact {
public:
    // Distance is from the tracked palm to the weapon's support grip.
    bool update(bool ready,bool tracked,float distance,uint64_t time,float acquireRadius=.10f,float detachRadius=.30f);
    bool attached() const {return attached_;}
    void reset(){attached_=false;candidate_=false;since_=lastTime_=0;}
private:
    bool attached_{},candidate_{};
    uint64_t since_{},lastTime_{};
};
// Keep contact in the acquired weapon frame. During release, retain the last
// presented offset so an unrelated stow animation cannot drag the free hand.
class SupportPose {
public:
    std::optional<Pose> update(Pose nativeOffset,bool attached,bool manipulating);
    void reset(){attached_=false;acquired_={};presented_.reset();}
private:
    bool attached_{};
    Pose acquired_{};
    std::optional<Pose> presented_;
};
// OpenXR grip frame from the native wrist and the index/little metacarpal heads.
// Unlike activation-pose calibration this is independent of the camera and of
// where the user happened to hold the controller when enabling VR.
std::optional<Pose> anatomicalGrip(Pose wrist,Vec3 indexKnuckle,Vec3 littleKnuckle);
// Firearm-only local pointing pitch. Controller/device calibration remains
// separate; the corrected direction is solved through the native firing wrist.
std::optional<Pose> weaponAimWithPitch(Pose aim,float pitchDegrees);
// Align the authored muzzle axis with runtime aim -Z at the tracked palm
// pivot. The solved wrist still owns the hand, weapon, sights and shot socket.
std::optional<Pose> aimedWeaponGrip(Pose primary,Pose aim,Vec3 forwardInPrimary);
// Swing the authored barrel direction toward the other controller while
// keeping the primary palm fixed. Roll stays owned by the primary controller.
std::optional<Pose> twoHandGrip(Pose primary,Pose support,Vec3 forwardInPrimary,float influence);
struct PointThrow { Vec3 origin,velocity; };
// Start at the rendered palm; point along OpenXR aim -Z. The native solver
// supplies speed at neutral camera angles, so looking/turning cannot steer it.
std::optional<PointThrow> pointThrow(Pose renderedPalm,Pose gripFromAim,Vec3 nativeVelocity);
// Local rotations for the authored straight finger chains (thumb through pinky).
// Native weapon/contact animation remains authoritative while a hand is attached.
std::optional<Quat> fingerJointRotation(bool right,unsigned finger,unsigned joint,float curl);
float freeFingerCurl(unsigned finger,float trigger,float squeeze,bool triggerTouched,bool thumbTouched,
                     float resting=.08f,float touched=.20f);
std::optional<Pose> nativeAffinePose(const std::array<float,16>& rowMatrix);
}
