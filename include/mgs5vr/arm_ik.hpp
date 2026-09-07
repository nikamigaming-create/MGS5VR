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
    bool update(bool ready,bool tracked,float distance);
    void reset(){attached_=false;}
private:
    bool attached_{};
};
// OpenXR grip frame from the native wrist and the index/little metacarpal heads.
// Unlike activation-pose calibration this is independent of the camera and of
// where the user happened to hold the controller when enabling VR.
std::optional<Pose> anatomicalGrip(Pose wrist,Vec3 indexKnuckle,Vec3 littleKnuckle);
std::optional<Pose> nativeAffinePose(const std::array<float,16>& rowMatrix);
}
