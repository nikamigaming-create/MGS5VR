#pragma once
#include <array>
#include <cstdint>
#include <optional>

namespace mgs5vr {
using NativeGameStatusRecord = std::array<uint8_t, 0x78>;
inline constexpr uint64_t nativeGameStatusEmptyOwner = 0xb8a0bf169f98ull;

// Retail IsSet reports the aggregate, even when passed an owner name. Resolve
// membership from the supported status record instead. Inconsistent snapshots
// cannot acknowledge an applied policy.
inline std::optional<bool> nativeGameStatusHasOwner(
    const NativeGameStatusRecord& record, uint64_t owner) noexcept {
    constexpr uint64_t hashMask = 0xffffffffffffull;
    if (!owner || owner > hashMask || owner == nativeGameStatusEmptyOwner || record[0x70] > 4)
        return {};
    std::array<uint64_t, 4> hashes{};
    unsigned occupied{};
    bool present{};
    for (unsigned slot = 0; slot < hashes.size(); ++slot) {
        // The upper two bytes are native metadata, not part of the owner hash.
        for (unsigned byte = 0; byte < 6; ++byte)
            hashes[slot] |= uint64_t{record[0x10 + slot * 0x18 + byte]} << (8 * byte);
        if (hashes[slot] == nativeGameStatusEmptyOwner) continue;
        for (unsigned previous = 0; previous < slot; ++previous)
            if (hashes[previous] == hashes[slot]) return {};
        ++occupied;
        present |= hashes[slot] == owner;
    }
    if (occupied != record[0x70]) return {};
    return present;
}
}
