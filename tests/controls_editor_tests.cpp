#include "mgs5vr/controls_editor.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace mgs5vr;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
    ControlBindings bindings;const std::string original="; personal settings\n[gameplay]\nequip_binoculars=hold(left_grip + y,300) ; keep this comment\n[settings]\nhand_rest_curl_percent=10\n";
    auto changed=replaceControlBinding(original,"gameplay.equip_binoculars","hold(left_grip + y,400)");
    check(changed.find("; keep this comment")!=std::string::npos&&changed.find("hand_rest_curl_percent=10")!=std::string::npos,"Patch damaged other settings/comments");
    ControlBindings parsed;std::istringstream stream(changed);check(parsed.load(stream).empty(),"Valid patch rejected");
    check(parsed.bindingText("gameplay.equip_binoculars")=="hold(y + left_grip,400)","Gesture timing lost");
    const auto temp=std::filesystem::temp_directory_path()/"mgs5vr-controls-editor-test.ini";
    std::filesystem::remove(temp);std::filesystem::remove(temp.wstring()+L".vr-backup");
    {std::ofstream file(temp,std::ios::binary);file<<original;}
    ControlsEditor editor;editor.open({},ControlsMode::acc,bindings,temp,1);
    const auto selected=std::find_if(editor.frame().rows.begin(),editor.frame().rows.end(),[](const auto& r){return r.action=="gameplay.equip_binoculars";});
    check(selected!=editor.frame().rows.end(),"ACC omitted its physical binocular equip action");
    while(editor.frame().page*11+11<=static_cast<size_t>(selected-editor.frame().rows.begin()))editor.command(105,bindings);
    editor.command(static_cast<int>((selected-editor.frame().rows.begin())%11),bindings);
    editor.command(115,bindings);editor.command(106,bindings);
    PhysicalControls p{};std::array<Pose,2> aims{};std::array<bool,2> tracked{true,true};
    p.buttons[10]=1;editor.update(p,aims,tracked,ControlsMode::acc,bindings,1,1);
    check(editor.frame().phase==ControlsEditPhase::release,"Initiating trigger was captured");
    p={};editor.update(p,aims,tracked,ControlsMode::acc,bindings,2,1);
    p.buttons[3]=p.buttons[7]=1;editor.update(p,aims,tracked,ControlsMode::acc,bindings,3,1);
    p={};editor.update(p,aims,tracked,ControlsMode::acc,bindings,4,1);
    check(editor.frame().phase==ControlsEditPhase::confirm&&editor.frame().proposed=="hold(y + left_grip,400)","Two-hand chord or hold time not retained");
    editor.command(113,bindings);check(editor.frame().phase==ControlsEditPhase::applying,"Confirmed binding did not save");
    std::string error;check(!saveControlBinding(temp,original,changed,error),"External/stale edit overwrote the current file");
    editor.close();p.buttons[10]=1;editor.update(p,aims,tracked,ControlsMode::acc,bindings,5,1);
    check(editor.ownsInput(),"Closing trigger leaked into gameplay");p={};editor.update(p,aims,tracked,ControlsMode::acc,bindings,6,1);check(!editor.ownsInput(),"Neutral release did not restore gameplay");
    editor.open({},ControlsMode::driving,bindings,temp,2);
    check(std::any_of(editor.frame().rows.begin(),editor.frame().rows.end(),[](const auto& r){return r.action=="vehicle.accelerate";}),"Driving omitted vehicle actions");
    check(std::none_of(editor.frame().rows.begin(),editor.frame().rows.end(),[](const auto& r){return r.action=="gameplay.dive";}),"Driving showed an on-foot action");
    check(controlsModeContext(ControlsMode::idroid)==ControlContext::menus&&controlsModeContext(ControlsMode::binoculars)==ControlContext::binoculars,"Mode/context routing disagreed");
    editor.command(117,bindings);
    check(editor.frame().settings&&editor.frame().rows.size()==settingDefinitions().size()+1,"Headset omitted a live VR setting");
    const auto chooseSetting=[&](std::string_view name){
        const auto& rows=editor.frame().rows;const auto row=std::find_if(rows.begin(),rows.end(),[&](const auto& r){return r.action==name;});
        check(row!=rows.end(),"Setting missing");const auto index=static_cast<size_t>(row-rows.begin());
        while(editor.frame().page<index/11)editor.command(105,bindings);
        while(editor.frame().page>index/11)editor.command(104,bindings);
        editor.command(static_cast<int>(index%11),bindings);
    };
    chooseSetting("settings.binocular_auto_mark");editor.command(120,bindings);
    check(editor.frame().proposed=="0"&&editor.frame().validProposal,"Boolean VR setting failed to toggle");
    editor.command(112,bindings);check(editor.frame().binding=="1"&&editor.frame().proposed.empty(),"Cancel changed live setting");
    chooseSetting("settings.turn_mode");editor.command(118,bindings);
    check(editor.frame().proposed=="snap"&&editor.frame().validProposal,"Enumerated turn setting lost parser syntax");
    editor.command(112,bindings);chooseSetting("settings.hand_touch_curl_percent");
    for(int n=0;n<13;++n)editor.command(118,bindings);
    check(!editor.frame().validProposal,"Coupled hand curl constraint accepted an invalid value");
    editor.command(113,bindings);check(editor.frame().phase==ControlsEditPhase::confirm,"Invalid setting was committed");
    editor.command(112,bindings);chooseSetting("settings.player_height_offset_cm");editor.command(119,bindings);editor.command(113,bindings);
    ControlBindings live;std::ifstream settingsFile(temp);check(live.load(settingsFile).empty()&&live.setting("settings.player_height_offset_cm")==1,"Numeric setting did not save");
    settingsFile.close();
    editor.update({},aims,tracked,ControlsMode::driving,live,10,2);
    check(editor.frame().phase==ControlsEditPhase::browse&&editor.frame().binding=="1","Setting was not acknowledged live");
    chooseSetting("system.toggle_vr");editor.command(120,live);editor.command(113,live);
    check(editor.takeVrRequest()==std::optional<bool>{false},"VR off request missing");
    editor.update({},aims,tracked,ControlsMode::driving,live,11,2,false);
    check(editor.visible()&&editor.frame().binding=="0","VR exit removed the settings panel");
    editor.command(120,live);editor.command(113,live);check(editor.takeVrRequest()==std::optional<bool>{true},"VR return request missing");
    editor.update({},aims,tracked,ControlsMode::driving,live,12,2,true);editor.command(116,live);
    check(!editor.frame().settings&&std::any_of(editor.frame().rows.begin(),editor.frame().rows.end(),[](const auto& row){return row.action=="vehicle.accelerate";}),"Settings damaged contextual mapping");
    // Menu and its stick modifier arrive in separate XR samples. Reserving
    // the completed chord must cancel the pending native iDroid tap.
    ControlBindings routed; routed.update({},ControlContext::gameplay,100);
    p={};p.buttons[4]=1;routed.update(p,ControlContext::gameplay,120);
    p.buttons[5]=1;check(routed.bindingChordHeld("system.controls",p),"Editor opening chord was not recognized");
    routed.requireRelease(routed.heldInputMask("system.controls",p));
    routed.update({},ControlContext::gameplay,150);check(!routed.active("system.idroid"),"Opening editor manufactured a Menu tap");
    std::filesystem::remove(temp);std::filesystem::remove(temp.wstring()+L".vr-backup");
    std::cout<<"Capture release, two-hand chords, context filtering, confirmed save, external-edit protection and close handoff passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
