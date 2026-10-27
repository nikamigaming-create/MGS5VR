#pragma once
#include <array>
#include <cstddef>
#include <memory>
#include <mutex>
#include <utility>

namespace mgs5vr {
// Reuse only completed CPU copies. The encoder copies into its own Media
// Foundation buffer before returning one here; no GPU or encoder-owned memory
// is shared with the render thread. A take retains at most four idle buffers.
class NativeVideoBuffers {
    struct Entry {std::unique_ptr<unsigned char[]> bytes;size_t length{};};
    std::mutex mutex_;
    std::array<Entry,4> idle_{};
public:
    std::unique_ptr<unsigned char[]> acquire(size_t length){
        {
            std::lock_guard lock(mutex_);
            for(auto& entry:idle_)if(entry.bytes&&entry.length==length){
                entry.length=0;return std::move(entry.bytes);
            }
        }
        return std::make_unique_for_overwrite<unsigned char[]>(length);
    }
    void recycle(std::unique_ptr<unsigned char[]> bytes,size_t length){
        if(!bytes)return;
        std::lock_guard lock(mutex_);
        for(auto& entry:idle_)if(!entry.bytes){entry.bytes=std::move(bytes);entry.length=length;return;}
    }
    void clear(){
        std::lock_guard lock(mutex_);
        for(auto& entry:idle_)entry={};
    }
};
}
