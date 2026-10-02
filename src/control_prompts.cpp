#include "mgs5vr/control_prompts.hpp"
#include <atomic>
#include <utility>

namespace mgs5vr {
namespace {
std::atomic<std::shared_ptr<const ControlBindings>> promptBindings;
struct Route {std::string_view token,action;};
constexpr Route menuRoutes[]{
    {"DECISION","menus.confirm"},{"CANCEL","menus.back"},
    {"PAD_A","menus.confirm"},{"PAD_B","menus.back"},
    {"PAD_X","menus.action_x"},{"PAD_Y","menus.action_y"},
    {"PAD_L1","menus.previous_tab"},{"PAD_R1","menus.next_tab"},
    {"PAD_L2","menus.left_trigger"},{"PAD_R2","menus.right_trigger"},
    {"PAD_L3","menus.left_click"},{"PAD_R3","menus.right_click"},
    {"UI_STOCK","menus.right_click"},
    {"PAD_UP","menus.dpad_up"},{"PAD_DOWN","menus.dpad_down"},
    {"PAD_LEFT","menus.dpad_left"},{"PAD_RIGHT","menus.dpad_right"},
    {"PAD_LS","axes.menu"},{"PAD_RS","axes.map"},
    {"PAD_START","system.idroid"},{"PAD_SELECT","system.pause"}};
constexpr Route nativeRoutes[]{
    {"PAD_A","native.a"},{"PAD_B","native.b"},{"PAD_X","native.x"},{"PAD_Y","native.y"},
    {"PAD_L1","native.left_shoulder"},{"PAD_R1","native.right_shoulder"},
    {"PAD_L2","native.left_trigger"},{"PAD_R2","native.right_trigger"},
    {"PAD_L3","native.left_click"},{"PAD_R3","native.right_click"},
    {"PAD_UP","native.dpad_up"},{"PAD_DOWN","native.dpad_down"},
    {"PAD_LEFT","native.dpad_left"},{"PAD_RIGHT","native.dpad_right"},
    {"PAD_START","native.start"},{"PAD_SELECT","native.back"},
    {"PAD_LS","axes.native_move"},{"PAD_RS","axes.native_look"}};
template<size_t N> std::string_view find(std::string_view token,const Route (&routes)[N]) noexcept {
    for(const auto& r:routes)if(r.token==token)return r.action;
    return {};
}
std::string promptLabel(std::string label){
    // Keep the full gesture/chord; shorten only conventional trigger names so
    // native bottom-row help does not spend its entire width on two controls.
    for(const auto& [from,to]:{std::pair{"L TRIGGER","LT"},std::pair{"R TRIGGER","RT"}}){
        for(size_t position=0;(position=label.find(from,position))!=std::string::npos;){
            label.replace(position,std::char_traits<char>::length(from),to);
            position+=std::char_traits<char>::length(to);
        }
    }
    return label;
}
}
void publishControlPromptBindings(const ControlBindings& bindings){
    promptBindings.store(std::make_shared<const ControlBindings>(bindings));
}
std::shared_ptr<const ControlBindings> controlPromptBindings(){return promptBindings.load();}
std::string_view controlPromptAction(std::string_view token,ControlContext context) noexcept {
    if(context==ControlContext::nativeButtons)return find(token,nativeRoutes);
    if(context==ControlContext::menus)if(const auto route=find(token,menuRoutes);!route.empty())return route;
    if(token=="MBDEVICE")return "system.idroid";
    if(token=="PAUSE")return "system.pause";
    if(context==ControlContext::binoculars){
        constexpr Route routes[]{
            {"DECISION","binoculars.mark"},{"CANCEL","binoculars.stow"},
            {"PAD_R3","binoculars.zoom"},{"PAD_R2","binoculars.mark"},
            {"STANCE","binoculars.stance"},{"EVADE","binoculars.dive"},
            {"DASH","binoculars.run"},{"PAD_LS","axes.move"}};
        return find(token,routes);
    }
    if(context==ControlContext::equipment){
        constexpr Route routes[]{
            {"DECISION","equipment.use"},{"PAD_A","equipment.use"},
            {"CANCEL","equipment.back"},{"PAD_B","equipment.back"},
            {"PAD_UP","equipment.primary"},{"PAD_DOWN","equipment.secondary"},
            {"PAD_RIGHT","equipment.support"},{"PAD_LEFT","equipment.items"},
            {"PAD_RS","axes.equipment"}};
        return find(token,routes);
    }
    if(context==ControlContext::commands){
        constexpr Route routes[]{
            {"DECISION","commands.confirm"},{"PAD_A","commands.confirm"},
            {"CANCEL","commands.back"},{"PAD_B","commands.back"},
            {"PAD_RS","axes.commands"},{"CALL","commands.keep_open"}};
        return find(token,routes);
    }
    if(context==ControlContext::vehicle){
        constexpr Route routes[]{
            {"ACTION","vehicle.interact"},{"PAD_Y","vehicle.interact"},
            {"PAD_X","vehicle.weapon_or_call"},{"PAD_A","vehicle.native_a"},
            {"PAD_B","vehicle.native_b"},{"PAD_L2","vehicle.brake_reverse"},
            {"PAD_R2","vehicle.accelerate"},{"PAD_LS","axes.vehicle_steering"}};
        return find(token,routes);
    }
    if(context==ControlContext::horse){
        constexpr Route routes[]{
            {"DASH","horse.gallop"},{"ACTION","horse.interact"},
            {"STANCE","horse.stance"},{"RELOAD","horse.reload"},
            {"BINOS","gameplay.equip_binoculars"},{"PAD_LS","axes.move"}};
        return find(token,routes);
    }
    // Semantic gameplay help also appears in tutorial/item-description menus.
    // Literal pad icons in menus were resolved above to menu actions instead.
    if(context==ControlContext::gameplay||context==ControlContext::menus){
        constexpr Route routes[]{
            {"RELOAD","gameplay.reload"},{"BINOS","gameplay.equip_binoculars"},
            {"HOLD","gameplay.ready_weapon"},{"ATTACK","gameplay.fire_or_cqc"},
            {"CQC","gameplay.fire_or_cqc"},{"STANCE","gameplay.stance"},
            {"EVADE","gameplay.dive"},{"DASH","gameplay.run"},
            {"ACTION","gameplay.interact"},{"CALL","commands.open"},
            {"PAD_LS","axes.move"}};
        return find(token,routes);
    }
    return {};
}
ControlPromptText rewriteControlPrompt(std::string_view text,const ControlBindings& bindings,ControlContext context){
    ControlPromptText result;
    if(text.size()>16384){result.text=text;return result;}
    constexpr std::string_view prefix="<I=G=";
    size_t position=0;
    std::string_view previousToken;
    while(position<text.size()){
        const auto begin=text.find(prefix,position);
        if(begin==std::string_view::npos){result.text.append(text.substr(position));break;}
        result.text.append(text.substr(position,begin-position));
        const auto end=text.find('>',begin+prefix.size());
        if(end==std::string_view::npos){result.text.append(text.substr(begin));break;}
        const auto token=text.substr(begin+prefix.size(),end-begin-prefix.size());
        const auto action=controlPromptAction(token,context);
        if(action.empty()){
            result.text.append(text.substr(begin,end-begin+1));++result.unresolved;previousToken={};
        }else{
            // The verified menu trigger pair is its zoom alternative. Do not
            // infer chord/alternative semantics for other adjacent icons.
            if(context==ControlContext::menus&&previousToken=="PAD_L2"&&token=="PAD_R2"&&begin==position){result.text.pop_back();result.text+='/';}
            else result.text+='[';
            result.text+=promptLabel(bindings.label(action));result.text+=']';++result.replaced;
            previousToken=token;
        }
        position=end+1;
    }
    return result;
}
ControlPromptText rewriteControlCaption(std::string_view caption,std::string_view token,const ControlBindings& bindings,ControlContext context){
    ControlPromptText result;
    const auto action=controlPromptAction(token,context);
    if(caption.empty()||caption.size()>16384||action.empty()){
        result.text=caption;result.unresolved=action.empty()?1:0;return result;
    }
    result.text='['+promptLabel(bindings.label(action))+"] ";
    result.text.append(caption);result.replaced=1;
    return result;
}
}
