#pragma once
#include "head_camera.hpp"
namespace mgs5vr {
void installGzRecon(uintptr_t base);
void publishGzReconFrame(const HeadCameraSample& frame);
void stopGzRecon() noexcept;
}
