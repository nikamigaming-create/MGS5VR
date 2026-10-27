#pragma once
#include "core.hpp"
#include <array>

namespace mgs5vr {
// Cabin-local, metre-sized rigid objects. Identity is the cassette index, never
// a rack location. The immutable snapshot travels with the camera publication.
enum class CabinTapeState : uint8_t { resting, falling, held, guiding, seated, ejecting, presented };
struct CabinTape {
    Pose pose{};
    Vec3 velocity{},angularVelocity{};
    CabinTapeState state{CabinTapeState::resting};
    int hand{-1};
};
struct CabinHand {
    Pose pose{}; bool tracked{}; float grip{},trigger{};
    Vec3 indexTip{}; bool indexTipTracked{};
};
struct CabinLayout {
    Pose room{},seat{},player{},slot{},board{};
    Vec3 seatHalf{.250f,.035f,.180f},eject{};
    float floor{-1.1f};
    uint64_t generation{};
    bool verified{};
};
// Derived from the owned uth0_intr0 seat and board, with the native helicopter
// transform at (0,1200,0). The camera anchor is sampled by the game thread.
CabinLayout nativeCabinLayout(Pose worldOrigin,uint64_t generation) noexcept;
CabinLayout cabinDefaultLayout() noexcept;
struct CabinBenchFrame {
    std::array<CabinTape,6> tapes{};
    Pose player{};
    int hovered{-1},inserted{-1},activated{-1};
    bool ejectPressed{},blocked{};
    uint8_t hapticMask{};
    float lidOpen{1};
    CabinLayout layout{};
};
struct CabinBox { Vec3 center{},half{}; };
inline constexpr Vec3 cabinTapeHalf{.05f,.031875f,.005414f};
inline constexpr float cabinTapeMass=.045f;
inline constexpr Pose cabinNativeSeat{{},{0,-2.375f,-1.0675f}};
// Plane fitted to 301 owned uth0_pho00 left-wall vertices; 19 mm inward
// clears their irregular photo layers so new paper rests on their front.
inline constexpr Pose cabinNativeBoard{{.0810f,.70245f,-.0810f,.70245f},{-.9165f,-1.6743f,-1.145f}};
inline constexpr Vec3 cabinBoardHalf{.205f,.235f,.01f};
float cabinNativeSupportHeight(Vec3 roomPoint) noexcept;
// Compatibility helpers describe the measured native seat; no new tray is drawn.
CabinBox cabinBench() noexcept;
Pose cabinPlayerPose() noexcept;
Pose cabinSlotPose() noexcept;
Pose cabinCarrierPose(Pose player,float lidOpen) noexcept;
Vec3 cabinEjectButton() noexcept;
std::array<Pose,6> cabinTapeHomes() noexcept;

class CabinProps {
public:
    CabinBenchFrame update(const std::array<CabinHand,2>& hands,uint64_t now,
        CabinLayout layout=cabinDefaultLayout());
    void reset() noexcept;
private:
    void step(float dt);
    CabinBenchFrame frame_{};
    std::array<bool,2> grabbed_{},triggered_{};
    std::array<bool,2> buttonContact_{};
    std::array<Pose,2> attachment_{};
    std::array<Quat,6> seatedOrientation_{};
    Pose insertionOffset_{};
    uint64_t lastTime_{};
    float insertionTime_{},ejectionTime_{};
    bool initialized_{},activationSent_{};
};
}
