#pragma once

#include <array>
#include "core.hpp"

struct ID3D11DeviceContext;
struct ID3D11Texture2D;

namespace mgs5vr {
struct OpticWaypoints;

// Copy the independent device-camera scene before the normal HMD eyes are
// rendered. This texture never enters the stereo mailbox.
bool capturePhysicalOpticScene(ID3D11DeviceContext* context,
    const std::array<float,16>& view,const std::array<float,16>& projection,
    Vec3 cameraPosition,ID3D11Texture2D** output,bool waypoints=true,
    const OpticWaypoints* markers=nullptr) noexcept;

// Reproject acquired native people/waypoints from their world positions using
// this exact eye. Both eyes receive one immutable marker snapshot.
void drawWorldWaypoints(ID3D11DeviceContext* context,
    const std::array<float,16>& view,const std::array<float,16>& projection,
    Vec3 cameraPosition,const OpticWaypoints& markers) noexcept;

// Only the aperture is drawn: the equipped game's scope already supplies its
// housing and lighting. No binocular asset is required by this path.
bool drawPhysicalWeaponScope(ID3D11DeviceContext* context,
    const std::array<float,16>& ocularWorld,const std::array<float,16>& view,
    const std::array<float,16>& projection,float radius,float magnification,
    ID3D11Texture2D* sceneSource) noexcept;

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
