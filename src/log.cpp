#include "mgs5vr/log.hpp"
#include <windows.h>
#include <fstream>
#include <mutex>
#include <chrono>
namespace mgs5vr {
static std::mutex mutex;
static std::filesystem::path logPath;
void setLogPath(const std::filesystem::path& p) { std::lock_guard guard(mutex); logPath=p; }
void log(std::string_view message) noexcept {
    try {
        std::lock_guard guard(mutex);
        const auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        const auto line=std::to_string(ms)+" "+std::string(message)+"\n";
        OutputDebugStringA(line.c_str());
        if(!logPath.empty()) { std::ofstream out(logPath,std::ios::app); out<<line; }
    } catch(...) {}
}
}
