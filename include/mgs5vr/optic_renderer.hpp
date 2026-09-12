#pragma once

#include <array>
#include "core.hpp"

struct ID3D11DeviceContext;
struct ID3D11Texture2D;

namespace mgs5vr {

// Copy the independent device-camera scene before the normal HMD eyes are
// rendered. This texture never enters the stereo mailbox.
bool capturePhysicalOpticScene(ID3D11DeviceContext* context,
    const std::array<float,16>& view,const std::array<float,16>& projection,
    Vec3 cameraPosition,ID3D11Texture2D** output) noexcept;

// Draw the physical binocular housing into the current native eye scene.
// `world` is the retail FOX world matrix for the tracked optic body, `view`
// and `projection` are the exact matrices used for this eye's scene draw.
// The function is fail-closed: an unavailable device, shader, or render
// target leaves the native scene untouched.
bool drawPhysicalBinoculars(ID3D11DeviceContext* context,
    const std::array<float,16>& world,
    const std::array<float,16>& view,
    const std::array<float,16>& projection,
    float magnification,
    ID3D11Texture2D* sceneSource,
    bool leftEye) noexcept;

void stopPhysicalOpticRenderer() noexcept;

}
