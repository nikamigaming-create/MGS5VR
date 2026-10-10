#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include <string_view>
namespace mgs5vr {
using EquipmentLabels=std::array<std::array<char,96>,4>;
struct WristSelectorImage {uint32_t width{},height{};std::vector<uint32_t> bgra;};
// An authored action guide, not synthetic inventory: actual item cards still
// come from the native game after the user chooses a category.
WristSelectorImage makeWristSelectorImage(const EquipmentLabels& labels);
// The effective opening gesture, followed by the native route to deployment.
// Cached by the stereo renderer; this is an ACC help label, not a menu replica.
WristSelectorImage makeAccWristHintImage(std::string_view idroidBinding);
}
