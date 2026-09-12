#pragma once
#include "mailbox.hpp"
#include <memory>

namespace mgs5vr {
// Optional private capture of the actual native eye texture. A UTF-8 absolute
// .mp4 path in mgs5vr-recording.txt starts a take; removing it finishes the take.
// GPU readback and encoding are bounded and never wait on the render thread.
// Takes finalize automatically after two minutes or below 25 GiB free space.
class NativeVideoRecorder {
public:
    NativeVideoRecorder();
    ~NativeVideoRecorder();
    void frame(ID3D11Device* device,ID3D11DeviceContext* context,
               ID3D11Texture2D* source,uint32_t slice) noexcept;
private:
    struct State;
    std::unique_ptr<State> state_;
};
}
