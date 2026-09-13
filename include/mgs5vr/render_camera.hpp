#pragma once
#include <cstdint>
#include <filesystem>
#include "stereo.hpp"
#include "render_layout.hpp"
namespace mgs5vr {
void installRenderCamera(uintptr_t moduleBase,const std::filesystem::path& evidenceDirectory,
                         RenderBuild build=RenderBuild::phantomPain_1_0_15_4);
void reportRenderCamera();
EyeFrame observeRenderPresent(void* swapchain) noexcept;
void stopRenderCamera() noexcept;
}
