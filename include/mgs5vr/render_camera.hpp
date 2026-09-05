#pragma once
#include <cstdint>
#include <filesystem>
#include "stereo.hpp"
namespace mgs5vr {
void installRenderCamera(uintptr_t moduleBase,const std::filesystem::path& evidenceDirectory);
void reportRenderCamera();
EyeFrame observeRenderPresent(void* swapchain) noexcept;
void stopRenderCamera() noexcept;
}
