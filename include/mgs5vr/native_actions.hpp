#pragma once
#include <cstdint>
#include <filesystem>
namespace mgs5vr {
// Local development requests run at the end of a native Lua transaction.
void installNativeActions(uintptr_t imageBase,const std::filesystem::path& folder);
void stopNativeActions() noexcept;
}
