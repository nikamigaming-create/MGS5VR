#pragma once
#include <array>
#include <cstdint>
#include <istream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mgs5vr {
enum class ControlContext { gameplay, equipment, commands, binoculars, menus, horse, vehicle, nativeButtons };
enum class ControllerFaceLayout { standard, triggerMenu, simple };
ControllerFaceLayout controllerFaceLayout(std::string_view profile);
// Keep legacy trigger/select fallbacks separate from physical A/B/X/Y. Some
// simulator action overrides aggregate suggested bindings across profiles.
std::array<bool,4> isolatedFaceButtons(const std::array<ControllerFaceLayout,2>& layouts,
    const std::array<bool,4>& face,const std::array<bool,2>& select,const std::array<bool,2>& back);
struct PhysicalControls {
    // a,b,x,y,menu,L3,R3,left grip,right grip,left trigger,right trigger;
    // indices 11..18 are derived stick directions; 19..20 are left/right
    // thumb-rest touch; 21..24 are A/B/X/Y touch, 25..26 stick touch,
    // 27..28 trigger touch. Appending sensors preserves existing indices.
    std::array<float,29> buttons{};
    std::array<float,2> leftStick{},rightStick{};
};
// A stick held across an ownership change must be centered before it can
// navigate, walk or aim for its new owner. Both axes transfer together.
class ControlAxisOwnership {
public:
    PhysicalControls update(PhysicalControls input,uint32_t owner) noexcept;
    void reset() noexcept {*this=ControlAxisOwnership{};}
private:
    uint32_t owner_{};
    bool initialized_{};
    std::array<bool,2> blocked_{};
};
struct ControlInputAudit {
    ControlContext context{};
    PhysicalControls physical{};
    uint64_t time{};
    uint16_t nativeButtons{};
    bool rigInput{};
    int travelMode{};
    // Final XR-mapped packet, before XInput ownership/arbitration. This is not
    // an observation of the native game's input consumer. LX,LY,RX,RY; LT,RT.
    std::array<int16_t,4> nativeAxes{};
    std::array<uint8_t,2> nativeTriggers{};
};
void publishControlInputAudit(ControlInputAudit sample);
// Copy before reading the observation clock, so a newer XR publication cannot
// appear to come from the future halfway through building diagnostics.
ControlInputAudit controlInputSnapshot();
constexpr bool freshControlInputAudit(const ControlInputAudit& sample,uint64_t now) noexcept {
    return sample.time&&now>=sample.time&&now-sample.time<=250;
}
std::optional<ControlInputAudit> controlInputAudit(uint64_t now);
std::string_view controlContextName(ControlContext context);
struct ControlDefinition {std::string_view name,binding;uint32_t contexts{};bool modifier{};};
const std::vector<ControlDefinition>& controlDefinitions();
const std::array<std::string_view,29>& controlInputNames();
uint32_t physicalControlMask(const PhysicalControls& input,bool touches=true) noexcept;
struct SettingDefinition {std::string_view name;float value{},minimum{},maximum{};};
const std::vector<SettingDefinition>& settingDefinitions();
struct NativeButtonDefinition {std::string_view name,binding;uint16_t mask{};};
// All fourteen game-facing XInput buttons, excluding the platform Guide button.
const std::array<NativeButtonDefinition,14>& nativeButtonDefinitions();
// Selection is an input latch, not a tracking-validity test. Missing poses
// temporarily withhold rendering/marking, never manufacture a stow press.
bool updateBinocularSelection(bool selected,bool equip,bool stow,bool contextAllowed);
class ControlBindings {
public:
    ControlBindings();
    // An invalid file is rejected as a whole; the previous bindings survive.
    std::vector<std::string> load(std::istream& input);
    void update(PhysicalControls input,ControlContext context,uint64_t time);
    void suspend();
    // Discard gestures owned by a closing UI until their physical inputs are
    // released, even when the native menu/gameplay publication changes later.
    // An already-held firearm can retain its separate ready/fire actions.
    void requireRelease(uint32_t inputs,bool preserveWeapon=false);
    float value(std::string_view action) const;
    bool active(std::string_view action) const {return value(action)>.5f;}
    // Inputs of the alternatives actually contributing to this update, not
    // every configured alternative. Used to give a consuming UI chord priority.
    uint32_t activeInputMask(std::string_view action) const;
    // Inputs from a complete chord that remain down after a partial release.
    uint32_t retainedInputMask(std::string_view action) const;
    // Derived direction inputs owned by the configured physical stick.
    uint32_t axisInputMask(std::string_view axis) const;
    bool hasBindingInput(std::string_view input) const;
    // Level modifiers can choose an input owner before its context is resolved.
    bool heldModifier(std::string_view action,const PhysicalControls& input) const;
    // Arbitration only: the effective chord is physically held, regardless
    // of its press/tap/hold gesture. Activation still comes from update().
    bool bindingChordHeld(std::string_view action,const PhysicalControls& input) const;
    uint32_t heldInputMask(std::string_view action,const PhysicalControls& input) const;
    // Physical buttons/sticks retain ordinary ownership thresholds. A touch
    // requests XR ownership only when an effective binding uses that sensor.
    bool hasPhysicalIntent(const PhysicalControls& input) const;
    std::array<float,2> axis(std::string_view name,const PhysicalControls& input) const;
    float setting(std::string_view name) const;
    std::string settingText(std::string_view name) const;
    std::string label(std::string_view action) const;
    // Canonical parser syntax, retaining alternatives and gesture timing.
    std::string bindingText(std::string_view action) const;
    // Effective parsed bindings, including overrides and timing. Used by the
    // test bot and generated instructions; never reconstruct this from labels.
    std::string bindingsJson() const;
private:
    enum class Gesture { level,press,release,tap,hold };
    struct Binding {uint32_t mask{};Gesture gesture{};uint64_t milliseconds{300};};
    struct State {uint64_t since{},until{};bool held{},blocked{},enabled{};};
    struct Entry {std::string name;std::vector<Binding> bindings;std::vector<State> states;uint32_t contexts{};bool modifier{};float value{};uint32_t activeInputs{},retainedInputs{},handoffInputs{};};
    struct Setting {std::string name;float value{},minimum{},maximum{};};
    struct Axis {std::string name;int source{};};
    std::vector<Entry> entries_;
    std::vector<Setting> settings_;
    std::vector<Axis> axes_;
    std::array<int,2> stickDirections_{};
    uint64_t lastTime_{};
    static std::vector<Binding> parse(std::string text);
};
// A complete file is validated away from the active gesture state. Applying a
// replacement requires physically neutral controls, including analog axes.
// Each file starts from defaults, just like a new game session; deleting an
// override therefore really restores the default instead of retaining it.
class LiveControls {
public:
    std::vector<std::string> stage(std::istream& input);
    bool apply(ControlBindings& destination,const PhysicalControls& physical);
    bool pending() const {return pending_.has_value();}
    void cancel(){pending_.reset();}
private:
    std::optional<ControlBindings> pending_;
};
}
