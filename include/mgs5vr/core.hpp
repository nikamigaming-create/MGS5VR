#pragma once
#include <array>
#include <cstdint>
#include <optional>

namespace mgs5vr {
// All core poses: right handed, meters, +X right, +Y up, -Z forward.
struct Vec3 { float x{}, y{}, z{}; };
struct Quat { float x{}, y{}, z{}, w{1}; };
struct Pose { Quat orientation{}; Vec3 position{}; };
Vec3 operator+(Vec3 a, Vec3 b);
Vec3 operator-(Vec3 a, Vec3 b);
Vec3 operator*(Vec3 a, float b);
float dot(Vec3 a, Vec3 b);
Vec3 rotate(Quat q, Vec3 v);
Pose compose(Pose parentFromChild, Pose childFromLocal);
Pose inverse(Pose pose);
bool valid(Pose pose);
Pose recenteredScreen(Pose head, float distance);

struct FrameId {
    uint64_t epoch{}, sequence{};
    bool operator==(const FrameId&) const = default;
};
struct PoseFrame { FrameId id; Pose head, leftGrip, rightGrip; int64_t predictedXrTime{}; };
class PoseHistory {
public:
    void reset(uint64_t epoch);
    bool put(const PoseFrame& frame);
    std::optional<PoseFrame> find(FrameId id) const;
private:
    uint64_t epoch_{};
    uint64_t newest_{};
    std::array<std::optional<PoseFrame>, 64> entries_{};
};

struct Panel { Pose pose; float widthMeters{}, heightMeters{}; uint32_t pixelWidth{}, pixelHeight{}; };
struct Hit { float u{}, v{}, distance{}; uint32_t x{}, y{}; };
std::optional<Hit> intersectPanel(const Panel& panel, Pose aim, float maxDistance);

enum class WristState { hidden, candidate, visible, cooldown };
class WristFocus {
public:
    WristState update(double monotonicSeconds, bool tracking, bool contentReady,
                      float facingCosine, float distanceMeters);
    WristState state() const { return state_; }
private:
    WristState state_{WristState::hidden};
    double since_{}, lastTime_{-1};
};

enum class Scene { unknown, gameplay, cinematic, loading, menu };
// Semantic skip is gated by the engine's readiness, and emitted once per press.
class SkipGate {
public:
    bool update(Scene scene, uint64_t sceneGeneration, bool canSkip, bool focused, bool pressed);
private:
    bool previous_{};
    uint64_t generation_{};
};

struct WeaponSockets { Pose weaponFromGrip; Pose weaponFromMuzzle; bool calibrated{}; };
struct WeaponPose { Pose referenceFromWeapon, referenceFromMuzzle; };
std::optional<WeaponPose> solveWeapon(Pose referenceFromGrip, const WeaponSockets& sockets,
                                    bool tracked);
}
