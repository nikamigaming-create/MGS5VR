#pragma once
#include <array>
#include <cstdint>
#include <vector>
namespace mgs5vr {
using EquipmentLabels=std::array<std::array<char,96>,4>;
struct WristSelectorImage {uint32_t width{},height{};std::vector<uint32_t> bgra;};
// An authored action guide, not synthetic inventory: actual item cards still
// come from the native game after the user chooses a category.
WristSelectorImage makeWristSelectorImage(const EquipmentLabels& labels);
}
