#pragma once
#include <windows.h>
#include <d3d11_4.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <memory>
#include <mutex>
#include <atomic>
#include <array>
#include "core.hpp"
#include "stereo.hpp"

namespace mgs5vr {
using Microsoft::WRL::ComPtr;
void checkHr(HRESULT result, const char* operation);
enum class TextureSlotState { free, producerOwned, producerPending, ready, consumerOwned, consumerPending };
enum class TextureCompletionPreference { preferFence, eventOnly };
struct TextureCompletion {
    ComPtr<ID3D11DeviceContext4> context;
    ComPtr<ID3D11Fence> fence;
    uint64_t nextValue{};
};
inline constexpr size_t textureMailboxSlots=3;
struct TextureSlot {
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<IDXGIKeyedMutex> mutex;
    ComPtr<ID3D11Query> producerComplete;
    uint64_t producerMarker{};
    HANDLE sharedHandle{}; // Legacy DXGI handle: owned by texture, never CloseHandle.
    std::atomic<TextureSlotState> state{TextureSlotState::free};
    // Written only while producerOwned; immutable through both completion gates.
    FrameId published{};
    std::array<EyeFrame,2> eyes{};
};
struct TextureChannel {
    ComPtr<ID3D11Device> producerDevice;
    TextureCompletion producerCompletion;
    std::array<TextureSlot,textureMailboxSlots> slots;
    D3D11_TEXTURE2D_DESC desc{};
    LUID adapterLuid{};
    uint64_t epoch{};
    // Reset/failure retires the entire epoch; pending slots are never guessed free.
    std::atomic_bool retired{false};
};
class TextureMailbox {
public:
    explicit TextureMailbox(TextureCompletionPreference preference=TextureCompletionPreference::preferFence):preference_(preference){}
    // Called only on the game's immediate-context/render thread. Never waits for XR.
    bool publish(ID3D11Texture2D* source, ID3D11DeviceContext* context,EyeFrame eye={});
    bool publishStereo(const std::array<ID3D11Texture2D*,2>& sources,ID3D11DeviceContext* context,const std::array<EyeFrame,2>& eyes);
    // Poll once per pending slot on the producer's immediate-context thread only.
    // publish() also polls. This permits draining a final publication without a new image.
    void poll(ID3D11DeviceContext* context);
    std::shared_ptr<TextureChannel> latest() const;
    void invalidate();
private:
    bool publishImages(const std::array<ID3D11Texture2D*,2>& sources,uint32_t count,ID3D11DeviceContext* context,const std::array<EyeFrame,2>& eyes);
    void pollLocked(const std::shared_ptr<TextureChannel>& channel,ID3D11DeviceContext* context);
    mutable std::mutex pointerMutex_;
    std::mutex producerMutex_;
    std::shared_ptr<TextureChannel> channel_;
    uint64_t epoch_{}, sequence_{};
    TextureCompletionPreference preference_;
};
class TextureConsumer {
public:
    explicit TextureConsumer(ID3D11Device* device,TextureCompletionPreference preference=TextureCompletionPreference::preferFence);
    ~TextureConsumer();
    TextureConsumer(const TextureConsumer&)=delete;
    TextureConsumer& operator=(const TextureConsumer&)=delete;
    // Open/copy on a separate D3D11 device; the game's context is never used here.
    bool consume(const std::shared_ptr<TextureChannel>& channel);
    ID3D11Texture2D* texture() const { return cached_.Get(); }
    FrameId frame() const { return frame_; }
    EyeFrame eye() const { return eyes_[0]; }
    std::array<EyeFrame,2> eyes() const { return eyes_; }
    bool usingCompletionFence() const { return static_cast<bool>(completion_.fence); }
    void reset();
private:
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    std::shared_ptr<TextureChannel> channel_;
    struct Slot {
        ComPtr<ID3D11Texture2D> shared;
        ComPtr<IDXGIKeyedMutex> mutex;
        ComPtr<ID3D11Query> complete;
        uint64_t marker{};
    };
    std::array<Slot,textureMailboxSlots> slots_;
    TextureCompletion completion_;
    TextureCompletionPreference preference_;
    ComPtr<ID3D11Texture2D> cached_;
    FrameId frame_{};
    std::array<EyeFrame,2> eyes_{};
};
}
