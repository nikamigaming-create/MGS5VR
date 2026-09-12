#pragma once
#include "mailbox.hpp"

namespace mgs5vr {
void installSceneCapture(ID3D11Device* probeDevice);
void observeSceneSwapchain(IDXGISwapChain* swap);
bool sceneCaptureAvailable();
// Return the current native eye backbuffer when it belongs to the same D3D11
// device as the rendering context. The caller owns the returned reference and
// may copy it into a private shader-resource texture before adding a pass.
bool sceneSourceTexture(ID3D11DeviceContext* context,ID3D11Texture2D** source) noexcept;
void beginSceneTiming(ID3D11DeviceContext* context,uint64_t sourceSequence) noexcept;
void endSceneTiming(ID3D11DeviceContext* context,uint64_t sourceSequence,bool complete) noexcept;
// Append the copy to the game's own context, including deferred contexts.
// The two calls belong to one invocation of the native scene-render job.
// Capture the current native eye into the simulator mailbox. When `output` is
// supplied, it also returns an AddRef'd shader-readable copy of that same
// native scene. The copy is recorded on the caller's context, so later optic
// draws in that context see the scene pixels before the physical housing pass.
bool captureSceneEye(ID3D11DeviceContext* context,const EyeFrame& eye,ID3D11Texture2D** output=nullptr);
void cancelSceneEyes(uint64_t sourceSequence);
bool publishSceneEyes(IDXGISwapChain* swap,ID3D11DeviceContext* immediate,TextureMailbox& destination);
void invalidateSceneCapture();
std::array<uint64_t,8> sceneCaptureCounters();
}
