#pragma once
#include <dxgi.h>
#include <filesystem>
namespace mgs5vr {
// Independent native render canvas and PC preview; never changes a monitor mode.
void installRenderSizeHooks(IDXGISwapChain* probe,const std::filesystem::path& configuration={});
void observeRenderWindow(IDXGISwapChain* swap);
}
