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
    // the last eight entries are derived stick directions, not caller input.
    std::array<float,19> buttons{};
    std::array<float,2> leftStick{},rightStick{};
};
struct ControlDefinition {std::string_view name,binding;uint32_t contexts{};bool modifier{};};
const std::vector<ControlDefinition>& controlDefinitions();
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
    float value(std::string_view action) const;
    bool active(std::string_view action) const {return value(action)>.5f;}
    std::array<float,2> axis(std::string_view name,const PhysicalControls& input) const;
    float setting(std::string_view name) const;
    std::string label(std::string_view action) const;
private:
    enum class Gesture { level,press,release,tap,hold };
    struct Binding {uint32_t mask{};Gesture gesture{};uint64_t milliseconds{300};};
    struct State {uint64_t since{},until{};bool held{},blocked{},enabled{};};
    struct Entry {std::string name;std::vector<Binding> bindings;std::vector<State> states;uint32_t contexts{};bool modifier{};float value{};};
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
