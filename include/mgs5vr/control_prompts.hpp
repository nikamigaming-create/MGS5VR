#pragma once
#include "controls.hpp"
#include <memory>

namespace mgs5vr {
// A native icon name is an action, not necessarily a physical button.
// Empty means its routing has not been established in this context.
// The focused handheld iDroid has a separate Previous tab chord. Other native
// menus keep their ordinary shoulder mapping, including Pause over iDroid.
std::string_view controlPromptAction(std::string_view token,ControlContext context,bool handheldIdroid=false) noexcept;
struct ControlPromptText {
    std::string text;
    unsigned replaced{},unresolved{};
};
// Replace only input tags. Keep translated prose, styling, non-input icons and
// unidentified tags byte-for-byte; never search for English caption text.
ControlPromptText rewriteControlPrompt(std::string_view text,const ControlBindings& bindings,ControlContext context,bool handheldIdroid=false);
// Some native help owners keep the icon separate from the translated caption.
// Resolve the owner's verified action token without matching caption wording.
ControlPromptText rewriteControlCaption(std::string_view caption,std::string_view token,const ControlBindings& bindings,ControlContext context,bool handheldIdroid=false);
// Publish only after a complete validated binding load/live neutral handoff.
// Native UI workers read immutable snapshots, never the XR gesture state.
void publishControlPromptBindings(const ControlBindings& bindings);
std::shared_ptr<const ControlBindings> controlPromptBindings();
}
