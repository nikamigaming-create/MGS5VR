#pragma once
#include "core.hpp"

namespace mgs5vr {
struct EyeFov { float left{},right{},up{},down{}; };
std::optional<std::array<float,16>> uiPanelProjection(const std::array<float,16>& uiProjection,
    const std::array<float,16>& eyeView,EyeFov fov,Pose panel,float width,float height,
    float centerX=0,float centerY=0);
struct EyeView { Pose pose{}; EyeFov fov{}; };
// Carried beside the pixels under the GPU mailbox mutex. Never reconstructed
// from the newest tracking sample at presentation time.
struct EyeFrame {
    EyeView view{};
    uint64_t sourceSequence{},trackingSequence{},activation{},sampleTime{};
    uint32_t eye{};
    bool projected{},joined{};
};
bool valid(EyeFov fov);
// FOX row-vector projection: camera +Z forward, -X screen-right. Retains the
// engine's depth mapping while replacing the angular field and optical center.
bool setEyeProjection(std::array<float,16>& matrix,EyeFov fov);
Pose nativeEyePose(Pose nativeHead,Pose sourceHead,Pose sourceEye,float units=1);
// The resulting controller axes retain the OpenXR grip/aim convention. Unlike
// a FOX camera, a controller is not conjugated back into FOX camera-local axes.
Pose nativeTrackedPose(Pose nativeHead,Pose sourceHead,Pose trackedPose,float units=1);
bool readyEyePair(const std::array<EyeFrame,2>& eyes,uint64_t activation,uint64_t now);
// One visibility decision for the entire stereo family. A wrist plane near
// edge-on must never be accepted by one eye and rejected by the other.
bool panelFacesBothEyes(Pose panel,const std::array<Pose,2>& eyes);
}
