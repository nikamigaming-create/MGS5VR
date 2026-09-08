#pragma once
#include "core.hpp"

namespace mgs5vr {
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
// Landscape display: +X runs elbow to wrist; +Z is the back of the forearm.
std::optional<Pose> forearmPanel(Pose elbow,Pose wrist,Vec3 dorsal);
class SupportContact {
public:
    // Distance is from the tracked palm to the weapon's support grip.
    bool update(bool ready,bool tracked,float distance,uint64_t time);
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
std::optional<Pose> nativeAffinePose(const std::array<float,16>& rowMatrix);
}
