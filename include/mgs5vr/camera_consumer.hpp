#pragma once
#include <cstdint>
#include <filesystem>

namespace mgs5vr {
// Diagnostic only: preserve native getter results and record who consumes them.
// The caller verifies the executable hash before passing its loaded module base.
void installCameraConsumerObserver(uintptr_t moduleBase,const std::filesystem::path& evidenceDirectory);
void reportCameraConsumers();
void stopCameraConsumerObserver() noexcept;
}
