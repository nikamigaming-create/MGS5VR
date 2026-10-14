#pragma once
#include "cabin_studio.hpp"
#include <array>
#include <cstdint>
#include <memory>

namespace mgs5vr {
struct NativeTextKey { uint16_t character{}; };
class NativeTextKeyboard {
public:
    std::shared_ptr<const CabinStudioFrame> update(bool allowed,Pose head,
        const std::array<StudioHand,2>& hands,uint64_t generation,uint64_t referenceEpoch=1,uint64_t activation=1);
    NativeTextKey takeKey() noexcept;
private:
    CabinStudioFrame frame_{};
    std::array<bool,2> pressed_{};
    uint64_t generation_{},referenceEpoch_{},activation_{};
    bool ready_{};
    NativeTextKey key_{};
};
bool nativeTextKeyEnabled(size_t key) noexcept;
uint16_t nativeTextKeyCharacter(size_t key) noexcept;
void installNativeTextInput(uintptr_t imageBase) noexcept;
void stopNativeTextInput() noexcept;
void publishNativeProfileMenu(bool active) noexcept;
bool nativeProfileMenuActive() noexcept;
bool nativeTextInputReady() noexcept;
void publishNativeTextPanel(bool visible,uint64_t generation=0,uint64_t referenceEpoch=0,uint64_t activation=0) noexcept;
bool queueNativeTextKey(uint16_t character,uint64_t generation,uint64_t referenceEpoch,uint64_t activation) noexcept;
void cancelNativeTextKeys() noexcept;
void pumpNativeTextInput() noexcept;
struct NativeTextInputStatus {uint64_t down{},up{},converted{},discarded{};uint32_t pending{};bool ready{},active{},visible{};};
NativeTextInputStatus nativeTextInputStatus() noexcept;
}
