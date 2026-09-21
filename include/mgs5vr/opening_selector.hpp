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
    bool cabinActive{};
    int selection{-1};
    OpeningTapeAction action{OpeningTapeAction::continueGame};
    OpeningPulse pulse{OpeningPulse::none};
    bool blocked{}; // A spatial tape that has no safe native route was touched.
    bool dogPetted{};
    bool continueReady{};
    Pose origin{};
};

// One shared layout is used by rendering and input.  Entry zero is the real
// radio/deck; entries one through six are the six action tapes in label order.
std::array<Vec3,7> openingPropOffsets() noexcept;
std::array<float,7> openingPropScales() noexcept;
bool openingPropsAvailable() noexcept;
bool openingCabinEnabled() noexcept;

// The title rack is deliberately fail-closed.  Continue is accepted only
// from the native title's known initial focus; every other tape remains
// visible but cannot leak a guessed D-pad sequence into the game.  No actor,
// menu, save-data, or Lua state is fabricated by this class.
class OpeningSelector {
public:
    OpeningSelectorFrame update(bool title,bool assetsAvailable,bool dogAvailable,Pose head,
        const std::array<TrackedHand,2>& hands,uint64_t now,std::optional<Pose> anchoredOrigin={});
    OpeningSelectorFrame updateCabin(bool active,Pose head,
        const std::array<TrackedHand,2>& hands,uint64_t now);
    void reset() noexcept;

private:
    enum class Phase : uint8_t { idle, confirm };
    Phase phase_{Phase::idle};
    int target_{-1};
    uint64_t confirmUntil_{};
    bool previousTrigger_{};
    bool initialized_{};
    Pose origin_{};
};

}
