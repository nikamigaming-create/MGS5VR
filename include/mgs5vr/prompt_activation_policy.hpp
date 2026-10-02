#pragma once
#include <cstdint>
#include <limits>

namespace mgs5vr {
struct PromptActivationPolicy {
    bool inlineMarkup{},mapCaption{};
};
// The accepted exact-owner Map adapter is ordinary behavior. General markup
// remains experimental; its INI switch must not silently disable Map. Preserve
// the legacy environment switch as an explicit all-prompt opt-out, and permit
// a separate Map-only opt-out without changing the user's bindings.
constexpr PromptActivationPolicy promptActivationPolicy(bool environmentPresent,
        bool environmentEnabled,bool inlineExperiment,bool mapConfigured) noexcept {
    return {environmentPresent?environmentEnabled:inlineExperiment,
            mapConfigured&&(!environmentPresent||environmentEnabled)};
}
// The actual native Map capture uses mode 82 and caption entries 35..38.
// Historical enum-name extraction misidentified modes 46/47 as this page;
// those unobserved rows must not acquire the default Map adapter. Actions
// still come from typed markup, never from these page/row identifiers.
constexpr bool mapFooterMode(unsigned mode) noexcept {return mode==82;}
constexpr bool promptVrOwned(bool nativeGamepad,bool nativeButtons) noexcept {
    return !nativeGamepad&&!nativeButtons;
}
constexpr bool promptMarkupAllowed(bool genericEnabled,bool mapEnabled,bool exactMapFooter) noexcept {
    return genericEnabled||(mapEnabled&&exactMapFooter);
}
constexpr bool mapFooterRowEligible(uintptr_t owner,uintptr_t node,uintptr_t units,
        unsigned count,unsigned slot,unsigned mode,unsigned helpId) noexcept {
    if(!owner||!node||!units||count!=5||slot>=4||!mapFooterMode(mode))return false;
    const auto offset=uintptr_t{0xd0}+uintptr_t{slot}*0xa0;
    return owner<=std::numeric_limits<uintptr_t>::max()-offset
        &&units==owner+offset&&helpId==35+slot;
}
}
