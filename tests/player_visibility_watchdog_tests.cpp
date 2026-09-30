#include "mgs5vr/player_visibility.hpp"

static_assert(!mgs5vr::shouldRestoreStalePlayerVisibility(249, false, false, false));
static_assert(mgs5vr::shouldRestoreStalePlayerVisibility(250, false, false, false));
static_assert(mgs5vr::shouldRestoreStalePlayerVisibility(500, false, false, true));
static_assert(!mgs5vr::shouldRestoreStalePlayerVisibility(500, true, false, false));
static_assert(mgs5vr::shouldRestoreStalePlayerVisibility(500, true, true, false));
static_assert(!mgs5vr::shouldRestoreStalePlayerVisibility(500, true, true, true));

int main() { return 0; }
