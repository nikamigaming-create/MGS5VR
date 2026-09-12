#pragma once
#include "head_camera.hpp"

namespace mgs5vr {
struct OpticWaypoint {Vec3 position{};uint8_t letter{};};
struct OpticWaypoints {
    std::array<OpticWaypoint,128> points{}; // Letter zero denotes a native marked person.
    size_t count{};
    uint64_t sampleTime{},activation{};
};
void installOpticMarkers(uintptr_t moduleBase);
void publishOpticMarkerFrame(const HeadCameraSample& frame);
OpticWaypoints opticWaypoints();
void stopOpticMarkers() noexcept;
}
