#include "mgs5vr/controls_editor.hpp"
#include <windows.h>
#include <algorithm>
#include <bit>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <mutex>
#include <iomanip>

namespace mgs5vr {
namespace {
std::mutex frameMutex;
ControlsEditorFrame publishedFrame;
std::string trim(std::string value){const auto a=value.find_first_not_of(" \t\r\n"),b=value.find_last_not_of(" \t\r\n");
    return a==std::string::npos?std::string{}:value.substr(a,b-a+1);}
std::string lower(std::string value){for(auto& c:value)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return value;}
bool neutral(const PhysicalControls& physical,bool touches=false){
    if(physicalControlMask(physical,touches))return false;
    for(const auto& stick:{physical.leftStick,physical.rightStick})for(float axis:stick)if(!std::isfinite(axis)||std::abs(axis)>.18f)return false;
    for(size_t n=7;n<11;++n)if(!std::isfinite(physical.buttons[n])||physical.buttons[n]>.09f)return false;
    return true;
}
size_t settingsGroup(std::string_view name){
    if(name=="system.toggle_vr"||name=="settings.full_body"||name=="settings.eyepatch"||name=="settings.seated_mode"||name.find("height")!=name.npos)return 1;
    if(name.find("rocket_arm")!=name.npos)return 4;
    if(name.find("enemy_")!=name.npos||name.find("binocular")!=name.npos||name.find("scope")!=name.npos)return 2;
    if(name.find("idroid")!=name.npos||name.find("handheld")!=name.npos||name.find("menu_quad")!=name.npos)return 6;
    if(name.find("wrist")!=name.npos||name.find("hud")!=name.npos)return 5;
    if(name.find("hand_")!=name.npos||name.find("support_")!=name.npos||name.find("weapon")!=name.npos||name.find("cover")!=name.npos||name.find("god_mode")!=name.npos)return 3;
    return 7;
}
std::string title(std::string_view action){
    const auto dot=action.find('.');std::string text(action.substr(dot==std::string_view::npos?0:dot+1));
    std::replace(text.begin(),text.end(),'_',' ');if(!text.empty())text[0]=static_cast<char>(std::toupper(text[0]));return text;
}
uint32_t textMask(std::string_view text){uint32_t result{};
    for(size_t n=0;n<controlInputNames().size();++n){const auto token=controlInputNames()[n];size_t at{};
        while((at=text.find(token,at))!=std::string_view::npos){
            const auto identifier=[](char c){return std::isalnum(static_cast<unsigned char>(c))||c=='_';};
            if((!at||!identifier(text[at-1]))&&(at+token.size()==text.size()||!identifier(text[at+token.size()])))result|=1u<<n;
            at+=token.size();}}
    return result;
}
std::string editorValue(const ControlsEditorFrame& f,const ControlBindings& bindings){
    return !f.settings?bindings.bindingText(f.action):f.action=="system.toggle_vr"?
        (f.vrEnabled?"1":"0"):bindings.settingText(f.action);
}
std::string settingValueText(std::string_view action,float value){
    if(action=="settings.turn_mode")return value==0?"snap":value==1?"native_smooth":"off";
    if(action=="settings.hud_mode")return value==0?"full":value==1?"binoculars_only":"off";
    std::ostringstream out;out<<value;return out.str();
}
}
void publishControlsEditor(const ControlsEditorFrame& frame){std::lock_guard lock(frameMutex);publishedFrame=frame;}
ControlsEditorFrame controlsEditorSnapshot(){std::lock_guard lock(frameMutex);return publishedFrame;}
void reportControlsEditor(std::ostream& out){
    const auto f=controlsEditorSnapshot();
    out<<",\"controls_editor\":{\"visible\":"<<f.visible<<",\"owns_input\":"<<f.ownsInput
       <<",\"automatic\":"<<f.automatic<<",\"mode\":"<<std::quoted(std::string(controlsModeName(f.mode)))
       <<",\"live_mode\":"<<std::quoted(std::string(controlsModeName(f.liveMode)))
       <<",\"phase\":"<<static_cast<unsigned>(f.phase)<<",\"page\":"<<f.page<<",\"selected\":"<<f.selected
       <<",\"hover\":"<<f.hover<<",\"action\":"<<std::quoted(f.action)<<",\"binding\":"<<std::quoted(f.binding)
       <<",\"proposed\":"<<std::quoted(f.proposed)<<",\"status\":"<<std::quoted(f.status)
       <<",\"settings\":"<<f.settings<<",\"vr_enabled\":"<<f.vrEnabled<<",\"valid_proposal\":"<<f.validProposal
       <<",\"setting_value\":"<<f.settingValue<<",\"minimum\":"<<f.minimum<<",\"maximum\":"<<f.maximum
       <<",\"milliseconds\":"<<f.milliseconds<<",\"assigned_inputs\":"<<f.selectedInputs
       <<",\"live_inputs\":"<<physicalControlMask(f.physical)<<",\"matched\":"<<f.matched
       <<",\"panel\":{\"position\":["<<f.panel.position.x<<','<<f.panel.position.y<<','<<f.panel.position.z
       <<"],\"orientation\":["<<f.panel.orientation.x<<','<<f.panel.orientation.y<<','<<f.panel.orientation.z<<','<<f.panel.orientation.w<<"]},\"rows\":[";
    bool first=true;for(const auto& row:f.rows){if(!first)out<<',';first=false;out<<"{\"action\":"<<std::quoted(row.action)<<",\"axis\":"<<row.axis<<'}';}
    out<<"],\"layout\":[";first=true;for(const auto& hit:controlsEditorLayout(f)){
        if(!first)out<<',';first=false;out<<"{\"id\":"<<hit.id<<",\"x\":"<<hit.x<<",\"y\":"<<hit.y<<",\"width\":"<<hit.width<<",\"height\":"<<hit.height<<'}';}
    out<<"]}";
}
std::string_view controlsSettingsGroupName(size_t group) noexcept{
    constexpr std::array<std::string_view,8> names{"ALL", "BODY", "OPTICS", "COMBAT", "ROCKET", "WRIST HUD", "MENUS", "MOVEMENT"};
    return group<names.size()?names[group]:names[0];
}
std::string_view controlsModeName(ControlsMode mode) noexcept{
    constexpr std::array<std::string_view,12> names{"ON FOOT","ACC","LAUNCH CABIN","DRIVING","HORSEBACK","BINOCULARS",
        "EQUIPMENT","COMMANDS","IDROID","MENUS","NATIVE BUTTONS","CUTSCENE"};
    const auto n=static_cast<size_t>(mode);return n<names.size()?names[n]:"UNKNOWN";
}
ControlContext controlsModeContext(ControlsMode mode) noexcept{
    switch(mode){case ControlsMode::driving:return ControlContext::vehicle;case ControlsMode::horse:return ControlContext::horse;
    case ControlsMode::binoculars:return ControlContext::binoculars;case ControlsMode::equipment:return ControlContext::equipment;
    case ControlsMode::commands:return ControlContext::commands;case ControlsMode::idroid:case ControlsMode::menus:case ControlsMode::title:return ControlContext::menus;
    case ControlsMode::nativeButtons:return ControlContext::nativeButtons;case ControlsMode::cinematic:return ControlContext::menus;default:return ControlContext::gameplay;}
}
std::string replaceControlBinding(std::string contents,std::string_view action,std::string_view binding){
    const auto dot=action.find('.');if(dot==std::string_view::npos)return contents;
    const std::string section(action.substr(0,dot)),key(action.substr(dot+1));
    const auto newline=contents.find("\r\n")!=std::string::npos?"\r\n":"\n";
    std::string current;size_t insert=std::string::npos;
    for(size_t at=0;at<contents.size();){const auto end=contents.find('\n',at);
        const size_t after=end==std::string::npos?contents.size():end+1;
        const auto line=trim(contents.substr(at,(end==std::string::npos?contents.size():end)-at));
        if(line.size()>2&&line.front()=='['&&line.back()==']'){
            if(current==section)insert=at;
            current=lower(trim(line.substr(1,line.size()-2)));if(current==section)insert=after;
        }else if(current==section){
            insert=after;const auto equal=line.find('=');
            if(equal!=std::string::npos&&lower(trim(line.substr(0,equal)))==key){
                const auto comment=line.find(';',equal+1);const auto suffix=comment==std::string::npos?std::string{}:" "+line.substr(comment);
                contents.replace(at,after-at,key+"="+std::string(binding)+suffix+newline);return contents;}}
        at=after;
    }
    const auto entry=key+"="+std::string(binding)+newline;
    if(insert!=std::string::npos){if(insert&&contents[insert-1]!='\n')contents.insert(insert,newline),insert+=std::char_traits<char>::length(newline);
        contents.insert(insert,entry);}
    else {if(!contents.empty()&&contents.back()!='\n')contents+=newline;contents+=newline+std::string("[")+section+"]"+newline+entry;}
    return contents;
}
bool saveControlBinding(const std::filesystem::path& path,const std::string& expected,const std::string& replacement,std::string& error){
    ControlBindings candidate;std::istringstream parsed(replacement);const auto errors=candidate.load(parsed);
    if(!errors.empty()){error=errors.front();return false;}
    const bool exists=std::filesystem::exists(path);
    HANDLE original=exists?CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr):INVALID_HANDLE_VALUE;
    if(exists&&original==INVALID_HANDLE_VALUE){error="Controls file is busy. Try again.";return false;}
    std::string actual;bool readOk=true;
    if(exists){LARGE_INTEGER size{};readOk=GetFileSizeEx(original,&size)&&size.QuadPart<=262144&&size.QuadPart>=0;
        if(readOk){actual.resize(static_cast<size_t>(size.QuadPart));DWORD read{};readOk=ReadFile(original,actual.data(),static_cast<DWORD>(actual.size()),&read,nullptr)&&read==actual.size();}}
    if(!readOk||actual!=expected){if(exists)CloseHandle(original);error="Controls changed elsewhere. Close and reopen to refresh.";return false;}
    const auto temporary=path.wstring()+L".vr-pending";
    HANDLE output=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(output==INVALID_HANDLE_VALUE){if(exists)CloseHandle(original);error="Could not stage controls. Your layout is unchanged.";return false;}
    DWORD written{};const bool staged=WriteFile(output,replacement.data(),static_cast<DWORD>(replacement.size()),&written,nullptr)
        &&written==replacement.size()&&FlushFileBuffers(output);CloseHandle(output);
    const auto backup=path.wstring()+L".vr-backup";
    const bool committed=staged&&(exists?ReplaceFileW(path.c_str(),temporary.c_str(),backup.c_str(),0,nullptr,nullptr)
        :MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_WRITE_THROUGH));
    if(exists)CloseHandle(original);
    if(!committed){DeleteFileW(temporary.c_str());error="Could not save controls. Your layout is unchanged.";}
    return committed;
}
std::vector<ControlsHit> controlsEditorLayout(const ControlsEditorFrame& f){
    std::vector<ControlsHit> hits{{100,.92f,.015f,.055f,.060f,"X"},
        {116,.35f,.105f,.075f,.050f,"CONTROLLER"},{117,.43f,.105f,.075f,.050f,"GEAR"},
        {104,.02f,.895f,.065f,.060f,"<"},{105,.37f,.895f,.065f,.060f,">"}};
    if(f.settings){hits.push_back({121,.02f,.105f,.05f,.050f,"<"});hits.push_back({122,.075f,.105f,.20f,.050f,controlsSettingsGroupName(f.settingsGroup)});hits.push_back({123,.28f,.105f,.05f,.050f,">"});}
    if(!f.settings){hits.push_back({101,.02f,.105f,.07f,.050f,"<"});hits.push_back({102,.095f,.105f,.16f,.050f,"AUTO"});hits.push_back({103,.26f,.105f,.07f,.050f,">"});}
    for(size_t n=0;n<11&&f.page*11+n<f.rows.size();++n)hits.push_back({static_cast<int>(n),.02f,.19f+static_cast<float>(n)*.061f,.42f,.056f,{}});
    if(!f.action.empty()){
        hits.push_back({110,.49f,.895f,.09f,.060f,"RESET"});hits.push_back({111,.59f,.895f,.09f,.060f,"OFF"});
        hits.push_back({112,.75f,.895f,.10f,.060f,"X"});hits.push_back({113,.86f,.895f,.10f,.060f,"CHECK"});
        if(f.settings){
            if(f.minimum==0&&f.maximum==1)hits.push_back({120,.68f,.49f,.10f,.09f,"TOGGLE"});
            else {hits.push_back({118,.55f,.49f,.09f,.09f,"-"});hits.push_back({119,.83f,.49f,.09f,.09f,"+"});}
            if(!(f.minimum==0&&f.maximum==1)&&f.action!="settings.turn_mode"&&f.action!="settings.hud_mode")
                std::erase_if(hits,[](const auto& hit){return hit.id==111;});
        }else {
            hits.push_back({106,.49f,.78f,.11f,.060f,"REC"});hits.push_back({107,.61f,.78f,.13f,.060f,"GESTURE"});
            hits.push_back({108,.75f,.78f,.09f,.060f,"+ ALT"});hits.push_back({109,.85f,.78f,.11f,.060f,"TOUCH"});
            if(f.gesture=="tap"||f.gesture=="hold"){
                hits.push_back({114,.76f,.72f,.06f,.045f,"-"});hits.push_back({115,.90f,.72f,.06f,.045f,"+"});}}}
    return hits;
}
void ControlsEditor::rows(const ControlBindings& bindings){
    frame_.rows.clear();const uint32_t context=1u<<static_cast<unsigned>(controlsModeContext(frame_.mode));
    if(frame_.settings){
        frame_.rows.push_back({"system.toggle_vr","Immersive VR",frame_.vrEnabled?"On":"Off",false});
        // Keep the common headset visibility choices on the first page, then
        // all remaining switches before the numeric fitting controls.
        constexpr std::array<std::string_view,10> first{"settings.god_mode","settings.full_body","settings.eyepatch",
            "settings.cover_assistance","settings.block_firing_near_cover","settings.tracked_weapon_obstruction",
            "settings.enemy_world_markers","settings.binocular_auto_mark","settings.binocular_actor_glow","settings.seated_mode"};
        for(const auto name:first)
            frame_.rows.push_back({std::string(name),title(name),bindings.setting(name)>0?"On":"Off",false});
        for(const bool toggles:{true,false})for(const auto& d:settingDefinitions()){
            const bool toggle=d.minimum==0&&d.maximum==1;
            if(toggle!=toggles||std::find(first.begin(),first.end(),d.name)!=first.end())continue;
            const auto label=toggle?(bindings.setting(d.name)>0?"On":"Off"):bindings.settingText(d.name);
            frame_.rows.push_back({std::string(d.name),title(d.name),label,false});}
        if(frame_.settingsGroup)std::erase_if(frame_.rows,[&](const auto& row){return settingsGroup(row.action)!=frame_.settingsGroup;});
        frame_.page=std::min(frame_.page,(frame_.rows.size()-1)/11);return;
    }
    for(const auto& d:controlDefinitions())if(d.contexts&context){
        if(frame_.mode==ControlsMode::acc&&d.name.starts_with("gameplay.")&&d.name!="gameplay.equip_binoculars")continue;
        if(frame_.mode==ControlsMode::idroid&&d.name.starts_with("menus.")&&d.name=="menus.previous_tab")continue;
        frame_.rows.push_back({std::string(d.name),title(d.name),bindings.label(d.name),false});}
    constexpr std::array<std::pair<std::string_view,uint32_t>,10> axes{{{"axes.move",1u|8u|32u},{"axes.turn",1u|8u|32u|64u},
        {"axes.equipment",2u},{"axes.commands",4u},{"axes.menu",16u},{"axes.map",16u},
        {"axes.vehicle_steering",64u},{"axes.vehicle_pedals",64u},{"axes.native_move",128u},{"axes.native_look",128u}}};
    for(const auto& [name,modes]:axes)if((modes&context)&&frame_.mode!=ControlsMode::acc)
        frame_.rows.push_back({std::string(name),title(name),bindings.label(name),true});
    if(frame_.mode==ControlsMode::cinematic)for(const auto name:{"axes.move","axes.turn"})
        frame_.rows.push_back({name,title(name),bindings.label(name),true});
    frame_.page=std::min(frame_.page,frame_.rows.empty()?0:(frame_.rows.size()-1)/11);
}
void ControlsEditor::select(size_t index,const ControlBindings& bindings){
    if(index>=frame_.rows.size())return;frame_.selected=index;frame_.action=frame_.rows[index].action;
    frame_.binding=editorValue(frame_,bindings);frame_.selectedInputs=frame_.settings?0:textMask(frame_.binding);
    frame_.proposed.clear();frame_.capturedInputs=0;frame_.phase=ControlsEditPhase::browse;frame_.status="Point and select REC, then use your controllers.";
    frame_.validProposal=false;
    if(frame_.settings){
        frame_.minimum=0;frame_.maximum=1;frame_.step=1;frame_.settingValue=frame_.vrEnabled?1.f:0.f;
        if(frame_.action!="system.toggle_vr")for(const auto& d:settingDefinitions())if(d.name==frame_.action){
            frame_.minimum=d.minimum;frame_.maximum=d.maximum;frame_.settingValue=bindings.setting(d.name);
            frame_.step=d.name.ends_with("_degrees")?5.f:d.name.ends_with("_ms")?50.f:d.name=="settings.wrist_text_scale"?.05f:
                d.name=="settings.rocket_arm_sensitivity"?.25f:d.name=="settings.enemy_marker_scale_percent"?5.f:1.f;break;}
        frame_.status="Adjust, then check to apply live. X cancels.";frame_.gesture.clear();return;
    }
    const auto open=frame_.binding.find('(');frame_.gesture=open==std::string::npos?"level":frame_.binding.substr(0,open);
    const auto comma=frame_.binding.find(',');if(comma!=std::string::npos)frame_.milliseconds=std::stoull(frame_.binding.substr(comma+1));
    sandbox_=bindings;sandbox_.suspend();matchHeld_=false;
}
void ControlsEditor::open(Pose head,ControlsMode mode,const ControlBindings& bindings,const std::filesystem::path& path,uint64_t epoch){
    frame_={};frame_.visible=frame_.ownsInput=true;frame_.mode=frame_.liveMode=mode;epoch_=epoch;path_=path;
    frame_.panel=compose(head,Pose{{},{0,-.06f,-1.15f}});
    baseline_.clear();std::ifstream file(path,std::ios::binary);if(file)baseline_.assign(std::istreambuf_iterator<char>(file),{});
    rows(bindings);if(!frame_.rows.empty())select(0,bindings);ready_=false;trigger_={};++frame_.revision;
}
void ControlsEditor::close() noexcept{frame_.visible=false;frame_.ownsInput=true;frame_.proposed.clear();replacement_.clear();vrRequest_.reset();ready_=false;++frame_.revision;}
void ControlsEditor::propose(std::string binding,const ControlBindings& bindings){
    if(frame_.settings&&frame_.action=="system.toggle_vr"){
        frame_.proposed=std::move(binding);frame_.phase=ControlsEditPhase::confirm;frame_.validProposal=true;
        frame_.status="Check to switch presentation. This panel stays available.";++frame_.revision;return;
    }
    if(!frame_.settings&&frame_.append&&!frame_.rows[frame_.selected].axis&&frame_.binding!="disabled"&&binding!="disabled")binding=frame_.binding+" | "+binding;
    replacement_=replaceControlBinding(baseline_,frame_.action,binding);
    ControlBindings candidate;std::istringstream stream(replacement_);const auto errors=candidate.load(stream);
    frame_.proposed=std::move(binding);frame_.capturedInputs=frame_.settings?0:textMask(frame_.proposed);frame_.validProposal=errors.empty();
    frame_.phase=ControlsEditPhase::confirm;
    frame_.status=errors.empty()?"Check to apply live. X keeps the current value.":errors.front();
    // Preview is decoded in isolation; it never sends game input.
    sandbox_=errors.empty()?std::move(candidate):bindings;sandbox_.suspend();++frame_.revision;
}
void ControlsEditor::command(int control,const ControlBindings& bindings){
    if(frame_.phase==ControlsEditPhase::applying&&control!=100)return;
    if(control>=0&&control<11){select(frame_.page*11+static_cast<size_t>(control),bindings);++frame_.revision;return;}
    switch(control){
    case 100:close();return;
    case 121:case 122:case 123:
        if(frame_.settings){frame_.settingsGroup=(frame_.settingsGroup+(control==121?7:1))%8;
            frame_.page=0;rows(bindings);select(0,bindings);}break;
    case 101:case 103:{frame_.automatic=false;const auto count=static_cast<int>(ControlsMode::count);
        frame_.mode=static_cast<ControlsMode>((static_cast<int>(frame_.mode)+(control==101?count-1:1))%count);
        frame_.page=0;rows(bindings);select(0,bindings);break;}
    case 102:frame_.automatic=true;frame_.mode=frame_.liveMode;frame_.page=0;rows(bindings);select(0,bindings);break;
    case 104:if(frame_.page)--frame_.page;break;
    case 105:if((frame_.page+1)*11<frame_.rows.size())++frame_.page;break;
    case 106:frame_.phase=ControlsEditPhase::release;frame_.status="Release buttons and center sticks to begin.";peak_=0;captureAt_=0;break;
    case 107:{constexpr std::array<std::string_view,5> types{"level","press","tap","hold","release"};
        const auto found=std::find(types.begin(),types.end(),frame_.gesture);frame_.gesture=types[(static_cast<size_t>(found-types.begin())+1)%types.size()];break;}
    case 108:frame_.append=!frame_.append;break;
    case 109:frame_.touches=!frame_.touches;break;
    case 110:{ControlBindings defaults;if(frame_.settings){frame_.settingValue=frame_.action=="system.toggle_vr"?1.f:defaults.setting(frame_.action);
            propose(settingValueText(frame_.action,frame_.settingValue),bindings);}else propose(defaults.bindingText(frame_.action),bindings);break;}
    case 111:if(frame_.settings){frame_.settingValue=frame_.maximum==2?2.f:0.f;propose(settingValueText(frame_.action,frame_.settingValue),bindings);}else propose("disabled",bindings);break;
    case 112:select(frame_.selected,bindings);break;
    case 113:
        if(frame_.phase==ControlsEditPhase::confirm&&frame_.validProposal&&frame_.settings&&frame_.action=="system.toggle_vr"){
            vrRequest_=frame_.proposed=="1";frame_.phase=ControlsEditPhase::applying;break;
        }
        if(frame_.phase==ControlsEditPhase::confirm&&frame_.validProposal&&!replacement_.empty()){
            std::string error;if(saveControlBinding(path_,baseline_,replacement_,error)){
                baseline_=replacement_;frame_.phase=ControlsEditPhase::applying;frame_.status="Saved. Release controls to apply live.";
            }else frame_.status=std::move(error);}
        break;
    case 114:frame_.milliseconds=std::max<uint64_t>(100,frame_.milliseconds-100);break;
    case 115:frame_.milliseconds=std::min<uint64_t>(3000,frame_.milliseconds+100);break;
    case 116:case 117:frame_.settings=control==117;frame_.page=0;rows(bindings);select(0,bindings);break;
    case 118:case 119:case 120:
        if(frame_.settings){
            frame_.settingValue=control==120?(frame_.settingValue==0?1.f:0.f):
                std::clamp(frame_.settingValue+(control==118?-frame_.step:frame_.step),frame_.minimum,frame_.maximum);
            propose(settingValueText(frame_.action,frame_.settingValue),bindings);
        }
        break;
    default:break;}
    ++frame_.revision;
}
void ControlsEditor::update(const PhysicalControls& physical,const std::array<Pose,2>& aims,
    const std::array<bool,2>& tracked,ControlsMode mode,const ControlBindings& bindings,uint64_t now,uint64_t epoch,bool vrEnabled){
    frame_.physical=physical;frame_.liveMode=mode;frame_.vrEnabled=vrEnabled;
    if(!frame_.visible){if(neutral(physical))frame_.ownsInput=false;return;}
    if(epoch!=epoch_||(!tracked[0]&&!tracked[1])){close();return;}
    if(frame_.automatic&&frame_.mode!=mode&&frame_.phase==ControlsEditPhase::browse){frame_.mode=mode;if(!frame_.settings){frame_.page=0;rows(bindings);select(0,bindings);}++frame_.revision;}
    if(frame_.phase==ControlsEditPhase::applying&&editorValue(frame_,bindings)==frame_.proposed){
        rows(bindings);select(frame_.selected,bindings);frame_.status="Applied live. No restart needed.";++frame_.revision;}
    if(!frame_.settings)sandbox_.update(physical,controlsModeContext(frame_.mode),now);
    const bool match=!frame_.settings&&sandbox_.active(frame_.action);frame_.matched=match;
    if(match&&!matchHeld_){frame_.status="Binding matched. Game controls remain held while editing.";++frame_.revision;}matchHeld_=match;
    const bool idle=neutral(physical,frame_.touches);
    if(frame_.phase==ControlsEditPhase::release){if(idle){frame_.phase=ControlsEditPhase::capture;frame_.status="Press your new button or chord, then release.";++frame_.revision;}return;}
    if(frame_.phase==ControlsEditPhase::capture){
        if(frame_.rows[frame_.selected].axis){
            if(std::max(std::abs(physical.leftStick[0]),std::abs(physical.leftStick[1]))>.7f)peak_=1;
            if(std::max(std::abs(physical.rightStick[0]),std::abs(physical.rightStick[1]))>.7f)peak_=2;
            if(peak_&&idle)propose(peak_==1?"left_stick":"right_stick",bindings);
        }else {const auto mask=physicalControlMask(physical,frame_.touches);
            if(mask&&std::popcount(mask)>std::popcount(peak_))peak_=mask;
            if(peak_&&!captureAt_)captureAt_=now;
            if(peak_&&idle){std::string binding;for(size_t n=0;n<controlInputNames().size();++n)if(peak_&(1u<<n)){
                    if(!binding.empty())binding+=" + ";binding+=controlInputNames()[n];}
                if(frame_.gesture!="level")binding=frame_.gesture+"("+binding+((frame_.gesture=="tap"||frame_.gesture=="hold")?","+std::to_string(frame_.milliseconds):"")+")";
                propose(std::move(binding),bindings);}
            else if(captureAt_&&now-captureAt_>10000){frame_.phase=ControlsEditPhase::release;frame_.status="Release controls; capture timed out.";peak_=0;captureAt_=0;}}
        return;
    }
    if(!ready_){if(idle)ready_=true;return;}
    frame_.hover=-1;frame_.cursorValid=false;
    const auto layout=controlsEditorLayout(frame_);
    for(size_t hand=0;hand<2;++hand){
        const bool pressed=physical.buttons[9+hand]>.5f;
        if(tracked[hand]){
            const Pose ray=compose(inverse(frame_.panel),aims[hand]);const auto direction=rotate(ray.orientation,{0,0,-1});
            if(direction.z<-.05f){const float distance=-ray.position.z/direction.z;
                if(distance>0&&distance<4){const auto point=ray.position+direction*distance;
                    const float x=.5f+point.x/1.2f,y=.5f-point.y/.75f;
                    if(x>=0&&x<=1&&y>=0&&y<=1){frame_.cursor={x,y};frame_.cursorValid=true;
                        for(const auto& hit:layout)if(x>=hit.x&&x<=hit.x+hit.width&&y>=hit.y&&y<=hit.y+hit.height){
                            frame_.hover=hit.id;if(pressed&&!trigger_[hand])command(hit.id,bindings);break;}}}}
        }
        trigger_[hand]=pressed;
    }
    if(now-lastAt_>=40){lastAt_=now;++frame_.revision;}
}
}
