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
    bool lensVisible) noexcept;

// Draw the hand-carried iDroid housing around the native UI screen. The
// center stays open so the live game pixels remain the device's front display.
bool drawPhysicalIdroid(ID3D11DeviceContext* context,
    const std::array<float,16>& world,
    const std::array<float,16>& view,
    const std::array<float,16>& projection) noexcept;

// Draw the live iDroid pointer at the exact front-display hit returned by the
// tracked aim ray. `world` is the hit pose in the same native FOX space as the
// housing, so the cursor cannot drift into a separate overlay space.
bool drawPhysicalIdroidCursor(ID3D11DeviceContext* context,
    const std::array<float,16>& world,
    const std::array<float,16>& view,
    const std::array<float,16>& projection) noexcept;

// Draw the owned title-opening props in the native cabin eye. The world array
// is ordered radio/deck, then the six action tapes in openingTapeLabels order.
// Each matrix must be the same native FOX world space used by the current eye
// replay. `selection` is a tape index, or -1 when no tape is hovered.
bool drawOpeningProps(ID3D11DeviceContext* context,
    const std::array<std::array<float,16>,7>& worlds,
    const std::array<float,16>& view,
    const std::array<float,16>& projection,int selection=-1) noexcept;

void stopPhysicalOpticRenderer() noexcept;

}
