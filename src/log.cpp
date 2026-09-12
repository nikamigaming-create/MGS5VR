#include "mgs5vr/log.hpp"
#include <windows.h>
#include <fstream>
#include <mutex>
#include <chrono>
namespace mgs5vr {
static std::mutex mutex;
static std::filesystem::path logPath;
static uintmax_t logBytes{};
constexpr uintmax_t maximumLogBytes=16ull*1024*1024;
void setLogPath(const std::filesystem::path& p) {
    std::lock_guard guard(mutex);logPath=p;
    std::error_code error;logBytes=std::filesystem::file_size(p,error);
    if(error)logBytes=0;
}
void log(std::string_view message) noexcept {
    try {
        std::lock_guard guard(mutex);
        const auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        const auto line=std::to_string(ms)+" "+std::string(message)+"\n";
        OutputDebugStringA(line.c_str());
        if(!logPath.empty()) {
            if(logBytes+line.size()>maximumLogBytes){
                auto previous=logPath;previous+=L".1";
                std::error_code error;std::filesystem::remove(previous,error);
                if(error)return;
                std::filesystem::rename(logPath,previous,error);
                if(error)return;
                logBytes=0;
            }
            std::ofstream out(logPath,std::ios::app);out<<line;
            if(out)logBytes+=line.size();
        }
    } catch(...) {}
}
}
