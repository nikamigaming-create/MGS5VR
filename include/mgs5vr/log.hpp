#pragma once
#include <filesystem>
#include <string_view>
namespace mgs5vr {
void setLogPath(const std::filesystem::path& path);
void log(std::string_view message) noexcept;
}
