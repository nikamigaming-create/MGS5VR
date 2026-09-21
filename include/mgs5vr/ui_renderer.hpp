#pragma once
#include "stereo.hpp"
#include "head_camera.hpp"
#include <array>
#include <cstdint>
#include <iosfwd>

namespace mgs5vr {
void installUiRenderer(uintptr_t moduleBase);
// Exact native IsMbDvcTerminalOpened reader; no UI state is written.
std::optional<bool> nativeMenuOpen() noexcept;
// The terminal bit is kept separate from the actual native pause menu. This
// lets the tracked iDroid remain a live gameplay overlay without treating a
// deliberate Pause screen as locomotion-capable.
bool nativeIdroidOpen() noexcept;
// Closing needs native character updates to stow the device. Unknown state
// is treated as closing so our optional pause can never trap that transition.
bool nativeIdroidClosing() noexcept;
bool nativeTitleMenuOpen() noexcept;
void publishNativeAvatarEdit(bool active) noexcept;
bool nativeAvatarEditActive() noexcept;
void publishNativeScriptedDemo(bool active) noexcept;
bool nativeScriptedDemoActive() noexcept;
// Published on the native Lua transaction thread. A still-updating terminal
// must not resurrect cassette geometry after ClearTitleMode has run.
void publishNativeTitleMode(bool active) noexcept;
bool nativeTitleModeActive() noexcept;
// The physical cassette selector is valid only for the native helicopter
// title cabin. Fresh-start title flows can keep TitleMode while showing the
// hospital intro and must retain their native menu and input.
void publishNativeTitleCabinMode(bool active) noexcept;
bool nativeTitleCabinMode() noexcept;
// The native title closes before a saved helicopter-cabin session publishes a
// player rig. Keep that verified cabin scene alive until the field scene loads.
void publishNativeCabinPlay(bool active) noexcept;
bool nativeCabinPlay() noexcept;
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
void stopUiRenderer() noexcept;
}
