#pragma once
#include <array>
#include <cstdint>

namespace mgs5vr {
// Keep the native body visible for scripted scenes, even while immersive
// presentation is active; ordinary first-person gameplay still hides it.
constexpr bool hidePlayerInFirstPerson(bool immersiveActive,bool scriptedDemo) noexcept {
    return immersiveActive&&!scriptedDemo;
}
// A short camera-publication gap is only permission to restore concealed
// groups when current presentation policy also says concealment is no longer
// wanted. An active stereo menu retains first-person concealment through Pause.
constexpr bool shouldRestoreStalePlayerVisibility(uint64_t elapsedMs,bool immersiveActive,
                                                   bool scriptedPresentation,bool stereoMenuOpen) noexcept {
    return elapsedMs>=250&&(!immersiveActive||(scriptedPresentation&&!stereoMenuOpen));
}
void initializePlayerVisibility(uintptr_t moduleBase) noexcept;
// Called on the native player camera publication thread, after the player has
// updated its appearance. Only verified models owned by this player are changed.
void updatePlayerVisibility(uintptr_t cameraOwner,bool firstPerson,bool hideArms=false) noexcept;
// Exclude the owned player only from the native-camera image copied onto the
// Title panel. The opening selector may preserve the verified arm subtree so
// the tracked hands remain connected to the visible forearms.
// Scope restoration never re-enables native-hidden groups or a replaced model.
class MenuCapturePlayerExclusion {
public:
    explicit MenuCapturePlayerExclusion(uintptr_t cameraOwner,bool preserveArms=false) noexcept;
    ~MenuCapturePlayerExclusion();
    MenuCapturePlayerExclusion(const MenuCapturePlayerExclusion&)=delete;
    MenuCapturePlayerExclusion& operator=(const MenuCapturePlayerExclusion&)=delete;
private:
    uintptr_t owner_{},character_{},parts_{},model_{};
    bool preserveArms_{};
    std::array<uint32_t,128> groups_{};
    std::array<uint8_t,128> flags_{};
    uint16_t count_{};
};
}
