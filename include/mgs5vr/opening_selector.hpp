#pragma once

#include "head_camera.hpp"
#include <array>
#include <cstdint>
#include <string_view>

namespace mgs5vr {

// These are the six labels shown by the native English Title screen.  The
// physical tapes keep those same semantics; they are not a second custom
// menu with invented names.
enum class OpeningTapeAction : uint8_t {
    continueGame,
    downloadGzSaveData,
    metalGearOnline,
    options,
    deleteSaveData,
    quitGame
};

inline constexpr std::array<std::string_view,6> openingTapeLabels{{
    "CONTINUE",
    "DOWNLOAD MGSV: GZ SAVE DATA",
    "METAL GEAR ONLINE",
    "OPTIONS",
    "DELETE SAVE DATA",
    "QUIT GAME"
}};

enum class OpeningPulse : uint8_t { none, up, down, confirm };

struct OpeningSelectorFrame {
    bool active{};
    int selection{-1};
    OpeningTapeAction action{OpeningTapeAction::continueGame};
    OpeningPulse pulse{OpeningPulse::none};
};

// One shared layout is used by rendering and input.  Entry zero is the real
// radio/deck; entries one through six are the six action tapes in label order.
std::array<Vec3,7> openingPropOffsets() noexcept;
std::array<float,7> openingPropScales() noexcept;
bool openingPropsAvailable() noexcept;

// The first interaction slice is intentionally small and deterministic:
// bring a tracked hand near a named tape, press the trigger, then drive the
// existing native Title controls to that row.  No save data or Lua API is
// fabricated by this class.
class OpeningSelector {
public:
    OpeningSelectorFrame update(bool title,bool assetsAvailable,Pose head,
        const TrackedHand& hand,uint64_t now);
    void reset() noexcept;

private:
    enum class Phase : uint8_t { idle, resetToTop, moveDown, confirm };
    Phase phase_{Phase::idle};
    int target_{-1};
    unsigned upRemaining_{};
    unsigned downRemaining_{};
    uint64_t nextPulseAt_{};
    bool previousTrigger_{};
};

}
