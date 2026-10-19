#pragma once
#include "head_camera.hpp"
#include "cover_probe.hpp"

namespace mgs5vr {
struct OpticWaypoint {Vec3 position{};uint8_t letter{};uint16_t actor{0xffff};};
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
    uint8_t aimKind{}; // 0 torso policy, 1 person marker, 2 explicitly leased marker.
    uint32_t rayResult{},hitCount{};
    int32_t hitIndex{-1};
    Vec3 rawSurface{};
    float rawAlong{},rawLateralSquared{};
    // Explicit-marker diagnostics only; never used to admit a ray or a shot.
    std::array<uint64_t,12> hitRecordWords{};
    std::array<float,16> hitTransform{};
    bool hitTransformRead{};
    uint8_t queryReason{}; // 1 owner, 2 endpoint, 3 miss, 4 pool, 5 point,
                           // 6 behind, 7 beyond segment, 8 off ray, 9 valid hit.
};
// Explicit diagnostic target identity. This never changes native marking or AI.
struct CombatMarkerRecord {
    uint16_t id{0xffff};
    Vec3 position{};
    uint8_t kind{},flags{};
    bool found{};
};
struct CombatSightSnapshot {
    std::array<CombatSightTarget,16> targets{};
    size_t count{},candidates{};
    uint64_t sampleTime{},activation{},presentationEpoch{},playerOwner{},trackingSequence{};
    Pose muzzle{};
    bool muzzleTracked{};
    CombatMarkerRecord requestedMarker{};
};
bool leaseCombatSight(uint64_t now,uint64_t milliseconds,uint16_t marker=0xffff) noexcept;
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
