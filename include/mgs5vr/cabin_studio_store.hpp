#pragma once
#include "cabin_studio.hpp"
#include <cstdint>
struct ID3D11DeviceContext;
struct ID3D11Texture2D;
namespace mgs5vr {
struct StudioPhotoPixels { uint32_t width{},height{};std::vector<uint32_t> bgra; };
StudioCollection loadCabinStudio();
void saveCabinStudio(std::shared_ptr<const CabinStudioFrame> frame) noexcept;
uint64_t completedStudioPhoto() noexcept;
uint64_t acceptedStudioShutter() noexcept;
// Copy only the independent, camera-posed native scene. Completion is polled
// after native command submission at Present; PNG/state I/O runs off rendering.
bool captureStudioPhoto(ID3D11DeviceContext*,ID3D11Texture2D*,uint64_t shutter) noexcept;
void pollStudioPhoto(ID3D11DeviceContext*) noexcept;
std::shared_ptr<const StudioPhotoPixels> studioPhotoPixels(uint64_t photo);
void stopCabinStudioStore() noexcept;
}
