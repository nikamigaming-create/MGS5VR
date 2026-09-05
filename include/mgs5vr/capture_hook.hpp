#pragma once
#include "mailbox.hpp"
namespace mgs5vr {
// Process lifetime hook; call once, outside DllMain. Caller pins its module.
void installCaptureHook(TextureMailbox& mailbox);
void stopCapture() noexcept;
}
