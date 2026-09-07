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
    EyeFov displayFov{}; // Requested optics; view.fov describes the rendered pixels.
};
bool valid(EyeFov fov);
std::optional<EyeFov> enclosingEyeFov(EyeFov requested);
struct EyeImageRegion { int32_t x{},y{},width{},height{}; EyeFov fov{}; };
// D3D top-left pixel rectangle. The returned FOV describes the rounded rectangle
// exactly, rather than stretching a rounded crop onto different angular rays.
std::optional<EyeImageRegion> eyeImageRegion(EyeFov rendered,EyeFov requested,uint32_t width,uint32_t height);
// FOX row-vector projection: camera +Z forward, -X screen-right. Retains the
// engine's depth mapping while replacing the angular field and optical center.
bool setEyeProjection(std::array<float,16>& matrix,EyeFov fov);
// Conservative angular visibility coverage before either eye is drawn. Keeps
// native coverage if it is wider and adds a small symmetric head-turn margin.
bool widenVisibilityProjection(std::array<float,16>& matrix,Pose head,const std::array<EyeView,2>& views);
Pose nativeEyePose(Pose nativeHead,Pose sourceHead,Pose sourceEye,float units=1);
// The resulting controller axes retain the OpenXR grip/aim convention. Unlike
// a FOX camera, a controller is not conjugated back into FOX camera-local axes.
Pose nativeTrackedPose(Pose nativeHead,Pose sourceHead,Pose trackedPose,float units=1);
bool readyEyePair(const std::array<EyeFrame,2>& eyes,uint64_t activation,uint64_t now,uint64_t maximumAgeMs=150);
// One visibility decision for the entire stereo family. A wrist plane near
// edge-on must never be accepted by one eye and rejected by the other.
bool panelFacesBothEyes(Pose panel,const std::array<Pose,2>& eyes);
}
