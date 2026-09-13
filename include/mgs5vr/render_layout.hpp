#pragma once
#include <cstddef>
#include <cstdint>

namespace mgs5vr {
enum class RenderBuild { phantomPain_1_0_15_4, groundZeroes_1_0_0_5 };

// Contracts from the exact supported executables. The render algorithm is
// shared; native object layouts and call-site ownership are never inferred
// from the other game's offsets. Zero marks an adapter not yet connected.
struct RenderLayout {
    uintptr_t world,view,extents,viewport,projection,scene,registerTarget,listener,virtualListener,publisher;
    uintptr_t worldReturn,viewReturn,inverseWorldReturn,viewportReturn;
    uintptr_t clipReturn,gpuReturn,registerReturn,listenerReturn,virtualListenerReturn,uiProjectionReturn;
    size_t cameraPose,viewInputToCamera,inversePose,alternateListenerPose;
    size_t viewportCamera,viewportMatrices,gpuProjection,clipProjection,previousView,previousProjection;
    size_t viewportWidth,viewportHeight,viewportScale;
    size_t renderViewports,viewportNext,renderTarget,graphicsContext;
    size_t presentCount,presentCapacity,presentData;
};
inline constexpr RenderLayout phantomPainRender{
    0x438ac0,0x438c20,0x1c4fa0,0x1b9490,0x241b00,0x1beec0,0x2496a0,0x1d6a490,0x1d6a550,0,
    0x437c64,0x437c90,0x438c66,0x4380b9,
    0x1b9691,0x1b9724,0x1bef27,0x438132,0x43814e,0x2e68a0,
    0xf0,0x30,0x120,0x130,
    0x570,0x280,0x280,0x300,0x3c0,0x400,
    0x5d8,0x5dc,0x5e0,
    0xa0,0x30,0x98,0x150,
    0x110,0x114,0x118
};
inline constexpr RenderLayout groundZeroesRender{
    0x31ee30,0x39d2d0,0xf8edb0,0xf42270,0xf5f110,0xf47b50,0xfd3310,0x120edb0,0x120ee70,0x39c510,
    0x39c605,0x39c631,0x39d316,0x39c875,
    0xf42528,0xf425bb,0xf47bb2,0x39c8d3,0x39c8f0,0,
    0xc0,0,0xc0,0xe0,
    0x4b0,0x1c0,0x1c0,0x240,0x300,0x340,
    0x514,0x518,0x51c,
    0xa8,0x28,0xa0,0x120,
    0xe0,0xe4,0xe8
};
constexpr const RenderLayout& renderLayout(RenderBuild build) noexcept {
    return build==RenderBuild::groundZeroes_1_0_0_5?groundZeroesRender:phantomPainRender;
}
}
