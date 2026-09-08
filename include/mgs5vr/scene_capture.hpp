#pragma once
#include "mailbox.hpp"

namespace mgs5vr {
void installSceneCapture(ID3D11Device* probeDevice);
void observeSceneSwapchain(IDXGISwapChain* swap);
bool sceneCaptureAvailable();
void beginSceneTiming(ID3D11DeviceContext* context,uint64_t sourceSequence) noexcept;
void endSceneTiming(ID3D11DeviceContext* context,uint64_t sourceSequence,bool complete) noexcept;
// Append the copy to the game's own context, including deferred contexts.
// The two calls belong to one invocation of the native scene-render job.
bool captureSceneEye(ID3D11DeviceContext* context,const EyeFrame& eye);
void cancelSceneEyes(uint64_t sourceSequence);
bool publishSceneEyes(IDXGISwapChain* swap,ID3D11DeviceContext* immediate,TextureMailbox& destination);
void invalidateSceneCapture();
std::array<uint64_t,8> sceneCaptureCounters();
}
