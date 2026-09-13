#pragma once
#include <filesystem>
#include "render_layout.hpp"
namespace mgs5vr {
// Diagnostic only. Does not change any camera/weapon/HUD value.
// Caller must first validate the supported executable's SHA256.
void installCameraObserver(const std::filesystem::path& evidenceDirectory = {},
    RenderBuild build=RenderBuild::phantomPain_1_0_15_4);
void stopCameraObserver() noexcept;
}
