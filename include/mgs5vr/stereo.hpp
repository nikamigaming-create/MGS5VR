#pragma once
#include "core.hpp"

namespace mgs5vr {
struct EyeFov { float left{},right{},up{},down{}; };
enum class SpatialUiLayout { frontEndMenu, pauseMenu, avatarEditor, wristMenu };
SpatialUiLayout selectSpatialUiLayout(bool frontEnd,bool avatarEditor,bool menuOpen,bool idroidMenu,bool wristMounted=false) noexcept;
std::array<float,16> spatialUiProjection(const std::array<float,16>& nativeProjection,SpatialUiLayout layout) noexcept;
float spatialUiPlaneCenterX(SpatialUiLayout layout) noexcept;
// Restore the native 128 x 72 layout canvas independently of render-target
// shape. Native layout cameras inherit the active eye viewport's aspect.
std::array<float,16> nativeUiCanvasProjection(const std::array<float,16>& uiProjection,
    const std::array<float,16>& eyeProjection) noexcept;
std::optional<std::array<float,16>> uiPanelProjection(const std::array<float,16>& uiProjection,
    const std::array<float,16>& eyeView,EyeFov fov,Pose panel,float width,float height,
    float centerX=0,float centerY=0);
struct EyeView { Pose pose{}; EyeFov fov{}; };
// Bound an automatic startup/loading panel to the common neutral-gaze field
// of both eyes, with a border and allowance for the eye offsets from the head.
std::optional<float> fittedScreenWidth(float requested,float distance,float aspect,
    const std::array<EyeFov,2>& fovs,float eyeRadius);
// Keep the popup centered above the rendered forearm, including when the wrist
// leaves the view. Head orientation supplies readability, never a new position.
// All poses must belong to the same source transaction; native world Y is up.
std::optional<Pose> wristPopupPose(Pose forearm,Pose head,float height);
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
struct NativeProjectionScales {float focal{},aspect{};};
// Keep native camera consumers on the same angular projection as rasterization.
// Focal scales X/Y together; viewport aspect scales Y independently in FOX.
std::optional<NativeProjectionScales> nativeProjectionScales(float focal,float aspect,
    const std::array<float,16>& nativeProjection,EyeFov renderedFov);
// Conservative angular visibility coverage before either eye is drawn. Keeps
// native coverage if it is wider and adds a small symmetric head-turn margin.
bool widenVisibilityProjection(std::array<float,16>& matrix,Pose head,const std::array<EyeView,2>& views);
Pose nativeEyePose(Pose nativeHead,Pose sourceHead,Pose sourceEye,float units=1);
// The resulting controller axes retain the OpenXR grip/aim convention. Unlike
// a FOX camera, a controller is not conjugated back into FOX camera-local axes.
Pose nativeTrackedPose(Pose nativeHead,Pose sourceHead,Pose trackedPose,float units=1);
bool readyEyePair(const std::array<EyeFrame,2>& eyes,uint64_t activation,uint64_t now,uint64_t maximumAgeMs=150);
// Admission requires a fresh, joined pair. Once uploaded, that exact image and
// its original poses can be reprojected during a producer/runtime stall. Age
// does not turn valid stored pixels into a missing image. Epoch, activation,
// clock and camera/tracking ownership must still be checked at presentation.
class AcceptedStereoFrame {
public:
    bool accept(const std::array<EyeFrame,2>& eyes,uint64_t epoch,uint64_t activation,uint64_t now,uint64_t referenceEpoch=0);
    bool presentable(uint64_t epoch,uint64_t activation,uint64_t now) const;
    // A positively identified native shot handoff may display the PREVIOUS
    // image with its original poses for at most 250 ms while new eyes arrive.
    // This does not admit pixels or make them valid for the new activation.
    bool transitionPresentable(uint64_t epoch,uint64_t activation,uint64_t authorizedActivation,
        uint64_t referenceEpoch,uint64_t now);
    void clear(){eyes_={};epoch_=referenceEpoch_=transitionAt_=0;transitionStarted_=false;}
    const std::array<EyeFrame,2>& eyes() const {return eyes_;}
private:
    std::array<EyeFrame,2> eyes_{};
    uint64_t epoch_{};
    uint64_t referenceEpoch_{},transitionAt_{};
    bool transitionStarted_{};
};
// One visibility decision for the entire stereo family. A wrist plane near
// edge-on must never be accepted by one eye and rejected by the other.
bool panelFacesBothEyes(Pose panel,const std::array<Pose,2>& eyes);
}
