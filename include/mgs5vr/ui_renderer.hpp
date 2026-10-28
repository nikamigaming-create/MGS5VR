#pragma once
#include "stereo.hpp"
#include "head_camera.hpp"
#include "native_demo_state.hpp"
#include "native_popup_state.hpp"
#include <array>
#include <cstdint>
#include <iosfwd>
#include <string>

namespace mgs5vr {
void installUiRenderer(uintptr_t moduleBase);
void installGzUiRenderer(uintptr_t moduleBase);
// Exact native IsMbDvcTerminalOpened reader; no UI state is written.
std::optional<bool> nativeMenuOpen() noexcept;
// The terminal bit is kept separate from the actual native pause menu. This
// lets the tracked iDroid remain a live gameplay overlay without treating a
// deliberate Pause screen as locomotion-capable.
bool nativeIdroidOpen() noexcept;
bool nativeIdroidOverlayOnly() noexcept;
// Cached native Pause-menu ownership, separate from iDroid and scene menus.
bool nativePauseMenuOpen() noexcept;
// Closing needs native character updates to stow the device. Unknown state
// is treated as closing so our optional pause can never trap that transition.
bool nativeIdroidClosing() noexcept;
std::optional<unsigned> nativeIdroidTutorialMode() noexcept;
// Read the verified guide's own active gameplay-pause slot; never release it.
bool nativeIdroidTutorialPaused() noexcept;
// Strictly read-only popup identity. Unknown/current/last-result are distinct;
// the result is not a live focus or choice publication.
NativePopupSnapshot nativePopupSnapshot() noexcept;
// Native tutorial pause ownership is separate from its input restriction.
// Caller must establish the completed ACC guide in the native Lua transaction.
bool releaseNativeIdroidTutorialPause(bool requestedStow=false) noexcept;
uintptr_t captureNativeIdroidStowHelpOwner() noexcept;
uintptr_t nativeIdroidInformationalHelpOwner() noexcept;
enum class NativeIdroidStowHelpState { unavailable, ready, closed };
// Read only: the actual Help UI must handle its Confirm and close its card.
NativeIdroidStowHelpState nativeIdroidStowHelpState(uintptr_t owner) noexcept;
bool nativeTitleMenuOpen() noexcept;
void publishNativeHospitalTitleMenu(bool active) noexcept;
struct OpeningNativeFocus;
OpeningNativeFocus nativeTitleFocus() noexcept;
void publishNativeAvatarEdit(bool active) noexcept;
bool nativeAvatarEditActive() noexcept;
void publishNativeSceneClear(bool visible) noexcept;
bool nativeSceneClear() noexcept;
// NativeDemoMode is defined with the dependency-free shared snapshot.
void publishNativeDemoMode(NativeDemoMode mode,uint64_t candidateKey=0,uint64_t lookLessonKey=0) noexcept;
NativeDemoMode nativeDemoMode() noexcept;
uint64_t nativeDemoCandidateKey() noexcept;
uint64_t nativeDemoCandidateGeneration() noexcept;
bool nativeScriptedDemoActive() noexcept;
// Published on the native Lua transaction thread. A still-updating terminal
// must not resurrect cassette geometry after ClearTitleMode has run.
void publishNativeTitleMode(bool active) noexcept;
bool nativeTitleModeActive() noexcept;
// First-launch hospital scenes retain retail TitleMode without being startup UI.
void publishNativeOpeningCinematic(bool active) noexcept;
bool nativeOpeningCinematic() noexcept;
// The physical cassette selector is valid only for the native helicopter
// title cabin. Fresh-start title flows can keep TitleMode while showing the
// hospital intro and must retain their native menu and input.
void publishNativeTitleCabinMode(bool active) noexcept;
bool nativeTitleCabinMode() noexcept;
// The native title closes before a saved helicopter-cabin session publishes a
// player rig. Keep that verified cabin scene alive until the field scene loads.
void publishNativeCabinPlay(bool active) noexcept;
bool nativeCabinPlay() noexcept;
void publishNativeCabinTransit(bool active) noexcept;
bool nativeCabinTransit() noexcept;
// Retail sortie/customization and game-over screens have no iDroid open bit.
void publishNativeSceneMenu(bool active) noexcept;
void publishNativeSceneCanvas(bool active) noexcept;
bool nativeSceneCanvasActive() noexcept;
void publishNativeSortiePreview(bool active) noexcept;
bool nativeSortiePreviewActive() noexcept;
bool nativeLoadingTipsOpen() noexcept;
// Source time of the latest expanded native equipment-description draw.
uint64_t nativeEquipmentPickerDrawTime() noexcept;
// Request the game's neutral four-category equipment cross, without a D-pad
// press. Its UI update owns opening/closing; no equipment action is synthesized.
void requestNativeEquipmentPreview(bool visible) noexcept;
uint64_t nativeCommandsDrawTime() noexcept;
std::optional<Pose> wristPickerPose(const HeadCameraSample& rig) noexcept;
// Producer scope is the native scene invocation; the UI may execute on a worker.
void setUiRenderSource(const EyeFrame& eye,uintptr_t camera,const std::array<float,16>& view,
                       const std::array<float,16>& projection,const HeadCameraSample& rig,const std::array<float,16>& authoredView,
                       const std::array<float,16>& authoredProjection,HudView hudView);
void clearUiRenderSource() noexcept;
// Native recon bodies are scene models, not UI nodes. Restrict their material
// opacity during one native scene pass and restore the exact native colors.
class ReconModelVisibilityScope {
public:
    ReconModelVisibilityScope(HudMode mode,HudView view,bool glow) noexcept;
    ~ReconModelVisibilityScope();
    ReconModelVisibilityScope(const ReconModelVisibilityScope&)=delete;
    ReconModelVisibilityScope& operator=(const ReconModelVisibilityScope&)=delete;
private:
    struct Change {uintptr_t owner{},model{};uint32_t parameter{};std::array<float,4> color{};};
    std::array<Change,128> changes_{};
    size_t count_{};
};
// Called only at the verified native UI perspective-builder return address.
bool applyUiEyeProjection(float* output) noexcept;
void reportUiRenderer(std::ostream& output);
// Reset/read only the bounded draw inventory; never changes native UI state.
std::string inspectUiDrawInventory(bool reset);
void stopUiRenderer() noexcept;
}
