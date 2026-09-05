#pragma once
#include "mailbox.hpp"
#include <atomic>
#include <chrono>
#include <string>

namespace mgs5vr {
struct TheatreConfig { float widthMeters{8}, distanceMeters{6}; };
struct RuntimeProbe { bool instanceAvailable{}, headsetAvailable{}; std::string runtime, system, error; };
struct RuntimeStats { uint64_t frames{}, submittedScreens{}, sourceFrames{}, trackingInvalidFrames{}; };
RuntimeProbe probeRuntime();
// This is a 2D theatre presenter. It does not claim native stereo game rendering.
RuntimeStats runTheatre(TextureMailbox& source, const TheatreConfig& config,
                        const std::atomic_bool& stop, std::chrono::seconds duration = {});
}
