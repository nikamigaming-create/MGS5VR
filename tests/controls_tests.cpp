#include "mgs5vr/controls.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/native_controls.hpp"
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
struct NativeFixture {
    ControlBindings controls;
    NativeControls native;
    PhysicalControls input;
    NativeControlSample output;
    ControlContext normalContext{ControlContext::gameplay};
    uint64_t time{100};
    NativeFixture(){tick();}
    void tick(uint64_t elapsed=11){
        time+=elapsed;
        controls.update(input,native.selected()?ControlContext::nativeButtons:normalContext,time);
        output=native.update(controls,input);
        if(output.changed)controls.suspend();
    }
    void neutral(){input={};tick();tick();}
    void enter(){input.buttons[4]=input.buttons[0]=1;tick();tick(550);neutral();}
};
}
int main(int argc,char** argv){
    {
        IdroidBackRecovery recovery;
        expect(!recovery.update(true,true,true,100)&&!recovery.update(true,true,true,1000),
            "held Back at menu entry cannot dismiss a newly opened iDroid");
        recovery.update(true,false,true,1010);
        expect(!recovery.update(true,true,true,1020)&&!recovery.update(true,true,true,1200),
            "ordinary Back taps remain native submenu navigation");
        recovery.update(true,false,true,1210);
        recovery.update(true,true,true,1220);
        expect(!recovery.update(true,true,true,1969)&&recovery.update(true,true,true,1970)
            &&!recovery.update(true,true,true,2200),"held Back requests one native recovery after 750 ms");
        recovery.update(false,true,true,2210);recovery.update(true,true,true,2400);
        expect(!recovery.update(true,true,true,3400),"holding Back across close/reopen cannot close the new terminal");
        recovery.update(true,false,true,3410);recovery.update(true,true,true,3420);
        recovery.update(true,true,false,3800);recovery.update(true,true,true,4000);
        expect(!recovery.update(true,true,true,5000),"focus loss cancels recovery and requires fresh Back");
        recovery.update(true,false,true,5010);recovery.update(true,true,true,5020);
        expect(!recovery.update(true,true,true,50)&&!recovery.update(true,true,true,900),
            "clock reset cannot manufacture a held-Back recovery");
        Fixture original;original.mode=ControlContext::menus;original.tick();original.tick();
        original.input.buttons[1]=1;original.tick();
        expect(original.active("menus.back"),
            "original right B still sends native Back without a new binding");
        original.input={};original.tick();original.input.buttons[4]=1;original.tick();
        original.input={};original.tick(120);
        expect(original.active("system.idroid")&&!original.active("system.pause"),
            "original left Menu tap still opens iDroid");
    }
    {
        GamepadSample menuInput{};
        menuInput.leftX=10000;menuInput.leftY=26000;
        menuInput.rightX=-13000;menuInput.rightY=19000;
        menuInput.buttons=0xffff;menuInput.leftTrigger=menuInput.rightTrigger=255;
        expect(cabinTitleGamepad(menuInput,true,false)==GamepadSample{},
            "walking and pressing buttons at the spatial rack cannot navigate or confirm the hidden title menu");
        GamepadSample tapeConfirm{};tapeConfirm.buttons=0x1000;
        expect(cabinTitleGamepad(menuInput,true,true)==tapeConfirm,
            "selecting Continue publishes only its native confirm while both sticks are held");
        expect(cabinTitleGamepad(menuInput,false,false)==menuInput,
            "ordinary menus and scripted gameplay keep their existing input outside the spatial rack");
    }
    {
        ControlBindings controls;LiveControls live;PhysicalControls physical;
        std::istringstream file("[settings]\nturn_mode=snap\n");
        expect(live.stage(file).empty()&&live.pending(),"valid complete edit waits away from active bindings");
        physical.buttons[10]=1;
        expect(!live.apply(controls,physical)&&controls.setting("settings.turn_mode")==1,"live edit cannot interrupt a held trigger");
        physical={};physical.leftStick={.2f,0};
        expect(!live.apply(controls,physical),"live edit waits for left stick neutral");
        physical={};physical.rightStick={0,-.2f};
        expect(!live.apply(controls,physical),"live edit waits for right stick neutral");
        physical={};physical.buttons[7]=.1f;
        expect(!live.apply(controls,physical),"live edit waits for analog grip release below action threshold");
        physical={};physical.buttons[0]=std::numeric_limits<float>::quiet_NaN();
        expect(!live.apply(controls,physical),"invalid physical state cannot accept a live edit");
        physical={};
        expect(live.apply(controls,physical)&&!live.pending()&&controls.setting("settings.turn_mode")==0,"neutral input atomically applies an explicit snap-turn selection");
        controls.update(physical,ControlContext::gameplay,100);
        expect(!controls.active("gameplay.fire_or_cqc")&&!controls.active("system.pause"),"live replacement cannot manufacture an action");
        physical.buttons[10]=1;controls.update(physical,ControlContext::gameplay,111);
        expect(controls.active("gameplay.fire_or_cqc"),"fresh press works immediately after live replacement");
        std::istringstream valid("[settings]\nturn_mode=off\n"),invalid("[gameplay]\ndive=press(y)\n");
        expect(live.stage(valid).empty()&&!live.stage(invalid).empty()&&!live.pending(),"new invalid edit cancels an older queued valid edit");
        expect(!live.apply(controls,{})&&controls.setting("settings.turn_mode")==0,"invalid live edit preserves the last working layout instead of defaults");
        std::istringstream defaults("; remove overrides\n");
        expect(live.stage(defaults).empty()&&live.apply(controls,{})&&controls.setting("settings.turn_mode")==1,"deleting an override restores smooth turning without enabling snap");
    }
    {
        Fixture f;
        expect(f.controls.setting("settings.hud_mode")==1,"binocular-only recon is the default");
        expect(f.controls.setting("settings.binocular_pitch_degrees")==-90,"binoculars tilt down ninety degrees into the palm by default");
        expect(f.controls.setting("settings.binocular_auto_mark")==1&&f.controls.setting("settings.binocular_mark_dwell_ms")==650,
            "binocular observation acquires visible people with a configurable dwell");
        expect(f.controls.setting("settings.binocular_actor_glow")==1,"native recon body glow is enabled within the selected HUD view by default");
        expect(f.load("[settings]\nbinocular_actor_glow=0\n")&&f.controls.setting("settings.binocular_actor_glow")==0,
            "native recon body glow can be disabled independently of target acquisition");
        expect(!f.load("[settings]\nbinocular_actor_glow=2\n")&&f.controls.setting("settings.binocular_actor_glow")==0,
            "invalid recon glow setting preserves the last working preference");
        expect(f.load("[settings]\nbinocular_auto_mark=0\nbinocular_mark_dwell_ms=1200\n")
            &&f.controls.setting("settings.binocular_auto_mark")==0&&f.controls.setting("settings.binocular_mark_dwell_ms")==1200,
            "automatic binocular marking can be disabled and retimed");
        expect(!f.load("[settings]\nbinocular_mark_dwell_ms=0\n"),"automatic dwell cannot become an accidental instant mark");
        expect(f.load("[settings]\nhud_mode=binoculars_only\n")&&f.controls.setting("settings.hud_mode")==1,
            "binocular-only world HUD is configurable");
        expect(f.load("[settings]\nhud_mode=off\n")&&f.controls.setting("settings.hud_mode")==2,
            "world HUD can be disabled independently of wrist controls");
        expect(!f.load("[settings]\nhud_mode=guess\n")&&f.controls.setting("settings.hud_mode")==2,
            "invalid HUD preference keeps the previous configuration");
    }
    {
        Fixture f;
        expect(f.load("[gameplay]\nrun=left_stick_click\nstance=a\nreload=tap(left_grip + b,300)\npickup_carry=b\nswitch_weapon=press(right_grip + right_stick_click)\nzoom=press(right_stick_up)\nequip_binoculars=hold(left_grip + y,300)\n"),
            "Quest3 tester direct-B pickup, A stance and grip-R3 switch layout is valid");
        f.tick();f.input.buttons[1]=1;f.tick();f.tick(1000);
        expect(f.active("gameplay.pickup_carry")&&!f.active("gameplay.reload")&&!f.active("gameplay.equip_binoculars"),
            "tester direct B retains native pickup hold without reload or binoculars");
        f.input={};f.tick();f.input.buttons[8]=1;f.tick();f.input.buttons[6]=1;f.tick();
        expect(f.active("gameplay.switch_weapon")&&!f.active("gameplay.dive"),"tester ready-grip R3 switch consumes Dive");
        expect(f.load("[gameplay]\nswitch_weapon=press(right_grip + x)\n"),
            "X is not forbidden in gameplay chords when Commands opens on bare X");
        f.input={};f.tick();f.input.buttons[8]=1;f.tick();f.input.buttons[2]=1;f.tick();
        expect(f.active("gameplay.switch_weapon")&&!f.active("commands.open"),
            "a gameplay X chord suppresses Commands opening in its actual gameplay context");
    }
    {
        Fixture f;f.mode=ControlContext::gameplay;f.tick();
        f.input.rightStick={0,1};f.tick();
        expect(f.active("gameplay.zoom")&&!f.active("gameplay.run"),"right-stick up is the native weapon-scope zoom");
        f.input={};f.tick();f.input.buttons[5]=1;f.tick();f.tick(700);
        expect(f.active("gameplay.run")&&!f.active("gameplay.zoom"),"left-stick click holds native sprint without zoom");
        f.input={};f.tick();f.mode=ControlContext::binoculars;f.tick();
        f.input.rightStick={0,1};f.tick();
        expect(f.active("binoculars.run")&&!f.active("binoculars.zoom"),"right-stick up runs with binoculars without zoom");
        f.input={};f.tick();f.input.buttons[5]=1;f.tick();
        expect(f.active("binoculars.zoom")&&!f.active("binoculars.run"),"binocular left-stick click zooms without sprinting");
        expect(f.controls.setting("settings.turn_mode")==1,"snap turning is opt-in; the default uses native smooth turning");
        expect(f.load("[settings]\nturn_mode=snap\n")&&f.controls.setting("settings.turn_mode")==0,
            "snap turning remains an explicit selectable option");
        expect(f.load("[settings]\nturn_mode=native_smooth\n")&&f.controls.setting("settings.turn_mode")==1,
            "native smooth turn has a readable config name");
        expect(f.load("[settings]\nturn_mode=off\n")&&f.controls.setting("settings.turn_mode")==2,
            "stick turning can be disabled entirely");
        expect(!f.load("[settings]\nturn_mode=guess\n"),"invalid turning mode is explained instead of guessed");
        NativeSmoothTurn smooth;
        expect(!smooth.update(1,0,true),"held stick cannot turn on mode entry");
        smooth.update(0,0,true);
        expect(smooth.update(.5f,0,true)==16383&&smooth.update(.5f,0,true)==16383
            &&smooth.update(-1,0,true)==-32767,"native smooth yaw stays analog and continuous in both directions");
        expect(!smooth.update(.5f,-1,true)&&!smooth.update(.5f,0,true),"stance gesture consumes turning until centered");
        smooth.update(0,0,true);expect(!smooth.update(1,0,false)&&!smooth.update(1,0,true),"menu/focus transition cannot leak smooth turning");
    }
    {
        uint16_t allButtons{};
        for(const auto& button:nativeButtonDefinitions()){
            expect((allButtons&button.mask)==0&&!button.binding.empty()&&button.binding!="disabled",
                "every native button has one distinct non-disabled default mapping");
            allButtons|=button.mask;
        }
        expect(allButtons==0xf3ff,"all fourteen game-facing XInput button bits are covered");
        for(const auto context:{ControlContext::gameplay,ControlContext::equipment,ControlContext::commands,
            ControlContext::binoculars,ControlContext::menus,ControlContext::horse,ControlContext::vehicle}){
            NativeFixture f;f.normalContext=context;f.tick();f.enter();
            expect(f.native.selected()&&f.output.exclusive,"complete native buttons can be entered from every VR/game context");
            expect(!f.controls.active("gameplay.fire_or_cqc")&&!f.controls.active("equipment.open")
                &&!f.controls.active("commands.open")&&!f.controls.active("system.pause"),
                "native button mode has exclusive ownership instead of duplicate VR actions");
        }
    }
    {
        NativeFixture f;f.enter();
        struct ButtonCase {unsigned physical;uint16_t expected;};
        const std::array<ButtonCase,8> buttons{{
            {0,0x1000},{1,0x2000},{2,0x4000},{3,0x8000},{7,0x0100},{8,0x0200},{5,0x0040},{6,0x0080}}};
        for(const auto [physical,expected]:buttons){
            f.neutral();f.input.buttons[physical]=1;f.tick();
            expect(f.output.gamepad.buttons==expected,"default native face/shoulder/click sends its exact native bit");
            f.tick(1200);
            expect(f.output.gamepad.buttons==expected,"native button holds are not shortened into reload/zoom/menu pulses");
            f.neutral();expect(f.output.gamepad.buttons==0,"native button release reaches the game");
        }
        for(const auto [grip,expected]:std::array<ButtonCase,2>{{{7,0x0010},{8,0x0020}}}){
            f.neutral();f.input.buttons[4]=1;f.tick();f.input.buttons[grip]=1;f.tick();f.tick(1200);
            expect(f.output.gamepad.buttons==expected,"native Start/Back hold has no shoulder or system-menu duplicate");
        }
        f.neutral();f.input.buttons[9]=.35f;f.input.buttons[10]=.8f;
        f.input.leftStick={.4f,-.6f};f.input.rightStick={-.7f,.25f};f.tick();
        expect(f.output.gamepad.leftTrigger==89&&f.output.gamepad.rightTrigger==204,
            "both native triggers retain partial pressure for native CQC/charge/pedals");
        expect(f.output.gamepad.leftX==13106&&f.output.gamepad.leftY==-19660
            &&f.output.gamepad.rightX==-22936&&f.output.gamepad.rightY==8191,
            "both native sticks retain all four signed analog axes without VR turning interception");
        f.input.buttons[7]=f.input.buttons[6]=1;f.tick();f.tick(1400);
        expect(f.output.gamepad.buttons==0x0180&&f.output.gamepad.leftTrigger==89&&f.output.gamepad.rightTrigger==204,
            "native CQC/aim, Call and R3 confirmation can be held together independently");
    }
    {
        NativeFixture f;f.enter();
        struct Direction {float x,y;uint16_t expected;};
        for(const auto direction:std::array<Direction,4>{{{0,1,1},{0,-1,2},{-1,0,4},{1,0,8}}}){
            f.neutral();f.input.buttons[4]=1;f.tick();
            expect(!f.output.gamepad.buttons,"native D-pad modifier alone selects nothing");
            f.input.rightStick={direction.x,direction.y};f.tick();
            expect(f.output.gamepad.buttons==direction.expected&&f.output.gamepad.rightX==0&&f.output.gamepad.rightY==0,
                "native D-pad chooses the requested category without moving the native camera");
            f.input.rightStick={};f.tick();f.input.rightStick={.5f,-.5f};f.tick();
            expect(f.output.gamepad.buttons==direction.expected&&f.output.gamepad.rightX==16383&&f.output.gamepad.rightY==-16383,
                "native D-pad remains held while the independently centered right stick browses cards");
            f.tick(1700);expect(f.output.gamepad.buttons==direction.expected,"D-pad hold lasts as long as its modifier");
            f.neutral();expect(!f.output.gamepad.buttons,"releasing native D-pad modifier releases the category");
        }
        f.input.rightStick={1,0};f.tick();f.input.buttons[4]=1;f.tick();
        expect(!f.output.gamepad.buttons&&!f.output.gamepad.rightX,"held camera direction cannot preselect a native D-pad category");
    }
    {
        NativeFixture f;
        f.input.buttons[4]=f.input.buttons[0]=f.input.buttons[10]=1;f.tick();f.tick(550);
        expect(f.output.changed&&f.native.selected()&&f.output.gamepad==GamepadSample{},"mode entry releases the game before routing native buttons");
        f.input.buttons[4]=f.input.buttons[0]=0;f.tick();f.tick(500);
        expect(f.output.gamepad==GamepadSample{},"held trigger cannot fire when the input layout changes");
        f.neutral();f.input.buttons[10]=1;f.tick();expect(f.output.gamepad.rightTrigger==255,"fresh native trigger works after mode-entry release");
        f.native.suspend();f.controls.suspend();f.tick();
        expect(f.native.selected()&&f.output.gamepad==GamepadSample{},"focus loss keeps chosen layout but releases every native input");
        f.neutral();f.input.buttons[4]=f.input.buttons[0]=1;f.tick();f.tick(550);
        expect(!f.native.selected()&&f.output.exclusive&&f.output.gamepad==GamepadSample{},"mode exit consumes the toggle instead of leaking normal quick-switch");
        f.neutral();expect(!f.output.exclusive,"normal VR layout resumes after neutral mode exit");
    }
    {
        NativeFixture f;
        std::istringstream config("[native]\ndpad_hold=disabled\ndpad_up=y\ny=menu + right_stick_up\n[axes]\nnative_move=right_stick\nnative_look=left_stick\n");
        expect(f.controls.load(config).empty(),"native buttons and both stick roles are configurable without source edits");
        f.tick();f.enter();f.input.buttons[3]=1;f.input.leftStick={.2f,.3f};f.input.rightStick={-.4f,-.5f};f.tick();
        expect(f.output.gamepad.buttons==1&&f.output.gamepad.leftX==-13106&&f.output.gamepad.rightX==6553,
            "direct remapped D-pad works without its default hold modifier and both remapped axes apply");
        f.input.buttons[3]=0;f.tick();expect(!f.output.gamepad.buttons,"direct D-pad release is not latched without its modifier");
    }
    {
        Fixture f;
        expect(f.load("[gameplay]\nnative_dpad_up=press(right_grip + x)\n"),"direct native weapon actions accept a deliberate chord");
        f.tick();f.input.buttons[8]=1;f.tick();f.input.buttons[2]=1;f.tick();
        expect(f.active("gameplay.native_dpad_up")&&f.active("gameplay.ready_weapon")&&!f.active("commands.open"),
            "native D-pad chord keeps weapon ready without opening Commands");
        f.tick(101);expect(!f.active("gameplay.native_dpad_up"),"direct native action has one bounded press");
        f.input.buttons[2]=0;f.tick();expect(!f.active("commands.open"),"chord release does not leak into Commands");
    }
    {
        Fixture f;
        expect(f.controls.setting("settings.wrist_picker_width_cm")==75,"native cards use the readable default width");
        expect(f.load("[settings]\nwrist_picker_width_cm=100\n"),"native picker width is configurable");
        expect(f.controls.setting("settings.wrist_picker_width_cm")==100,"maximum picker width is retained");
        expect(!f.load("[settings]\nwrist_picker_width_cm=101\n")&&!f.load("[settings]\nwrist_picker_width_cm=41\n"),
            "picker dimensions cannot escape their fitted envelope limits");
    }
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
        Fixture f;f.input.buttons[1]=1;f.tick();f.tick(650);
        expect(f.active("gameplay.pickup_carry")&&!f.active("gameplay.equip_binoculars")&&!f.active("gameplay.reload"),
            "bare B holds native pickup without binocular/reload leakage");
        f.tick(700);expect(f.active("gameplay.pickup_carry"),"pickup remains down beyond a one-shot pulse");
        f.input.buttons[1]=0;f.tick();
        expect(!f.active("gameplay.reload")&&!f.active("gameplay.equip_binoculars"),"pickup release consumes no other actions");
        f.input.buttons[8]=1;f.tick();f.input.buttons[6]=1;f.tick();
        expect(f.active("gameplay.switch_weapon")&&f.active("gameplay.ready_weapon")&&!f.active("gameplay.dive"),
            "right grip plus R3 requests weapon switch while keeping the weapon ready");
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
        expect(f.active("gameplay.pickup_carry")&&!f.active("gameplay.reload")&&!f.active("gameplay.equip_binoculars"),
            "bare B holds native pickup/carry without reload or binoculars");
        f.tick(700);expect(f.active("gameplay.pickup_carry"),"pickup remains down beyond a one-shot pulse");
        f.input={};f.tick();
        expect(!f.active("gameplay.reload")&&!f.active("gameplay.equip_binoculars"),"pickup release does not leak another action");
        f.input.buttons[7]=1;f.tick();f.input.buttons[1]=1;f.tick();
        expect(f.active("gameplay.support_grip")&&!f.active("gameplay.pickup_carry")&&!f.active("gameplay.reload"),
            "left grip plus B reserves the native reload chord");
        f.input={};f.tick();
        expect(f.active("gameplay.reload")&&!f.active("gameplay.pickup_carry"),"left grip plus B tap reloads on release");
        f.tick(101);expect(!f.active("gameplay.reload"),"reload pulse ends");
        f.input.buttons[7]=1;f.tick();f.input.buttons[1]=1;f.tick();f.tick(400);f.input={};f.tick();
        expect(!f.active("gameplay.reload"),"a long pickup chord does not become an accidental reload");
        f.input.buttons[7]=1;f.tick();f.input.buttons[3]=1;f.tick();f.tick(300);
        expect(f.active("gameplay.equip_binoculars")&&!f.active("gameplay.interact"),"left grip plus Y equips binoculars without context duplication");
        f.input={};f.tick();f.mode=ControlContext::binoculars;f.tick();
        f.input.buttons[1]=1;f.tick();
        expect(f.active("binoculars.stow"),"a fresh B press stows binoculars");
        f.input={};f.mode=ControlContext::gameplay;f.tick();
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
        Fixture f;expect(f.load("[gameplay]\nrun=right_stick_up\nzoom=disabled\n"),"right-up sprint remains an explicit configurable option");f.tick();
        f.input.rightStick={0,1};f.tick();
        expect(f.active("gameplay.run"),"explicitly configured right stick up runs");
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
        Fixture f;f.input.buttons[8]=1;f.input.buttons[10]=.8f;f.tick();
        f.input.buttons[2]=1;f.tick();f.mode=ControlContext::commands;f.tick();
        expect(f.active("gameplay.ready_weapon")&&f.controls.value("gameplay.fire_or_cqc")>.79f,
            "configured aim/CQC holds stay observable across interrogation menu ownership");
        f.input.buttons[8]=0;f.input.buttons[10]=0;f.tick();
        expect(!f.active("gameplay.ready_weapon")&&!f.active("gameplay.fire_or_cqc"),
            "combat releases are not swallowed inside Commands");
        f.mode=ControlContext::menus;f.input.buttons[8]=1;f.input.buttons[10]=1;f.tick();
        expect(!f.active("gameplay.ready_weapon")&&!f.active("gameplay.fire_or_cqc"),
            "combat continuity does not apply to iDroid, Pause or title menus");
        expect(f.load("[commands]\nconfirm=press(right_trigger)\n"),
            "a custom command confirmation may share the continuation-only CQC input");
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
        f.input.rightStick={-.45f,.65f};f.tick();
        expect(f.controls.axis("axes.equipment",f.input)==f.input.rightStick,
            "vehicle camera and turret look retain both native right-stick axes");
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
        expect(f.active("binoculars.zoom")&&!f.active("binoculars.dive")&&!f.active("binoculars.run"),"binocular left click only zooms");
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
