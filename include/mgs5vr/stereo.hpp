#pragma once
#include "core.hpp"

namespace mgs5vr {
struct EyeFov { float left{},right{},up{},down{}; };
std::optional<std::array<float,16>> uiPanelProjection(const std::array<float,16>& uiProjection,
    const std::array<float,16>& eyeView,EyeFov fov,Pose panel,float width,float height,
    float centerX=0,float centerY=0);
struct EyeView { Pose pose{}; EyeFov fov{}; };
// Unfold a wrist-anchored panel at readable depth. Preserve its physical size,
// and move it inward only as far as needed to fit both requested eye frustums.
// All poses use -Z forward and must come from the same source transaction.
std::optional<Pose> fitWristPanel(Pose head,Vec3 anchor,const std::array<EyeView,2>& eyes,
                                float width,float height);
// Carried beside the pixels under the GPU mailbox mutex. Never reconstructed
// from the newest tracking sample at presentation time.
struct EyeFrame {
    EyeView view{};
    uint64_t sourceSequence{},trackingSequence{},activation{},sampleTime{};
    uint32_t eye{};
    bool projected{},joined{};
    EyeFov displayFov{}; // Requested optics; view.fov describes the rendered pixels.
    float magnification{1}; // Scene angles are optically magnified into displayFov.
};
bool valid(EyeFov fov);
std::optional<EyeFov> opticalFov(EyeFov fov,float magnification);
std::optional<EyeFov> enclosingEyeFov(EyeFov requested);
struct EyeImageRegion { int32_t x{},y{},width{},height{}; EyeFov fov{}; };
// D3D top-left pixel rectangle. The returned FOV describes the rounded rectangle
// exactly, rather than stretching a rounded crop onto different angular rays.
std::optional<EyeImageRegion> eyeImageRegion(EyeFov rendered,EyeFov requested,uint32_t width,uint32_t height);
// FOX row-vector projection: camera +Z forward, -X screen-right. Retains the
// engine's depth mapping while replacing the angular field and optical center.
bool setEyeProjection(std::array<float,16>& matrix,EyeFov fov);
// The native perspective builder must consume this near plane itself, so its
// depth coefficients, reconstruction and visibility agree. Never raise an
// already closer plane, alter the far plane, or repair invalid native inputs.
float trackedNearPlane(float nativeNear,float nativeFar);
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
