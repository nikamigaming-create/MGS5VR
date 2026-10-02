#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include "stereo.hpp"
#include "render_layout.hpp"
namespace mgs5vr {
void installRenderCamera(uintptr_t moduleBase,const std::filesystem::path& evidenceDirectory,
                         RenderBuild build=RenderBuild::phantomPain_1_0_15_4);
void reportRenderCamera();
std::string renderCameraExposureDiagnostics();
// Bounded diagnostic A/B overrides; supported TPP isolation defaults on.
bool setRenderCameraExposureIsolation(bool enabled);
bool setRenderCameraLensRendering(bool renderLens);
EyeFrame observeRenderPresent(void* swapchain) noexcept;
void stopRenderCamera() noexcept;
}
