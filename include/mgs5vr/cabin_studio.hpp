#pragma once
#include "cabin_props.hpp"
#include "gear_contact.hpp"
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mgs5vr {
struct StudioPoint { float u{},v{}; };
struct StudioStroke { std::vector<StudioPoint> points; };
struct StudioInk { uint64_t revision{}; std::string text; std::vector<StudioStroke> strokes; };
struct StudioPaper {
    uint32_t id{};
    uint64_t photo{};
    Pose pose{}; // Authored helicopter model coordinates, retained across visits.
    Vec3 velocity{};
    int hand{-1};
    bool pinned{};
    // A newly printed field copy is offered at the camera, then goes to the
    // cabin when stowed. Its retained pose always belongs to the cabin.
    bool field{};
    Pose fieldPose{}; // OpenXR LOCAL; transient, never written as a cabin pose.
    std::shared_ptr<const StudioInk> ink;
};
struct StudioHand { Pose grip{},aim{}; bool tracked{},aimTracked{}; float squeeze{},trigger{}; };
enum class StudioControl : uint8_t { back,next,print,erase,live,freeze,count };
enum class StudioFreeze : uint8_t { live,pending,frozen,unavailable };
struct CameraFreezeRequest {bool requested{};uint64_t activation{},referenceEpoch{},at{};};
void requestCameraFreeze(bool requested,uint64_t activation=0,uint64_t referenceEpoch=0) noexcept;
CameraFreezeRequest cameraFreezeRequest() noexcept;
void acknowledgeCameraFreeze(StudioFreeze status,bool owned) noexcept;
StudioFreeze cameraFreezeState() noexcept;
bool cameraWorldFrozen() noexcept;
struct StudioCollection {
    std::vector<StudioPaper> papers;
    std::vector<uint64_t> photos;
    uint64_t selectedPhoto{};
};
struct CabinStudioFrame {
    Pose roomTracking{},camera{},marker{},keyboard{};
    std::optional<Pose> fieldCameraHolster; // LOCAL, from the admitted native body/hip publication.
    std::vector<StudioPaper> papers;
    std::vector<uint64_t> photos; // Digital library. Paper objects are independent.
    int cameraHand{-1},markerHand{-1},editor{-1},hoverKey{-1};
    int hoverControl{-1};
    uint64_t shutter{},revision{};
    uint64_t lastPhoto{};
    uint64_t selectedPhoto{},confirmPhoto{};
    uint32_t confirmPaper{};
    Pose shutterPose{};
    uint8_t inputMask{},hapticMask{};
    uint8_t cameraCandidateMask{};
    bool cabin{},available{},capacityReached{},keyboardOnly{};
    bool review{},freezeRequested{},freezeSupported{};
    StudioFreeze freezeState{StudioFreeze::live};
};
inline constexpr size_t studioPaperLimit=32;
inline constexpr size_t studioPhotoLimit=512;
inline constexpr size_t studioTextLimit=96;
inline constexpr Vec3 studioNotesHome{.775f,-2.207f,-.900f};
inline constexpr Vec3 studioCameraHome{.760f,-2.130f,-1.180f};
inline constexpr Pose studioCameraLens{{},{-.019f,.003f,-.071f}};
inline constexpr Vec3 studioCameraHalf{.093f,.076f,.071f};
inline constexpr Vec3 studioCameraRightGrip{.076f,-.007f,.010f};
inline constexpr Vec3 studioCameraLeftGrip{-.061f,-.007f,.010f};
inline constexpr Vec3 studioMarkerHome{.650f,-2.204f,-.900f};
inline constexpr Vec3 studioPrintHome{.390f,-2.209f,-1.130f};
inline constexpr Vec3 studioBoardCenter=cabinNativeBoard.position;
Vec3 studioPaperHalf(bool photo) noexcept;
Pose studioCameraPalm(Pose camera,size_t side) noexcept;
// Native anatomical frames in one shared space: +X left, +Y up, +Z forward.
// The mod-authored camera has a -Z lens and rests outside the right hip.
std::optional<Pose> studioFieldCameraHolster(Pose pelvis,Pose rightHip) noexcept;
void joinStudioBody(CabinStudioFrame& frame,Pose pelvis,Pose rightHip);
std::string_view studioKey(size_t index) noexcept;
Pose studioKeyPose(Pose keyboard,size_t index) noexcept;
Pose studioControlPose(Pose camera,StudioControl control) noexcept;
std::string_view studioControlLabel(StudioControl control) noexcept;
inline constexpr Vec3 studioControlHalf{.025f,.008f,.002f};
// All arguments use OpenXR LOCAL space. Rejoin held geometry to the palms
// from the completed native skin transaction, including anatomical limits.
void joinStudioHands(CabinStudioFrame& frame,const std::array<Pose,2>& inputGrips,
    const std::array<Pose,2>& renderedPalms,const std::array<bool,2>& tracked);
class CabinStudio {
public:
    std::shared_ptr<const CabinStudioFrame> update(bool allowed,bool cabin,Pose head,Pose roomTracking,
        const std::array<StudioHand,2>& hands,uint64_t now,uint64_t newPhoto=0,
        bool freezeSupported=false,StudioFreeze freeze=StudioFreeze::live,
        std::optional<Pose> fieldCameraHolster={});
    void restore(std::vector<StudioPaper> papers);
    void restore(StudioCollection collection);
private:
    void print(uint64_t photo,bool field);
    void control(StudioControl control,uint64_t now);
    CabinStudioFrame frame_{};
    std::array<Pose,2> attachment_{};
    std::array<bool,2> squeezed_{},triggered_{};
    std::array<GearContact,2> cameraContact_{};
    uint64_t last_{},lastShot_{},processedPhoto_{};
    uint32_t nextId_{1};
    bool initialized_{},cameraDropped_{},markerDropped_{},writing_{};
    Vec3 cameraVelocity_{},markerVelocity_{};
};
}
