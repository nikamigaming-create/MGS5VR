#pragma once
#include "controls.hpp"
#include "core.hpp"
#include <filesystem>
#include <iosfwd>

namespace mgs5vr {
enum class ControlsMode { field,acc,title,driving,horse,binoculars,equipment,commands,idroid,menus,nativeButtons,cinematic,count };
std::string_view controlsModeName(ControlsMode mode) noexcept;
ControlContext controlsModeContext(ControlsMode mode) noexcept;
struct ControlsRow {std::string action,title,label;bool axis{};};
enum class ControlsEditPhase { browse,release,capture,confirm,applying };
struct ControlsEditorFrame {
    bool visible{},ownsInput{},automatic{true},touches{},append{},matched{},settings{},vrEnabled{true},validProposal{};
    ControlsMode mode{ControlsMode::field},liveMode{ControlsMode::field};
    ControlsEditPhase phase{ControlsEditPhase::browse};
    Pose panel{};
    PhysicalControls physical{};
    std::vector<ControlsRow> rows;
    size_t selected{},page{};
    int hover{-1};
    std::array<float,2> cursor{};
    bool cursorValid{};
    uint32_t selectedInputs{},capturedInputs{};
    float settingValue{},minimum{},maximum{},step{1};
    std::string action,binding,proposed,status,gesture{"press"};
    uint64_t milliseconds{300},revision{};
};
// Patch one override while retaining comments, other actions and all tuning.
std::string replaceControlBinding(std::string contents,std::string_view action,std::string_view binding);
bool saveControlBinding(const std::filesystem::path& path,const std::string& expected,
    const std::string& replacement,std::string& error);
class ControlsEditor {
public:
    bool visible() const noexcept{return frame_.visible;}
    bool ownsInput() const noexcept{return frame_.ownsInput;}
    const ControlsEditorFrame& frame() const noexcept{return frame_;}
    std::optional<bool> takeVrRequest() noexcept {auto request=vrRequest_;vrRequest_.reset();return request;}
    void open(Pose head,ControlsMode mode,const ControlBindings& bindings,const std::filesystem::path& path,uint64_t epoch);
    void close() noexcept;
    // UI ray/trigger is fixed while editing, so a remapped or disabled action
    // cannot remove Confirm/Cancel. The actual game decoder remains separate.
    void update(const PhysicalControls& physical,const std::array<Pose,2>& aims,
        const std::array<bool,2>& tracked,ControlsMode mode,const ControlBindings& bindings,uint64_t now,uint64_t epoch,bool vrEnabled=true);
    void command(int control,const ControlBindings& bindings);
private:
    ControlsEditorFrame frame_;
    ControlBindings sandbox_;
    std::filesystem::path path_;
    std::string baseline_,replacement_;
    uint64_t epoch_{},captureAt_{},lastAt_{};
    uint32_t peak_{};
    std::array<bool,2> trigger_{};
    bool ready_{},matchHeld_{};
    std::optional<bool> vrRequest_;
    void rows(const ControlBindings& bindings);
    void select(size_t index,const ControlBindings& bindings);
    void propose(std::string binding,const ControlBindings& bindings);
};
// Shared normalized layout IDs: rows 0..10, then controls starting at 100.
struct ControlsHit {int id;float x,y,width,height;std::string_view text;};
std::vector<ControlsHit> controlsEditorLayout(const ControlsEditorFrame& frame);
void publishControlsEditor(const ControlsEditorFrame& frame);
ControlsEditorFrame controlsEditorSnapshot();
void reportControlsEditor(std::ostream& output);
}
