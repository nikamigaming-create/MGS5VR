#pragma once
#include <filesystem>
namespace mgs5vr {
// Diagnostic only. Does not change any camera/weapon/HUD value.
// Caller must first validate the supported executable's SHA256.
void installCameraObserver(const std::filesystem::path& evidenceDirectory = {});
void stopCameraObserver() noexcept;
}
