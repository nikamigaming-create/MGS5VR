#pragma once
#include "mailbox.hpp"
#include <array>
#include <optional>

namespace mgs5vr {
// Timestamp markers can span deferred command lists. The disjoint clock query
// surrounds their playback on the immediate context. Readback never waits.
class GpuTiming {
public:
    bool begin(ID3D11DeviceContext* context) noexcept;
    void end(ID3D11DeviceContext* context) noexcept;
    void beginExecution(ID3D11DeviceContext* context) noexcept;
    void endExecution(ID3D11DeviceContext* context) noexcept;
    std::optional<double> poll(ID3D11DeviceContext* context) noexcept;
    bool pending() const noexcept;
private:
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11Query> clock_,start_,finish_;
    bool pending_{},started_{},ended_{};
    bool sameDevice(ID3D11DeviceContext* context) const noexcept;
};
}
