#include "mgs5vr/controls.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cctype>
#include <set>
#include <stdexcept>

namespace mgs5vr {
namespace {
constexpr uint32_t mask(ControlContext c){return 1u<<static_cast<unsigned>(c);}
constexpr uint32_t foot=mask(ControlContext::gameplay),wrist=mask(ControlContext::equipment),
    commands=mask(ControlContext::commands),optic=mask(ControlContext::binoculars),menus=mask(ControlContext::menus),
    horse=mask(ControlContext::horse),vehicle=mask(ControlContext::vehicle),normal=127,
    native=mask(ControlContext::nativeButtons),all=normal|native;
std::string trim(std::string text){
    const auto begin=text.find_first_not_of(" \t\r\n"),end=text.find_last_not_of(" \t\r\n");
    return begin==std::string::npos?std::string{}:text.substr(begin,end-begin+1);
}
std::string lower(std::string text){for(auto& c:text)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return text;}
bool commandContinuation(std::string_view name){return name=="gameplay.ready_weapon"||name=="gameplay.fire_or_cqc";}
constexpr std::array<std::string_view,19> names{"a","b","x","y","menu","left_stick_click","right_stick_click",
    "left_grip","right_grip","left_trigger","right_trigger","left_stick_up","left_stick_down","left_stick_left","left_stick_right",
    "right_stick_up","right_stick_down","right_stick_left","right_stick_right"};
}
ControllerFaceLayout controllerFaceLayout(std::string_view profile){
    if(profile=="/interaction_profiles/khr/simple_controller")return ControllerFaceLayout::simple;
    if(profile=="/interaction_profiles/htc/vive_controller"||profile=="/interaction_profiles/microsoft/motion_controller")
        return ControllerFaceLayout::triggerMenu;
    return ControllerFaceLayout::standard;
}
std::array<bool,4> isolatedFaceButtons(const std::array<ControllerFaceLayout,2>& layouts,
    const std::array<bool,4>& face,const std::array<bool,2>& select,const std::array<bool,2>& back){
    const auto left=layouts[0],right=layouts[1];
    return {right==ControllerFaceLayout::standard?face[0]:select[1],
        right==ControllerFaceLayout::triggerMenu?back[1]:right==ControllerFaceLayout::simple?
            left==ControllerFaceLayout::simple&&select[0]:face[1],
        left==ControllerFaceLayout::standard&&face[2],left==ControllerFaceLayout::standard&&face[3]};
}
const std::array<NativeButtonDefinition,14>& nativeButtonDefinitions(){
    static constexpr std::array<NativeButtonDefinition,14> definitions{{
        {"native.a","a",0x1000},{"native.b","b",0x2000},
        {"native.x","x",0x4000},{"native.y","y",0x8000},
        {"native.left_shoulder","left_grip",0x0100},{"native.right_shoulder","right_grip",0x0200},
        {"native.left_click","left_stick_click",0x0040},{"native.right_click","right_stick_click",0x0080},
        {"native.start","left_grip + menu",0x0010},{"native.back","right_grip + menu",0x0020},
        {"native.dpad_up","menu + right_stick_up",0x0001},
        {"native.dpad_down","menu + right_stick_down",0x0002},
        {"native.dpad_left","menu + right_stick_left",0x0004},
        {"native.dpad_right","menu + right_stick_right",0x0008}}};
    return definitions;
}
const std::vector<ControlDefinition>& controlDefinitions(){
    static const std::vector<ControlDefinition> definitions=[] {
        std::vector<ControlDefinition> result{
        {"system.idroid","tap(menu,550)",normal},{"system.pause","hold(menu,550)",normal},
        {"system.recenter","press(left_grip + menu)",normal},{"system.toggle_vr","disabled",all},
        {"system.native_buttons","hold(menu + a,550)",all},
        {"gameplay.run","left_stick_click",foot},{"gameplay.stance","a",foot},
        {"gameplay.dive","press(right_stick_click)",foot},{"gameplay.interact","y",foot},
        {"gameplay.reload","tap(left_grip + b,300)",foot},{"gameplay.ready_weapon","right_grip",foot|horse|commands,true},
        {"gameplay.pickup_carry","b",foot},
        {"gameplay.support_grip","left_grip",foot|horse,true},
        {"gameplay.switch_weapon","press(right_grip + right_stick_click)",foot},
        {"gameplay.zoom","press(right_stick_up)",foot},
        {"gameplay.fire_or_cqc","right_trigger",foot|horse|commands},{"gameplay.equip_binoculars","hold(left_grip + y,300)",foot},
        {"gameplay.native_a","disabled",foot},{"gameplay.native_x","disabled",foot},
        {"gameplay.native_left_shoulder","disabled",foot},{"gameplay.native_right_shoulder","disabled",foot},
        {"gameplay.native_right_click","disabled",foot},
        {"gameplay.native_dpad_up","disabled",foot},{"gameplay.native_dpad_down","disabled",foot},
        {"gameplay.native_dpad_left","disabled",foot},{"gameplay.native_dpad_right","disabled",foot},
        {"equipment.open","left_trigger",foot|wrist|commands|optic|horse,true},
        {"equipment.primary","right_stick_up",wrist},{"equipment.secondary","right_stick_down",wrist},
        {"equipment.support","right_stick_right",wrist},{"equipment.items","right_stick_left",wrist},
        {"equipment.back","press(b)",wrist},{"equipment.use","press(a)",wrist},
        {"commands.open","press(x)",foot|wrist},
        {"commands.keep_open","x",foot|wrist|commands,true},
        {"commands.mounted_open","press(left_trigger + x)",horse},
        {"commands.mounted_keep_open","left_trigger",horse|commands,true},
        {"commands.confirm","press(a)",commands},
        {"commands.back","press(b)",commands},
        {"binoculars.stow","press(b)",optic},{"binoculars.zoom","press(left_stick_click)",optic},
        {"binoculars.mark","press(right_trigger)",optic},{"binoculars.clear_mark","press(a)",optic},
        {"binoculars.support_grip","left_grip",optic,true},
        {"binoculars.run","right_stick_up",optic},{"binoculars.dive","press(right_stick_click)",optic},
        {"binoculars.stance","right_stick_down",optic},
        {"menus.confirm","a",menus},{"menus.back","b",menus},{"menus.action_x","x",menus},{"menus.action_y","y",menus},
        {"menus.previous_tab","left_grip",menus},{"menus.next_tab","right_grip",menus},
        {"menus.left_trigger","left_trigger",menus},{"menus.right_trigger","right_trigger",menus},
        {"menus.left_click","left_stick_click",menus},{"menus.right_click","right_stick_click",menus},
        {"menus.dpad_up","disabled",menus},{"menus.dpad_down","disabled",menus},
        {"menus.dpad_left","disabled",menus},{"menus.dpad_right","disabled",menus},
        {"horse.gallop","x",horse},{"horse.interact","y",horse},{"horse.stance","a",horse},
        {"horse.reload","b",horse},{"horse.left_click","left_stick_click",horse},{"horse.right_click","right_stick_click",horse},
        {"vehicle.accelerate","right_trigger",vehicle},{"vehicle.brake_reverse","left_trigger",vehicle},
        {"vehicle.wheel_grip","left_grip",vehicle|wrist,true},{"vehicle.equipment_open","right_grip",vehicle|wrist,true},
        {"vehicle.interact","y",vehicle},{"vehicle.weapon_or_call","x",vehicle},
        {"vehicle.native_a","a",vehicle},{"vehicle.native_b","b",vehicle},
        {"vehicle.left_click","left_stick_click",vehicle},{"vehicle.right_click","right_stick_click",vehicle},
        {"turn.left","right_stick_left",foot|optic|horse|vehicle},{"turn.right","right_stick_right",foot|optic|horse|vehicle},
        {"native.left_trigger","left_trigger",native},{"native.right_trigger","right_trigger",native},
        {"native.dpad_hold","menu",native,true}
        };
        for(const auto& button:nativeButtonDefinitions())result.push_back({button.name,button.binding,native});
        return result;
    }();
    return definitions;
}
bool updateBinocularSelection(bool selected,bool equip,bool stow,bool contextAllowed){
    return contextAllowed&&!stow&&(selected||equip);
}
std::vector<ControlBindings::Binding> ControlBindings::parse(std::string text){
    text=lower(trim(text));
    if(text=="disabled")return {};
    if(text.empty())throw std::runtime_error("empty binding; use disabled to turn an action off");
    std::vector<Binding> result;
    for(size_t start=0;start<text.size();){
        const auto end=text.find('|',start);auto part=trim(text.substr(start,end==std::string::npos?end:end-start));
        Binding binding;
        const auto open=part.find('(');
        if(open!=std::string::npos){
            if(part.back()!=')')throw std::runtime_error("binding needs a closing )");
            const auto gesture=trim(part.substr(0,open));
            if(gesture=="press")binding.gesture=Gesture::press;
            else if(gesture=="release")binding.gesture=Gesture::release;
            else if(gesture=="tap")binding.gesture=Gesture::tap;
            else if(gesture=="hold")binding.gesture=Gesture::hold;
            else throw std::runtime_error("unknown gesture: "+gesture);
            part=trim(part.substr(open+1,part.size()-open-2));
            if(const auto comma=part.find(',');comma!=std::string::npos){
                if(binding.gesture!=Gesture::tap&&binding.gesture!=Gesture::hold)throw std::runtime_error("only tap/hold accept milliseconds");
                const auto duration=trim(part.substr(comma+1));size_t consumed{};
                binding.milliseconds=std::stoull(duration,&consumed);
                if(consumed!=duration.size()||binding.milliseconds<100||binding.milliseconds>3000)throw std::runtime_error("hold/tap time must be 100..3000 ms");
                part=trim(part.substr(0,comma));
            }
        }
        for(size_t at=0;at<part.size();){
            const auto plus=part.find('+',at);const auto token=trim(part.substr(at,plus==std::string::npos?plus:plus-at));
            const auto found=std::find(names.begin(),names.end(),token);
            if(found==names.end())throw std::runtime_error("unknown input: "+token);
            const auto bit=1u<<static_cast<unsigned>(found-names.begin());
            if(binding.mask&bit)throw std::runtime_error("input repeated in a chord: "+token);
            binding.mask|=bit;
            if(plus==std::string::npos)break;
            at=plus+1;if(at==part.size())throw std::runtime_error("missing input after +");
        }
        if(!binding.mask)throw std::runtime_error("binding needs an input");
        result.push_back(binding);
        if(end==std::string::npos)break;
        start=end+1;if(start==text.size())throw std::runtime_error("missing alternative after |");
    }
    return result;
}
ControlBindings::ControlBindings(){
    for(const auto& d:controlDefinitions()){
        auto bindings=parse(std::string(d.binding));
        entries_.push_back({std::string(d.name),bindings,std::vector<State>(bindings.size()),d.contexts,d.modifier});
    }
    axes_={{"axes.move",0},{"axes.equipment",1},{"axes.commands",1},{"axes.menu",0},{"axes.map",1},{"axes.vehicle_steering",0},
        {"axes.native_move",0},{"axes.native_look",1},{"axes.turn",1}};
    settings_={{"settings.snap_turn_degrees",30,5,90},{"settings.motion_melee",1,0,1},{"settings.animal_touch",1,0,1},
        {"settings.wrist_surface_lift_cm",2,0,10},{"settings.wrist_selector_height_cm",12,5,30},
        {"settings.wrist_picker_width_cm",75,42,100},
        {"settings.scope_eye_relief_cm",10,3,20},{"settings.turn_mode",0,0,2},{"settings.hud_mode",1,0,2},
        {"settings.binocular_pitch_degrees",-90,-180,180},{"settings.binocular_yaw_degrees",0,-180,180},
        {"settings.binocular_roll_degrees",0,-180,180},
        {"settings.binocular_auto_mark",1,0,1},{"settings.binocular_actor_glow",1,0,1},
        {"settings.binocular_mark_dwell_ms",650,100,3000}};
}
std::vector<std::string> ControlBindings::load(std::istream& input){
    ControlBindings candidate=*this;std::vector<std::string> errors;std::set<std::string> seen;
    std::string line,section;unsigned number{};
    while(std::getline(input,line)){
        ++number;
        if(number==1&&line.starts_with("\xef\xbb\xbf"))line.erase(0,3);
        if(const auto comment=line.find_first_of(";#");comment!=std::string::npos)line.resize(comment);
        line=lower(trim(line));if(line.empty())continue;
        try{
            if(line.front()=='['){
                if(line.back()!=']')throw std::runtime_error("section needs a closing ]");
                section=trim(line.substr(1,line.size()-2));
                const bool known=std::any_of(entries_.begin(),entries_.end(),[&](const auto& e){return e.name.starts_with(section+".");})
                    ||section=="axes"||section=="settings";
                if(!known)throw std::runtime_error("unknown section: "+section);
                continue;
            }
            const auto equals=line.find('=');if(equals==std::string::npos)throw std::runtime_error("expected action = binding");
            const auto key=section+"."+trim(line.substr(0,equals)),value=trim(line.substr(equals+1));
            if(!seen.insert(key).second)throw std::runtime_error("duplicate action: "+key);
            if(auto entry=std::find_if(candidate.entries_.begin(),candidate.entries_.end(),[&](const auto& e){return e.name==key;});entry!=candidate.entries_.end()){
                entry->bindings=parse(value);entry->states=std::vector<State>(entry->bindings.size());
            }else if(auto axis=std::find_if(candidate.axes_.begin(),candidate.axes_.end(),[&](const auto& e){return e.name==key;});axis!=candidate.axes_.end()){
                if(value!="left_stick"&&value!="right_stick"&&value!="disabled")throw std::runtime_error("axis must be left_stick, right_stick or disabled");
                axis->source=value=="left_stick"?0:value=="right_stick"?1:-1;
            }else if(auto setting=std::find_if(candidate.settings_.begin(),candidate.settings_.end(),[&](const auto& e){return e.name==key;});setting!=candidate.settings_.end()){
                if(key=="settings.turn_mode"){
                    if(value!="snap"&&value!="native_smooth"&&value!="off")throw std::runtime_error("turn_mode must be snap, native_smooth or off");
                    setting->value=value=="snap"?0.f:value=="native_smooth"?1.f:2.f;continue;
                }
                if(key=="settings.hud_mode"){
                    if(value!="full"&&value!="binoculars_only"&&value!="off")throw std::runtime_error("hud_mode must be full, binoculars_only or off");
                    setting->value=value=="full"?0.f:value=="binoculars_only"?1.f:2.f;continue;
                }
                size_t consumed{};const auto parsed=std::stof(value,&consumed);
                if(consumed!=value.size()||!std::isfinite(parsed)||parsed<setting->minimum||parsed>setting->maximum
                   ||(setting->maximum==1&&parsed!=0&&parsed!=1))throw std::runtime_error("setting outside supported range: "+key);
                setting->value=parsed;
            }else throw std::runtime_error("unknown action: "+key);
        }catch(const std::exception& e){errors.push_back("line "+std::to_string(number)+": "+e.what());}
    }
    if(input.bad())errors.push_back("could not read the complete controls file");
    // Ambiguous identical bindings in the same context are almost always a typo.
    // Tap and hold on one button are intentional; disjoint contexts are safe.
    for(size_t a=0;a<candidate.entries_.size();++a)for(size_t b=a+1;b<candidate.entries_.size();++b){
        const auto& x=candidate.entries_[a];const auto& y=candidate.entries_[b];
        const auto shared=x.contexts&y.contexts;
        if(!shared||x.modifier||y.modifier||(shared==commands&&(commandContinuation(x.name)||commandContinuation(y.name))))continue;
        for(const auto& xb:x.bindings)for(const auto& yb:y.bindings)if(xb.mask==yb.mask){
            const bool split=(xb.gesture==Gesture::tap&&yb.gesture==Gesture::hold)||(xb.gesture==Gesture::hold&&yb.gesture==Gesture::tap);
            if(!split)errors.push_back("conflict: "+x.name+" and "+y.name+" use the same input in the same mode");
            else if(xb.milliseconds!=yb.milliseconds)errors.push_back("tap/hold times must match: "+x.name+" and "+y.name);
        }
    }
    if(errors.empty()){*this=std::move(candidate);suspend();}
    return errors;
}
void ControlBindings::suspend(){
    for(auto& e:entries_){e.value=0;for(auto& state:e.states){state={};state.blocked=true;}}
    stickDirections_={};lastTime_=0;
}
std::vector<std::string> LiveControls::stage(std::istream& input){
    pending_.reset();
    ControlBindings candidate;
    auto errors=candidate.load(input);
    if(errors.empty())pending_=std::move(candidate);
    return errors;
}
bool LiveControls::apply(ControlBindings& destination,const PhysicalControls& physical){
    if(!pending_)return false;
    for(size_t n=0;n<11;++n)
        if(!std::isfinite(physical.buttons[n])||std::abs(physical.buttons[n])>.09f)return false;
    for(const auto& stick:{physical.leftStick,physical.rightStick})
        for(const auto axis:stick)if(!std::isfinite(axis)||std::abs(axis)>=.18f)return false;
    destination=std::move(*pending_);pending_.reset();destination.suspend();
    return true;
}
void ControlBindings::update(PhysicalControls input,ControlContext context,uint64_t time){
    if(time<lastTime_)suspend();lastTime_=time;
    for(auto& v:input.buttons)v=std::isfinite(v)?std::clamp(v,0.f,1.f):0;
    for(size_t side=0;side<2;++side){
        const auto stick=side?input.rightStick:input.leftStick;
        auto& direction=stickDirections_[side];
        if(!std::isfinite(stick[0])||!std::isfinite(stick[1])||(std::abs(stick[0])<.35f&&std::abs(stick[1])<.35f))direction=0;
        else if(!direction&&std::max(std::abs(stick[0]),std::abs(stick[1]))>.7f)
            direction=std::abs(stick[1])>=std::abs(stick[0])?(stick[1]>0?1:2):(stick[0]<0?3:4);
        for(unsigned d=1;d<=4;++d)input.buttons[11+side*4+d-1]=direction==static_cast<int>(d)?1.f:0.f;
    }
    uint32_t down{};for(unsigned n=0;n<input.buttons.size();++n)if(input.buttons[n]>.5f)down|=1u<<n;
    const auto current=mask(context);
    for(auto& e:entries_){
        e.value=0;const bool enabled=(e.contexts&current)!=0;
        for(size_t n=0;n<e.bindings.size();++n){
            const auto& binding=e.bindings[n];auto& state=e.states[n];
            const bool held=(down&binding.mask)==binding.mask;
            const auto singleBit=std::countr_zero(binding.mask);
            const bool analog=std::popcount(binding.mask)==1&&singleBit>=7&&singleBit<=10;
            const bool occupied=analog?input.buttons[singleBit]>.09f:held;
            if(!enabled){state={};state.blocked=occupied;continue;}
            if(!state.enabled){state.enabled=true;if(occupied)state.blocked=true;}
            bool superseded=false;
            if(!e.modifier&&!(current==commands&&commandContinuation(e.name)))
                for(const auto& other:entries_)if((other.contexts&current)&&!other.modifier
                    &&!(current==commands&&commandContinuation(other.name)))for(const auto& chord:other.bindings)
                if(chord.mask!=binding.mask&&(chord.mask&binding.mask)==binding.mask&&(down&chord.mask)==chord.mask)superseded=true;
            if(superseded){state.blocked=true;state.until=0;}
            if(state.blocked){if(!occupied){state.blocked=false;state.held=false;}continue;}
            if(held&&!state.held)state.since=time;
            const bool rising=held&&!state.held,falling=!held&&state.held;
            bool pulse=(binding.gesture==Gesture::press&&rising)||(binding.gesture==Gesture::release&&falling)
                ||(binding.gesture==Gesture::tap&&falling&&time-state.since<binding.milliseconds)
                ||(binding.gesture==Gesture::hold&&held&&time-state.since>=binding.milliseconds&&(!state.until));
            if(pulse)state.until=time+100;
            float value=0;
            if(binding.gesture==Gesture::level){
                if(held){value=1;for(unsigned bit=0;bit<input.buttons.size();++bit)if(binding.mask&(1u<<bit))value=std::min(value,input.buttons[bit]);}
                // Single trigger/grip bindings retain their full analog travel.
                // Chords still use the deliberate half-press activation threshold.
                if(analog)value=input.buttons[singleBit];
            }else if(state.until&&time<state.until)value=1;
            // A completed hold cannot repeat until every use of that chord ends.
            if(falling&&binding.gesture==Gesture::hold)state.until=0;
            state.held=held;e.value=std::max(e.value,value);
        }
    }
}
float ControlBindings::value(std::string_view action) const{
    const auto it=std::find_if(entries_.begin(),entries_.end(),[&](const auto& e){return e.name==action;});
    return it==entries_.end()?0:it->value;
}
std::array<float,2> ControlBindings::axis(std::string_view name,const PhysicalControls& input) const{
    const auto it=std::find_if(axes_.begin(),axes_.end(),[&](const auto& e){return e.name==name;});
    if(it==axes_.end()||it->source<0)return {};
    auto result=it->source?input.rightStick:input.leftStick;
    for(auto& f:result)f=std::isfinite(f)?std::clamp(f,-1.f,1.f):0;
    return result;
}
float ControlBindings::setting(std::string_view name) const{
    const auto it=std::find_if(settings_.begin(),settings_.end(),[&](const auto& e){return e.name==name;});
    return it==settings_.end()?0:it->value;
}
std::string ControlBindings::label(std::string_view action) const{
    const auto entry=std::find_if(entries_.begin(),entries_.end(),[&](const auto& e){return e.name==action;});
    if(entry==entries_.end()||entry->bindings.empty())return "UNBOUND";
    constexpr std::array<std::string_view,19> labels{"A","B","X","Y","MENU","L CLICK","R CLICK","L GRIP","R GRIP","L TRIGGER","R TRIGGER",
        "L STICK UP","L STICK DOWN","L STICK LEFT","L STICK RIGHT","R STICK UP","R STICK DOWN","R STICK LEFT","R STICK RIGHT"};
    std::string text;
    for(const auto& binding:entry->bindings){
        if(!text.empty())text+=" / ";
        if(binding.gesture==Gesture::hold)text+="HOLD ";
        else if(binding.gesture==Gesture::tap)text+="TAP ";
        else if(binding.gesture==Gesture::release)text+="RELEASE ";
        bool first=true;
        for(unsigned bit=0;bit<labels.size();++bit)if(binding.mask&(1u<<bit)){
            if(!first)text+=" + ";text+=labels[bit];first=false;
        }
    }
    return text;
}
}
