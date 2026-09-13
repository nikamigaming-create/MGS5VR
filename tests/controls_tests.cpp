#include "mgs5vr/controls.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/wrist_selector.hpp"
#include "mgs5vr/game_target.hpp"
#include <windows.h>
#include <fstream>
#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>

using namespace mgs5vr;
namespace {
int failures{},checks{};
void expect(bool ok,const char* name){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<name<<'\n';}}
struct Fixture {
    ControlBindings controls;
    PhysicalControls input;
    ControlContext mode{ControlContext::gameplay};
    uint64_t time{100};
    Fixture(){tick();}
    void tick(uint64_t elapsed=11){time+=elapsed;controls.update(input,mode,time);}
    bool active(std::string_view action) const{return controls.active(action);}
    bool load(const char* source){std::istringstream file(source);return controls.load(file).empty();}
};
}
int main(int argc,char** argv){
    {
        const auto touch=controllerFaceLayout("/interaction_profiles/meta/touch_controller_plus");
        const auto simple=controllerFaceLayout("/interaction_profiles/khr/simple_controller");
        const auto vive=controllerFaceLayout("/interaction_profiles/htc/vive_controller");
        expect(touch==ControllerFaceLayout::standard&&simple==ControllerFaceLayout::simple&&vive==ControllerFaceLayout::triggerMenu,
            "active controller profile selects its own face layout");
        expect(isolatedFaceButtons({touch,touch},{},{true,true},{true,true})==std::array<bool,4>{},
            "Touch trigger/select fallback cannot synthesize A, B, X or Y");
        expect(isolatedFaceButtons({touch,touch},{true,false,true,false},{true,true},{true,true})==std::array<bool,4>{true,false,true,false},
            "real A/X remain usable with simultaneous trigger pulls");
        expect(isolatedFaceButtons({simple,simple},{},{true,true},{})==std::array<bool,4>{true,true,false,false},
            "simple-controller select keeps its declared confirm/back behavior");
        expect(isolatedFaceButtons({vive,vive},{true,true,true,true},{false,true},{false,true})==std::array<bool,4>{true,true,false,false},
            "legacy trigger/menu fallbacks are limited to their active layout");
        Fixture f;f.mode=ControlContext::menus;f.tick();f.input.buttons[10]=1;f.tick();
        expect(f.active("menus.right_trigger")&&!f.active("menus.confirm"),"firing in native fallback does not request stance/confirm");
    }
    {
        Fixture f;f.mode=ControlContext::menus;
        expect(f.load("[menus]\ndpad_up=press(right_stick_up)\ndpad_down=right_stick_down\n"),
            "native D-pad directions are configurable for adapter-independent gameplay");
        f.tick();f.input.rightStick[1]=1;f.tick();
        expect(f.active("menus.dpad_up")&&!f.active("gameplay.run"),
            "native D-pad does not also activate tracked running");
        f.tick(150);expect(!f.active("menus.dpad_up"),"configured D-pad press ends after its native-safe pulse");
        f.input.rightStick[1]=0;f.tick();f.input.rightStick[1]=-1;f.tick();
        expect(f.active("menus.dpad_down"),"native D-pad can be held to open native equipment cards");
    }
    {
        const auto* tpp=gameTarget(gameTargets[0].sha256);
        const auto* gz=gameTarget(gameTargets[1].sha256);
        expect(tpp&&tpp->nativeAdapter&&tpp->executable==L"mgsvtpp.exe","TPP keeps its own native adapter");
        expect(gz&&!gz->nativeAdapter&&gz->executable==L"MgsGroundZeroes.exe","GZ cannot use TPP native patches");
        expect(!gameTarget("unknown")&&!gameTarget(""),"unknown executables cannot enable a game adapter");
    }
    {
        Fixture f;f.input.buttons[7]=1;f.tick();f.input.buttons[1]=1;f.tick();f.tick(650);
        expect(f.active("gameplay.pickup_carry")&&!f.active("gameplay.equip_binoculars")&&!f.active("gameplay.reload"),
            "left grip plus B holds native pickup without binocular/reload leakage");
        f.tick(700);expect(f.active("gameplay.pickup_carry"),"pickup remains down beyond a one-shot pulse");
        f.input.buttons[7]=0;f.tick();f.input.buttons[1]=0;f.tick();
        expect(!f.active("gameplay.reload")&&!f.active("gameplay.equip_binoculars"),"pickup chord release consumes simpler B actions");
        f.input.buttons[8]=1;f.tick();f.input.buttons[0]=1;f.tick();
        expect(f.active("gameplay.switch_weapon")&&f.active("gameplay.ready_weapon")&&!f.active("gameplay.dive"),
            "A requests weapon switch while right grip readies the weapon");
    }
    {
        GamepadSample pad{0,255,255,1234,4321};bool ready=true;
        applyOnFootActions(pad,ready,{false,false,true});
        expect((pad.buttons&0x4000)&&!pad.leftTrigger&&!pad.rightTrigger&&!ready&&pad.leftX==1234&&pad.leftY==4321,
            "dive lowers aim and attack while preserving travel direction");
        pad={0,255,255};ready=true;applyOnFootActions(pad,ready,{false,false,false,true});
        expect((pad.buttons&0x2000)&&!pad.leftTrigger&&!pad.rightTrigger&&!ready,"pickup/carry holds native B with weapon lowered");
        pad={0,255,0};ready=true;applyOnFootActions(pad,ready,{false,false,false,false,true});
        expect((pad.buttons&0x4000)&&pad.leftTrigger==255&&ready,"weapon-switch retains aim");
        pad={};ready=false;applyOnFootActions(pad,ready,{false,false,false,false,true});
        expect(!(pad.buttons&0x4000),"weapon-switch without ready cannot become a dive");
        pad={};ready=false;applyOnFootActions(pad,ready,{false,true,true});
        expect((pad.buttons&0x5000)==0x4000,"simultaneous stance and dive chooses dive only");
        RigInput rig;rig.update({},false,false,TravelMode::onFoot,100);
        rig.requireAttackRelease();const auto held=rig.update({0,0,255},false,true,TravelMode::onFoot,110);
        expect(!held.gamepad.rightTrigger,"action completion cannot fire a still-held trigger");
        rig.update({},false,true,TravelMode::onFoot,120);
        expect(rig.update({0,0,255},false,true,TravelMode::onFoot,130).gamepad.rightTrigger==255,
            "release and a fresh trigger restore shooting");
    }
    {
        ControlBindings controls;EquipmentLabels labels{};
        constexpr std::array<std::string_view,4> actions{"equipment.primary","equipment.secondary","equipment.support","equipment.items"};
        for(size_t i=0;i<actions.size();++i){const auto label=controls.label(actions[i]);strcpy_s(labels[i].data(),labels[i].size(),label.c_str());}
        const auto preview=makeWristSelectorImage(labels);
        expect(preview.width==1440&&preview.height==320&&preview.bgra.size()==1440*320,"four-category bar has a complete readable image");
        expect(controls.label("equipment.primary")=="R STICK UP"&&controls.label("equipment.items")=="R STICK LEFT",
            "selector captions come from the active bindings");
        for(size_t n=0;n<4;++n){
            unsigned lightPixels{};
            for(size_t y=87;y<145;++y)for(size_t x=16+n*356;x<356+n*356;++x)
                if((preview.bgra[y*1440+x]&0x00ffffffu)>0x00b0b0b0u)++lightPixels;
            expect(lightPixels>500,"every category has visible title pixels");
        }
        if(argc==3&&std::string_view(argv[1])=="--selector-image"){
            BITMAPFILEHEADER file{};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);
            file.bfSize=file.bfOffBits+static_cast<DWORD>(preview.bgra.size()*4);
            BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=static_cast<LONG>(preview.width);
            info.biHeight=-static_cast<LONG>(preview.height);info.biPlanes=1;info.biBitCount=32;
            std::ofstream out(argv[2],std::ios::binary);out.write(reinterpret_cast<const char*>(&file),sizeof(file));
            out.write(reinterpret_cast<const char*>(&info),sizeof(info));
            out.write(reinterpret_cast<const char*>(preview.bgra.data()),static_cast<std::streamsize>(preview.bgra.size()*4));
            expect(bool(out),"selector image written");
        }
    }
    {
        bool selected=updateBinocularSelection(false,true,false,true);
        expect(selected,"equip input latches without requiring any hand pose");
        for(unsigned frame=0;frame<180;++frame)selected=updateBinocularSelection(selected,false,false,true);
        expect(selected,"no input or tracking-only interruption cannot cancel the selection");
        expect(!updateBinocularSelection(selected,false,true,true),"explicit stow clears the latch");
        expect(!updateBinocularSelection(selected,false,false,false),"leaving the supported gameplay context clears the latch");
    }
    {
        Fixture f;
        expect(f.load("; defaults\n"),"defaults have no conflicts");f.tick();
        f.input.buttons[1]=1;f.tick();
        expect(!f.active("gameplay.reload")&&!f.active("gameplay.equip_binoculars"),"B press waits to distinguish tap and hold");
        f.input.buttons[1]=0;f.tick(150);
        expect(f.active("gameplay.reload")&&!f.active("gameplay.equip_binoculars"),"short B release reloads only");
        f.tick(101);f.input.buttons[1]=1;f.tick();f.tick(300);
        expect(f.active("gameplay.equip_binoculars")&&!f.active("gameplay.reload"),"holding B equips without reload");
        f.tick(200);
        expect(!f.active("gameplay.equip_binoculars"),"a long hold fires once");
        f.mode=ControlContext::binoculars;f.tick();
        expect(!f.active("binoculars.stow"),"equip hold cannot immediately stow in the new mode");
        f.input.buttons[1]=0;f.tick();f.tick(101);
        expect(!f.active("binoculars.stow")&&!f.active("gameplay.reload"),"equip release does not stow or reload");
        f.input.buttons[1]=1;f.tick();
        expect(f.active("binoculars.stow"),"a fresh B press stows");
        f.mode=ControlContext::gameplay;f.tick();f.tick(400);f.input.buttons[1]=0;f.tick();
        expect(!f.active("gameplay.equip_binoculars")&&!f.active("gameplay.reload"),"stow input cannot leak into gameplay");
    }
    {
        Fixture f;f.input.buttons[7]=1;f.tick();f.input.buttons[4]=1;f.tick();
        expect(f.active("system.recenter")&&!f.active("system.idroid"),"grip plus Menu recenters without opening iDroid");
        f.tick(600);
        expect(!f.active("system.pause"),"recenter chord cannot also pause");
        f.input.buttons[7]=0;f.tick();f.input.buttons[4]=0;f.tick();
        expect(!f.active("system.idroid")&&!f.active("system.pause"),"releasing a chord consumes the simpler tap");
        f.input.buttons[7]=1;f.input.buttons[6]=1;f.tick();
        expect(f.active("gameplay.dive")&&!f.active("system.toggle_vr"),"right click dives without a hidden grip toggle");
        f.input.buttons[0]=1;f.input.buttons[2]=1;f.tick();
        expect(!f.active("gameplay.native_a")&&!f.active("gameplay.native_x"),"face buttons no longer duplicate stance or dive");
    }
    {
        Fixture f;f.input.buttons[9]=1;f.tick();f.mode=ControlContext::equipment;f.tick();
        expect(f.active("equipment.open"),"picker modifier persists across opening");
        expect(!f.active("equipment.primary"),"opening wrist waits without selecting a category");
        f.input.rightStick={0,1};f.tick();
        expect(f.active("equipment.primary")&&!f.active("gameplay.run"),"picker navigation does not sprint");
        f.input.buttons[2]=1;f.tick();
        expect(f.active("commands.open")&&f.active("commands.keep_open"),"X opens commands while preserving its hold");
        f.mode=ControlContext::commands;f.tick();
        expect(f.active("commands.keep_open")&&!f.active("equipment.primary"),"commands own navigation after opening");
        f.input.buttons[2]=0;f.tick();
        expect(!f.active("commands.keep_open"),"release closes commands");
    }
    {
        Fixture f;f.input.rightStick={0,1};f.tick();
        expect(f.active("gameplay.run"),"right stick up runs");
        f.mode=ControlContext::equipment;f.tick();
        expect(!f.active("equipment.primary"),"held run direction cannot select when the wrist opens");
        f.input.rightStick={};f.tick();f.input.rightStick={0,-1};f.tick();
        expect(f.active("equipment.secondary"),"fresh wrist direction selects");
        f.mode=ControlContext::gameplay;f.tick();
        expect(!f.active("gameplay.stance"),"held picker direction cannot go prone on closing");
        f.input.rightStick={};f.tick();f.input.rightStick={.8f,1};f.tick();
        expect(f.active("gameplay.run")&&!f.active("turn.right"),"diagonal stick does not run and snap simultaneously");
        f.input.rightStick={1,0};f.tick();
        expect(!f.active("turn.right"),"direction changes require centering");
        f.input.rightStick={};f.tick();f.input.rightStick={1,0};f.tick();
        expect(f.active("turn.right"),"fresh lateral flick snap turns");
    }
    {
        Fixture f;f.input.buttons[10]=1;f.tick();f.controls.suspend();f.tick();
        expect(!f.active("gameplay.fire_or_cqc"),"focus return cannot fire a held trigger");
        f.input.buttons[10]=0;f.tick();f.input.buttons[10]=.2f;f.tick();
        expect(std::abs(f.controls.value("gameplay.fire_or_cqc")-.2f)<.001f,"single triggers preserve analog travel");
        f.controls.suspend();f.tick();f.tick();
        expect(f.controls.value("gameplay.fire_or_cqc")==0,"partially held triggers also require release after lost focus");
        f.mode=ControlContext::vehicle;f.input={};f.tick();
        f.input.buttons[10]=.25f;f.input.buttons[9]=.35f;f.tick();
        expect(std::abs(f.controls.value("vehicle.accelerate")-.25f)<.001f
            &&std::abs(f.controls.value("vehicle.brake_reverse")-.35f)<.001f,"vehicle pedals retain proportional input");
        f.input.buttons[8]=1;f.tick();f.mode=ControlContext::equipment;f.tick();
        expect(f.active("vehicle.equipment_open"),"vehicle picker remains open while grip is held");
    }
    {
        Fixture f;
        expect(f.load("[gameplay]\ndive=press(left_grip + x)\n[axes]\nmove=right_stick\n[settings]\nmotion_melee=0\n"),"chords, axes and gesture switches are editable");
        f.tick();f.input.buttons[7]=1;f.input.buttons[2]=1;f.tick();
        expect(f.active("gameplay.dive"),"remapped dive executes");
        f.input.rightStick={.3f,-.5f};
        expect(f.controls.axis("axes.move",f.input)==f.input.rightStick&&f.controls.setting("settings.motion_melee")==0,
            "axis and gesture settings apply");
        expect(!f.load("[gameplay]\ndive=press(y)\n"),"same-mode conflict is rejected");
        f.input={};f.tick();f.input.buttons[7]=1;f.input.buttons[2]=1;f.tick();
        expect(f.active("gameplay.dive"),"invalid edits preserve the complete previous config");
        expect(!f.load("[gameplay]\nreload=tap(b,200)\n"),"mismatched tap and hold durations are rejected");
        expect(!f.load("[gameplay]\nunknown=a\n"),"unknown action is reported");
        expect(!f.load("[gameplay]\ndive=press(left_grip+)\n"),"malformed chord is reported");
        expect(!f.load("[settings]\nsnap_turn_degrees=nan\n"),"nonfinite settings are rejected");
    }
    {
        Fixture f;
        expect(f.load("[gameplay]\ndive=release(x)\n[commands]\nopen=disabled\nkeep_open=disabled\n"),"release gestures are supported");f.tick();
        f.input.buttons[2]=1;f.tick();expect(!f.active("gameplay.dive"),"release action waits");
        f.input.buttons[2]=0;f.tick();expect(f.active("gameplay.dive"),"release action fires on release");
        f.tick(101);expect(!f.active("gameplay.dive"),"release pulse ends");
        f.input.buttons[10]=std::numeric_limits<float>::quiet_NaN();f.input.leftStick={INFINITY,0};f.tick();
        expect(!f.active("gameplay.fire_or_cqc")&&f.controls.axis("axes.move",f.input)[0]==0,"invalid analog input is neutralized");
    }
    {
        Fixture f;f.input.buttons[7]=1;f.tick();
        expect(f.active("gameplay.support_grip")&&!f.active("gameplay.reload"),"left grip supports without reloading in empty space");
        f.input.buttons[2]=1;f.tick();
        expect(f.active("commands.open")&&f.active("commands.keep_open"),"Commands opens without a trigger chord on foot");
        f.input={};f.mode=ControlContext::binoculars;f.tick();
        f.input.buttons[5]=1;f.tick();
        expect(f.active("binoculars.zoom")&&!f.active("binoculars.dive"),"binocular left click only zooms");
        f.input={};f.tick(101);f.input.buttons[6]=1;f.tick();
        expect(f.active("binoculars.dive")&&!f.active("binoculars.zoom"),"binocular right click only dives");
        f.input={};f.tick(101);f.input.buttons[0]=1;f.tick();
        expect(f.active("binoculars.clear_mark")&&!f.active("binoculars.mark"),"binocular A clears, never marks");
        f.input={};f.mode=ControlContext::horse;f.tick();
        f.input.buttons[9]=1;f.tick();f.input.buttons[2]=1;f.tick();
        expect(f.active("commands.mounted_open")&&!f.active("horse.gallop"),"mounted command chord consumes gallop");
    }
    std::cout<<checks<<" controls checks, "<<failures<<" failures\n";return failures?1:0;
}
