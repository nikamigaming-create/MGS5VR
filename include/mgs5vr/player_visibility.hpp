#pragma once
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include "core.hpp"

namespace mgs5vr {
inline constexpr uint64_t nativeSt02Model=0x84a35edc984905ceull;
inline constexpr uint32_t nativeSt02ReticleGroup=0x5136384du;
constexpr std::optional<uint32_t> nativeCustomWeaponAssembly(uint32_t equipment) noexcept {
    if(equipment<871||equipment>876)return {};
    return equipment<873?(equipment-871)*3:equipment<875?(equipment-873)*3+1:(equipment-875)*3+2;
}
// Only the reproduced transparent 1x replacement claims the native finite
// reticle. Magnified/unknown optics retain native visibility pending review.
constexpr bool replaceNativeSt02Reticle(uint64_t model,uint64_t scopeIdentity,
        uint32_t resource,bool currentPhysicalFrame,bool tracked,bool dotReticle,
        bool opaqueAtOnePower,float power) noexcept {
    const auto sight=static_cast<uint32_t>(scopeIdentity);
    return model==nativeSt02Model&&currentPhysicalFrame&&tracked&&dotReticle&&!opaqueAtOnePower
        &&power==1.f&&resource&&scopeIdentity>>32==resource&&(sight==2||sight==4||sight==5)
        &&nativeCustomWeaponAssembly((resource>>5)&0x7ffu).has_value();
}
// Observe native Show/Hide even while our replacement suppresses the group.
// Restoring an old "visible" snapshot after native stow would revive a dot
// the game deliberately hid. Preserve its latest desired bits instead.
struct NativeReticleVisibility {
    uint8_t desired{};
    constexpr void observe(uint8_t flags) noexcept {desired=flags;}
    constexpr bool restoreVisible(uint8_t current) const noexcept {
        return (desired&4u)&&current==static_cast<uint8_t>(desired&~4u);
    }
};
// Resource admission is additional to the typed live TppBuddyDog2 owner,
// publisher callsite and fresh cabin frame checks. It must never authorize
// another character merely because a mesh happens to share a skeleton size.
constexpr bool cabinDogCameraResource(uint64_t resource) noexcept {
    // Owned /Assets/tpp/chara/ddg/Scenes/ddg0_main[0-4]_def.fmdl.
    // The previous main4-only check made other native outfits use the stock
    // helicopter camera and disappear until iDroid bypassed that fade query.
    switch(resource){
    case 0x84a0d026d7938ff5ull: // main0
    case 0x84a1d561958eb109ull: // main1
    case 0x84a03f16dbbc41b6ull: // main2
    case 0x84a2627e55f4658dull: // main3
    case 0x84a0e671b02ebb56ull: // main4
        return true;
    default:return false;
    }
}
struct CabinDogCameraContext {
    uint64_t resource{};
    bool currentImmersiveFrame{},cabinPlay{},frontEnd{},authoredCamera{},scriptedDemo{},nativeGamepad{};
    bool nativeTitleCabin{},openingBackend{},openingSelector{},openingNativeMenu{},loading{},avatarEditor{};
};
// Called only after the native buddy publisher's exact callsite and typed
// current owner have been joined. This removes its third-person proximity
// fade during physical cabin interaction, not other native visibility policy.
constexpr bool suppressCabinDogCameraNearFade(const CabinDogCameraContext& context) noexcept {
    const bool interactiveTitle=context.nativeTitleCabin&&context.frontEnd&&context.openingBackend
        &&context.openingSelector&&!context.openingNativeMenu;
    const bool interactiveCabin=(context.cabinPlay&&!context.frontEnd)||interactiveTitle;
    return cabinDogCameraResource(context.resource)&&context.currentImmersiveFrame&&interactiveCabin
        &&!context.loading&&!context.avatarEditor&&!context.authoredCamera&&!context.scriptedDemo&&!context.nativeGamepad;
}
// Render-only forearm policy shares the native player visibility owner.
void setForearmPlayer(uintptr_t owner,uintptr_t model) noexcept;
void requestFullBodyVisibility(bool enabled) noexcept;
void publishFullBodyVisibility(uintptr_t owner,uintptr_t model,bool ready) noexcept;
// Keep the native body visible for scripted scenes, even while immersive
// presentation is active; ordinary first-person gameplay still hides it.
constexpr bool hidePlayerInFirstPerson(bool immersiveActive,bool scriptedDemo) noexcept {
    return immersiveActive&&!scriptedDemo;
}
// Native custom slots 873/874 are the two back-weapon variants (assemblies
// 1/4). First-person stow concealment must release on draw and manipulation,
// and must never claim hip/sidearm assemblies or the physical cabin weapons.
constexpr bool concealStowedBackAssembly(uint32_t assembly,uint32_t equipment,
                                         bool fieldFirstPerson,bool drawn) noexcept {
    if(!fieldFirstPerson||(assembly!=1&&assembly!=4))return false;
    const bool selected=(assembly==1&&equipment==873)||(assembly==4&&equipment==874);
    return !(selected&&drawn);
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
void updatePlayerVisibility(uintptr_t cameraOwner,bool firstPerson,bool hideArms=false,bool authoredBody=false) noexcept;
struct AuthoredHeadVisibility {
    uintptr_t owner{},model{};
    Pose head{},viewer{};
    uint64_t time{},activation{};
    bool concealed{};
};
void publishAuthoredPlayerHead(uintptr_t owner,uintptr_t model,Pose worldHead,uint64_t activation) noexcept;
void updateAuthoredPlayerVisibility() noexcept;
AuthoredHeadVisibility authoredHeadVisibility() noexcept;
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
// The launched projectile keeps native ownership. Only the source-matched
// tracked hand's verified modular group is exposed during a headset replay.
class RocketHandPresentation {
public:
    explicit RocketHandPresentation(const HeadCameraSample&,bool headPass) noexcept;
    ~RocketHandPresentation();
    RocketHandPresentation(const RocketHandPresentation&)=delete;
    RocketHandPresentation& operator=(const RocketHandPresentation&)=delete;
private:
    uintptr_t owner_{},character_{},parts_{},record_{},renderer_{},model_{};
};
}
