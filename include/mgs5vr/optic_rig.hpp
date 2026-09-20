#pragma once
#include "core.hpp"
#include "stereo.hpp"
#include <array>
#include <cstdint>
#include <optional>

namespace mgs5vr {

enum class OpticKind { none, binocular, weaponScope };

// Weapon-local sockets must come from the equipped native sight, then be
// published with the solved weapon/skin frame. These are not HMD cameras.
struct WeaponScopeSample {
    Pose ocular{},objective{}; // OpenXR LOCAL, optical forward -Z.
    float radius{},eyeRelief{},magnification{};
    uint64_t weaponIdentity{};
    bool tracked{};
};
std::optional<EyeView> weaponScopeSceneView(const WeaponScopeSample& scope);
bool weaponScopeEyeVisible(const WeaponScopeSample& scope,Pose eye);

struct WeaponScopeGeometry {
    Pose ocular{},objective{}; // Equipped native weapon space; optical forward -Z.
    float radius{};
    uint32_t sight{};
    std::array<uint8_t,3> powers{};
};
// Match the equipped CNP_REAR_SIGHT / CNP_FRONT_SIGHT pair and native optical
// parameters to a measured round sight. Unknown/iron/holographic sights close.
std::optional<WeaponScopeGeometry> nativeWeaponScopeGeometry(Pose rear,Pose front,
    const std::array<uint8_t,4>& powersAndUi);
class WeaponScopeZoom {
public:
    float update(uint64_t identity,uint64_t sequence,const std::array<uint8_t,3>& powers);
private:
    uint64_t identity_{},sequence_{};
    std::array<uint8_t,3> powers_{};
    unsigned step_{};
};

// Configured presses stay active for 100 ms so the native game can sample
// them. Convert that stretched pulse back to one event before counting zoom.
class WeaponScopeZoomInput {
public:
    uint64_t update(bool requested,bool available);
private:
    uint64_t sequence_{};
    bool requested_{};
};

// The retail telescope's imported mesh frame, in metres. Both the housing
// shader and the optical camera use these sockets.
inline constexpr Vec3 binocularOcularCenter{-.032788f,-.000562f,.05512f};
inline constexpr Vec3 binocularObjectiveCenter{-.032788f,-.000562f,-.0505f};
inline constexpr float binocularOcularRadius=.0175f;
inline constexpr float binocularEyeRelief=.10f;
// Palm contact on the housing's two side walls, in the imported mesh frame.
inline constexpr Vec3 binocularPrimarySocket{.085f,-.008f,0};
inline constexpr Vec3 binocularSupportSocket{-.078f,-.008f,-.018f};
// Anatomical palm frame on each housing side. -Y runs wrist -> knuckles;
// -Z runs little -> index. Fingers rise over the top, thumbs face the ocular,
// and the mirrored palms face the two side walls. This is NOT a controller
// grip/aim calibration. Fit preserves this side-cup orientation: knuckles
// point to the housing top, never back toward the user's eyes.
inline constexpr Quat binocularPalmOrientation{1,0,0,0};

// The optical axis is continuously available while the primary hand is
// holding the device. It is a gameplay ray, not a visible laser: consumers
// can use it for native target acquisition while the binoculars are carried,
// before either eyepiece enters the stereo eye-relief gate.
struct OpticRay {
    Vec3 origin{};
    Vec3 direction{0,0,-1};
    bool tracked{};
};

// The device, ocular and tracked hands share one OpenXR LOCAL frame. The
// retail telescope has a single aperture that either eye can look through.
struct OpticPose {
    OpticKind kind{OpticKind::none};
    // Optical/body frame owned by the right grip. Eye relief and support
    // sockets are expressed in this frame.
    Pose body{};
    // The retail FMDL's broad +Z face is the physical ocular face. This is
    // the same grip-owned frame consumed by both the housing and the lens.
    Pose renderBody{};
    Pose leftEyepiece{};
    Pose rightEyepiece{};
    bool tracked{};
    bool primaryRight{true};
    bool supportHeld{};
    // The authored palm contact acquired by a nearby held left controller.
    // The first-person rig blends onto this side cup and releases it when
    // the tracked hand leaves the contact region or opens its grip.
    Pose supportGrip{};
    Pose primaryGrip{}; // Authored anatomical palm contact, not raw XR grip.
    // One-handed carry is intentionally less stable than a supported hold.
    // This is a presentation/gameplay hint; it never changes the native head
    // pose or stereo eye origins.
    float stability{};
    OpticRay ray{};
};

struct OpticSample {
    OpticPose pose{};
    bool aligned{};
    bool active{};
    bool held{};
    bool opened{};
    bool closed{};
    uint64_t sequence{};
};

// Build the binocular frame from the right primary grip/aim publication.  The
// left hand is optional support, just like a two-handed firearm.  The result is
// deliberately fail-closed when the primary hand or aim frame is missing; a
// stale primary hand must never leave an optic camera or native marker mode
// alive.
std::optional<OpticPose> solveBinocularPose(Pose leftGrip,Pose rightGrip,
    Pose leftAim,Pose rightAim,bool leftTracked,bool rightTracked,
    bool leftAimTracked,bool rightAimTracked,bool leftHeld,bool rightHeld,Quat gripRotation={});
// Local controller-aim to device rotation: yaw * pitch * roll. The primary
// palm socket stays fixed; housing, optical ray, eye relief and support rotate
// together, keeping the palms on the sides and fingers over the top. Default
// -90 pitch tilts down; zero restores the unadjusted aim direction.
Quat binocularGripRotation(float pitchDegrees,float yawDegrees,float rollDegrees);
// Move the entire device transaction onto the final IK palm. Never reapply
// controller aim calibration to a palm that has already acquired the device.
std::optional<OpticPose> attachBinocularToPalm(const OpticPose& optic,Pose primaryPalm);

// Constrain the primary palm before solving the arms, keeping the entire
// housing outside the face while preserving the hand/device attachment.
Pose binocularFaceSafeGrip(Pose head,Pose primary,const OpticPose& optic);

// Steady the complete palm-owned optic near either eye. Filtering is relative
// to the head, so normal head motion stays immediate. Housing, hands, optical
// scene and marking all consume this same adjusted grip publication.
class OpticStabilizer {
public:
    Pose update(Pose head,Pose grip,const OpticPose& optic,bool available,uint64_t time,uint64_t epoch);
    void reset(){*this=OpticStabilizer{};}
private:
    Pose filtered_{};
    uint64_t time_{},epoch_{};
    bool ready_{};
};

// Independent native scene camera. It follows the device even while carried;
// neither HMD eye pose nor its field of view is an input or an output.
std::optional<EyeView> binocularSceneView(const OpticPose& optic,float magnification);
// Only the eye behind the real exit pupil receives its magnified image.
bool binocularEyeVisible(const OpticPose& optic,Pose eye);

// Validate tracking without moving either native head-derived stereo origin.
// The independent device scene supplies magnification inside its aperture.
bool validateBinocularViews(const OpticSample& optic,const Pose& head,
    const std::array<EyeView,2>& views);

class OpticGate {
public:
    OpticSample update(Pose head,const std::array<EyeView,2>& eyes,
        Pose leftGrip,Pose rightGrip,Pose leftAim,Pose rightAim,
        bool leftTracked,bool rightTracked,bool leftAimTracked,bool rightAimTracked,
        bool leftHeld,bool rightHeld,bool available,uint64_t time,uint64_t epoch,Quat gripRotation={});
    void reset(){*this=OpticGate{};}
    bool active() const {return active_;}
private:
    bool active_{};
    uint64_t time_{},epoch_{},sequence_{};
};

}
