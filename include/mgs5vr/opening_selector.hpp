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
// Read-only native row/action table, sampled by the title's own update.
struct OpeningNativeFocus {
    std::array<uint8_t,7> actions{};
    uint8_t count{},row{};
    bool ready{};
};

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
    CabinBenchFrame bench{};
};

// One shared layout is used by rendering and input.  Entry zero is the real
// cassette player; entries one through six are tapes in label order.
std::array<Vec3,7> openingPropOffsets() noexcept;
std::array<float,7> openingPropScales() noexcept;
bool openingPropsAvailable() noexcept;
bool openingCabinEnabled() noexcept;

// Insertion navigates the observed native rows and confirms only the matching
// action. No menu, save-data or Lua state is fabricated by this class.
class OpeningSelector {
public:
    OpeningSelectorFrame update(bool title,bool assetsAvailable,bool dogAvailable,Pose head,
        const std::array<TrackedHand,2>& hands,uint64_t now,std::optional<Pose> anchoredOrigin={},
        OpeningNativeFocus focus={},CabinLayout layout=cabinDefaultLayout());
    OpeningSelectorFrame updateCabin(bool active,Pose head,
        const std::array<TrackedHand,2>& hands,uint64_t now,std::optional<Pose> anchoredOrigin={},
        CabinLayout layout=cabinDefaultLayout());
    void reset() noexcept;

private:
    enum class Phase : uint8_t { idle, navigate, release, confirm };
    Phase phase_{Phase::idle};
    int target_{-1};
    uint64_t confirmUntil_{},routeUntil_{},pulseUntil_{};
    uint8_t priorRow_{};
    OpeningPulse direction_{OpeningPulse::none};
    bool initialized_{};
    Pose origin_{};
    CabinProps props_{};
};

}
