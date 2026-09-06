#pragma once
#include "core.hpp"

namespace mgs5vr {
struct ArmPose { Pose shoulder,elbow,wrist; };
struct ArmSolution { ArmPose pose; bool reachClamped{}; };
std::optional<ArmSolution> solveArm(const ArmPose& animated,Pose wristTarget,Vec3 bendHint);
std::optional<Pose> nativeAffinePose(const std::array<float,16>& rowMatrix);
}
