#pragma once
#include <array>
#include <cstdint>
#include <limits>
#include <optional>

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
struct NativeOpaquePublication {
    uint8_t opacity{};
    uint8_t flags{};
};
// The native visual publisher consumes bit 0x20 as an immediate opacity
// command, then clears it. Retain its other draw, visibility and dirty bits.
constexpr NativeOpaquePublication firstPersonOpaquePublication(uint8_t nativeFlags) noexcept {
    return {255,static_cast<uint8_t>(nativeFlags|0x20u)};
}
// The owned visual pool has contiguous 0x80-byte records and a native base ID.
// Never treat the absolute visual ID as a zero-based array index.
constexpr std::optional<uintptr_t> nativeVisibilityRecordAddress(uintptr_t records,
        uint32_t count,uint32_t first,uint32_t visualId) noexcept {
    if(!records||!count||count>32||visualId<first||visualId-first>=count)return {};
    const auto offset=static_cast<uintptr_t>(visualId-first)*0x80u;
    if(records>(std::numeric_limits<uintptr_t>::max)()-offset)return {};
    return records+offset;
}
struct NativeVisibilityIdentity {
    uintptr_t owner{},character{},bodyInterface{},holder{},model{},pool{},records{},record{};
    uint32_t visualId{},first{},count{};
    bool operator==(const NativeVisibilityIdentity&) const=default;
};
constexpr bool sameNativeVisibilityIdentity(const NativeVisibilityIdentity& a,
        const NativeVisibilityIdentity& b) noexcept {
    const auto mapped=nativeVisibilityRecordAddress(a.records,a.count,a.first,a.visualId);
    return a.owner&&a.character&&a.bodyInterface&&a.holder&&a.model&&a.pool&&mapped
        &&*mapped==a.record&&a==b;
}
// Diagnostic admission only; it cannot change presentation or native state.
class VisibilityBoundaryBudget {
public:
    constexpr bool admit(uintptr_t owner,uint64_t activation,uint64_t menuGeneration,
                         bool idroid,uint64_t source) noexcept {
        if(!owner||!activation||!source||boundaries_>4)return false;
        if(owner_!=owner||activation_!=activation){
            owner_=owner;activation_=activation;source_=source;menu_=menuGeneration;
            idroid_=idroid;samples_=12;initialized_=true;
            return false;
        }
        if(!initialized_||source<=source_)return false;
        source_=source;
        if(idroid!=idroid_){
            const bool changed=menuGeneration>menu_;
            if(!changed)return false;
            idroid_=idroid;menu_=menuGeneration;
            ++boundaries_;samples_=0;
        }
        if(!boundaries_||boundaries_>4||samples_>=12)return false;
        ++samples_;return true;
    }
    constexpr uint32_t boundary() const noexcept{return boundaries_;}
    constexpr uint32_t sample() const noexcept{return samples_;}
    constexpr bool complete() const noexcept{return boundaries_>=4&&samples_>=12;}
private:
    uintptr_t owner_{};uint64_t activation_{},source_{},menu_{};
    uint32_t boundaries_{},samples_{};bool idroid_{},initialized_{};
};
class VisibilityInspectionBudget {
public:
    constexpr void arm(uint64_t now) noexcept {if(!armed_){started_=now;armed_=true;}}
    // Reserve before the native read, so failed ownership checks consume the
    // same finite budget as successful observations.
    constexpr bool reserve(uint64_t now) noexcept {
        if(!armed_||now<started_||now-started_>45000||attempts_>=2048)return false;
        ++attempts_;return true;
    }
private:
    uint64_t started_{};uint32_t attempts_{};bool armed_{};
};
struct HeadCameraSample;
// Opt-in bounded read-only evidence immediately before the source's two eye
// replays. This is a current native-record observation, not a GPU draw claim.
void observePlayerVisibilityForSource(const HeadCameraSample&,uint64_t sourceSequence) noexcept;
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
