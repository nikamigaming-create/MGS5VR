#pragma once
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <memory>
#include <mutex>
#include "core.hpp"
#include "stereo.hpp"

namespace mgs5vr {
using Microsoft::WRL::ComPtr;
void checkHr(HRESULT result, const char* operation);
struct TextureChannel {
    ComPtr<ID3D11Device> producerDevice;
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<IDXGIKeyedMutex> mutex;
    HANDLE sharedHandle{}; // Legacy DXGI handle: owned by texture, never CloseHandle.
    D3D11_TEXTURE2D_DESC desc{};
    LUID adapterLuid{};
    uint64_t epoch{};
    FrameId published{}; // Access only while holding the DXGI keyed mutex.
    std::array<EyeFrame,2> eyes{};
};
class TextureMailbox {
public:
    // Called only on the game's immediate-context/render thread. Never waits for XR.
    bool publish(ID3D11Texture2D* source, ID3D11DeviceContext* context,EyeFrame eye={});
    bool publishStereo(const std::array<ID3D11Texture2D*,2>& sources,ID3D11DeviceContext* context,const std::array<EyeFrame,2>& eyes);
    std::shared_ptr<TextureChannel> latest() const;
    void invalidate();
private:
    bool publishImages(const std::array<ID3D11Texture2D*,2>& sources,uint32_t count,ID3D11DeviceContext* context,const std::array<EyeFrame,2>& eyes);
    mutable std::mutex pointerMutex_;
    std::mutex producerMutex_;
    std::shared_ptr<TextureChannel> channel_;
    uint64_t epoch_{}, sequence_{};
};
class TextureConsumer {
public:
    explicit TextureConsumer(ID3D11Device* device);
    // Open/copy on a separate D3D11 device; the game's context is never used here.
    bool consume(const std::shared_ptr<TextureChannel>& channel);
    ID3D11Texture2D* texture() const { return cached_.Get(); }
    FrameId frame() const { return frame_; }
    EyeFrame eye() const { return eyes_[0]; }
    std::array<EyeFrame,2> eyes() const { return eyes_; }
    void reset();
private:
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    std::shared_ptr<TextureChannel> channel_;
    ComPtr<ID3D11Texture2D> shared_, cached_;
    ComPtr<IDXGIKeyedMutex> mutex_;
    FrameId frame_{};
    std::array<EyeFrame,2> eyes_{};
};
}
