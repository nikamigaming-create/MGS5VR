#pragma once
#include "core.hpp"

namespace mgs5vr {
struct ArmPose { Pose shoulder,elbow,wrist; };
struct ArmSolution { ArmPose pose; bool reachClamped{}; };
std::optional<ArmSolution> solveArm(const ArmPose& animated,Pose wristTarget,Vec3 bendHint,bool followWristTwist=false);
// OpenXR grip frame from the native wrist and the index/little metacarpal heads.
// Unlike activation-pose calibration this is independent of the camera and of
// where the user happened to hold the controller when enabling VR.
std::optional<Pose> anatomicalGrip(Pose wrist,Vec3 indexKnuckle,Vec3 littleKnuckle);
std::optional<Pose> nativeAffinePose(const std::array<float,16>& rowMatrix);
}
