#pragma once
#include "head_camera.hpp"
#include "cover_probe.hpp"

namespace mgs5vr {
struct OpticWaypoint {Vec3 position{};uint8_t letter{};};
struct OpticWaypoints {
    std::array<OpticWaypoint,128> points{}; // Letter zero denotes a native marked person.
    size_t count{};
    uint64_t sampleTime{},activation{};
};
// Opt-in bot observation, sampled on the native marker-update job. It does
// not acquire/mark actors, change their state, or submit player input.
struct CombatSightTarget {
    uint16_t id{0xffff};
    Vec3 marker{},aim{};
    float distance{},surfaceDistance{};
    bool queryValid{},unoccluded{};
    uint8_t aimKind{}; // 0 torso policy, 1 exact native person-marker point.
    uint32_t rayResult{},hitCount{};
    int32_t hitIndex{-1};
    Vec3 rawSurface{};
    float rawAlong{},rawLateralSquared{};
    uint8_t queryReason{}; // 1 owner, 2 endpoint, 3 miss, 4 pool, 5 point,
                           // 6 behind, 7 beyond segment, 8 off ray, 9 valid hit.
};
struct CombatSightSnapshot {
    std::array<CombatSightTarget,16> targets{};
    size_t count{},candidates{};
    uint64_t sampleTime{},activation{},presentationEpoch{},playerOwner{},trackingSequence{};
    Pose muzzle{};
    bool muzzleTracked{};
};
bool leaseCombatSight(uint64_t now,uint64_t milliseconds) noexcept;
CombatSightSnapshot combatSightSnapshot();
uint64_t combatSightLeaseExpiry() noexcept;
bool leaseCoverProbe(CoverProbeRequest request,uint64_t now) noexcept;
void clearCoverProbe() noexcept;
CoverProbeSnapshot coverProbeSnapshot(uint64_t now,uint64_t nonce);
void installOpticMarkers(uintptr_t moduleBase);
void publishOpticMarkerFrame(const HeadCameraSample& frame);
OpticWaypoints opticWaypoints();
void stopOpticMarkers() noexcept;
}
