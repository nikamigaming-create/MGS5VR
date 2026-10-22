#include "mgs5vr/core.hpp"
#include "mgs5vr/mounted_grip.hpp"
#include "mgs5vr/cabin_walk.hpp"
#include "mgs5vr/arm_ik.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/motion_melee.hpp"
#include "mgs5vr/head_camera.hpp"
#include "mgs5vr/rocket_arm.hpp"
#include "mgs5vr/paused_rig.hpp"
#include "mgs5vr/tracking_fault.hpp"
#include "mgs5vr/idroid_rig.hpp"
#include "mgs5vr/idroid_native_hold.hpp"
#include "mgs5vr/idroid_attachment.hpp"
#include "mgs5vr/idroid_ui.hpp"
#include "mgs5vr/menu_surface.hpp"
#include "mgs5vr/idroid_effect_probe.hpp"
#include "mgs5vr/opening_selector.hpp"
#include "mgs5vr/render_layout.hpp"
#include "mgs5vr/recon.hpp"
#include "mgs5vr/optic_events.hpp"
#include "mgs5vr/optic_radio.hpp"
#include "mgs5vr/player_visibility.hpp"
#include "mgs5vr/native_menu_restrictions.hpp"
#include "mgs5vr/native_game_status.hpp"
#include "mgs5vr/native_controls.hpp"
#include "mgs5vr/native_text_input.hpp"
#include "mgs5vr/vehicle_pedals.hpp"
#include "mgs5vr/weapon_obstruction.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

using namespace mgs5vr;
static int checks{},failures{};
static void expect(bool ok,const char* name){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<name<<'\n';}}
static bool near(float a,float b){return std::abs(a-b)<0.0001f;}
static bool same(Vec3 a,Vec3 b){return near(a.x,b.x)&&near(a.y,b.y)&&near(a.z,b.z);}
int main(){
    expect(menuSurface(true,false,false)==MenuSurface::fallback,"open menu with absent device gets spatial fallback");
    expect(menuSurface(true,false,true)==MenuSurface::device,"ready device keeps native menu on device");
    expect(menuSurface(true,true,true)==MenuSurface::spatial,"explicit spatial menu keeps one spatial owner");
    expect(menuSurface(false,false,false)==MenuSurface::none,"lost wrist cannot promote ordinary HUD to face panel");
    expect(menuSurface(false,true,false)==MenuSurface::none,"closed menu cannot resurrect stale panel");
    {
        constexpr uint64_t ours=0xc1dbf5bd3008ull;
        NativeGameStatusRecord record{};
        const auto slot=[&](unsigned index,uint64_t hash){
            for(unsigned byte=0;byte<8;++byte)
                record[0x10+index*0x18+byte]=static_cast<uint8_t>(hash>>(8*byte));
        };
        for(unsigned index=0;index<4;++index)slot(index,nativeGameStatusEmptyOwner);
        expect(nativeGameStatusHasOwner(record,ours)==std::optional<bool>{false},
            "empty native status acknowledges our release");
        slot(0,0x123456789abcull);record[0x70]=1;
        expect(nativeGameStatusHasOwner(record,ours)==std::optional<bool>{false},
            "another native pad owner does not prevent our release acknowledgement");
        slot(3,ours|0xabcd000000000000ull);record[0x70]=2;
        expect(nativeGameStatusHasOwner(record,ours)==std::optional<bool>{true},
            "our registration is found beside another owner with native metadata intact");
        slot(1,0x23456789abcdull);slot(2,0x3456789abcdeull);record[0x70]=4;
        expect(nativeGameStatusHasOwner(record,ours)==std::optional<bool>{true},
            "full native status table still recognizes our existing registration");
        slot(3,0x456789abcdefull);
        expect(nativeGameStatusHasOwner(record,ours)==std::optional<bool>{false},
            "full table without our owner cannot acknowledge an attempted exclusion");
        record[0x70]=3;
        expect(!nativeGameStatusHasOwner(record,ours).has_value(),
            "mismatched native count cannot acknowledge a policy");
        record[0x70]=5;
        expect(!nativeGameStatusHasOwner(record,ours).has_value(),
            "out-of-range native count fails closed");
        record[0x70]=4;slot(3,0x123456789abcull);
        expect(!nativeGameStatusHasOwner(record,ours).has_value(),
            "duplicate occupied owners reject a torn or invalid native table");
        expect(!nativeGameStatusHasOwner(record,nativeGameStatusEmptyOwner).has_value(),
            "empty-slot sentinel is never an owner");
    }
    {
        const ControllerFrame defaults;
        expect(!defaults.blockFiringNearCover&&defaults.trackedWeaponObstruction,
            "cover firing blocking defaults off while the optional tracked sweep remains selected");
        WeaponObstructionSample sample;
        sample.published=sample.current={11,22,33,44,5,0x10006};
        sample.frameTime=1000;sample.weaponTime=990;
        sample.exactCaller=sample.enabled=sample.verified=sample.ready=sample.tracked=true;
        const auto choose=[&](const WeaponObstructionSample& s,bool block=false,bool tracked=true,uint64_t now=1050){
            return weaponObstructionMode(s,now,block,tracked);
        };
        expect(choose(sample)==WeaponObstructionMode::ignoreProximity
            &&choose(sample,false,false)==WeaponObstructionMode::ignoreProximity,
            "cover Off suppresses the local proximity veto independently of tracked-sweep preference");
        expect(choose(sample,true)==WeaponObstructionMode::trackedBarrel
            &&choose(sample,true,false)==WeaponObstructionMode::native,
            "cover On selects tracked or original clearance without bypassing its result");
        for(const auto mode:{WeaponObstructionMode::native,WeaponObstructionMode::trackedBarrel,
                             WeaponObstructionMode::ignoreProximity}){
            expect(weaponObstructionHitCount(-1,mode)==-1&&weaponObstructionHitCount(0,mode)==0,
                "native incomplete/error and zero-hit results are never fabricated into positive results");
            expect(weaponObstructionHitCount(3,mode)==(mode==WeaponObstructionMode::ignoreProximity?0:3),
                "only cover Off removes a consumed positive proximity count");
        }
        for(int fault=0;fault<6;++fault){
            auto other=sample;
            switch(fault){
            case 0:++other.current.player;break;
            case 1:++other.current.component;break;
            case 2:++other.current.state;break;
            case 3:++other.current.physics;break;
            case 4:++other.current.playerIndex;break;
            case 5:++other.current.resource;break;
            }
            expect(choose(other)==WeaponObstructionMode::native
                &&weaponObstructionHitCount(2,choose(other))==2,
                "a switched player, gun instance, physics owner or resource cannot inherit the bypass");
        }
        for(int fault=0;fault<8;++fault){
            auto unavailable=sample;
            switch(fault){
            case 0:unavailable.exactCaller=false;break;
            case 1:unavailable.enabled=false;break;
            case 2:unavailable.verified=false;break;
            case 3:unavailable.ready=false;break;
            case 4:unavailable.tracked=false;break;
            case 5:unavailable.nativeControls=true;break;
            case 6:unavailable.vehicleAllowed=false;break;
            case 7:unavailable.published.player=unavailable.current.player=0;break;
            }
            expect(choose(unavailable)==WeaponObstructionMode::native
                &&choose(unavailable,true)==WeaponObstructionMode::native,
                "unrelated callers, unavailable tracking/readiness and unacknowledged vehicles retain native collision");
        }
        expect(choose(sample,false,true,1090)==WeaponObstructionMode::ignoreProximity
            &&choose(sample,false,true,1091)==WeaponObstructionMode::native,
            "the older weapon sample stops authorizing the bypass beyond 100 milliseconds");
        auto stale=sample;stale.frameTime=949;
        expect(choose(stale)==WeaponObstructionMode::native,"a stale rig cannot use a newer weapon sample to bypass cover");
        auto future=sample;future.frameTime=1051;
        expect(choose(future)==WeaponObstructionMode::native,"future rig timestamps refuse the bypass");
        future=sample;future.weaponTime=1051;
        expect(choose(future)==WeaponObstructionMode::native,"future weapon timestamps refuse the bypass");
        sample.ready=false;
        expect(weaponObstructionHitCount(1,choose(sample))==1,
            "releasing the gun immediately restores the native result on the same owned instance");
    }
    {
        NativeTermsMenu terms{100,41,5,1,0,2,3,true,true};
        expect(terms.ready(100)&&terms.ready(350),"a current typed Terms owner can retain native menu navigation");
        expect(!terms.ready(99)&&!terms.ready(351),"future or stale Terms data cannot override cinematic controls");
        auto incomplete=terms;incomplete.phase=4;
        expect(!incomplete.ready(100),"Terms initialization is not input-ready");
        incomplete=terms;incomplete.active=false;
        expect(!incomplete.ready(100),"a closed Terms page cannot keep its menu route");
        incomplete=terms;incomplete.verified=false;
        expect(!incomplete.ready(100),"an unverified native type cannot override a cinematic");
        expect(nativeSceneInputMode(terms.ready(100),true,false,false,false)==NativeMenuInput::menu,
               "the live first-run Terms page takes menu navigation during the startup cinematic");
    }
    {
        cancelHandheldIdroidBorrow();publishHandheldIdroidOwner(0,0);
        expect(!requestHandheldIdroidBorrow(41,42),"an unowned terminal cannot promise an automatic return");
        publishHandheldIdroidOwner(41,42);
        expect(requestHandheldIdroidBorrow(41,42),"native reload borrows its exact UI-only terminal");
        const auto borrow=handheldIdroidBorrow();
        auto changed=borrow;changed.activation=43;acknowledgeHandheldIdroidBorrow(changed);
        expect(!handheldIdroidBorrow().stopped,"a replaced activation cannot acknowledge an earlier stow");
        acknowledgeHandheldIdroidBorrow(borrow);returnHandheldIdroidBorrow(changed);
        expect(handheldIdroidBorrow().stopped&&!handheldIdroidBorrow().returnRequested,
            "stow acknowledgement does not authorize another owner's return");
        returnHandheldIdroidBorrow(borrow);
        expect(handheldIdroidBorrow().returnRequested,"the original native reload owner can return its handset");
        cancelHandheldIdroidBorrow();
        expect(!handheldIdroidBorrow().player,"explicit stow cancels a pending automatic return");
        expect(requestHandheldIdroidBorrow(41,42),"a fresh reload can borrow the remaining exact handset owner");
        const auto fresh=handheldIdroidBorrow();
        expect(!handheldIdroidBorrow(fresh.time+10001).player,"a stalled reload cannot reopen a former terminal later");
        publishHandheldIdroidOwner(0,0);
    }
    {
        const std::array<uint64_t,25> deviceScenes{
            0x504021c76a2a4cbaull,0x5040741ac64544cfull,0x504074e62baad619ull,
            0x504094fe110e6190ull,0x5041279d9bbad3e7ull,0x5041cbb01589c388ull,
            0x5041fbae29f09edaull,0x504234fc6f91c7d8ull,0x504261ee7ecaa18bull,
            0x5042b59328405fccull,0x5042c3876f5a59e2ull,
            0x5042eb67eeb4de89ull,0x504308200fe46da1ull,0x50430ad0dfd261d5ull,
            0x5043148fe514447dull,0x50432962f66ecdf4ull,0x50439c74ecdacf51ull,
            0x5043d809a6309ca0ull,0x5043e2ca55e8f7bbull,0x5043f3b3f0a7aff9ull,
            0x504272424c6b2689ull,0x50435510b520b581ull,0x504158c4dcffcc86ull,
            0x5043a3f10f91b84aull,0x5041a1a4d4cde5f3ull};
        const IdroidUiSource opened{1,2,3,4,1,95145,true,true,true,true};
        auto closed=opened;closed.menuGeneration=2;closed.source=95146;closed.menuOpen=closed.idroid=false;
        IdroidUiIdentity identity{11,12,13,14,15,deviceScenes.front()};
        IdroidUiOpenLease lease{identity,opened};
        for(const auto scene:deviceScenes){
            identity.resource=lease.identity.resource=scene;
            expect(idroidUiRole(scene)==IdroidUiRole::device&&outgoingIdroidUi(lease,identity,closed,1,true),
                "each source-identified Map/chrome scene requires its matching open lease after native close");
            auto noOpen=lease;noOpen.source={};
            expect(!outgoingIdroidUi(noOpen,identity,closed,1,true)
                &&!outgoingIdroidUi(lease,identity,closed,1,false),
                "a known device resource alone cannot claim an unobserved or unverified closing draw");
        }
        for(const auto scene:{0x50418b3d353471adull,0x50419149642d99b4ull,0x5043132d8cf49a67ull,0x5042c796d643f58eull}){
            identity.resource=lease.identity.resource=scene;
            expect(idroidUiRole(scene)==IdroidUiRole::personalHud&&!outgoingIdroidUi(lease,identity,closed,1,true),
                "four observed persistent ordinary HUD scenes never enter the device close route");
        }
        identity.resource=lease.identity.resource=0;
        expect(idroidUiRole(0)==IdroidUiRole::unknown&&!outgoingIdroidUi(lease,identity,closed,1,true),
            "an unknown resource keeps its existing routing even when pointers and menu generations match");
        identity.resource=lease.identity.resource=0x5042e66eb693eab8ull;
        const bool commonMissionOutgoing=outgoingIdroidUi(lease,identity,closed,1,true);
        expect(idroidUiRole(identity.resource)==IdroidUiRole::unknown&&!commonMissionOutgoing,
            "shared Mission Orders cannot acquire exclusive device ownership from a prior open tuple");
        expect(idroidUiClosingRoute(commonMissionOutgoing,false)==IdroidUiClosingRoute::unchanged,
            "an absent current handset pose cannot suppress shared Mission Orders");
        identity.resource=lease.identity.resource=deviceScenes.front();
        expect(!outgoingIdroidUi(lease,identity,closed,0,true)&&!outgoingIdroidUi(lease,identity,closed,2,true)
            &&!outgoingIdroidUi(lease,identity,closed,1,false),"missing, ambiguous and changed native owners are not device authority");
        for(const auto member:{&IdroidUiIdentity::owner,&IdroidUiIdentity::root,&IdroidUiIdentity::packet,
            &IdroidUiIdentity::buffer,&IdroidUiIdentity::camera}){
            auto changed=identity;++(changed.*member);
            expect(!outgoingIdroidUi(lease,changed,closed,1,true),"reusing one native layout link invalidates the old open owner");
        }
        for(const auto member:{&IdroidUiSource::activation,&IdroidUiSource::referenceEpoch,&IdroidUiSource::presentationEpoch,
            &IdroidUiSource::menuGeneration}){
            auto changed=closed;++(changed.*member);
            expect(!outgoingIdroidUi(lease,identity,changed,1,true),"a different menu or XR epoch cannot prolong the device content lease");
        }
        auto changed=closed;++changed.player;
        expect(!outgoingIdroidUi(lease,identity,changed,1,true),"another player cannot inherit a device layout");
        changed=closed;changed.focused=false;
        expect(!outgoingIdroidUi(lease,identity,changed,1,true),"lost focus does not authorize device pixels");
        changed=closed;changed.handheld=false;
        expect(!outgoingIdroidUi(lease,identity,changed,1,true),"spatial mode does not inherit a handheld close lease");
        changed=closed;changed.source=opened.source;
        expect(!outgoingIdroidUi(lease,identity,changed,1,true),"replayed source pixels cannot manufacture a closing transition");
        changed=closed;changed.menuOpen=true;
        expect(!outgoingIdroidUi(lease,identity,changed,1,true),"Pause or another open menu does not use outgoing Map authority");
        auto unobserved=lease;unobserved.source.menuOpen=false;
        expect(!outgoingIdroidUi(unobserved,identity,closed,1,true),"a scene never observed in the open iDroid cannot acquire close ownership");
        unobserved=lease;unobserved.source.menuGeneration=UINT64_MAX;changed=closed;changed.menuGeneration=0;
        expect(!outgoingIdroidUi(unobserved,identity,changed,1,true),"menu generation overflow is not an adjacent transition");
        expect(idroidUiClosingRoute(true,false)==IdroidUiClosingRoute::suppress,
            "missing exact native pose drops only verified outgoing device pixels instead of borrowing left-arm geometry");
        expect(idroidUiClosingRoute(true,true)==IdroidUiClosingRoute::display,
            "a verified source-matched authored pose can carry surviving device pixels");
        expect(idroidUiClosingRoute(false,false)==IdroidUiClosingRoute::unchanged
            &&idroidUiClosingRoute(false,true)==IdroidUiClosingRoute::unchanged,
            "missing or present device geometry never changes unknown or ordinary HUD handling");
    }
    {
        ControllerTrackingFaultProbe probe;
        expect(!probe.arm(4,7000,100),"normal builds cannot arm a tracking-loss fixture");
        probe.configure(true);
        expect(!probe.arm(4,7000,100),"an unfocused test session cannot arm tracking loss");
        probe.focused(true);
        expect(!probe.arm(16,7000,100)&&!probe.arm(4,8001,100)&&!probe.arm(4,99,100),
            "tracking-loss diagnostics reject unsupported masks and unbounded durations");
        expect(probe.arm(4,7000,100)&&probe.mask(7099)==4&&probe.mask(7100)==0,
            "tracking-loss expiry is independent of the runner and expires at its exact bound");
        probe.arm(15,7000,8000);probe.focused(false);probe.focused(true);
        expect(probe.mask(8010)==0,"focus loss clears every simulated tracking-loss bit");
        probe.arm(8,7000,9000);probe.configure(false);
        expect(probe.mask(9010)==0,"session replacement or normal configuration removes the diagnostic lease");
        TrackingFaultProbe headProbe{3};
        expect(!headProbe.arm(1,7000,100),"head loss cannot be armed in a normal session");
        headProbe.configure(true);headProbe.focused(true);
        expect(!headProbe.arm(4,7000,100),"head loss rejects controller masks");
        expect(headProbe.arm(3,7000,100)&&headProbe.mask(7099)==3&&headProbe.mask(7100)==0,
            "head/view loss expires independently of a blocked capture or runner");
        expect(probe.mask(7099)==0,"head/view loss never arms controller loss");
        headProbe.arm(1,1000,8000);headProbe.focused(false);headProbe.focused(true);
        expect(headProbe.mask(8010)==0,"head loss clears when session focus is lost");
    }
    {
        expect(nativeIdroidRecoveryForTutorialMode(std::nullopt)==NativeIdroidRecovery::refuse,
            "unread tutorial mode cannot authorize forced terminal recovery");
        expect(nativeIdroidRecoveryForTutorialMode(0)==NativeIdroidRecovery::ordinary,
            "readable inactive tutorial permits ordinary requested recovery");
        for(unsigned mode=1;mode<=16;++mode)
            expect(nativeIdroidRecoveryForTutorialMode(mode)==NativeIdroidRecovery::requestedStow,
                "readable active tutorials permit deliberate terminal stow without completing the lesson");
        expect(nativeIdroidRecoveryForTutorialMode(17)==NativeIdroidRecovery::refuse
            &&nativeIdroidRecoveryForTutorialMode(std::numeric_limits<unsigned>::max())==NativeIdroidRecovery::refuse,
            "unrecognized tutorial values cannot authorize forced terminal recovery");
        expect(nativeIdroidTutorialPauseReleaseAllowed(0,false,false,false),
            "an inactive tutorial may release its retained own pause");
        for(unsigned mode=1;mode<=16;++mode){
            expect(nativeIdroidTutorialPauseReleaseAllowed(mode,false,true,true),
                "explicit native closing may release only the active lesson's own pause");
            expect(!nativeIdroidTutorialPauseReleaseAllowed(mode,false,false,true)
                &&!nativeIdroidTutorialPauseReleaseAllowed(mode,false,true,false),
                "active lessons retain their pause without both explicit stow and native closing");
        }
        expect(!nativeIdroidTutorialPauseReleaseAllowed(0,true,true,true)
            &&!nativeIdroidTutorialPauseReleaseAllowed(11,true,true,true)
            &&!nativeIdroidTutorialPauseReleaseAllowed(std::nullopt,false,true,true)
            &&!nativeIdroidTutorialPauseReleaseAllowed(17,false,true,true),
            "Help/Pause ownership and unknown tutorial values cannot release a pause");
        expect(nativeIdroidStowHelpCloseAllowed(0,false,32,true),
            "the exact post-stow Help may close under its captured request");
        expect(!nativeIdroidStowHelpCloseAllowed(11,false,32,true)
            &&!nativeIdroidStowHelpCloseAllowed(0,true,32,true)
            &&!nativeIdroidStowHelpCloseAllowed(0,false,1,true)
            &&!nativeIdroidStowHelpCloseAllowed(0,false,32,false)
            &&!nativeIdroidStowHelpCloseAllowed(std::nullopt,false,32,true),
            "active lessons, open terminals, ordinary Pause and absent owners remain untouched");
        NativeIdroidSceneHandoff handoff;
        expect(!handoff.begin(11,12,true)&&!handoff.resume(10),
            "an unowned terminal cannot gain UI-only ownership through a movie");
        handoff.remember(10,11,12);
        expect(handoff.begin(11,12,true)&&handoff.suspended()&&handoff.observe(11,12,true),
            "the exact already-open UI-only resource survives an authored shot without an input owner");
        expect(handoff.resume(10)&&!handoff.suspended()&&!handoff.resume(10),
            "the same returning player may recover the resource exactly once");
        handoff.remember(10,11,12);handoff.begin(11,12,true);
        expect(!handoff.resume(20)&&!handoff.resume(10),
            "a replacement player cancels the old terminal's return authority");
        handoff.remember(10,11,12);handoff.begin(11,12,true);
        expect(!handoff.observe(11,12,false)&&!handoff.begin(11,12,true),
            "an observed close and reopen cannot revive the former terminal owner");
        handoff.remember(10,11,12);handoff.begin(11,12,true);
        expect(!handoff.observe(21,12,true)&&!handoff.resume(10),
            "a replacement UI system invalidates scene return");
        handoff.remember(10,11,12);handoff.begin(11,12,true);
        expect(!handoff.observe(11,22,true)&&!handoff.resume(10),
            "a replacement terminal invalidates scene return");
        handoff.remember(10,11,12);handoff.begin(11,12,true);handoff.reset();
        expect(!handoff.resume(10),"loading, title or cancelled ownership cannot resume an old terminal");
        NativeIdroidStowHelpLease helpLease;
        expect(!helpLease.pending(100).owner,"ordinary UI queries have no stow acknowledgment lease");
        helpLease.begin(0x1234,100);
        const auto help=helpLease.pending(110);
        expect(help.owner==0x1234&&!help.acknowledged&&!helpLease.finish(help),
            "native Help must receive its own Confirm before cleanup can complete");
        expect(helpLease.acknowledge(help,120)&&!helpLease.acknowledge(help,121)
            &&helpLease.pending(121).acknowledged,
            "an owned stow can acknowledge its informational Help only once");
        helpLease.begin(0x1234,130);
        expect(!helpLease.acknowledge(help,140)&&!helpLease.finish(help),
            "a newer stow on the same native owner invalidates an old UI claim");
        const auto newerHelp=helpLease.pending(140);
        auto otherHelp=newerHelp;otherHelp.owner=0x4321;
        expect(!helpLease.acknowledge(otherHelp,150)&&!helpLease.pending(129).owner,
            "replacement and future native owners cannot acknowledge Help");
        expect(!helpLease.pending(2131).owner&&!helpLease.acknowledge(newerHelp,2131),
            "a stow request cannot inject Confirm beyond its original two-second deadline");
        helpLease.begin(0,2200);
        expect(!helpLease.acknowledge(newerHelp,2200)&&!helpLease.pending(2200).owner,
            "cancellation prevents a late acknowledgment of the old card");
        helpLease.begin(0x4321,2300);const auto finalHelp=helpLease.pending(2301);
        expect(helpLease.acknowledge(finalHelp,2302)&&helpLease.finish(finalHelp)
            &&!helpLease.pending(2303).owner&&!helpLease.finish(finalHelp),
            "native close completion retires the exact acknowledged lease without replay");
        expect(nativeCabinIdroidStow(true,false,0,true,false,true,true,true)==NativeCabinIdroidStow::requestClose,
            "held Back in ordinary ACC requests native Close before terminal Stop");
        expect(nativeCabinIdroidStow(true,false,0,true,true,false,true,true)==NativeCabinIdroidStow::wait,
            "native closing alone never permits Stop while the seated player still owns SUBJECT");
        expect(nativeCabinIdroidStow(true,false,0,true,true,false,true,false)==NativeCabinIdroidStow::finishStop,
            "native root Back or held Back completes only after the verified seated owner exits SUBJECT");
        expect(nativeCabinIdroidStow(true,false,0,false,true,false,true,false)==NativeCabinIdroidStow::wait,
            "a terminal already closed by the native actor needs no further Stop");
        expect(nativeCabinIdroidStow(true,false,0,true,true,true,false,false)==NativeCabinIdroidStow::wait
            &&nativeCabinIdroidStow(true,false,0,true,true,true,true,std::nullopt)==NativeCabinIdroidStow::wait
            &&nativeCabinIdroidStow(true,false,0,true,std::nullopt,true,true,false)==NativeCabinIdroidStow::wait,
            "missing or replaced player, unread status and unread terminal cannot masquerade as completed stow");
        expect(nativeCabinIdroidStow(true,false,0,true,false,false,true,false)==NativeCabinIdroidStow::wait,
            "a live terminal without a native or deliberate close request remains open");
        expect(nativeCabinIdroidStow(false,false,0,true,true,true,true,false)==NativeCabinIdroidStow::unchanged
            &&nativeCabinIdroidStow(true,true,0,true,true,true,true,false)==NativeCabinIdroidStow::unchanged
            &&nativeCabinIdroidStow(true,false,std::nullopt,true,true,true,true,false)==NativeCabinIdroidStow::unchanged,
            "ordinary ACC ownership does not change field, UI-only or unknown-mode recovery policy");
        for(unsigned mode=1;mode<=16;++mode)
            expect(nativeCabinIdroidStow(true,false,mode,true,true,true,true,false)==NativeCabinIdroidStow::unchanged,
                "forced lessons retain their existing deliberate stow and post-close Help lifecycle");
        std::ostringstream state;state<<std::boolalpha;
        state<<"{\"unknown\":";writeNativeIdroidTutorialModeJson(state,std::nullopt);
        state<<",\"ordinary\":";writeNativeIdroidTutorialModeJson(state,0);
        state<<",\"active\":";writeNativeIdroidTutorialModeJson(state,10);
        state<<",\"invalid\":";writeNativeIdroidTutorialModeJson(state,17);state<<'}';
        expect(state.str()=="{\"unknown\":null,\"ordinary\":0,\"active\":10,\"invalid\":null}",
            "inspect JSON preserves unknown versus inactive and active tutorial mode without boolean coercion");
    }
    NativeMenuRestrictionLease menuLease;
    for(unsigned index=0;index<78;++index)menuLease.beginEntry(1,2,index,index==33);
    expect(menuLease.ready(1,2),"complete native tutorial begin has a restorable menu owner");
    expect(menuLease.prior(33)==true&&menuLease.prior(1)==false,"recovery preserves prior disabled entries rather than unlocking everything");
    expect(!menuLease.ready(3,2)&&!menuLease.ready(1,3),"replaced menu or tutorial cannot borrow an older snapshot");
    menuLease.independentDisable(1,1);
    expect(!menuLease.prior(1)&&menuLease.prior(2)==false,"another owner's disable survives guide recovery");
    menuLease.beginEntry(1,2,0,false);menuLease.beginEntry(1,2,2,false);
    expect(!menuLease.ready(1,2),"partial or interleaved native begin cannot restore flags");
    menuLease.reset();expect(!menuLease.prior(2),"consumed lease cannot be reused for another lesson");
    {
        ScopePoseStabilizer filter;
        const WeaponScopeSample original{Pose{{},{0,0,-.1f}},Pose{{},{0,0,-.3f}},.018f,.1f,8.f,17,true};
        auto makeScope=[&](Pose owner,Pose noise){
            auto s=original;s.ocular=compose(owner,compose(noise,original.ocular));
            s.objective=compose(owner,compose(noise,original.objective));return s;
        };
        filter.update({},original,true,1000,1,1);
        auto tiny=makeScope({},Pose{{},{.0005f,0,0}});
        auto stable=filter.update({},tiny,true,1010,1,1);
        expect(same(stable.ocular.position,original.ocular.position),"scope attachment dead zone removes sub-millimetre skin jitter");
        const Pose movement{{0,.38268343f,0,.92387953f},{.3f,.2f,-.4f}};
        auto moved=makeScope(movement,Pose{{},{.0005f,0,0}});
        stable=filter.update(movement,moved,true,1020,1,1);
        expect(same(stable.ocular.position,compose(movement,original.ocular).position)
            &&same(rotate(stable.ocular.orientation,{0,0,-1}),rotate(movement.orientation,{0,0,-1})),
            "scope stabilization follows deliberate controller translation and rotation without lag");
        expect(same(compose(inverse(stable.ocular),stable.objective).position,{0,0,-.2f}),
            "stabilized scope glass and scene keep their exact native ocular/objective relationship");
        float rawEnergy=0,filteredEnergy=0;filter.reset();
        filter.update({},original,true,2000,1,1);
        for(unsigned n=1;n<=400;++n){
            const float wave=std::sin(static_cast<float>(n)*1.3f);
            const auto noisy=makeScope({},Pose{{0,std::sin(.002618f*wave),0,std::cos(.002618f*wave)},{.002f*wave,0,0}});
            stable=filter.update({},noisy,true,2000+n*10,1,1);
            if(n>50){
                const auto raw=noisy.ocular.position-original.ocular.position;
                const auto corrected=stable.ocular.position-original.ocular.position;
                rawEnergy+=dot(raw,raw);filteredEnergy+=dot(corrected,corrected);
            }
        }
        expect(filteredEnergy<rawEnergy*.1f,"Coco scope filter removes at least 68 percent RMS attachment jitter at x8");
        std::cout<<"Scope attachment jitter RMS ratio "<<std::sqrt(filteredEnergy/rawEnergy)<<'\n';
        auto changed=makeScope({},Pose{{},{.005f,0,0}});changed.weaponIdentity=18;
        stable=filter.update({},changed,true,6010,1,1);
        expect(same(stable.ocular.position,changed.ocular.position),"scope swap starts from the actual new glass without stale history");
        changed.ocular.position.x+=.005f;changed.objective.position.x+=.005f;
        stable=filter.update({},changed,true,6020,2,1);
        expect(same(stable.ocular.position,changed.ocular.position),"XR reference-space reset discards scope attachment history");
        changed.ocular.position.x+=.005f;changed.objective.position.x+=.005f;
        stable=filter.update({},changed,true,6030,2,2);
        expect(same(stable.ocular.position,changed.ocular.position),"camera activation change cannot reuse an old scope attachment");
        changed.ocular.position.x+=.005f;changed.objective.position.x+=.005f;
        stable=filter.update({},changed,true,6230,2,2);
        expect(same(stable.ocular.position,changed.ocular.position),"scope tracking gap does not interpolate from an old pose");
        stable=filter.update({},tiny,false,6240,2,2);
        expect(same(stable.ocular.position,tiny.ocular.position),"disabled stabilization or a changing contact keeps the completed skin authoritative");
        stable=filter.update({},original,true,6250,2,2);
        expect(same(stable.ocular.position,original.ocular.position),"scope reactivation after menus or tracking loss starts without a jump");
        auto flipped=original;flipped.ocular.orientation.w=-1;flipped.objective.orientation.w=-1;
        stable=filter.update({},flipped,true,6260,2,2);
        expect(valid(stable.ocular)&&same(rotate(stable.ocular.orientation,{0,0,-1}),{0,0,-1}),
            "equivalent quaternion signs do not create a scope rotation spike");
        auto invalid=original;invalid.magnification=std::numeric_limits<float>::quiet_NaN();
        stable=filter.update({},invalid,true,6270,2,2);
        expect(!weaponScopeSceneView(stable),"invalid scope data cannot acquire a camera through stabilization");
        stable=filter.update({},tiny,true,6260,2,2);
        expect(same(stable.ocular.position,tiny.ocular.position),"scope filter clock regression starts with the current pose");
    }
    {
        OpticEventQueue events;
        events.push({OpticEventKind::raised,{},101,1});
        expect(!events.take(100,1),"an optic event newer than the consumer clock is not delivered early");
        const auto raised=events.take(102,1);
        expect(raised&&raised->kind==OpticEventKind::raised,"a producer racing the consumer clock retains the physical raise for the next update");
        expect(!events.take(103,1),"the physical raise is delivered only once");
        events.push({OpticEventKind::waypoint,{1,2,3},120,1});
        expect(!events.take(119,2)&&!events.take(121,2),"a different activation discards even a future-dated optic event");
        events.push({OpticEventKind::raised,{},130,2});
        expect(!events.take(381,2),"an expired optic event cannot advance a later scene");
        for(uint64_t n=0;n<17;++n)events.push({OpticEventKind::waypoint,{static_cast<float>(n),0,0},400+n,2});
        const auto first=events.take(450,2);
        expect(first&&first->position.x==1,"the optic event queue remains bounded to sixteen entries");
    }
    {
        const Pose ray{},box{{},{0,0,-10}};
        const auto hit=opticRadioBoxHit(ray,box,{2,1,1},0,0);
        expect(hit&&near(*hit,9),"physical intel ray enters the registered world volume");
        expect(!opticRadioBoxHit(ray,{{},{0,0,10}},{2,1,1},0,0),"intel behind the sight is rejected");
        expect(!opticRadioBoxHit(ray,{{},{3,0,-10}},{2,1,1},0,0),"parallel ray beside a volume cannot acquire it");
        expect(!opticRadioBoxHit(ray,box,{2,1,1},0,8),"native intel maximum distance is respected");
        expect(!opticRadioBoxHit(ray,box,{2,1,1},12,0),"native intel minimum distance is respected");
        const auto rotated=opticRadioBoxHit(ray,{{0,.70710678f,0,.70710678f},{0,0,-10}},{4,1,1},0,0);
        expect(rotated&&near(*rotated,6),"registered radio volume orientation rotates its extents");
        expect(!opticRadioBoxHit(ray,box,{0,1,1},0,0),"unavailable geometry cannot create intel");
        expect(!opticRadioBoxHit(ray,box,{2,1,1},0,std::numeric_limits<float>::quiet_NaN()),"invalid radio range is rejected");
    }
    {
        OpticUseEdge edge;
        expect(!edge.update(false,100,1),"carrying binoculars is not using the ocular");
        expect(edge.update(true,110,1),"actual eye-relief entry reports binocular use");
        expect(!edge.update(true,110,1)&&!edge.update(true,120,1),"a held or replayed ocular reports use only once");
        expect(!edge.update(false,130,1)&&edge.update(true,140,1),"lowering and raising the physical optic produces a new use");
        edge.reset();
        expect(!edge.update(false,150,1),"tracking loss cannot synthesize an ocular use");
        expect(!edge.update(true,0,1)&&!edge.update(true,200,0),"unidentified samples cannot notify mission logic");
        expect(edge.update(true,210,2),"fresh tracking after an activation begins a new use");
    }
    {
        const RigContinuityKey key{1,2,3,4,5};
        HandPresentationCache cache;
        const Pose first{{},{.2f,.3f,-.5f}},second{{0,.70710678f,0,.70710678f},{.4f,.5f,-.7f}};
        const HandPresentationPose firstPair{first,Pose{{},{.25f,.35f,-.55f}}};
        const HandPresentationPose secondPair{second,Pose{second.orientation,{.45f,.55f,-.75f}}};
        expect(!cache.update(firstPair,false,true,100,key),"missing tracking cannot invent a cached hand pair");
        expect(cache.update(firstPair,true,true,110,key).has_value(),"one complete controller sample starts presentation");
        const auto held=cache.update(secondPair,false,true,130,key);
        expect(held&&same(held->grip.position,first.position)&&same(held->aim.position,firstPair.aim.position),
            "an incomplete new sample cannot combine its grip with the old aim");
        const auto heldContact=held?idroidGripContact(held->grip,held->aim):std::nullopt;
        expect(heldContact&&same(heldContact->position,first.position),"retained aim and grip still form the same iDroid contact");
        const auto reacquired=cache.update(secondPair,true,true,150,key);
        expect(reacquired&&same(reacquired->grip.position,first.position)
            &&same(reacquired->aim.position,firstPair.aim.position),"both poses begin recovery continuously from the held sample");
        const auto middle=cache.update(secondPair,true,true,200,key),complete=cache.update(secondPair,true,true,250,key);
        expect(middle&&same(middle->grip.position,{.3f,.4f,-.6f})&&same(middle->aim.position,{.35f,.45f,-.65f})
            &&valid(middle->grip)&&valid(middle->aim),"grip and aim share the same rigid-pose recovery interval");
        expect(complete&&same(complete->grip.position,second.position)&&same(complete->aim.position,secondPair.aim.position),
            "both recovered poses reach current tracking after 100 milliseconds");
        bool retained=true;
        for(uint64_t time=500;time<=10500;time+=250){
            const auto idle=cache.update({},false,true,time,key);
            retained=retained&&idle&&same(idle->grip.position,second.position)&&same(idle->aim.position,secondPair.aim.position);
        }
        expect(retained,"fresh focused publications retain an idle controller pair beyond 250 milliseconds");
        const auto delayed=cache.update({},false,true,11000,key);
        const auto delayedAgain=cache.update({},false,true,21000,key);
        expect(delayed&&delayedAgain&&same(delayedAgain->grip.position,second.position)
            &&same(delayedAgain->aim.position,secondPair.aim.position),
            "scheduling gaps preserve visual continuity in the same focused owner and reference space");
        const auto longRecovery=cache.update(firstPair,true,true,21100,key);
        const auto longRecoveryEnd=cache.update(firstPair,true,true,21200,key);
        expect(longRecovery&&longRecoveryEnd&&same(longRecovery->grip.position,second.position)
            &&same(longRecovery->aim.position,secondPair.aim.position)
            &&same(longRecoveryEnd->grip.position,first.position)&&same(longRecoveryEnd->aim.position,firstPair.aim.position),
            "both poses recover together after a long idle hold and rendering gap");
        cache.update(firstPair,true,true,11000,key);
        expect(!cache.update({},false,true,11010,{1,2,3,6,5}),"a changed OpenXR reference space cannot reuse the old hand");
        cache.update(firstPair,true,true,11020,key);
        expect(!cache.update({},false,true,11030,{9,2,3,4,5}),"a different player cannot reuse the old hand");
        cache.update(firstPair,true,true,11040,key);
        expect(!cache.update({},false,true,11050,{1,9,3,4,5}),"a replacement model cannot reuse the old hand");
        cache.update(firstPair,true,true,11060,key);
        expect(!cache.update({},false,true,11070,{1,2,9,4,5}),"a new camera activation cannot reuse the old hand");
        cache.update(firstPair,true,true,11080,key);
        expect(!cache.update({},false,true,11090,{1,2,3,4,9}),"focus or controller-profile generation changes clear both poses");
        cache.update(firstPair,true,true,11100,key);
        expect(!cache.update({},false,false,11110,key)&&!cache.update({},false,true,11120,key),
            "focus loss clears presentation immediately and focus return cannot invent tracking");
        cache.update(firstPair,true,true,11130,key);
        expect(!cache.update({},false,true,11120,key),"clock reversal clears retained hand presentation");
        cache.update(firstPair,true,true,11140,key);
        auto invalidPair=firstPair;invalidPair.aim.orientation={0,0,0,0};
        expect(!cache.update(invalidPair,true,true,11150,key),"invalid claimed tracking cannot retain or seed a broken pair");
        std::array<HandPresentationCache,2> both;
        ControllerFrame raw;raw.presentationFocused=true;raw.presentationEpoch=5;
        for(size_t side=0;side<2;++side){
            const auto pair=side?secondPair:firstPair;
            both[side].update(pair,true,true,100,key);
            raw.hands[side].grip=pair.grip;raw.hands[side].aim=pair.aim;
        }
        bool bothVisible=true;
        for(uint64_t time=200;time<=1200;time+=100)for(size_t side=0;side<2;++side){
            const auto& hand=raw.hands[side];
            bothVisible=both[side].update({hand.grip,hand.aim},hand.gripTracked&&hand.aimTracked,
                raw.presentationFocused,time,key).has_value()&&bothVisible;
        }
        expect(bothVisible&&!raw.hands[0].gripTracked&&!raw.hands[0].aimTracked
            &&!raw.hands[1].gripTracked&&!raw.hands[1].aimTracked,"both idle hands stay visible without granting raw tracking authority");
        const auto passive=passiveControllerFrame(raw,1,4);
        expect(!passive.presentationFocused&&passive.presentationEpoch==5,"passive XR input explicitly withdraws presentation focus");
        StationaryBodyState stop;
        expect(!stop.update(true,{},100,key)&&!stop.update(true,{},200,key),"a cold stationary body does not invent prior movement");
        expect(!stop.update(true,{.03f,0,0},220,key),"native movement arms alignment without moving the pose");
        expect(!stop.update(true,{.03f,0,0},240,key)&&stop.update(true,{.03f,0,0},340,key),"stopping after movement enables the bounded counter-pivot");
        expect(!stop.update(true,{.06f,0,0},360,key),"new movement releases stationary alignment");
        expect(!stop.update(false,{.06f,0,0},380,key)&&!stop.update(true,{.06f,0,0},480,key),"menus and mounted states reset alignment history");
        const StationaryBodyPose body{{},{.1f,1,0},{-.1f,1,0},{.2f,1.4f,0},{-.2f,1.4f,0},Pose{{},{0,1.3f,0}}};
        const auto lower=stationaryLowerBodyDelta(body,Pose{{0,.70710678f,0,.70710678f},{2,.5f,0}});
        expect(lower&&near(lower->position.y,0)&&std::hypot(lower->position.x,lower->position.z)<=.50001f,
            "lower-body correction preserves floor height and caps displacement at half a metre");
        expect(lower&&same(rotate(lower->orientation,{0,1,0}),{0,1,0}),"stationary legs receive yaw only");
        auto invalid=body;invalid.leftHip=invalid.rightHip;
        expect(!stationaryLowerBodyDelta(invalid,{}),"a degenerate pelvis cannot enter skin publication");
    }
    {
        for(const size_t first:{93u,97u}){
            std::array<uint32_t,128> names{};
            std::array<int32_t,128> parents{};
            for(size_t j=0;j<armHelperNames.size();++j){names[first+j]=armHelperNames[j];parents[first+j]=armHelperParents[j];}
            const auto binding=armHelperIndices(names,parents);
            expect(binding&&(*binding)[0]==first&&(*binding)[13]==first+13,
                "cabin and field outfits resolve arm corrective joints by identity");
            parents[first+2]=12;
            expect(!armHelperIndices(names,parents),"a same-named correction under the wrong parent is rejected");
        }
    }
    expect(!hidePlayerInFirstPerson(false,false),"inactive immersive presentation leaves the native body visible");
    expect(hidePlayerInFirstPerson(true,false),"immersive first-person gameplay hides the native torso/head");
    const bool immersiveView=true;bool scriptedDemo=true;
    expect(!hidePlayerInFirstPerson(immersiveView,scriptedDemo),"scripted prologue scenes keep the complete native body visible");
    scriptedDemo=false;
    expect(hidePlayerInFirstPerson(immersiveView,scriptedDemo),"ending a scripted demo restores first-person body concealment");
    {
        expect(!reconModelVisible(HudMode::binocularsOnly,HudView::world,true),"recon model does not leak into unaided eyes");
        expect(reconModelVisible(HudMode::binocularsOnly,HudView::binoculars,true),"binocular view may retain native recon glow");
        expect(!reconModelVisible(HudMode::binocularsOnly,HudView::otherOptic,true),"rifle scopes cannot reveal recon bodies");
        expect(!reconModelVisible(HudMode::full,HudView::binoculars,false),"glow switch leaves body effect disabled even in full HUD mode");
        expect(!reconModelVisible(HudMode::off,HudView::binoculars,true),"HUD off disables recon body rendering");
        expect(!reconModelVisible(HudMode::full,HudView::world,true),"world enemy labels cannot enable native recon-body shaders");
        expect(worldMarkerVisible(HudMode::binocularsOnly,true,0),"world enemy labels are independently enabled");
        expect(!worldMarkerVisible(HudMode::full,false,0),"enemy labels can be disabled independently of full HUD");
        expect(worldMarkerVisible(HudMode::full,false,1),"enemy toggle preserves native waypoint labels");
        expect(!worldMarkerVisible(HudMode::binocularsOnly,true,1),"enemy toggle does not enable every world HUD layer");
        expect(!worldMarkerVisible(HudMode::off,true,0),"HUD Off also disables world enemy labels");
    }
    {
        ReconDwell dwell;constexpr uint16_t person=42,other=43;
        expect(!dwell.update(person,1000,1,650),"looking at a new person starts the recon dwell");
        for(uint64_t t=1050;t<1650;t+=50)expect(!dwell.update(person,t,1,650),"a brief glance does not acquire");
        expect(dwell.update(person,1650,1,650),"uninterrupted binocular observation acquires once");
        expect(!dwell.update(person,1700,1,650),"an acquired person does not pulse repeatedly");
        dwell.reset();dwell.update(person,2000,1,100);dwell.suppress();
        expect(!dwell.update(person,2100,1,100),"a deliberate clear stays cleared while looking at that person");
        dwell.update(std::nullopt,2150,1,100);dwell.update(person,2200,1,100);
        expect(dwell.update(person,2300,1,100),"looking away allows a new acquisition");
        dwell.reset();dwell.update(person,3000,1,100);
        expect(!dwell.update(person,3000,1,100),"replaying a frozen source image never advances dwell");
        expect(!dwell.update(person,3200,1,100),"a stale tracking gap restarts observation");
        expect(!dwell.update(other,3250,1,100),"sweeping across people never transfers dwell time");
        expect(!dwell.update(other,3300,2,100),"a new camera activation restarts observation");
        expect(!dwell.update(other,3200,2,100),"a clock reversal restarts observation");
        expect(!dwell.update(other,3300,2,0),"disabled dwell cannot acquire");
        expect(!dwell.update(other,3400,2,100),"re-enabling requires a fresh dwell");
    }
    {
        ReconClearPolicy clears;constexpr uint16_t person=42,other=43;
        clears.scene(10,20);
        expect(clears.automaticAllowed(person),"new native people remain available for binocular acquisition");
        clears.clear(person);
        expect(!clears.automaticAllowed(person)&&clears.automaticAllowed(other),"explicit clear suppresses only that actor");
        ReconDwell dwell;dwell.update(person,1000,1,100);dwell.suppress();
        dwell.update(std::nullopt,1100,1,100);dwell.update(person,1200,1,100);
        expect(dwell.update(person,1300,1,100)&&!clears.automaticAllowed(person),"looking away cannot undo an explicit clear");
        dwell.update(person,2000,2,100);dwell.update(person,2100,2,100);clears.scene(10,20);
        expect(!clears.automaticAllowed(person),"tracking gaps and new eye activations keep the native scene clear policy");
        clears.explicitMark(person);
        expect(clears.automaticAllowed(person),"deliberate remark permits acquisition again");
        clears.clear(person);clears.scene(11,20);
        expect(clears.automaticAllowed(person),"a new native player starts a new scene clear policy");
        clears.clear(person);clears.scene(11,21);
        expect(clears.automaticAllowed(person),"replaced native actor records cannot retain another scene's clears");
        clears.clear(person);clears.reset();clears.scene(11,21);
        expect(clears.automaticAllowed(person)&&!clears.automaticAllowed(0xffff),"loading reset releases prior clears and rejects an invalid actor");
    }
    {
        expect(hudLayer(151,100,false,false)==HudLayer::general,"subtitles/notification layers are not wrist status");
        for(const auto order:{146u,147u,148u}){
            expect(idroidPersonalStatus(order,135,true),"submitted status layers are excluded from the iDroid menu route");
            expect(!idroidPersonalStatus(order,100,true),"a reused menu priority is not the forearm HUD");
            expect(!idroidPersonalStatus(order,135,false),"Pause retains its own menu surface");
        }
        expect(!idroidPersonalStatus(151,135,true),"iDroid text is not mistaken for weapon status");
        for(const auto order:{53u,54u,55u,56u,57u,130u,131u,132u}){
            expect(suppressFlatFirearmReticle(order,100,true),"native desktop firearm aiming layers stay off the arm");
            expect(!suppressFlatFirearmReticle(order,100,false),"throwing and placement retain native aim feedback");
            expect(!suppressFlatFirearmReticle(order,150,true),"another native UI camera is not a firearm reticle");
        }
        for(const auto order:{50u,52u,133u,134u,135u,146u,151u})
            expect(!suppressFlatFirearmReticle(order,100,true),"reticle suppression preserves prompts, equipment and text");
        expect(nativeReconLayer(23,false,true)&&!nativeReconLayer(23,true,false),"person cue needs its native layout-camera identity");
        expect(nativeReconLayer(2,true,false)&&nativeReconLayer(3,true,false),"native world silhouettes are recon layers");
        for(const auto order:{49u,51u,52u,133u,137u,147u,151u,171u})
            expect(!nativeReconLayer(order,false,true),"recon policy cannot hide caption, interaction, picker or status layers");
        expect(hudLayer(133,100,false,false)==HudLayer::general,"ordinary layer 133 is not an equipment card");
        expect(hudLayer(133,150,false,false)==HudLayer::equipment&&hudLayer(133,100,true,false)==HudLayer::equipment,
            "equipment cameras and item context own their cards");
        expect(hudLayer(135,100,false,false,true)==HudLayer::equipment
            &&hudLayer(137,100,false,false,true)==HudLayer::equipment
            &&hudLayer(137,100,false,false)==HudLayer::general,
            "trigger-held native four-way selector uses its actual Z=100 orders");
        expect(hudLayer(137,100,false,true)==HudLayer::commands&&hudLayer(137,100,false,false)==HudLayer::general,
            "command layers require the active command context");
        expect(hudLayer(52,100,false,false)==HudLayer::context&&hudLayer(147,135,false,false)==HudLayer::status,
            "wrist action and status stay separate from general HUD");
        expect(worldHudVisible(HudMode::full,HudView::world)&&worldHudVisible(HudMode::full,HudView::binoculars),
            "full HUD exposes world cues in ordinary vision and binoculars");
        expect(ControllerFrame{}.hudMode==HudMode::binocularsOnly,"unconfigured source frames default to normal unaided vision");
        for(const auto mode:{HudMode::full,HudMode::binocularsOnly,HudMode::off}){
            for(unsigned eye=0;eye<2;++eye)for(bool binocularAtEye:{false,true}){
                const auto view=hudViewForPass(eye,binocularAtEye);
                expect(view==HudView::world&&worldHudVisible(mode,view)==(mode==HudMode::full),
                    "raising binoculars never enables enhanced information across an ordinary eye image");
            }
            expect(worldHudVisible(mode,hudViewForPass(2,true))==(mode!=HudMode::off),
                "enhanced information belongs to the aligned binocular scene only");
            expect(!worldHudVisible(mode,hudViewForPass(2,false)),
                "weapon scopes and binoculars held away from the eye do not reveal recon information");
            expect(!worldHudVisible(mode,hudViewForPass(3,true)),"unknown render passes cannot reveal recon information");
        }
    }
    {
        struct Launcher{uint32_t sight;Vec3 rear;float length,radius,inset;};
        const std::array<Launcher,4> launchers{{
            {21,{.057f,.128f,-.0640001f},.168000f,.00926f,.007614f},
            {22,{.079f,.184f,-.1429949f},.259995f,.01567f,.005989f},
            {23,{.1139957f,.169f,-.1099949f},.431995f,.01478f,.005795f},
            {24,{.1439957f,.2040864f,-.1533774f},.323995f,.01328f,.007445f}
        }};
        const Pose motion{{0,.38268343f,0,.92387953f},{3,2,-4}};
        for(const auto& launcher:launchers){
            const Pose rear{{},launcher.rear},front{{},launcher.rear+Vec3{0,0,launcher.length}};
            const auto optic=nativeWeaponScopeGeometry(rear,front,{2,4,6,1});
            expect(optic&&optic->sight==launcher.sight&&near(optic->radius,launcher.radius),"launcher CNP pair selects its own receiver aperture");
            expect(optic&&same(optic->ocular.position,launcher.rear+Vec3{0,0,launcher.inset}),"launcher lens sits at the measured inner opening, not the outer rim");
            expect(!nativeWeaponScopeGeometry(rear,front,{2,4,8,1}),"launcher cannot borrow rifle magnification steps");
            expect(!nativeWeaponScopeGeometry(front,rear,{2,4,6,1}),"reversed launcher sockets cannot reveal mirrored world pixels");
            const auto moved=nativeWeaponScopeGeometry(compose(motion,rear),compose(motion,front),{2,4,6,1});
            expect(optic&&moved&&same(moved->ocular.position,compose(motion,optic->ocular).position),"launcher scope remains attached under rigid motion");
        }
        WeaponScopeZoom zoom;
        const Pose stockRear{{},{0,.158995f,-.0380396f}},stockFront{{},{0,.158995f,.136873f}};
        const auto stockDot=nativeWeaponScopeGeometry(stockRear,stockFront,{1,0,0,2});
        expect(stockDot&&stockDot->dotReticle&&!stockDot->opaqueAtOnePower&&near(stockDot->radius,.0154731f),"baked stock reflex uses measured octagonal glass and transparent 1x dot");
        expect(stockDot&&near(stockDot->ocular.position.z,-.0291299f),"stock reflex overlay lies on the viewing side of native glass");
        const Pose fixedRear{{},{0,.155f,.0846941f}},fixedFront{{},{0,.155f,.259607f}};
        const auto fixedStack=nativeWeaponScopeGeometry(fixedRear,fixedFront,{1,2,0,1});
        expect(fixedStack&&fixedStack->opaqueAtOnePower&&fixedStack->dotReticle&&fixedStack->powers==std::array<uint8_t,3>{1,2,0},"fixed stock magnifier retains actual powers and scene pixels at 1x");
        expect(fixedStack&&near(fixedStack->ocular.position.z,-.087569f)&&near(fixedStack->objective.position.z,.069988f),"fixed magnifier aperture comes from its glass, not the forward dot CNP");
        const Pose stockMotion{{0,.38268343f,0,.92387953f},{3,-2,4}};
        const auto movedStack=nativeWeaponScopeGeometry(compose(stockMotion,fixedRear),compose(stockMotion,fixedFront),{1,2,0,1});
        expect(fixedStack&&movedStack&&same(movedStack->ocular.position,compose(stockMotion,fixedStack->ocular).position),"baked optic follows the native weapon rigidly");
        expect(!nativeWeaponScopeGeometry(fixedRear,fixedFront,{1,4,0,1})&&!nativeWeaponScopeGeometry(fixedFront,fixedRear,{1,2,0,1}),"unmeasured stock power or reversed sockets cannot borrow fixed optic geometry");
        expect(near(zoom.update(21,0,{2,4,6}),2)&&near(zoom.update(21,1,{2,4,6}),4)
            &&near(zoom.update(21,2,{2,4,6}),6)&&near(zoom.update(21,3,{2,4,6}),2),"launcher zoom cycles only its native 2x/4x/6x powers");
    }
    {
        WeaponScopeSample scope{Pose{{},{0,0,-.1f}},Pose{{},{0,0,-.3f}},.018f,.1f,4.f,17,true};
        const auto scene=weaponScopeSceneView(scope);
        expect(scene&&same(scene->pose.position,scope.objective.position),"scope scene starts at the native objective, not the head");
        expect(scene&&near(std::tan(scene->fov.right),.018f/(.1f*4.f)),"scope magnification controls only its independent optical frustum");
        expect(weaponScopeEyeVisible(scope,{}),"aligned eye sees the physical scope aperture");
        expect(weaponScopeEyeVisible(scope,Pose{{},{.064f,0,0}}),"scope glass stays live for the other eye; geometry confines its pixels");
        expect(!weaponScopeEyeVisible(scope,Pose{{},{0,0,-.2f}}),"scope does not show a reversed image through its front");
        expect(weaponScopeEyeVisible(scope,Pose{{},{0,0,.3f}}),"carried scope stays live beyond the old eye-relief cutoff");
        expect(weaponScopeEyeVisible(scope,compose(scope.ocular,Pose{{},{0,0,.01f}})),
            "approaching the glass does not toggle the scope image; the near plane clips geometry");
        const Pose motion{{0,.38268343f,0,.92387953f},{2,1,-5}};
        auto moved=scope;moved.ocular=compose(motion,scope.ocular);moved.objective=compose(motion,scope.objective);
        expect(weaponScopeEyeVisible(moved,motion),"scope and eye visibility share a rigid motion frame");
        moved=scope;moved.tracked=false;expect(!weaponScopeSceneView(moved),"missing scope tracking closes its scene");
        moved=scope;moved.weaponIdentity=0;expect(!weaponScopeSceneView(moved),"unidentified weapon cannot publish a scope");
        moved=scope;moved.objective.position.z=0;expect(!weaponScopeSceneView(moved),"reversed native scope socket is rejected");
        moved=scope;moved.magnification=std::numeric_limits<float>::quiet_NaN();expect(!weaponScopeSceneView(moved),"invalid scope power is rejected");
        const Pose rear{{},{0,.025f,-.0979877f}},front{{},{0,.025f,.1840025f}};
        const auto geometry=nativeWeaponScopeGeometry(rear,front,{4,0,0,1});
        expect(geometry&&geometry->sight==17&&near(geometry->radius,.01341f),"native named CNP pair selects measured BAMBETOV sight");
        expect(geometry&&rotate(geometry->ocular.orientation,{0,0,-1}).z>.999f,"native +Z barrel becomes optical -Z without reflection");
        expect(geometry&&near(geometry->ocular.position.z,rear.position.z+.00416f),"scope aperture follows authored glass plane");
        expect(!nativeWeaponScopeGeometry(front,rear,{4,0,0,1}),"reversed native CNP names cannot open scope");
        expect(!nativeWeaponScopeGeometry(rear,front,{4,0,0,2}),"reflex sight cannot borrow round scope profile");
        expect(!nativeWeaponScopeGeometry(rear,front,{8,0,0,1}),"different native optic parameters cannot borrow a scope profile");
        const Pose fixtureMove{{0,.38268343f,0,.92387953f},{3,-2,4}};
        const auto transformed=nativeWeaponScopeGeometry(compose(fixtureMove,rear),compose(fixtureMove,front),{4,0,0,1});
        expect(geometry&&transformed&&same(transformed->ocular.position,compose(fixtureMove,geometry->ocular).position),"sight attachment stays rigid through weapon motion");
        WeaponScopeZoom zoom;
        expect(near(zoom.update(42,7,{2,4,8}),2),"new scope starts at native lowest power");
        expect(near(zoom.update(42,8,{2,4,8}),4),"one zoom click selects next native power");
        expect(near(zoom.update(42,8,{2,4,8}),4),"duplicate skin publication cannot double zoom");
        expect(near(zoom.update(42,10,{2,4,8}),2),"multiple zoom clicks wrap through native powers");
        expect(near(zoom.update(43,11,{4,0,0}),4)&&near(zoom.update(43,12,{4,0,0}),4),"fixed-power scope never fabricates another zoom step");
        for(unsigned frequency:{72u,90u,120u}){
            WeaponScopeZoomInput input;WeaponScopeZoom variable;
            expect(near(variable.update(16,input.update(false,true),{2,4,8}),2),"variable sight starts at 2x before input");
            for(float expected:{4.f,8.f,2.f}){
                for(unsigned frame=0;frame<(frequency+9)/10;++frame)
                    expect(near(variable.update(16,input.update(true,true),{2,4,8}),expected),
                        "100ms configured click changes power once at every supported XR rate");
                input.update(false,true);
            }
            const auto sequence=input.update(true,false);
            expect(input.update(true,true)==sequence,"holding zoom through menu/ready transition cannot create a click");
            input.update(false,true);
            expect(input.update(true,true)==sequence+1,"fresh available click resumes zoom after transition");
        }
    }
    {
        constexpr uint64_t centerCode=0x84a35edc984905ceull,magnifierCode=0x84a1df03c6484ee1ull;
        const Pose center{{},{0,.1285718f,.1034005f}},magnifier{{},{0,.1285718f,-.0165436f}};
        const auto one=nativeMagnifierScopeGeometry(center,magnifier,centerCode,magnifierCode,{1,2,0,1},1);
        const auto two=nativeMagnifierScopeGeometry(center,magnifier,centerCode,magnifierCode,{1,2,0,1},2);
        expect(one&&two&&one->sight==4&&two->sight==4,"native magnifier modes retain one zoom owner");
        expect(one&&two&&one->ocular.position.z>two->ocular.position.z+.1f,
            "1x uses the collimator sight point and 2x uses the separate rear magnifier glass");
        expect(one&&near(one->ocular.position.z,.0754532f),
            "unmagnified dot stays 0.2 mm behind the observed native rear sight point");
        expect(one&&two&&near(one->radius,.0202493f)&&near(two->radius,.0147864f),
            "each lens fits its own owned polygonal aperture");
        const Pose motion{{0,.38268343f,0,.92387953f},{2,-1,4}};
        const auto moved=nativeMagnifierScopeGeometry(compose(motion,center),compose(motion,magnifier),centerCode,magnifierCode,{1,2,0,1},2);
        expect(two&&moved&&same(moved->ocular.position,compose(motion,two->ocular).position),
            "assembled magnifier glass follows the exact weapon root under motion");
        expect(!nativeMagnifierScopeGeometry(center,magnifier,centerCode,0,{1,2,0,1},2),
            "missing magnifier cannot manufacture a 2x lens");
        expect(!nativeMagnifierScopeGeometry(center,magnifier,0,magnifierCode,{1,2,0,1},1),
            "unknown central sight cannot borrow collimator geometry");
        expect(!nativeMagnifierScopeGeometry(center,magnifier,centerCode,magnifierCode,{1,2,0,1},4),
            "a copied power absent from this weapon cannot create another scope mode");
        const auto fixed=nativeMagnifierScopeGeometry(center,{},centerCode,0,{1,0,0,2},1);
        expect(fixed&&fixed->sight==2&&fixed->dotReticle,"the native standalone collimator retains its 1x sight");
    }
    {
        const Pose wrist{{},{-.35f,-.4f,-.3f}};
        const auto popup=wristPopupPose(wrist,{},.09f);
        expect(popup&&same(popup->position,{-.35f,-.31f,-.3f}),
            "wrist popup stays centered exactly nine centimeters above the rendered forearm");
        for(Vec3 position:{Vec3{-.7f,-.6f,-.2f},Vec3{.8f,.4f,-.2f},Vec3{0,0,.1f}}){
            const auto offscreen=wristPopupPose(Pose{{},position},{},.09f);
            expect(offscreen&&same(offscreen->position,position+Vec3{0,.09f,0}),
                "off-axis, close and behind-head wrists cannot be moved into the player's face");
        }
        for(Pose head:{Pose{{0,.70710678f,0,.70710678f},{.3f,.2f,-.1f}},
                       Pose{{.38268343f,0,0,.92387953f},{-.2f,.1f,.3f}}}){
            const auto turned=wristPopupPose(wrist,head,.09f);
            expect(popup&&turned&&same(popup->position,turned->position),
                "head-only rotation and translation cannot detach the popup from the fixed forearm");
        }
        const Pose movement{{0,.70710678f,0,.70710678f},{752.7f,322.6f,1214.5f}};
        const auto moved=wristPopupPose(compose(movement,wrist),movement,.09f);
        expect(popup&&moved&&same(moved->position,compose(movement,*popup).position),
            "wrist popup follows source-world translation and yaw at native field coordinates");
        auto invalid=wrist;invalid.position.x=std::numeric_limits<float>::quiet_NaN();
        expect(!wristPopupPose(invalid,{},.09f)&&!wristPopupPose(wrist,{},-.1f)
            &&!wristPopupPose(wrist,{},std::numeric_limits<float>::quiet_NaN()),
            "invalid wrist data cannot create a fallback face panel");
    }
    {
        constexpr auto tpp=renderLayout(RenderBuild::phantomPain_1_0_15_4);
        constexpr auto gz=renderLayout(RenderBuild::groundZeroes_1_0_0_5);
        expect(tpp.cameraPose+tpp.viewInputToCamera==tpp.inversePose&&gz.cameraPose+gz.viewInputToCamera==gz.inversePose,
            "each build joins the same source pose in its world and inverse builders");
        expect(gz.viewportCamera==0x4b0&&gz.graphicsContext==0x120&&gz.viewportNext==0x28,
            "GZ does not inherit TPP native object layout");
        for(const auto l:{tpp,gz}){
            expect(l.gpuProjection>=l.viewportMatrices&&l.previousProjection+64<=l.viewportMatrices+0x240
                &&l.previousView>=l.viewportMatrices&&l.previousView+64<=l.viewportMatrices+0x240,
                "both-eye matrix/history writes are covered by scoped native restoration");
            expect(l.presentCapacity==l.presentCount+4&&l.presentData==l.presentCount+8,
                "duplicate scene replay removes only its own native present-vector append");
        }
        expect(gz.uiProjectionReturn==0&&tpp.uiProjectionReturn!=0,"unmapped GZ UI cannot call the TPP adapter");
        HeadCameraStatus transition;transition.awaitingPlayer=true;transition.reason=HeadCameraStop::playerHeadUnavailable;
        expect(mayRetainStereoSurround(transition),"planned native loading retains its explicitly frozen surroundings");
        for(const auto reason:{HeadCameraStop::cameraChanged,HeadCameraStop::matrixMismatch,
                              HeadCameraStop::clockMismatch,HeadCameraStop::rigFrameMismatch}){
            transition.reason=reason;
            expect(!mayRetainStereoSurround(transition),"a failed camera transaction cannot linger behind a menu");
        }
    }
    {
        SnapTurn snap;
        expect(near(snap.update(1,0,true),0),"held stick cannot snap on activation");
        snap.update(0,0,true);
        expect(near(snap.update(1,0,true),-.523598776f),"right flick requests one thirty-degree right turn");
        expect(near(snap.update(1,0,true),0),"holding the turn stick cannot repeat the snap");
        snap.update(0,0,true);
        expect(near(snap.update(0,1,true),0)&&near(snap.update(1,0,true),0),"vertical flick does not pitch or spill into a snap");
        snap.update(0,0,true);snap.update(0,0,false);
        expect(near(snap.update(-1,0,true),0),"menu exit requires a fresh neutral stick");
        snap.update(0,0,true);
        expect(near(snap.update(-1,0,true),.523598776f),"left flick turns left after centering");
    }
    {
        RigLocomotion movement;
        const GamepadSample up{0,0,0,18000,27000,0,30000},down{0,255,0,18000,27000,0,-30000};
        expect(movement.update(up,true,100).gamepad.buttons==0,"a held vertical stick cannot run on gameplay activation");
        movement.update({},true,110);
        auto action=movement.update(up,true,120);
        expect(action.gamepad.buttons==0x0040&&action.gamepad.leftY==27000&&!action.stance,
            "right-stick up runs while left-stick movement is preserved");
        expect(movement.update(up,true,600).gamepad.buttons==0x0040,"holding up sustains the native run control");
        expect(movement.update(down,true,700).gamepad.buttons==0,"reversing the same gesture cannot also change stance");
        movement.update({},true,710);
        action=movement.update(down,true,720);
        expect(action.stance&&action.gamepad.buttons==0x1000&&action.gamepad.leftTrigger==0,
            "down requests native stance rather than the aiming-mode A action");
        expect(movement.update({},true,730).gamepad.buttons==0x1000,"a short down flick survives native frame polling");
        expect(movement.update({},true,830).gamepad.buttons==0,"stance tap releases to crouch or stand");
        movement.update(down,true,840);
        expect(movement.update(down,true,1840).gamepad.buttons==0x1000,"holding down retains native prone hold timing");
        movement.update({},true,1850);
        action=movement.update({0x0040},true,1860);
        expect(action.gamepad.buttons==0x4000,"left-stick click dives instead of sprinting");
        expect(movement.update(down,false,1900).gamepad==down,"native menus keep both stick axes and buttons");
        expect(movement.update(down,true,2000).gamepad.buttons==0,"closing a picker cannot replay its held downward selection");
        movement.update({0x0040},false,2005);
        expect(movement.update({0x0040},true,2010).gamepad.buttons==0,"a click held through a menu is consumed until release");
        movement.update({},true,2020);
        expect(movement.update({0x0040},true,2030).gamepad.buttons==0x4000,"a fresh click dives after release");
        movement.suspend();
        expect(movement.update(up,true,2040).gamepad.buttons==0,"focus recovery requires a fresh centered stick");
        movement.update({},true,2050);
        expect(movement.update(up,true,2060).gamepad.buttons==0x0040,"fresh upward gesture works after focus recovery");
    }
    {
        VehiclePedalInput pedals;
        GamepadSample heldShot{};
        heldShot.rightTrigger=255;heldShot.leftTrigger=180;
        heldShot.leftX=14000;heldShot.leftY=24000;heldShot.buttons=0x1000;
        auto drive=pedals.update(heldShot,.8f,true,true);
        expect(drive.leftTrigger==0&&drive.rightTrigger==0&&drive.leftY==0,
            "requesting a vehicle gun immediately removes held triggers and an already-deflected stick from pedals");
        expect(drive.leftX==heldShot.leftX&&drive.buttons==heldShot.buttons,
            "vehicle pedal ownership preserves the steering axis and unrelated buttons");
        drive=pedals.update(heldShot,-.8f,true,true);
        expect(drive.leftTrigger==0&&drive.rightTrigger==0,
            "reversing a stick held through gun acquisition cannot arm either pedal");
        drive=pedals.update(heldShot,0.f,true,true);
        expect(drive.leftTrigger==0&&drive.rightTrigger==0,
            "centering the new pedal stick does not turn a held firing trigger into acceleration");
        drive=pedals.update(heldShot,1.f,true,true);
        expect(drive.rightTrigger==255&&drive.leftTrigger==0&&drive.leftY==0,
            "a fresh forward pedal-stick gesture supplies full acceleration without a brake input");
        drive=pedals.update(heldShot,.59f,true,true);
        expect(drive.rightTrigger>=126&&drive.rightTrigger<=128&&drive.leftTrigger==0,
            "the vehicle pedal stick retains proportional acceleration between neutral and full travel");
        drive=pedals.update(heldShot,-1.f,true,true);
        expect(drive.leftTrigger==255&&drive.rightTrigger==0,
            "backward pedal-stick travel supplies brake or reverse without simultaneous acceleration");
        drive=pedals.update(heldShot,.1f,true,true);
        expect(drive.leftTrigger==0&&drive.rightTrigger==0,
            "small centered pedal-stick motion leaves both native pedals released");
        drive=pedals.update(heldShot,0.f,false,true);
        expect(drive.leftTrigger==0&&drive.rightTrigger==0,
            "stowing the vehicle gun cannot convert either held trigger into a pedal");
        drive=pedals.update(heldShot,0.f,false,true);
        expect(drive.leftTrigger==0&&drive.rightTrigger==0,
            "held triggers remain blocked across later native polls after gun stow");
        auto brakeHeld=heldShot;brakeHeld.rightTrigger=0;
        drive=pedals.update(brakeHeld,0.f,false,true);
        expect(drive.leftTrigger==0&&drive.rightTrigger==0,
            "releasing only the firing trigger cannot restore pedals while the other trigger is still held");
        auto released=heldShot;released.leftTrigger=released.rightTrigger=0;
        pedals.update(released,0.f,false,true);
        auto freshAccelerator=released;freshAccelerator.rightTrigger=220;
        drive=pedals.update(freshAccelerator,0.f,false,true);
        expect(drive==freshAccelerator,
            "after both triggers release, a fresh trigger press restores ordinary vehicle pedals unchanged");

        pedals.update(heldShot,0.f,true,true);
        drive=pedals.update(heldShot,0.f,false,false);
        expect(drive==heldShot,
            "temporary pedal unavailability leaves the other input context's sample unchanged");
        pedals.update(released,0.f,false,false);
        drive=pedals.update(heldShot,0.f,false,true);
        expect(drive.leftTrigger==0&&drive.rightTrigger==0,
            "resuming pedal input after another context cannot reinterpret held firing triggers as pedals");
        drive=pedals.update(heldShot,0.f,false,true);
        expect(drive.leftTrigger==0&&drive.rightTrigger==0,
            "the resumed pedal owner keeps its release latch until both triggers release in that context");
        pedals.update(released,0.f,false,true);
        drive=pedals.update(freshAccelerator,0.f,false,true);
        expect(drive==freshAccelerator,
            "a fresh trigger press restores pedals after release in the resumed input context");
    }
    {
        WheelSteering steering;const Pose contact{{},{-.2f,-.3f,.5f}};
        steering.update(contact,contact,true,false,100,1);
        auto wheel=steering.update(contact,contact,true,true,111,1);
        expect(wheel.gripped&&wheel.engaged&&wheel.axis==0,"fresh near-wheel grip acquires without steering jump");
        auto turn=contact;turn.orientation={0,0,-.258819f,.965926f};
        wheel=steering.update(turn,contact,true,true,122,1);
        expect(wheel.gripped&&!wheel.engaged&&std::abs(wheel.axis-30.f/35.f)<.001f,"one-hand steering reaches useful range with a thirty-degree wrist turn");
        const auto lookAway=steering.update(turn,contact,true,true,127,1,Vec3{1,0,0});
        expect(near(lookAway.axis,wheel.axis),"head turns while holding cannot change the steering basis");
        turn.orientation={0,0,.258819f,.965926f};
        expect(std::abs(steering.update(turn,contact,true,true,129,1).axis+30.f/35.f)<.001f,"counterclockwise hand roll has the same increased range");
        expect(!steering.update(turn,contact,true,false,133,1).gripped,"releasing the grip releases the wheel");
        turn.position.x+=1;
        expect(!steering.update(turn,contact,true,true,144,1).gripped,"distant squeezing cannot snap to the wheel");
        steering.update(contact,contact,true,false,155,1);
        steering.update(contact,contact,true,true,166,1);
        expect(!steering.update(contact,contact,false,true,177,1).gripped,"tracking or vehicle loss releases the wheel");
        expect(!steering.update(contact,contact,true,true,188,1).gripped,"held grip cannot reattach after tracking loss");
        WheelSteering joining;
        joining.update(contact,contact,true,false,200,1);
        const auto joined=joining.update(contact,contact,true,true,210,1,{0,0,-1},35.f,.6f);
        expect(joined.gripped&&near(joined.axis,.6f),"second hand takes the current steering angle without straightening the vehicle");
        turn=contact;turn.orientation={0,0,-.70710678f,.70710678f};
        expect(near(joining.update(turn,contact,true,true,220,1).axis,1.f),"wheel response remains clamped at native full lock");
        RumbleMailbox rumble;rumble.publish({.4f,.2f,100});
        expect(rumble.read(110).low==.4f&&rumble.read(351).low==0,"native rumble expires without a new game sample");
    }
    {
        BinocularHold binocular;
        expect(!binocular.update(true,true,100).selected,"B press waits before equipping binoculars");
        expect(!binocular.update(true,true,399).selected,"short B hold does not equip binoculars");
        auto b=binocular.update(true,true,400);
        expect(b.selected&&!b.nativePress,"long B hold equips binoculars without a native B action");
        expect(binocular.update(true,true,600).selected,"continued B hold cannot stow the binoculars it just equipped");
        b=binocular.update(true,false,610);
        expect(b.selected&&!b.nativePress,"releasing B keeps binoculars equipped and does not reload");
        b=binocular.update(true,true,650);
        expect(!b.selected&&!b.nativePress,"a fresh B press stows binoculars without reloading");
        b=binocular.update(true,true,1000);
        expect(!b.selected&&!b.nativePress,"holding the stow press cannot reopen binoculars");
        b=binocular.update(true,false,1010);
        expect(!b.selected&&!b.nativePress,"releasing the stow press cannot reload");
        BinocularHold tap;
        tap.update(true,true,100);
        b=tap.update(true,false,150);
        expect(!b.selected&&b.nativePress,"short B tap remains a native B action");
        expect(tap.update(true,false,220).nativePress,"native B tap remains available through its pulse");
        expect(!tap.update(true,false,260).nativePress,"native B pulse expires");
        b=tap.update(false,true,300);
        expect(b.nativePress&&!b.selected,"menus and wrist controls retain immediate B");
        expect(!tap.update(true,true,700).selected,"holding menu B across a transition cannot equip binoculars");
        tap.update(true,false,710);tap.update(true,true,720);
        expect(tap.update(true,true,1020).selected,"fresh gameplay B hold equips after a menu transition");
        tap.suspend();
        b=tap.update(true,true,1400);
        expect(!b.selected&&!b.nativePress,"focus loss consumes held B until release");

        RigOptics optics;
        GamepadSample input{};
        auto view=optics.update(input,true,false);
        expect(view.magnification==1&&!view.exclusive,"unheld optics leave gun handling alone");
        view=optics.update(input,true,true);
        expect(view.magnification==2&&view.exclusive&&!view.nativeActive&&view.gamepad.buttons==0,
            "one-handed carry has full 2x power without entering a native camera mode");
        GamepadSample browse{};browse.leftY=24000;browse.rightX=12000;browse.buttons=0x80;
        view=optics.update(browse,true,true);
        expect(view.magnification==4&&view.gamepad.leftY==24000&&view.gamepad.rightX==12000
            &&view.gamepad.buttons==0,"one-handed zoom preserves walking and turn");
        expect(optics.update(browse,true,true).magnification==4,"held R3 cannot cycle twice");
        GamepadSample mark{};mark.rightTrigger=255;
        view=optics.update(mark,true,true);
        expect(view.markRequested&&view.gamepad.buttons==0&&view.gamepad.rightTrigger==0,
            "right trigger requests an optic mark without firing or opening Commands");
        expect(!optics.update(mark,true,true).markRequested,"held trigger cannot repeat a mark");
        optics.update({},true,true);
        expect(optics.update(mark,true,true).markRequested,"released trigger allows the next mark");
        GamepadSample clear{};clear.rightY=-32767;
        view=optics.update(clear,true,true);
        expect(view.clearRequested&&!view.markRequested&&view.gamepad.rightY==0,
            "down flick removes an optic mark without pitching the camera");
        expect(!optics.update(clear,true,true).clearRequested,"held down cannot remove a second mark");
        optics.update({},true,true);
        expect(optics.update(clear,true,true).clearRequested,"centering rearms optic clearing");
        expect(!optics.update(clear,true,false).clearRequested,"stowed binoculars cannot clear marks");
        expect(!optics.update(clear,true,true).clearRequested,"regripping a held down stick requires neutral");
        view=optics.update(mark,true,false);
        expect(view.exclusive&&view.gamepad.rightTrigger==0&&view.gamepad.buttons==0,
            "stowing with trigger held cannot discharge the restored firearm");
        expect(!optics.update({},true,false).exclusive,"release returns ordinary gameplay input");
        expect(optics.update({},true,true).magnification==4,"carrying and regripping retain the selected zoom");
        GamepadSample intel{};intel.buttons=0x1000;
        view=optics.update(intel,true,true,true);
        expect(!view.nativeActive&&view.gamepad.buttons==0,
            "eye relief cannot enter an authored native camera mode or leak A into stance");
        expect(optics.update({},true,true,false).gamepad.buttons==0,
            "lowering the physical optic releases native scanning");
        view=optics.update(mark,false,false);
        expect(view.gamepad.buttons==0&&view.gamepad.rightTrigger==0,
            "tracking loss never injects native B or a weapon trigger");
    }
    {
        const Pose head{{},{0,1.6f,0}};
        const std::array<EyeView,2> eyes{{
            {Pose{{},{-.032f,1.6f,0}},EyeFov{-.8f,.8f,.7f,-.7f}},
            {Pose{{},{.032f,1.6f,0}},EyeFov{-.8f,.8f,.7f,-.7f}}}};
        const Pose left{{},{-.30f,1.25f,.18f}},right{{},{0,1.54f,.18f}};
        const Pose aim{{},{}};
        expect(solveBinocularPose(left,right,aim,aim,true,true,true,true,true,true).has_value(),
            "a right primary hand publishes one authored binocular frame");
        const auto single=solveBinocularPose(
            Pose{{},{-.4f,1.2f,.2f}},Pose{{},{.03f,1.3f,.2f}},
            Pose{{},{-.4f,1.2f,.2f}},Pose{{},{.03f,1.3f,.2f}},
            false,true,false,true,false,true);
        expect(single&&single->primaryRight&&!single->supportHeld
            &&single->ray.tracked
            &&near(single->ray.origin.x,single->rightEyepiece.position.x)
            &&near(single->ray.origin.y,single->rightEyepiece.position.y)
            &&near(single->ray.origin.z,single->rightEyepiece.position.z)
            &&near(dot(single->ray.direction,rotate(single->rightEyepiece.orientation,{0,0,-1})),1.f),
            "a single right-hand optic stays attached and publishes its calibrated carry ray");
        const auto primaryAperture=compose(single->body,Pose{{0,0,0,1},binocularOcularCenter}).position;
        expect(near(single->rightEyepiece.position.x,primaryAperture.x)
            &&near(single->rightEyepiece.position.y,primaryAperture.y)
            &&near(single->rightEyepiece.position.z,primaryAperture.z),
            "the dominant eye relief point is the real retail ocular, without a synthetic side offset");
        expect(near(dot(single->ray.direction,rotate(single->renderBody.orientation,{0,0,1})),-1.f),
            "the optical ray enters the housing opposite the outward ocular normal");
        const Pose rotatedGrip{{0,.258819f,0,.965926f},{.03f,1.3f,.2f}};
        const Pose differentAim{{0,-.258819f,0,.965926f},{.03f,1.3f,.2f}};
        const auto gripOwned=solveBinocularPose({},rotatedGrip,{},differentAim,
            false,true,false,true,false,true);
        const auto gripReference=solveBinocularPose({},rotatedGrip,{},rotatedGrip,
            false,true,false,true,false,true);
        expect(gripOwned&&gripReference
            &&same(compose(gripOwned->body,Pose{{},binocularPrimarySocket}).position,rotatedGrip.position)
            &&same(compose(gripReference->body,Pose{{},binocularPrimarySocket}).position,rotatedGrip.position)
            &&same(gripOwned->ray.direction,rotate(differentAim.orientation,{0,0,-1})),
            "optic follows the controller aim basis without moving its primary palm socket");
        const Pose quarterTurnGrip{{.70710678f,0,0,.70710678f},rotatedGrip.position};
        const auto calibrated=solveBinocularPose({},quarterTurnGrip,{},Pose{{},{}},false,true,false,true,false,true);
        expect(calibrated&&same(calibrated->ray.direction,{0,0,-1})
            &&same(compose(calibrated->body,Pose{{},binocularPrimarySocket}).position,quarterTurnGrip.position),
            "a controller with a ninety-degree grip/aim difference needs no backwards wrist bend");
        for(const auto controllerGrip:{Pose{{},rotatedGrip.position},quarterTurnGrip}){
            const auto device=solveBinocularPose({},controllerGrip,{},Pose{},false,true,false,true,false,true);
            const auto palmInBody=compose(inverse(device->body),device->primaryGrip);
            expect(same(palmInBody.position,binocularPrimarySocket)
                &&same(rotate(palmInBody.orientation,{0,-1,0}),{0,1,0})
                &&same(rotate(palmInBody.orientation,{0,0,-1}),{0,0,1}),
                "binocular palm contact cups over the housing with thumb toward ocular, independent of controller profile");
            const auto attached=attachBinocularToPalm(*device,device->primaryGrip);
            expect(attached&&same(attached->body.position,device->body.position)
                &&same(attached->ray.direction,device->ray.direction),
                "acquiring the authored palm does not apply a second ninety-degree aim correction");
        }
        const auto correction=binocularGripRotation(20,-30,15);
        const auto corrected=solveBinocularPose({},quarterTurnGrip,{},Pose{},false,true,false,true,false,true,correction);
        const auto correctedContact=compose(inverse(corrected->body),corrected->primaryGrip);
        expect(corrected&&same(corrected->primaryGrip.position,calibrated->primaryGrip.position)
            &&same(rotate(correctedContact.orientation,{0,-1,0}),{0,1,0})
            &&same(rotate(correctedContact.orientation,{-1,0,0}),{-1,0,0}),
            "device fit retains a side-facing palm and upward knuckles about the fixed primary position");
        const auto down=solveBinocularPose({},quarterTurnGrip,{},Pose{},false,true,false,true,false,true,binocularGripRotation(-90,0,0));
        const auto downSupport=compose(down->body,Pose{{},binocularSupportSocket});
        const auto downSupported=solveBinocularPose(downSupport,quarterTurnGrip,Pose{},Pose{},true,true,true,true,true,true,binocularGripRotation(-90,0,0));
        const auto downPalm=compose(inverse(down->body),down->primaryGrip);
        expect(down&&same(down->ray.direction,{0,-1,0})
            &&same(down->primaryGrip.position,calibrated->primaryGrip.position)
            &&same(rotate(down->primaryGrip.orientation,{0,-1,0}),rotate(quarterTurnGrip.orientation,{0,-1,0}))
            &&same(rotate(down->primaryGrip.orientation,{0,0,-1}),rotate(quarterTurnGrip.orientation,{0,0,-1}))
            &&same(rotate(downPalm.orientation,{0,-1,0}),{0,1,0})
            &&same(rotate(downPalm.orientation,{-1,0,0}),{-1,0,0})
            &&same(down->ray.direction,rotate(down->renderBody.orientation,{0,0,-1}))
            &&same(down->ray.direction,rotate(down->rightEyepiece.orientation,{0,0,-1})),
            "minus ninety pitches the optic down while the right palm stays on its side with fingers over the top");
        const auto supportPalm=compose(inverse(downSupported->body),downSupported->supportGrip);
        expect(downSupported&&downSupported->supportHeld
            &&same(downSupported->supportGrip.position,downSupport.position)
            &&same(rotate(supportPalm.orientation,{0,-1,0}),{0,1,0})
            &&same(rotate(supportPalm.orientation,{1,0,0}),{1,0,0}),
            "left palm faces inward from the opposite side with fingers over the top, not toward the ocular");
        const Pose handMove{{0,.258819f,0,.965926f},{.1f,.02f,-.03f}};
        const auto resolved=attachBinocularToPalm(*corrected,compose(handMove,corrected->primaryGrip));
        const auto resolvedAgain=attachBinocularToPalm(*resolved,resolved->primaryGrip);
        expect(resolved&&resolvedAgain
            &&same(resolved->body.position,compose(handMove,corrected->body).position)
            &&same(resolved->ray.direction,rotate(handMove.orientation,corrected->ray.direction))
            &&same(resolvedAgain->body.position,resolved->body.position)
            &&same(resolvedAgain->ray.direction,resolved->ray.direction),
            "final skin reattachment preserves user calibration and never accumulates rotation across repeated solves");
        expect(!attachBinocularToPalm(OpticPose{},{}),"untracked binoculars cannot acquire a rendered palm");
        const Pose motion{{0,.258819f,0,.965926f},{.25f,.1f,-.2f}};
        const auto movedRig=solveBinocularPose({},compose(motion,rotatedGrip),{},compose(motion,differentAim),
            false,true,false,true,false,true);
        expect(movedRig&&gripOwned&&same(movedRig->body.position,compose(motion,gripOwned->body).position)
            &&same(movedRig->ray.direction,rotate(motion.orientation,gripOwned->ray.direction)),
            "binocular attachment and ray share one transform during hand motion");
        const auto projectLandmark=[](Pose hand,Vec3 landmark){
            const auto device=solveBinocularPose({},hand,{},hand,false,true,false,true,false,true);
            const auto camera=binocularSceneView(*device,2);
            const auto nativeCamera=nativeEyePose({}, {},camera->pose);
            const auto nativePoint=nativeTrackedPose({}, {},Pose{{},landmark});
            const auto point=compose(inverse(nativeCamera),nativePoint).position;
            std::array<float,16> projection{};projection[11]=1;
            setEyeProjection(projection,camera->fov);
            return Vec3{point.x*projection[0]/point.z,point.y*projection[5]/point.z,point.z};
        };
        const auto centerLandmark=projectLandmark(Pose{{},{0,0,-.2f}},{0,0,-20});
        const auto rightMove=projectLandmark(Pose{{},{.1f,0,-.2f}},{0,0,-20});
        const auto upMove=projectLandmark(Pose{{},{0,.1f,-.2f}},{0,0,-20});
        const auto rightPan=projectLandmark(Pose{{0,-.0436194f,0,.9990482f},{0,0,-.2f}},{0,0,-20});
        expect(centerLandmark.z>0&&rightMove.x<centerLandmark.x&&rightPan.x<centerLandmark.x
            &&upMove.y<centerLandmark.y,
            "moving or panning the optic right moves a fixed landscape landmark left; lifting moves it down");
        std::array<float,16> portalProjection{};portalProjection[11]=1;
        setEyeProjection(portalProjection,{-.8f,.8f,.7f,-.7f});
        const auto rightRim=nativeTrackedPose({}, {},Pose{{},{.02f,0,-.2f}}).position;
        const auto leftRim=nativeTrackedPose({}, {},Pose{{},{-.02f,0,-.2f}}).position;
        expect(rightRim.x*portalProjection[0]/rightRim.z>leftRim.x*portalProjection[0]/leftRim.z,
            "positive aperture X lands on screen right, so source U must increase without a mirror");
        OpticGate gate;
        auto sample=gate.update(head,eyes,left,right,aim,aim,true,true,true,true,true,true,true,100,1);
        expect(!sample.active&&!sample.aligned,"a device held below the face cannot open binocular mode");
        const Pose nearLeft{{},{-.30f,1.25f,.18f}},nearRight{{},{0,1.54f,-.01f}};
        const auto nearPose=solveBinocularPose(nearLeft,nearRight,nearLeft,nearRight,
            true,true,true,true,true,true);
        std::array<EyeView,2> nearEyes{};
        if(nearPose){
            nearEyes={{{nearPose->leftEyepiece,eyes[0].fov},{nearPose->rightEyepiece,eyes[1].fov}}};
        }
        sample=gate.update(head,nearEyes,nearLeft,nearRight,nearLeft,nearRight,true,true,true,true,true,true,true,111,1);
        expect(sample.active&&sample.opened&&sample.pose.kind==OpticKind::binocular,
            "binocular mode opens only when both authored eyepieces meet the two eyes");
        sample=gate.update(head,nearEyes,nearLeft,nearRight,nearLeft,nearRight,true,true,true,true,true,true,true,122,1);
        expect(sample.active&&!sample.opened,"held eye relief does not re-open or flicker the native optic state");
        const Pose loweredRight{{},{0,1.54f,.3f}};
        sample=gate.update(head,nearEyes,nearLeft,loweredRight,nearLeft,loweredRight,true,true,true,true,true,true,true,133,1);
        expect(sample.closed&&!sample.active,"leaving eye relief closes the optic instead of retaining zoom");

    Pose physicalHead{{},{}};
    std::array<EyeView,2> physicalViews{{
        {Pose{{},{-.032f,0,0}},EyeFov{-.8f,.8f,.7f,-.7f}},
        {Pose{{},{.032f,0,0}},EyeFov{-.8f,.8f,.7f,-.7f}}
    }};
    OpticGate physicalGate;
    const Pose physicalLeft{{},{-.30f,-.35f,-.35f}},physicalRight{{.61595203f,-.00760909f,.07982154f,.78369236f},{0,-.06f,-.08f}};
    const auto physicalPose=solveBinocularPose(physicalLeft,physicalRight,
        physicalLeft,physicalRight,true,true,true,true,true,true);
    if(physicalPose){
        physicalViews={{{physicalPose->leftEyepiece,physicalViews[0].fov},
                        {physicalPose->rightEyepiece,physicalViews[1].fov}}};
    }
    auto aimed=physicalGate.update(physicalHead,physicalViews,
        physicalLeft,physicalRight,physicalLeft,physicalRight,
        true,true,true,true,true,true,true,1,1);
    const auto nearVec=[](Vec3 a,Vec3 b){return dot(a-b,a-b)<1e-8f;};
    expect(aimed.active,"a valid paired device pose activates the optic gate");
    const auto camera2=binocularSceneView(aimed.pose,2);
    const auto camera4=binocularSceneView(aimed.pose,4);
    expect(camera2&&camera4&&near(std::tan(camera2->fov.right),2*std::tan(camera4->fov.right)),
        "4x renders twice the angular detail of 2x in the dedicated lens camera");
    const auto objectiveAdvance=camera2?camera2->pose.position-aimed.pose.ray.origin:Vec3{};
    expect(camera2&&dot(objectiveAdvance,aimed.pose.ray.direction)>.10f
        &&nearVec(objectiveAdvance,aimed.pose.ray.direction*dot(objectiveAdvance,aimed.pose.ray.direction))
        &&nearVec(rotate(camera2->pose.orientation,{0,0,-1}),aimed.pose.ray.direction),
        "lens camera clears the housing at the front glass on the same marking axis");
    auto supportController=compose(aimed.pose.body,Pose{{},binocularSupportSocket});
    supportController.position.x-=.025f;
    const auto supported=solveBinocularPose(supportController,physicalRight,
        supportController,physicalRight,true,true,true,true,true,true);
    expect(supported&&supported->supportHeld&&nearVec(supported->body.position,aimed.pose.body.position)
        &&nearVec(compose(inverse(supported->body),supported->supportGrip).position,binocularSupportSocket),
        "a nearby supporting hand cups the side without pulling the primary grip");
    supportController.position.x-=.25f;
    const auto detached=solveBinocularPose(supportController,physicalRight,
        supportController,physicalRight,true,true,true,true,true,true);
    expect(detached&&!detached->supportHeld&&nearVec(detached->body.position,aimed.pose.body.position),
        "pulling the support hand away releases its cup and keeps one-handed carry");
    auto moved=aimed.pose;moved.rightEyepiece.position.x+=.25f;
    const auto carriedCamera=binocularSceneView(moved,2);
    expect(carriedCamera&&camera2&&near(carriedCamera->pose.position.x-camera2->pose.position.x,.25f),
        "the lens camera follows hand motion independently of either HMD eye");
    {
        const Pose reference{{.61595203f,-.00760909f,.07982154f,.78369236f},{.10f,0,-.04f}};
        const auto close=solveBinocularPose(physicalLeft,reference,physicalLeft,reference,true,true,true,true,false,true);
        const auto safe=close?binocularFaceSafeGrip(physicalHead,reference,*close):reference;
        const auto protectedPose=solveBinocularPose(physicalLeft,safe,physicalLeft,safe,true,true,true,true,false,true);
        expect(protectedPose&&protectedPose->rightEyepiece.position.z<=-binocularFaceClearance+.0001f,
            "bringing the housing through the eye stops at face clearance without fading");
        const auto unchanged=protectedPose?binocularFaceSafeGrip(physicalHead,safe,*protectedPose):reference;
        expect(nearVec(unchanged.position,safe.position),"face contact is stable and does not push the palm again each frame");
        auto carry=reference;carry.position.z=-.4f;
        const auto carryPose=solveBinocularPose(physicalLeft,carry,physicalLeft,carry,true,true,true,true,false,true);
        expect(carryPose&&nearVec(binocularFaceSafeGrip(physicalHead,carry,*carryPose).position,carry.position),
            "face clearance leaves normal one-handed carry at the tracked palm");
        const Pose eyeGrip{{},{.149788f,-.007438f,-.09512f}};
        const Pose eyeAim{{.70710678f,0,0,.70710678f},eyeGrip.position};
        const auto nearEye=solveBinocularPose(physicalLeft,eyeGrip,physicalLeft,eyeAim,true,true,true,true,false,true,
            binocularGripRotation(-90,0,0));
        const auto nearSafe=nearEye?binocularFaceSafeGrip(physicalHead,nearEye->primaryGrip,*nearEye):eyeGrip;
        const auto nearAttached=nearEye?attachBinocularToPalm(*nearEye,nearSafe):std::nullopt;
        expect(nearEye&&nearAttached&&near(nearAttached->rightEyepiece.position.z,-.04f),
            "four-centimetre binocular eye distance follows the hand instead of stopping at optical eye relief");
        expect(nearEye&&nearAttached&&nearVec(nearEye->primaryGrip.position,nearAttached->primaryGrip.position)
            &&nearVec(nearEye->ray.origin,nearAttached->ray.origin),
            "close binocular glass, housing, palm and marking ray remain one attachment");
    }
    expect(!binocularSceneView(aimed.pose,std::numeric_limits<float>::quiet_NaN())
        &&!binocularSceneView(aimed.pose,0),"invalid optical powers cannot reach native projection");
    {
        const auto eye=compose(aimed.pose.rightEyepiece,Pose{{},{0,0,binocularEyeRelief}});
        expect(binocularEyeVisible(aimed.pose,eye),"the eye behind the binocular pupil receives its image");
        expect(binocularEyeVisible(aimed.pose,compose(eye,Pose{{},{.032f,.015f,.06f}})),
            "binocular image tolerates a natural raised-hand offset beyond the small glass radius");
        expect(binocularEyeVisible(aimed.pose,compose(eye,Pose{{},{0,0,.3f}})),
            "carried binocular glass stays live beyond the old proximity cutoff");
        const auto fartherEye=compose(aimed.pose.rightEyepiece,Pose{{},{0,0,.35f}});
        expect(binocularEyeVisible(aimed.pose,fartherEye,.30f)&&binocularEyeVisible(aimed.pose,fartherEye,.40f),
            "eye-distance preference does not toggle presentation on the physical glass");
        expect(!binocularEyeVisible(aimed.pose,fartherEye,std::numeric_limits<float>::quiet_NaN()),
            "invalid binocular distance cannot open the lens");
        expect(binocularEyeVisible(aimed.pose,compose(eye,Pose{{},{.064f,0,0}})),
            "sideways head motion cannot blink the binocular glass");
        expect(binocularEyeVisible(aimed.pose,compose(aimed.pose.rightEyepiece,Pose{{},{0,0,.01f}})),
            "close binocular glass remains live; near-plane clipping confines its pixels");
        expect(!binocularEyeVisible(aimed.pose,compose(aimed.pose.rightEyepiece,Pose{{},{0,0,-.1f}})),
            "a reversed binocular has no image through its front");
    }
    const auto originalHead=physicalHead;const auto originalViews=physicalViews;
    expect(validateBinocularViews(aimed,physicalHead,physicalViews),
        "active binoculars accept their physical eyepiece pair without changing stereo origins");
    expect(nearVec(physicalHead.position,originalHead.position)
        &&nearVec(physicalViews[0].pose.position,originalViews[0].pose.position)
        &&nearVec(physicalViews[1].pose.position,originalViews[1].pose.position),
        "active binoculars preserve the head-derived stereo transaction");
        sample=gate.update(head,eyes,nearLeft,nearRight,aim,aim,true,false,true,false,true,true,true,144,1);
        expect(!sample.active&&!sample.pose.tracked,"lost primary hand tracking fails closed");
    }
    {
        RigCommands commands;
        GamepadSample input{0x4000,220,0,0,20000,25000,0};
        auto result=commands.update(input,true,1000,990);
        expect(result.active&&result.gamepad.buttons==0x100&&result.gamepad.leftY==20000&&result.gamepad.rightX==0,
            "Commands preserves walking but consumes navigation held during opening");
        input.buttons=0;input.rightX=0;commands.update(input,true,1020,1010);
        input.rightY=25000;result=commands.update(input,true,1030,1020);
        expect(result.active&&result.gamepad.rightY==25000&&result.gamepad.rightX==0,
            "X can be released and upward command navigation stays upward");
        input.rightTrigger=255;result=commands.update(input,true,1040,1030);
        expect(result.gamepad.buttons==0x180&&result.gamepad.rightTrigger==0,"command confirm cannot fire the weapon");
        expect(result.gamepad.rightY==25000,"native command confirmation includes its selected direction");
        result=commands.update(input,true,1150,1140);
        expect(result.gamepad.buttons==0x100,"holding command confirm cannot repeat it");
        expect(result.gamepad.rightX==0&&result.gamepad.rightY==0,"held selection cannot turn the camera after command confirmation");
        input.leftTrigger=0;result=commands.update(input,true,1160,1150);
        expect(!result.active&&result.exclusive&&result.gamepad.rightTrigger==0&&result.gamepad.rightY==0&&result.gamepad.leftY==20000,
            "release closes commands without leaking held fire or turning");
        commands.update({},true,1170,1160);input={0x4000,220};commands.update(input,true,1180,1170);
        commands.suspend();expect(!commands.update(input,true,1190,1180).active,"tracking or focus loss requires a fresh commands chord");
    }
    {
        RigCommands commands;
        commands.observeGameplay({0,255,0},true);
        auto result=commands.update({0x4000,255},true,1000,990,255,0);
        expect(result.active&&result.weaponReady&&result.gamepad.leftTrigger==255&&!result.gamepad.rightTrigger,
            "opening interrogation Commands preserves an already-readied hold-up weapon");
        commands.update({0,255},true,1020,1010,255,0);
        result=commands.update({0,255,255,0,0,25000},true,1040,1030,255,255);
        expect(result.gamepad.buttons==0x180&&result.gamepad.leftTrigger==255&&!result.gamepad.rightTrigger,
            "interrogation confirmation retains aim and cannot fire even with physical trigger held");
        result=commands.update({},true,1060,1050,255,255);
        expect(!result.active&&result.exclusive&&result.weaponReady&&result.gamepad.leftTrigger==255&&!result.gamepad.rightTrigger,
            "closing Commands keeps the hold-up aim without admitting a newly held attack");
        commands.suspend();
        result=commands.update({0x4000,255},true,1080,1070,255,255);
        expect(!result.weaponReady&&!result.gamepad.leftTrigger&&!result.gamepad.rightTrigger,
            "focus suspension clears all command combat continuity");
    }
    {
        RigCommands commands;
        commands.observeGameplay({0,0,230},false);
        auto result=commands.update({0x4000,255},true,1000,990,0,230);
        expect(result.active&&!result.weaponReady&&!result.gamepad.leftTrigger&&result.gamepad.rightTrigger==230,
            "opening interrogation retains the previously published lowered-weapon CQC hold");
        commands.update({0,255},true,1020,1010,0,230);
        result=commands.update({0,255,255,0,0,0,25000},true,1040,1030,0,230);
        expect(result.gamepad.rightTrigger==230&&result.gamepad.buttons==0x180,
            "CQC hold and one command confirmation use independent native inputs");
        result=commands.update({0,255},true,1060,1050,0,0);
        expect(!result.gamepad.rightTrigger,"releasing the physical CQC trigger releases native restraint immediately");
        result=commands.update({0,255},true,1080,1070,0,255);
        expect(!result.gamepad.rightTrigger,"Commands cannot re-grab or attack after a released CQC hold");
        result=commands.update({0,255},false,1100,1090,0,255);
        expect(!result.gamepad.rightTrigger&&!result.active,"unavailable Commands cannot retain an attack");
        commands.reset();
        result=commands.update({0x4000,255},true,1200,1190,255,255);
        expect(!result.gamepad.leftTrigger&&!result.gamepad.rightTrigger,
            "fresh aim and attack on the menu-opening frame do not acquire combat ownership");
    }
    {
        MotionMelee melee;const Pose head{{},{0,1.6f,0}};
        const uint64_t actorTag=(uint64_t{1}<<63)|(uint64_t{1}<<59);
        for(uint64_t index=0;index<512;++index){
            expect(protectedMotionMeleeTarget(actorTag|(19u<<9)|index),"every D-Dog ID ignores automatic hand/weapon strikes");
            expect(protectedMotionMeleeTarget(actorTag|(20u<<9)|index),"puppies ignore automatic hand/weapon strikes");
        }
        for(uint64_t type:{13u,18u,39u,40u,41u,42u})
            expect(protectedMotionMeleeTarget(actorTag|(type<<9)),"companion horse and Quiet types ignore automatic strikes");
        for(uint64_t type=28;type<=34;++type)
            for(uint64_t index=0;index<512;++index)
                expect(protectedMotionMeleeTarget(actorTag|(type<<9)|index),"non-hostile wildlife cannot be hit by an automatic hand strike");
        expect(!protectedMotionMeleeTarget(actorTag|(2u<<9)),"enemy soldiers retain native motion-melee processing");
        expect(!protectedMotionMeleeTarget(actorTag|(25u<<9)),"hostile Quiet encounter retains native rules");
        expect(!protectedMotionMeleeTarget(uint64_t{19}<<9),"untagged scene geometry is not a companion");
        Pose fist{{},{-.22f,1.25f,-.2f}};
        melee.update(head,fist,true,1000,1);
        unsigned starts{},sweeps{};
        for(unsigned n=1;n<=18;++n){
            fist.position.z=-.2f-.018f*n;
            const auto hit=melee.update(head,fist,true,1000+11*n,1);
            starts+=hit.started;sweeps+=hit.strike;
            if(hit.strike)expect(hit.curl==1&&same(hit.end,fist.position),"motion strike closes the fist at its actual contact point");
        }
        expect(starts==1&&sweeps>1,"one button-free punch keeps a contact window without repeating its stroke");
        fist.position.z+=.04f;
        expect(!melee.update(head,fist,true,1209,1).strike,"returning a fist does not strike again");
        expect(!melee.update(head,fist,true,1800,1).strike,"tracking gaps rebaseline rather than inventing velocity");
        fist.position.z-=.5f;
        expect(!melee.update(head,fist,true,1811,1).strike,"tracking teleport cannot punch");
        expect(!melee.update(head,fist,true,1822,2).strike,"recenter starts a new motion baseline");
        melee.reset();Pose walkingHead=head;fist.position={-.22f,1.25f,-.2f};
        for(unsigned n=0;n<25;++n){
            walkingHead.position.z=-.03f*n;fist.position.z=walkingHead.position.z-.2f;
            expect(!melee.update(walkingHead,fist,true,2000+11*n,1).strike,"walking moves head and fist together without a punch");
        }
        melee.reset();fist.position={-.22f,1.25f,-.2f};
        for(unsigned n=0;n<18;++n){
            auto duck=head;duck.position.y+=.02f*n;
            expect(!melee.update(duck,fist,true,3000+11*n,1).strike,"head motion alone cannot turn a stationary hand into a strike");
        }
        melee.reset();unsigned weaponStarts{};
        for(unsigned n=0;n<22;++n){
            const float angle=1.2f-.045f*n;
            const Pose tip{{},{0,1.6f-std::sin(angle)*.6f,-.2f-std::cos(angle)*.6f}};
            weaponStarts+=melee.update(head,tip,true,4000+11*n,1,true).started;
        }
        expect(weaponStarts==1,"swinging an authored weapon tip can strike while its grip pivot stays still");
        expect(!melee.update(head,fist,false,4300,1).strike,"menu and native manipulation disable motion strikes");
    }
    {
        const Pose palm{{},{2,3,4}};
        const auto straight=pointThrow(palm,{},Vec3{3,4,0});
        expect(straight&&same(straight->origin,palm.position)&&same(straight->velocity,{0,0,-5}),
            "grenade starts at rendered palm and uses aim -Z with native speed");
        const Pose aimPitch{{.70710678f,0,0,.70710678f},{.1f,0,-.1f}};
        const auto upward=pointThrow(palm,aimPitch,Vec3{0,0,5});
        expect(upward&&same(upward->origin,palm.position)&&same(upward->velocity,{0,5,0}),
            "controller aim pitch raises grenade arc without moving its palm origin");
        const Pose turned{{0,.70710678f,0,.70710678f},{-2,8,1}};
        const auto side=pointThrow(turned,{},Vec3{0,5,0});
        expect(side&&same(side->origin,turned.position)&&same(side->velocity,{-5,0,0}),
            "translated and rotated rendered hand controls grenade launch in world space");
        expect(!pointThrow(palm,{},{}),"zero native grenade speed cannot create a bogus arc");
        expect(!pointThrow(palm,{},Vec3{0,std::numeric_limits<float>::quiet_NaN(),0}),"invalid grenade velocity is rejected");
        expect(!pointThrow(Pose{{0,0,0,0},{}},{},Vec3{1,0,0}),"invalid hand transform is rejected for throwing");
    }

    const EyeFov asymmetric{-0.8f,0.9f,0.75f,-0.7f};
    {
        expect(phantomPainRender.cameraFocalScale==0x10c&&groundZeroesRender.cameraFocalScale==0xf8,
            "TPP and GZ native focal parameters use independently mapped fields");
        for(float aspect:{.8f,1.f,1.4f})for(float focal:{.5f,1.f,2.f}){
            std::array<float,16> native{};native[0]=-focal;native[5]=focal*aspect*16.f/9.f;native[11]=1;native[14]=.1f;
            for(const auto fov:{EyeFov{-.94f,.94f,.87f,-.87f},EyeFov{-.07f,.07f,.07f,-.07f}}){
                const auto parameters=nativeProjectionScales(focal,aspect,native,fov);
                auto target=native;setEyeProjection(target,fov);
                expect(parameters&&near(-parameters->focal,target[0])&&near(parameters->focal*parameters->aspect*16.f/9.f,target[5]),
                    "native focal/aspect inputs reproduce both normal-eye and scope raster scales");
            }
            expect(!nativeProjectionScales(focal,aspect,native,asymmetric),"asymmetric optical crop is not misrepresented as scalar native FOV");
            expect(!nativeProjectionScales(NAN,aspect,native,{-.8f,.8f,.8f,-.8f}),"invalid native focal scale is not written");
        }
    }
    expect(phantomPainRender.cameraNearPlane==0x168&&groundZeroesRender.cameraNearPlane==0,
        "tracked near-plane field is verified only for the TPP graphics camera");
    expect(near(trackedNearPlane(.069f,4000.f),.02f),"close held geometry uses a two-centimeter native near plane");
    expect(trackedNearPlane(.01f,4000.f)==.01f,"an already closer native plane is preserved");
    expect(trackedNearPlane(.1f,.05f)==.1f&&trackedNearPlane(0,4000.f)==0&&trackedNearPlane(-1,4000.f)==-1,
        "invalid native plane ordering is not silently rewritten");
    expect(trackedNearPlane(.1f,INFINITY)==.1f&&std::isnan(trackedNearPlane(NAN,4000.f)),
        "nonfinite native depth inputs are left to their original caller");
    for(float nativeFar:{100.f,4000.f})for(float farDepth:{-1.f,0.f}){
        const float nativeNear=trackedNearPlane(.069f,nativeFar);
        std::array<float,16> depth{};depth[11]=1;
        depth[10]=(farDepth*nativeFar-nativeNear)/(nativeFar-nativeNear);
        depth[14]=(1-farDepth)*nativeNear*nativeFar/(nativeFar-nativeNear);
        const auto originalDepth=depth;
        expect(setEyeProjection(depth,asymmetric)&&depth[10]==originalDepth[10]&&depth[14]==originalDepth[14],
            "tracked angular projection preserves native-built close clip and reversed-Z coefficients");
        expect(near(depth[10]+depth[14]/nativeNear,1)&&near(depth[10]+depth[14]/nativeFar,farDepth),
            "close native projection retains its original far plane and clip/GPU depth endpoints");
    }
    std::array<float,16> projection{1,0,0,0,0,1,0,0,0,0,-0.0002f,1,0,0,0.1f,0};
    expect(setEyeProjection(projection,asymmetric),"native perspective accepts runtime asymmetric field of view");
    {
        const Pose head{{0,.258819f,0,.965926f},{3,2,1}};
        std::array<EyeView,2> eyes{{{head,asymmetric},{head,{-asymmetric.right,-asymmetric.left,asymmetric.up,asymmetric.down}}}};
        auto visibility=projection;
        expect(widenVisibilityProjection(visibility,head,eyes),"visibility can enclose both asymmetric eyes in the source-head frame");
        expect(near(visibility[8],0)&&near(visibility[9],0)&&std::abs(visibility[0])<std::abs(projection[0])
            &&visibility[10]==projection[10]&&visibility[14]==projection[14],
            "visibility adds symmetric coverage while preserving native depth");
        for(const auto& eye:eyes)for(float angle:{eye.fov.left,eye.fov.right})
            expect(std::abs(std::tan(angle)*visibility[0])<1,"both eye edges fit inside the visibility frustum");
        auto repeated=visibility;
        expect(widenVisibilityProjection(repeated,head,eyes)&&repeated==visibility,"repeated visibility preparation cannot keep expanding coverage");
        eyes[1].pose.orientation={0,0,0,0};
        expect(!widenVisibilityProjection(repeated,head,eyes)&&repeated==visibility,"invalid eye tracking leaves native visibility untouched");
    }
    const auto ndc=[&](float x,float y,float z){return Vec3{(x*projection[0]+z*projection[8])/z,(y*projection[5]+z*projection[9])/z,(z*projection[10]+projection[14])/z};};
    expect(near(ndc(-std::tan(asymmetric.left)*3,0,3).x,-1),"left eye frustum boundary projects to left edge");
    expect(near(ndc(-std::tan(asymmetric.right)*3,0,3).x,1),"right eye frustum boundary projects to right edge");
    expect(near(ndc(0,std::tan(asymmetric.up)*3,3).y,1),"upper eye frustum boundary projects to upper edge");
    expect(near(ndc(0,std::tan(asymmetric.down)*3,3).y,-1),"lower eye frustum boundary projects to lower edge");
    expect(near(projection[10],-0.0002f)&&near(projection[14],0.1f),"eye optics preserve native depth convention");
    const EyeFov opticalLeft{-.9424778f,.6981317f,.8726646f,-.8552113f};
    const EyeFov opticalRight{-opticalLeft.right,-opticalLeft.left,opticalLeft.up,opticalLeft.down};
    for(const auto eye:{opticalLeft,opticalRight})for(float power:{1.f,2.f,4.f}){
        const auto narrow=opticalFov(eye,power),restored=narrow?opticalFov(*narrow,1/power):std::nullopt;
        expect(narrow&&restored&&near(std::tan(narrow->right)*power,std::tan(eye.right))
            &&near(restored->left,eye.left)&&near(restored->right,eye.right)
            &&near(restored->up,eye.up)&&near(restored->down,eye.down),"stereo magnification scales rays and preserves each eye's asymmetric optical center");
    }
    expect(!opticalFov(opticalLeft,0)&&!opticalFov(opticalLeft,5)
        &&!opticalFov(opticalLeft,std::numeric_limits<float>::quiet_NaN()),"invalid magnification cannot enter scene projection");
    const auto completeFov=enclosingEyeFov(opticalLeft);
    expect(completeFov&&near(completeFov->left,-completeFov->right)&&near(completeFov->up,-completeFov->down),
        "lighting coverage has a centered enclosing render field");
    const auto cropLeft=eyeImageRegion(*completeFov,opticalLeft,1280,720);
    const auto cropRight=eyeImageRegion(*completeFov,opticalRight,1280,720);
    expect(cropLeft&&cropRight&&cropLeft->x==0&&cropRight->x>0&&cropLeft->width==cropRight->width,
        "left and right runtime optical centers select opposite texture regions");
    const auto rayFromPixel=[](EyeFov f,double x,double y,double w,double h){
        return Vec3{float(std::tan(f.left)+(std::tan(f.right)-std::tan(f.left))*x/w),
            float(std::tan(f.up)-(std::tan(f.up)-std::tan(f.down))*y/h),-1};};
    for(const auto& region:{*cropLeft,*cropRight})for(float u:{0.f,.13f,.5f,1.f})for(float v:{0.f,.37f,1.f}){
        const auto sourceRay=rayFromPixel(*completeFov,region.x+u*region.width,region.y+v*region.height,1280,720);
        const auto displayedRay=rayFromPixel(region.fov,u,v,1,1);
        expect(same(sourceRay,displayedRay),"cropped pixels retain their exact original angular ray");
    }
    expect(cropLeft->fov.left<=opticalLeft.left&&cropLeft->fov.right>=opticalLeft.right
        &&cropLeft->fov.up>=opticalLeft.up&&cropLeft->fov.down<=opticalLeft.down,
        "integer rounding covers every requested eye ray");
    expect(!eyeImageRegion(opticalLeft,*completeFov,1280,720),"missing rendered coverage is rejected rather than stretched");
    expect(!eyeImageRegion(*completeFov,opticalLeft,0,720),"zero-size eye cannot produce a projection region");
    const auto sameSize=eyeImageRegion(opticalLeft,opticalLeft,1280,720);
    expect(sameSize&&sameSize->x==0&&sameSize->y==0&&sameSize->width==1280&&sameSize->height==720,
        "unchanged optics retain the full texture");
    const std::array<float,16> identityMatrix{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    const EyeFov squareEye{-0.785398163f,0.785398163f,0.785398163f,-0.785398163f};
    const auto panelClip=[](const std::array<float,16>& m,float x,float y){
        const float w=x*m[3]+y*m[7]+m[15];
        return Vec3{(x*m[0]+y*m[4]+m[12])/w,(x*m[1]+y*m[5]+m[13])/w,(x*m[2]+y*m[6]+m[14])/w};};
    {
        auto authored=identityMatrix;authored[0]=-1.19531f;authored[5]=2.12499f;
        auto eye=authored;eye[0]=-.726542f;eye[5]=.8391f;
        auto canvas=identityMatrix;canvas[0]=-.015625f;canvas[5]=.02777778f;
        const float inheritedAspect=(-eye[5]/eye[0])/(-authored[5]/authored[0]);
        auto replayCanvas=canvas;replayCanvas[5]*=inheritedAspect;
        const auto restored=nativeUiCanvasProjection(replayCanvas,eye);
        // The status anchor is authored on a 128 x 72 canvas. An eye replay
        // must not move it upward by 8 cm on a 1.2 m layout or squash its text.
        const auto anchor=panelClip(restored,-46.08f,-25.2f);
        expect(near(anchor.x,.72f)&&near(anchor.y,-.70f),
            "eye viewport aspect cannot displace the authored wrist HUD anchor");
        expect(near(restored[5],canvas[5])&&near(restored[0],canvas[0]),
            "native HUD retains its authored text proportions under wide eye replay");
        auto zoomedEye=eye;zoomedEye[0]*=4;zoomedEye[5]*=4;
        expect(nativeUiCanvasProjection(replayCanvas,zoomedEye)==restored,
            "canvas correction depends on aspect, not scene magnification");
        auto widescreen=identityMatrix;widescreen[0]=-1;widescreen[5]=128.f/72.f;
        expect(nativeUiCanvasProjection(canvas,widescreen)==canvas,
            "unchanged viewport preserves every native canvas projection term");
        auto invalidEye=eye;invalidEye[0]=0;
        expect(nativeUiCanvasProjection(canvas,invalidEye)==canvas,
            "invalid scene projection cannot corrupt a native UI canvas");
        for(const float renderAspect:{1680.f/1760.f,1.f,2560.f/1440.f}){
            auto renderEye=authored;renderEye[0]=-renderEye[5]/renderAspect;
            auto renderCanvas=canvas;renderCanvas[5]=-.015625f*renderEye[5]/renderEye[0];
            const auto fitted=nativeUiCanvasProjection(renderCanvas,renderEye);
            const auto statusAnchor=panelClip(fitted,-46.08f,-25.2f);
            expect(near(statusAnchor.x,.72f)&&near(statusAnchor.y,-.70f)
                &&near(fitted[5],canvas[5]),
                "square and runtime-recommended eye targets preserve the authored forearm anchor and text aspect");
        }
    }
    const Pose spatialPanel{{0,1,0,0},{0,0,2}};
    const auto uiProjection=uiPanelProjection(identityMatrix,identityMatrix,squareEye,spatialPanel,0.8f,0.4f);
    expect(uiProjection&&same(panelClip(*uiProjection,1,1),{0.2f,0.1f,0.015f}),
           "native UI corners project at the physical panel size and distance");
    auto leftView=identityMatrix,rightView=identityMatrix;leftView[12]=-0.032f;rightView[12]=0.032f;
    const auto leftPanel=uiPanelProjection(identityMatrix,leftView,squareEye,spatialPanel,0.8f,0.4f);
    const auto rightPanel=uiPanelProjection(identityMatrix,rightView,squareEye,spatialPanel,0.8f,0.4f);
    expect(leftPanel&&rightPanel&&near(panelClip(*leftPanel,0,0).x-panelClip(*rightPanel,0,0).x,0.032f),
           "forearm UI has real per-eye disparity rather than a shared flat overlay");
    const Pose setupPanel{{0,1,0,0},{0,0,1.3f}};
    const auto titleUi=spatialUiProjection(identityMatrix,SpatialUiLayout::frontEndMenu);
    const auto pauseUi=spatialUiProjection(identityMatrix,SpatialUiLayout::pauseMenu);
    const auto avatarUi=spatialUiProjection(identityMatrix,SpatialUiLayout::avatarEditor);
    const auto wristUi=selectSpatialUiLayout(false,false,true,false,true);
    expect(wristUi==SpatialUiLayout::wristMenu
        &&spatialUiProjection(identityMatrix,wristUi)==identityMatrix
        &&spatialUiPlaneCenterX(wristUi)==0,
        "wrist Pause keeps the complete native canvas centered on its arm anchor");
    expect(selectSpatialUiLayout(false,false,true,false)==SpatialUiLayout::pauseMenu
        &&selectSpatialUiLayout(false,false,true,true)==SpatialUiLayout::frontEndMenu
        &&selectSpatialUiLayout(true,false,true,false)==SpatialUiLayout::frontEndMenu
        &&selectSpatialUiLayout(false,true,false,false)==SpatialUiLayout::avatarEditor,
        "pause, handheld iDroid, title and avatar editor select distinct spatial UI layouts");
    const auto titleLeft=uiPanelProjection(titleUi,leftView,squareEye,setupPanel,1.6f,0.9f);
    const auto titleRight=uiPanelProjection(titleUi,rightView,squareEye,setupPanel,1.6f,0.9f);
    const auto pauseLeft=uiPanelProjection(pauseUi,leftView,squareEye,setupPanel,1.6f,0.9f,
        spatialUiPlaneCenterX(SpatialUiLayout::pauseMenu));
    const auto pauseRight=uiPanelProjection(pauseUi,rightView,squareEye,setupPanel,1.6f,0.9f,
        spatialUiPlaneCenterX(SpatialUiLayout::pauseMenu));
    const auto avatarLeft=uiPanelProjection(avatarUi,leftView,squareEye,setupPanel,1.6f,0.9f);
    const auto avatarRight=uiPanelProjection(avatarUi,rightView,squareEye,setupPanel,1.6f,0.9f);
    expect(titleLeft&&titleRight&&panelClip(*titleLeft,1,0).x<1&&panelClip(*titleRight,1,0).x<1,
        "front-end Continue menu keeps the full 16:9 canvas inside both stereo eyes");
    expect(pauseLeft&&pauseRight&&panelClip(*titleLeft,2,0).x>1&&panelClip(*titleRight,2,0).x>1
        &&panelClip(*pauseLeft,0,0).x>-1&&panelClip(*pauseRight,0,0).x>-1
        &&panelClip(*pauseLeft,2,0).x<1&&panelClip(*pauseRight,2,0).x<1,
        "right-authored pause rows are recentered and kept inside both stereo eyes");
    expect(avatarLeft&&avatarRight&&panelClip(*avatarLeft,.5f,0).x<1&&panelClip(*avatarRight,.5f,0).x<1
        &&panelClip(*avatarLeft,.5f,0).x>panelClip(*titleLeft,.5f,0).x
        &&panelClip(*avatarRight,.5f,0).x>panelClip(*titleRight,.5f,0).x,
        "centered avatar/name controls stay enlarged and visible on the bed quad in both eyes");
    expect(!uiPanelProjection(identityMatrix,identityMatrix,squareEye,spatialPanel,0,0.4f),
           "invalid physical UI extent cannot replace native projection");
    const std::array<Pose,2> panelEyes{Pose{{},{-.032f,0,0}},Pose{{},{.032f,0,0}}};
    expect(panelFacesBothEyes(Pose{{},{0,0,-.4f}},panelEyes),"front-facing wrist display is visible to the stereo pair");
    expect(!panelFacesBothEyes(Pose{{0,1,0,0},{0,0,-.4f}},panelEyes),"back-facing wrist display is hidden in both eyes");
    const Pose edgePanel{{0,.70710678f,0,.70710678f},{0,0,-.4f}};
    expect(!panelFacesBothEyes(edgePanel,panelEyes),"plane separating the eyes cannot become a monocular wrist display");
    auto invalidEyes=panelEyes;invalidEyes[1].position.x=std::numeric_limits<float>::quiet_NaN();
    expect(!panelFacesBothEyes(Pose{{},{0,0,-.4f}},invalidEyes),"invalid eye pose cannot expose half of the wrist display");
    auto ortho=projection;ortho[11]=0;ortho[15]=1;
    expect(!setEyeProjection(ortho,asymmetric),"orthographic pass cannot be mistaken for scene projection");
    std::array<EyeFrame,2> pair{};
    for(uint32_t n=0;n<2;++n)pair[n]={EyeView{Pose{{},{n?0.032f:-0.032f,0,0}},asymmetric},9,31,4,100,n,true,true,asymmetric};
    expect(readyEyePair(pair,4,110),"both native eyes from one simulation and tracking transaction are eligible");
    pair[0].magnification=2;
    expect(!readyEyePair(pair,4,110),"different zoom powers cannot be submitted as one stereo pair");
    pair[1].magnification=2;
    expect(readyEyePair(pair,4,110),"matching zoom powers retain independent stereo eye poses");
    pair[0].magnification=pair[1].magnification=1;
    pair[1].sourceSequence=10;
    expect(!readyEyePair(pair,4,110),"alternate-eye consecutive simulation frames are rejected");
    pair[1].sourceSequence=9;pair[1].trackingSequence=32;
    expect(!readyEyePair(pair,4,110),"different pose generations cannot be submitted as one native frame");
    pair[1].trackingSequence=31;pair[0].joined=false;
    expect(!readyEyePair(pair,4,110),"unjoined native image never receives newer tracking metadata");
    pair[0].joined=true;
    expect(!readyEyePair(pair,5,110),"old activation cannot survive recenter");
    expect(!readyEyePair(pair,4,251),"stale incoming eye images cannot be admitted");
    expect(!readyEyePair(pair,4,400,501),"new-source admission cannot request unbounded freshness");
    AcceptedStereoFrame displayed;
    expect(!displayed.presentable(7,4,110),"no accepted image cannot become a stall fallback");
    expect(displayed.accept(pair,7,4,110),"a fresh joined stereo transaction is accepted");
    expect(displayed.presentable(7,4,601)&&displayed.presentable(7,4,5100),
        "a runtime stall beyond500ms reprojects the accepted image instead of blinking black");
    {
        AcceptedStereoFrame idle;
        expect(idle.accept(pair,7,4,110,42),"idle recovery starts with a valid joined stereo pair in its reference space");
        HeadCameraStatus lost{};lost.active=true;lost.suspended=true;lost.activation=4;
        lost.reason=HeadCameraStop::trackingLost;
        expect(mayReprojectAcceptedStereo(lost)&&idle.presentable(7,4,7110,42)
            &&!mayAcceptNewStereoPair(lost),"head or view validity loss retains original stereo without admitting untracked pixels");
        expect(!idle.presentable(7,4,7110,43)&&!idle.presentable(7,4,7110)
            &&!idle.presentable(8,4,7110,42)&&!idle.presentable(7,5,7110,42)
            &&!idle.presentable(7,4,99,42),"tracking loss cannot reuse pixels across reference spaces, graphics resets, camera changes or clock reversal");
        expect(idle.eyes()[0].sampleTime==100&&idle.eyes()[0].trackingSequence==31
            &&near(idle.eyes()[0].view.pose.position.x,-.032f)
            &&near(idle.eyes()[1].view.pose.position.x,.032f),"idle retention preserves original time, generation and both eye offsets");
        for(auto reason:{HeadCameraStop::cameraChanged,HeadCameraStop::matrixMismatch,
            HeadCameraStop::clockMismatch,HeadCameraStop::sceneUnavailable,HeadCameraStop::manual}){
            lost.reason=reason;
            expect(!mayReprojectAcceptedStereo(lost),"tracking continuity cannot authorize another camera or invalid scene");
        }
        lost.reason=HeadCameraStop::trackingLost;lost.active=false;
        expect(!mayReprojectAcceptedStereo(lost),"an inactive camera cannot revive stereo after tracking loss");
    }
    HeadCameraStatus delayedMismatch{};delayedMismatch.active=true;delayedMismatch.suspended=true;
    delayedMismatch.reason=HeadCameraStop::rigFrameMismatch;delayedMismatch.activation=4;
    delayedMismatch.rigRejectFlags=10;delayedMismatch.rigAgeMs=151;
    const auto retainedPresentable=[&](HeadCameraStatus status,uint64_t epoch,uint64_t now){
        return mayReprojectAcceptedStereo(status)&&displayed.presentable(epoch,status.activation,now);
    };
    expect(retainedPresentable(delayedMismatch,7,601),
        "an old accepted pair survives only the aged source-pose mismatch stall");
    expect(!mayAcceptNewStereoPair(delayedMismatch),
        "the aged mismatched publication cannot admit new stereo pixels");
    auto rejectedMismatch=pair;
    for(auto& eye:rejectedMismatch){eye.sourceSequence=10;eye.sampleTime=601;}
    rejectedMismatch[1].trackingSequence=32;
    expect(!displayed.accept(rejectedMismatch,7,4,601)&&displayed.eyes()[0].sourceSequence==9,
        "a mismatched new pair cannot replace the retained accepted pair");
    for(const auto [flags,age]:std::array<std::pair<uint32_t,uint64_t>,5>{{{2,100},{3,500},{6,0},{10,150},{11,500}}}){
        auto rejected=delayedMismatch;rejected.rigRejectFlags=flags;rejected.rigAgeMs=age;
        expect(!retainedPresentable(rejected,7,601),"only exact aged rig-reject flags 10 may retain accepted stereo");
    }
    expect(!retainedPresentable(delayedMismatch,8,601),
        "an accepted pair cannot cross a texture epoch during aged mismatch retention");
    delayedMismatch.activation=5;
    expect(!retainedPresentable(delayedMismatch,7,601),
        "an accepted pair cannot cross camera activation during aged mismatch retention");
    delayedMismatch.activation=4;
    expect(!retainedPresentable(delayedMismatch,7,99),
        "an accepted pair cannot survive clock reversal during aged mismatch retention");
    expect(displayed.eyes()[0].sampleTime==100&&displayed.eyes()[0].trackingSequence==31
        &&near(displayed.eyes()[0].view.pose.position.x,-.032f),
        "retention preserves the exact source time, pose generation and eye pose");
    auto delayed=pair;for(auto& eye:delayed)eye.sourceSequence=10;
    expect(!displayed.accept(delayed,7,4,5100)&&displayed.eyes()[0].sourceSequence==9,
        "an old incoming image cannot replace the last accepted pair during recovery");
    expect(!displayed.presentable(8,4,5100)&&!displayed.presentable(7,5,5100)
        &&!displayed.presentable(7,4,99),"retention fails across texture epochs, camera activations and clock reversal");
    for(auto& eye:delayed)eye.sampleTime=5100;
    delayed[1].trackingSequence=32;
    expect(!displayed.accept(delayed,7,4,5110),"stall recovery still rejects mismatched pose generations");
    delayed[1].trackingSequence=31;
    expect(displayed.accept(delayed,7,4,5110)&&displayed.eyes()[0].sourceSequence==10,
        "the first fresh complete pair replaces the frozen image after a stall");
    displayed.clear();
    expect(!displayed.presentable(7,4,5110),"an inactive screen or graphics reset clears retained gameplay");
    {
        AcceptedStereoFrame handoff;
        expect(handoff.accept(pair,7,4,110,42),"camera handoff starts with a strictly admitted image and reference space");
        expect(!handoff.transitionPresentable(7,5,0,42,610)
            &&!handoff.transitionPresentable(7,5,6,42,610),"unidentified camera changes cannot retain an old scene");
        expect(!handoff.transitionPresentable(8,5,5,42,610)
            &&!handoff.transitionPresentable(7,5,5,43,610)
            &&!handoff.transitionPresentable(7,5,5,0,610),"graphics reset and recenter cannot carry cinematic pixels into another space");
        expect(handoff.transitionPresentable(7,5,5,42,610),"the preceding shot covers an authorized cut after FOX's retired-camera delay");
        expect(!handoff.presentable(7,5,610)&&!readyEyePair(handoff.eyes(),5,610),
            "a frozen cinematic transition is never accepted as fresh gameplay in the new activation");
        expect(handoff.transitionPresentable(7,5,5,42,860)
            &&!handoff.transitionPresentable(7,5,5,42,861),"cinematic carry expires exactly after 250 ms");
        expect(!handoff.transitionPresentable(7,6,6,42,862)
            &&!handoff.transitionPresentable(7,5,5,42,609),"repeated cuts and clock reversal cannot renew a frozen scene");
        expect(handoff.eyes()[0].sourceSequence==9&&handoff.eyes()[0].trackingSequence==31
            &&handoff.eyes()[0].sampleTime==100&&near(handoff.eyes()[0].view.pose.position.x,-.032f),
            "transition retention leaves source identity, timestamps and stereo eye poses intact");
        auto nextShot=pair;
        for(auto& eye:nextShot){eye.activation=5;eye.sourceSequence=10;eye.sampleTime=870;}
        nextShot[1].trackingSequence=32;
        expect(!handoff.accept(nextShot,7,5,870,42)&&!handoff.transitionPresentable(7,5,5,42,870),
            "a broken new stereo join cannot replace old pixels or refresh the transition deadline");
        nextShot[1].trackingSequence=31;
        expect(handoff.accept(nextShot,7,5,870,42)&&handoff.presentable(7,5,870,42)
            &&!handoff.transitionPresentable(7,5,5,42,870),"a fresh complete shot replaces the transition immediately");
        expect(handoff.transitionPresentable(7,6,6,42,900),"a later cut can retain its own newly accepted source image");
        handoff.clear();
        expect(!handoff.transitionPresentable(7,6,6,42,900),"cleared graphics cannot revive a cinematic transition");
    }
    const auto l=nativeEyePose(Pose{{},{10,20,30}},Pose{},Pose{{},{-0.032f,0,0}});
    const auto r=nativeEyePose(Pose{{},{10,20,30}},Pose{},Pose{{},{0.032f,0,0}});
    expect(near(l.position.x,10.032f)&&near(r.position.x,9.968f),"same-frame eye offsets preserve runtime IPD in FOX camera axes");
    expect(near(l.position.y,r.position.y)&&near(l.position.z,r.position.z),"parallel eye cameras do not introduce toe-in or vertical disparity");
    HeadCamera camera;
    {
        HeadCamera loading;loading.configure(true);loading.track({},true,100);loading.toggle();
        const auto before=loading.resolve(7,{},100);
        loading.awaitScene(false);
        expect(loading.active()&&!loading.status().awaitingPlayer
            &&loading.status().activation==before.activation,
            "a source publisher stall alone cannot switch gameplay into a loading quad");
        loading.track({},true,701);
        const auto stalled=loading.resolve(7,{},701);
        expect(stalled.applied&&stalled.activation==before.activation,
            "fresh camera publication after a stall resumes the same gameplay generation");
        loading.awaitScene(true);
        expect(before.applied&&!loading.active()&&loading.status().awaitingPlayer,
            "a native loading screen gets mono pixels without discarding the VR request");
        expect(!mayRetainStereoSurround(loading.status()),
            "a loading frame cannot be duplicated as the stereo surroundings behind itself");
        loading.track({},true,800);const auto after=loading.resolve(7,{},800);
        expect(after.applied&&loading.active()&&after.activation>before.activation,
            "the same native camera resumes with a new image generation after loading");
        loading.awaitScene(true);loading.track({},true,900);
        expect(!loading.resolve(8,{},900).applied&&loading.status().reason==HeadCameraStop::cameraChanged,
            "a loading gap does not authorize adopting an unrelated camera");
        loading.awaitScene(true);
        expect(loading.status().reason==HeadCameraStop::cameraChanged,
            "a verified loading terminal cannot relabel a rejected camera as an accepted scene boundary");
        loading.toggle();loading.resolve(7,{},900);loading.awaitScene(true);loading.toggle();
        expect(!loading.resolve(7,{},900).applied&&!loading.status().awaitingPlayer,
            "manual VR exit remains effective while a loading screen is visible");
    }
    const std::array<float,16> playerRoot{0,0,-1,0,0,1,0,0,1,0,0,0,500,300,1300,1};
    {
        RocketArmCameraOwner owner{0x100,7,7,0x200,0x200,0x120,2,11,-1,true,true};
        expect(rocketArmCameraOwned(owner),"typed native guided-flight producer is eligible");
        const Pose distantFlight{{.616072953f,-.35409385f,.361318499f,.60375458f},{757.784241f,333.007141f,1663.47791f}};
        const auto rigidFlight=rocketArmRenderPose(distantFlight);
        expect(rigidFlight.has_value(),"observed native flight rotation is valid for rigid replay");
        const auto roundTrip=compose(*rigidFlight,inverse(*rigidFlight)).position;
        expect(dot(roundTrip,roundTrip)<.001f*.001f,"native flight replay remains rigid at kilometre-scale coordinates");
        expect(!rocketArmRenderPose(Pose{{0,0,0,0},{}}),"invalid flight rotation is never repaired into an accepted camera");
        auto other=owner;other.mode=10;
        expect(!rocketArmCameraOwned(other),"Rocket Arm preparation is not a projectile feed");
        other=owner;other.producerOwner++;
        expect(!rocketArmCameraOwned(other),"another player's projectile producer cannot own this feed");
        other=owner;other.selectedProducer++;
        expect(!rocketArmCameraOwned(other),"retired camera producer cannot own this feed");
        other=owner;other.overrideMode=11;
        expect(!rocketArmCameraOwned(other),"forced camera modes do not certify native flight");
        other=owner;other.readerVerified=false;
        expect(!rocketArmCameraOwned(other),"unknown native reader cannot enable remote camera acceptance");
        HeadCamera flight;flight.configure(true,1,true);flight.track({},true,100);flight.toggle();
        auto head=identityMatrix;head[13]=1.6f;
        expect(flight.publishPlayerHead(7,8,{},identityMatrix,head,100),"ordinary head publication admitted");
        flight.resolve(7,{},100);
        const Pose projectile{{0,std::sin(.5f),0,std::cos(.5f)},{40,3,20}};
        expect(!flight.publishPlayerHead(7,8,projectile,identityMatrix,head,110),
            "ordinary distant cameras retain the 12 metre rejection");
        flight.track({},true,110);
        expect(flight.publishPlayerHead(7,8,projectile,identityMatrix,head,110,true),
            "verified native flight admits a fresh distant source camera");
        const auto guided=flight.resolve(7,projectile,110);
        expect(guided.applied&&guided.rocketArmGuidance&&guided.rocketArmCamera.position.x==40
            &&std::abs(guided.nativePose.position.x)<1&&std::abs(guided.nativePose.orientation.y)<.001f,
            "projectile view stays separate from the live Snake camera and heading");
        flight.track({},true,300);
        expect(!flight.resolve(7,projectile,300).applied,"remote flight never permits stale player publication");
        flight.publishPlayerHead(7,8,{},identityMatrix,head,310);flight.track({},true,310);
        const auto returned=flight.resolve(7,{},310);
        expect(returned.applied&&!returned.rocketArmGuidance,"native return retires the projectile feed");
    }
    {
        HeadCamera loading;loading.configure(true,1,true);
        const std::array<EyeView,2> eyes{EyeView{Pose{{},{-.032f,0,0}},squareEye},EyeView{Pose{{},{.032f,0,0}},squareEye}};
        ControllerFrame input;input.frontEnd=true;
        auto loadingHead=identityMatrix;loadingHead[13]=1.6f;
        loading.publishPlayerHead(7,8,{},identityMatrix,loadingHead,100);
        loading.trackStereo({},eyes,true,100,input);loading.toggle();
        const auto title=loading.resolve(7,{},100);
        input.loading=true;
        loading.trackStereo({},eyes,true,110,input);
        expect(!loading.resolve(7,{},110).applied
            &&loading.status().reason==HeadCameraStop::playerHeadUnavailable,
            "native camera retirement can precede the XR loading observation");
        loading.awaitScene(false);
        expect(loading.status().reason==HeadCameraStop::playerHeadUnavailable,
            "missing player publication alone does not authorize a loading panel");
        loading.awaitScene(true);
        expect(!loading.resolve(7,{},110).applied&&!loading.active()&&loading.status().awaitingPlayer,
            "a decorative loading camera cannot hide native Start Mission pixels");
        expect(loading.status().reason==HeadCameraStop::sceneUnavailable,
            "verified loading after player retirement must replace the old Pause view with its native panel");
        input.loading=false;input.authoredCamera=true;
        loading.trackStereo({},eyes,true,115,input);
        expect(loading.resolve(7,{},115).applied&&loading.active(),
            "the hospital title/intro remains a tracked stereo scene");
        input.authoredCamera=false;
        input.loading=false;input.frontEnd=false;
        loading.publishPlayerHead(7,8,{},identityMatrix,loadingHead,120);
        loading.trackStereo({},eyes,true,120,input);
        const auto field=loading.resolve(7,{},120);
        expect(field.applied&&field.activation>title.activation&&!field.controllers.frontEnd,
            "loading confirmation resumes a fresh gameplay generation without another VR toggle");
    }
    {
        HeadCamera posture;posture.configure(true,1,true);posture.track({},true,100);posture.toggle();
        auto localHead=identityMatrix;localHead[13]=1.6f;
        posture.publishPlayerHead(7,8,{},identityMatrix,localHead,100);posture.resolve(7,{},100);
        localHead[12]=.8f;localHead[14]=-.5f;
        HeadCameraSample moved;
        for(uint64_t t=120;t<=2120;t+=20){
            posture.track({},true,t);posture.publishPlayerHead(7,8,{},identityMatrix,localHead,t);
            moved=posture.resolve(7,{},t);
        }
        expect(moved.applied&&same(moved.nativePose.position,{.8f,1.85f,-.85f}),
            "a changed stance or mounted head offset cannot remain frozen at its original horizontal anchor");
    }
    {
        HeadCamera steady;steady.configure(true,1,true);steady.track({},true,100);steady.toggle();
        auto gaitHead=identityMatrix;gaitHead[13]=1.6f;
        steady.publishPlayerHead(7,8,{},identityMatrix,gaitHead,100);const auto anchor=steady.resolve(7,{},100);
        bool stable=true;
        for(uint64_t t=120;t<=1120;t+=20){
            const float wave=std::sin(float(t)*.03f);
            gaitHead[12]=.06f*wave;gaitHead[14]=.05f*wave;gaitHead[13]=1.6f+.1f*wave;
            steady.track({},true,t);steady.publishPlayerHead(7,8,{},identityMatrix,gaitHead,t);
            const auto view=steady.resolve(7,{},t);stable=stable&&view.applied&&same(view.nativePose.position,anchor.nativePose.position);
        }
        expect(stable,"native gait sway and vertical bob do not move Coco's stabilized eye anchor");
        gaitHead[13]=1.69f;steady.publishPlayerHead(7,8,{},identityMatrix,gaitHead,1120);
        const auto repeated=steady.resolve(7,{},1120);
        expect(repeated.applied&&same(repeated.nativePose.position,anchor.nativePose.position),
            "two publishers in one millisecond cannot reseed the eye anchor from gait bob");
        auto turnedRoot=identityMatrix;turnedRoot[0]=0;turnedRoot[2]=-1;turnedRoot[8]=1;turnedRoot[10]=0;
        steady.track({},true,1130);steady.publishPlayerHead(7,8,{},turnedRoot,gaitHead,1130);
        const auto turned=steady.resolve(7,{},1130);
        expect(turned.applied&&same(turned.nativePose.position,anchor.nativePose.position),
            "native actor turning toward movement cannot orbit the VR eye anchor");
        steady.track(Pose{{},{.1f,.1f,0}},true,1140);steady.publishPlayerHead(7,8,{},identityMatrix,gaitHead,1140);
        const auto physical=steady.resolve(7,{},1140);
        expect(physical.applied&&same(physical.nativePose.position-anchor.nativePose.position,{-.1f,.1f,0}),
            "bob filtering preserves physical head translation without smoothing it");
    }
    {
        HeadCamera calibrated;calibrated.configure(true,1,true);
        const std::array<EyeView,2> eyes{EyeView{Pose{{},{-.032f,0,0}},squareEye},EyeView{Pose{{},{.032f,0,0}},squareEye}};
        ControllerFrame input;input.fullBody=true;input.physicalFloorTracked=true;input.physicalFloorY=0;
        input.playerPosture=PlayerPosture::crouched;
        Pose tracked{{},{0,1.65f,0}};auto head=identityMatrix;head[13]=.98f;
        calibrated.trackStereo(tracked,eyes,true,100,input);calibrated.toggle();
        calibrated.publishPlayerHead(7,8,{},identityMatrix,head,100);calibrated.resolve(7,{},100);
        input.playerPosture=PlayerPosture::standing;
        HeadCameraSample view;
        for(uint64_t t=120;t<=1320;t+=20){
            head[13]=.98f+.62f*std::min(1.f,float(t-120)/500.f);
            calibrated.trackStereo(tracked,eyes,true,t,input);
            calibrated.publishPlayerHead(7,8,{},identityMatrix,head,t);view=calibrated.resolve(7,{},t);
        }
        expect(view.applied&&near(view.nativePose.position.y,1.65f),
            "standing calibration uses the settled rise anchor, not the first low STAND sample");
        tracked.position.y=2.25f;calibrated.trackStereo(tracked,eyes,true,1340,input);
        calibrated.publishPlayerHead(7,8,{},identityMatrix,head,1340);view=calibrated.resolve(7,{},1340);
        expect(view.applied&&near(view.nativePose.position.y,2.25f),
            "standing height follows the runtime floor after a crouch-to-stand calibration");
    }
    {
        HeadCamera exitView;exitView.configure(true,1,true);
        const std::array<EyeView,2> eyes{EyeView{Pose{{},{-.032f,0,0}},squareEye},EyeView{Pose{{},{.032f,0,0}},squareEye}};
        ControllerFrame input;input.fullBody=true;input.playerPosture=PlayerPosture::crouched;input.cabinTransit=true;
        auto head=identityMatrix;head[13]=1.3f;
        exitView.trackStereo({},eyes,true,100,input);exitView.toggle();
        exitView.publishPlayerHead(7,8,{},identityMatrix,head,100);exitView.resolve(7,{},100);
        input.cabinTransit=false;
        HeadCameraSample settled;
        for(uint64_t t=120;t<=1320;t+=20){
            head[13]=1.3f-.55f*std::min(1.f,float(t-120)/500.f);
            exitView.trackStereo({},eyes,true,t,input);
            exitView.publishPlayerHead(7,8,{},identityMatrix,head,t);settled=exitView.resolve(7,{},t);
        }
        expect(settled.applied&&near(settled.nativePose.position.y,.75f+nativeEyeFromHead.y),
            "helicopter exit settles native eye height even when crouched posture never changes");
        head[13]+=.18f;exitView.trackStereo({},eyes,true,1340,input);
        exitView.publishPlayerHead(7,8,{},identityMatrix,head,1340);
        expect(same(exitView.resolve(7,{},1340).nativePose.position,settled.nativePose.position),
            "settled helicopter exit resumes gait suppression without a stance-toggle workaround");
    }
    auto headBone=std::array<float,16>{1,0,0,0,0,1,0,0,0,0,1,0,0,1.6f,0.1f,1};
    {
        HeadCamera bodyView;bodyView.configure(true,1,true);
        const std::array<EyeView,2> eyes{EyeView{Pose{{},{-.032f,0,0}},squareEye},EyeView{Pose{{},{.032f,0,0}},squareEye}};
        ControllerFrame input;input.fullBody=true;input.playerPosture=PlayerPosture::standing;
        bodyView.trackStereo({},eyes,true,100,input);bodyView.toggle();
        auto head=identityMatrix;head[13]=1.6f;bodyView.publishPlayerHead(7,8,{},identityMatrix,head,100);
        const auto anchor=bodyView.resolve(7,{},100);
        for(uint64_t t=120;t<=500;t+=20){
            head[13]=1.6f+.18f*std::sin(float(t)*.03f);
            bodyView.trackStereo({},eyes,true,t,input);bodyView.publishPlayerHead(7,8,{},identityMatrix,head,t);
            expect(same(bodyView.resolve(7,{},t).nativePose.position,anchor.nativePose.position),"verified standing stance rejects gait motion beyond the old bob deadband");
        }
        input.playerPosture=PlayerPosture::prone;head[13]=.3f;head[14]=.7f;
        for(uint64_t t=520;t<=1720;t+=20){
            bodyView.trackStereo({},eyes,true,t,input);bodyView.publishPlayerHead(7,8,{},identityMatrix,head,t);bodyView.resolve(7,{},t);
        }
        const auto prone=bodyView.resolve(7,{},1720);
        expect(prone.applied&&near(prone.nativePose.position.y,.3f+nativeEyeFromHead.y)&&near(prone.nativePose.position.z,.7f+nativeEyeFromHead.z),"verified prone transition reaches its measured eye anchor without leaving 15 cm of deadband");
    }
    {
        HeadCamera nativeEyes;nativeEyes.configure(true,1,true);
        const std::array<EyeView,2> eyes{EyeView{Pose{{},{-.032f,0,0}},squareEye},EyeView{Pose{{},{.032f,0,0}},squareEye}};
        ControllerFrame input;input.fullBody=true;input.playerPosture=PlayerPosture::standing;
        auto cached=identityMatrix;cached[13]=1.6f;
        NativeBodyHead original{8,9,Pose{{},{0,1.6f,0}}};
        nativeEyes.trackStereo({},eyes,true,100,input);nativeEyes.toggle();
        nativeEyes.publishPlayerHead(7,8,{},identityMatrix,cached,100);
        nativeEyes.resolve(7,{},100,original);
        // The measured prone cached affine and original skin disagree by
        // 107.6 mm at the eye. Neither is a yaw-only approximation of the other.
        cached={.99643862f,-.06679464f,-.05146364f,0,.08348436f,.86725718f,.49081087f,0,
                .01184867f,-.49335933f,.86974484f,0,.06484260f,.30262727f,.50746000f,1};
        original.worldFromHead={{.35874426f,.01026186f,-.07569768f,.93030488f},
                                {.07060973f,.22547168f,.52300447f}};
        const auto trueEye=compose(original.worldFromHead,Pose{{},nativeEyeFromHead}).position;
        input.playerPosture=PlayerPosture::prone;
        HeadCameraSample settled;
        for(uint64_t t=520;t<=1720;t+=20){
            nativeEyes.trackStereo({},eyes,true,t,input);nativeEyes.publishPlayerHead(7,8,{},identityMatrix,cached,t);
            nativeEyes.resolve(7,{},t); // Cached camera beats its owned skin at the SAME timestamp.
            settled=nativeEyes.resolve(7,{},t,original);
            if(t==1240){
                auto earlyCamera=cached;earlyCamera[13]+=.5f;
                nativeEyes.trackStereo({},eyes,true,t+1,input);
                nativeEyes.publishPlayerHead(7,8,{},identityMatrix,earlyCamera,t+1);
                nativeEyes.resolve(7,{},t+1); // Camera arrives before its skin.
            }
        }
        bool stable=settled.applied&&settled.anatomicalEyeModel==9&&same(settled.nativePose.position,trueEye);
        for(uint64_t t=1740;t<=1920;t+=20){
            auto gait=original;gait.worldFromHead.position.y+=.08f*std::sin(float(t)*.03f);
            nativeEyes.trackStereo({},eyes,true,t,input);nativeEyes.publishPlayerHead(7,8,{},identityMatrix,cached,t);
            stable=stable&&same(nativeEyes.resolve(7,{},t,gait).nativePose.position,trueEye);
        }
        original.model=10;original.worldFromHead.position.y+=.2f;
        nativeEyes.trackStereo({},eyes,true,1940,input);nativeEyes.publishPlayerHead(7,8,{},identityMatrix,cached,1940);
        const auto replacement=nativeEyes.resolve(7,{},1940,original);
        const bool replaced=replacement.anatomicalEyeModel==10&&same(replacement.nativePose.position,trueEye+Vec3{0,.2f,0});
        const auto rejected=nativeEyes.resolve(7,{},1940,NativeBodyHead{99,11,Pose{{},{0,8,0}}});
        const bool currentJoin=rejected.applied&&!rejected.anatomicalEyeModel
            &&same(rejected.nativePose.position,replacement.nativePose.position);
        input.fullBody=false;
        nativeEyes.trackStereo({},eyes,true,1960,input);nativeEyes.publishPlayerHead(7,8,{},identityMatrix,cached,1960);
        const auto legacy=nativeEyes.resolve(7,{},1960,original);
        const bool reverted=same(legacy.nativePose.position,{cached[12],cached[13]+.25f,cached[14]-.35f});
        input.fullBody=true;
        nativeEyes.trackStereo({},eyes,true,1980,input);nativeEyes.publishPlayerHead(7,8,{},identityMatrix,cached,1980);
        const auto foreign=nativeEyes.resolve(7,{},1980,NativeBodyHead{99,11,Pose{{},{0,8,0}}});
        const float cachedEyeY=cached[13]+nativeEyeFromHead.y*cached[5]+nativeEyeFromHead.z*cached[9];
        expect(stable&&replaced&&currentJoin&&reverted&&!foreign.anatomicalEyeModel&&near(foreign.nativePose.position.y,cachedEyeY)
            &&same(replacement.playerHead,{cached[12],cached[13],cached[14]}),
            "owned prone skin supplies the anatomical eye through camera gaps and gait, resets on model replacement, and preserves cached HEAD identity and body-off behavior");
        original.model=12;original.bodyFramesTracked=true;
        original.worldFromRoot.position={.3f,0,.2f};
        original.worldFromChest.orientation={0,.70710678f,0,.70710678f};
        nativeEyes.trackStereo({},eyes,true,2000,input);nativeEyes.publishPlayerHead(7,8,{},identityMatrix,cached,2000);
        const auto bodyAnchor=nativeEyes.resolve(7,{},2000,original);
        const auto expectedAnchor=compose(original.worldFromHead,Pose{{},nativeEyeFromHead}).position;
        original.worldFromRoot.position.x=std::numeric_limits<float>::quiet_NaN();
        const auto invalidBody=nativeEyes.resolve(7,{},2000,original);
        expect(bodyAnchor.anatomicalEyeModel==12&&same(bodyAnchor.nativePose.position,expectedAnchor)
            &&!invalidBody.anatomicalEyeModel&&same(invalidBody.nativePose.position,bodyAnchor.nativePose.position),
            "a prone camera/chest yaw disagreement cannot orbit the owned eyes; an invalid body join cannot partly change the anchor");
        // Retained M1 native skin: the independent camera yaw previously put
        // the prone viewer 77 cm from these original eyes, through the slope.
        original.model=13;original.worldFromRoot.position={384.37f,321.062f,185.68f};
        original.worldFromChest={{.031363f,.624354f,-.767401f,-.142456f},{384.359f,321.259f,185.532f}};
        original.worldFromHead={{.0648627f,.884946f,-.456135f,-.0678508f},{384.292f,321.22f,185.163f}};
        const Pose independentCamera{{0,-.766082f,0,.642742f},{384,323,186}};
        auto capturedRoot=identityMatrix;
        capturedRoot[12]=original.worldFromRoot.position.x;
        capturedRoot[13]=original.worldFromRoot.position.y;
        capturedRoot[14]=original.worldFromRoot.position.z;
        nativeEyes.trackStereo({},eyes,true,2020,input);
        nativeEyes.publishPlayerHead(7,8,independentCamera,capturedRoot,cached,2020);
        const auto capturedProne=nativeEyes.resolve(7,independentCamera,2020,original);
        expect(capturedProne.applied&&capturedProne.anatomicalEyeModel==13
            &&same(capturedProne.nativePose.position,compose(original.worldFromHead,Pose{{},nativeEyeFromHead}).position),
            "captured prone eyes stay at the untouched native skin when its camera looks across the body heading");
        original.worldFromChest.orientation={0,0,0,1};
        original.worldFromHead.position.x+=.08f;
        nativeEyes.trackStereo({},eyes,true,2040,input);
        nativeEyes.publishPlayerHead(7,8,independentCamera,capturedRoot,cached,2040);
        const auto crawlStride=nativeEyes.resolve(7,independentCamera,2040,original);
        expect(same(crawlStride.nativePose.position,capturedProne.nativePose.position),
            "crawling chest yaw and head sway do not move the root-anchored viewpoint");
        nativeEyes.recenter();original.worldFromHead.position.y-=.4f;
        nativeEyes.trackStereo({},eyes,true,2060,input);
        nativeEyes.publishPlayerHead(7,8,independentCamera,capturedRoot,cached,2060);
        const auto recenteredStride=nativeEyes.resolve(7,independentCamera,2060,original);
        expect(same(recenteredStride.nativePose.position,crawlStride.nativePose.position),
            "recenter cannot reseed anatomical height from a transient lowered animation frame");
        // Retained Mother Base cover: camera yaw differs from the wall-facing
        // actor. Rotating its .60 m eye footprint into view yaw crossed the wall.
        input.playerPosture=PlayerPosture::crouched;
        original.model=14;original.wallCover=true;
        original.worldFromRoot={{0,-.708316f,0,.705895f},{.671257f,0,-18.3761f}};
        original.worldFromChest={{.544735f,-.736943f,.2287f,.328443f},{.551112f,.664783f,-18.3739f}};
        original.worldFromHead={{.252559f,-.541534f,.22732f,.768948f},{.20093f,.802117f,-18.3525f}};
        const Pose coverCamera{{0,.0153247f,0,.999883f},{.634334f,.757389f,-17.7507f}};
        capturedRoot[12]=.671257f;capturedRoot[13]=0;capturedRoot[14]=-18.3761f;
        nativeEyes.trackStereo({},eyes,true,2080,input);
        nativeEyes.publishPlayerHead(7,8,coverCamera,capturedRoot,cached,2080);
        const auto covered=nativeEyes.resolve(7,coverCamera,2080,original);
        expect(covered.applied&&same(covered.nativePose.position,
            compose(original.worldFromHead,Pose{{},nativeEyeFromHead}).position),
            "captured cover eyes retain their wall-relative footprint despite independent view yaw");
        original.wallCover=false;
        nativeEyes.trackStereo({},eyes,true,2100,input);
        nativeEyes.publishPlayerHead(7,8,coverCamera,capturedRoot,cached,2100);
        const auto coverExit=nativeEyes.resolve(7,coverCamera,2100,original);
        expect(coverExit.applied&&same(coverExit.nativePose.position,covered.nativePose.position),
            "clearing cover before its crouched animation ends cannot rotate the eyes through the wall");
    }
    const auto headPoint=playerHeadPosition(playerRoot,headBone);
    expect(headPoint&&same(*headPoint,{500.1f,301.6f,1300}),"animated head position uses local-then-world transform order");
    auto invalidRoot=playerRoot;invalidRoot[0]=std::numeric_limits<float>::quiet_NaN();
    expect(!playerHeadPosition(invalidRoot,headBone),"nonfinite player root cannot move the VR camera");
    invalidRoot=playerRoot;invalidRoot[2]=1;
    expect(!playerHeadPosition(invalidRoot,headBone),"reflected player transform cannot reverse stereo handedness");
    invalidRoot=playerRoot;invalidRoot[5]=2;
    expect(!playerHeadPosition(invalidRoot,headBone),"scaled player transform requires a separate world-scale contract");
    HeadCamera firstPerson;firstPerson.configure(true,1,true);firstPerson.track({},true,100);
    const Pose thirdPerson{{},{500,303,1305}};
    expect(firstPerson.publishPlayerHead(11,22,thirdPerson,playerRoot,headBone,100),"native player head joins its camera publication");
    firstPerson.toggle();auto firstView=firstPerson.resolve(11,thirdPerson,100);
    expect(firstView.applied&&same(firstView.nativePose.position,{499.75f,301.85f,1300.f})
        &&firstView.playerOwner==22&&firstView.playerSequence==1,
           "VR starts at the stabilized player head instead of the native boom camera");
    headBone[13]=0.3f;const Pose lowered{{},{500,301,1303}};
    firstPerson.track(Pose{{},{0.2f,0.1f,-0.1f}},true,110);
    firstPerson.publishPlayerHead(11,22,lowered,playerRoot,headBone,110);
    firstView=firstPerson.resolve(11,lowered,110);
        expect(firstView.applied&&same(firstView.nativePose.position,{499.55f,301.88659f,1300.1f}),
            "posture height eases toward the player head without inheriting the third-person boom");
    const auto savedHeadView=firstView;
    auto mismatchedCamera=lowered;mismatchedCamera.position.z+=1;
    expect(!firstPerson.resolve(11,mismatchedCamera,110).applied&&firstPerson.status().reason==HeadCameraStop::playerHeadUnavailable,
           "unmatched camera generation stops instead of attaching an unrelated player pose");
    expect(firstPerson.status().suspended&&!firstPerson.status().awaitingPlayer,
           "fresh mismatched player publication alone is not evidence of a native menu");
    // Exercise a real native menu boundary separately from the brief skin gap.
    firstPerson.setNativeMenuOpen(true);
    firstPerson.resolve(11,mismatchedCamera,110);
    firstPerson.setNativeMenuOpen(false);
    const auto menuActivation=firstPerson.status().activation;
    expect(!firstPerson.active()&&!firstPerson.status().pending&&firstPerson.status().awaitingPlayer,
           "native menu without an accepted anchor exposes native pixels and controls while retaining VR intent");
    firstPerson.track(Pose{{},{0.3f,0.1f,-0.1f}},true,120);
    firstPerson.publishPlayerHead(12,23,lowered,playerRoot,headBone,120);
    expect(!firstPerson.resolve(12,lowered,120).applied&&firstPerson.status().awaitingPlayer,
           "an unrelated camera and actor cannot resume VR after a menu");
    firstPerson.publishPlayerHead(11,23,lowered,playerRoot,headBone,120);
    expect(!firstPerson.resolve(11,lowered,120).applied&&firstPerson.status().awaitingPlayer,
           "recycled camera address cannot attach VR to a different player owner");
    firstPerson.publishPlayerHead(11,22,lowered,playerRoot,headBone,120);
    const auto resumedPlayer=firstPerson.resolve(11,lowered,120);
    expect(resumedPlayer.applied&&!firstPerson.status().awaitingPlayer
        &&firstPerson.status().activation==menuActivation+1
          &&same(resumedPlayer.nativePose.position,{499.45f,300.65f,1300.1f}),
              "the same live player resumes with a fresh stabilized eye generation");
    firstPerson.resolve(11,mismatchedCamera,120);firstPerson.toggle();
    expect(!firstPerson.status().awaitingPlayer&&!firstPerson.active(),"manual disable cancels automatic menu return");
    expect(!firstPerson.resolve(11,lowered,120).applied,"fresh gameplay cannot undo a manual VR disable");
    firstPerson.toggle();firstPerson.resolve(11,lowered,120);
    firstPerson.setNativeMenuOpen(true);
    const auto menuView=firstPerson.resolve(11,lowered,120);
    expect(menuView.applied&&menuView.menuOpen&&firstPerson.active()&&!firstPerson.status().awaitingPlayer,
           "iDroid retains the native stereo viewpoint with a world-space menu panel");
    firstPerson.track(Pose{{},{0.4f,0.1f,-0.1f}},true,125);
    const auto movedMenu=firstPerson.resolve(11,mismatchedCamera,125);
    expect(movedMenu.applied&&same(movedMenu.menuPanel.position,menuView.menuPanel.position)
        &&near(std::abs(movedMenu.nativePose.position.x-menuView.nativePose.position.x),.1f),
           "leaning moves the menu world camera while the panel stays anchored and native iDroid camera animation is ignored");
    expect(!firstPerson.resolve(12,lowered,125).applied&&firstPerson.active(),
           "an unrelated native camera cannot borrow the menu viewpoint");
    firstPerson.setNativeMenuOpen(false);
    expect(firstPerson.resolve(11,lowered,125).applied,"closing the native iDroid state restores the same tracked player");
    firstPerson.setNativeMenuOpen(true,true);
    const auto liveMenuView=firstPerson.resolve(11,lowered,125);
    auto liveNative=lowered;liveNative.position.x+=.25f;
    const auto liveMovedMenu=firstPerson.resolve(11,liveNative,125);
    expect(liveMenuView.applied&&liveMovedMenu.applied&&liveMovedMenu.menuOpen
        &&same(liveMovedMenu.nativePose.position,liveMenuView.nativePose.position)
        &&same(liveMovedMenu.menuPanel.position,liveMenuView.menuPanel.position),
        "default paused iDroid ignores native camera animation and keeps its world panel anchored");
    firstPerson.setNativeMenuOpen(false);
    firstPerson.setNativeMenuOpen(true);firstPerson.toggle();firstPerson.setNativeMenuOpen(false);
    expect(!firstPerson.resolve(11,lowered,125).applied,"manual disable inside iDroid prevents automatic return");
    expect(same(savedHeadView.nativePose.position,{499.55f,301.88659f,1300.1f}),"published stabilized player-eye frame remains immutable");
    {
        HeadCamera interrupted;interrupted.configure(true,1,true);interrupted.track({},true,100);
        interrupted.publishPlayerHead(11,22,thirdPerson,playerRoot,headBone,100);
        interrupted.toggle();const auto before=interrupted.resolve(11,thirdPerson,100);
        expect(before.applied&&!interrupted.resolve(12,lowered,100).applied
            &&interrupted.active()&&!interrupted.status().suspended
            &&interrupted.status().activation==before.activation,
            "an unpublished secondary camera cannot interrupt active player VR");
        interrupted.track(Pose{{},{.1f,0,0}},true,700);
        expect(!interrupted.resolve(11,thirdPerson,700).applied&&interrupted.active()
            &&interrupted.status().suspended&&!interrupted.status().awaitingPlayer
            &&interrupted.status().activation==before.activation,
            "a brief same-camera skin gap rejects stale geometry without resetting VR or input context");
        interrupted.publishPlayerHead(11,22,thirdPerson,playerRoot,headBone,701);
        interrupted.track(Pose{{},{.1f,0,0}},true,701);
        const auto after=interrupted.resolve(11,thirdPerson,701);
        expect(after.applied&&!interrupted.status().suspended&&after.activation==before.activation,
            "fresh player skin resumes the existing stereo generation after a publication gap");
        interrupted.track({},true,702);
        expect(!interrupted.resolve(11,lowered,702).applied&&interrupted.active()
            &&interrupted.status().suspended&&!interrupted.status().awaitingPlayer
            &&interrupted.status().activation==before.activation
            &&!mayAcceptNewStereoPair(interrupted.status())
            &&mayReprojectAcceptedStereo(interrupted.status()),
            "fresh same-owner skin with a different source pose rejects new pixels without retiring gameplay");
        interrupted.publishPlayerHead(11,22,lowered,playerRoot,headBone,703);
        interrupted.track({},true,703);
        const auto moved=interrupted.resolve(11,lowered,703);
        expect(moved.applied&&!interrupted.status().suspended&&moved.activation==before.activation,
            "exact fresh source join after native camera motion resumes the same activation");
        interrupted.track({},true,1802);
        expect(!interrupted.resolve(11,thirdPerson,1802).applied&&interrupted.status().awaitingPlayer,
            "a retired primary still yields native scene control after the bounded recovery window");
    }
    {
        HeadCamera travel;travel.configure(true,1,true);travel.track({},true,100);
        travel.publishPlayerHead(11,22,thirdPerson,playerRoot,headBone,100);
        travel.toggle();const auto acc=travel.resolve(11,thirdPerson,100);
        travel.track({},true,200);
        travel.publishPlayerHead(12,23,lowered,playerRoot,headBone,200);
        expect(!travel.resolve(12,lowered,200).applied&&travel.active(),
            "fresh alternate player cannot steal the live camera or cancel VR intent");
        travel.track(Pose{{},{.4f,0,0}},true,1200);
        expect(!travel.resolve(12,lowered,1200).applied,
            "travel cannot adopt a stale replacement publication");
        travel.publishPlayerHead(12,23,lowered,playerRoot,headBone,1200);
        const auto field=travel.resolve(12,lowered,1200);
        expect(field.applied&&field.playerOwner==23&&field.activation>acc.activation
            &&!field.rigSequence&&same(field.nativePose.position,*playerHeadPosition(playerRoot,headBone)),
            "new mission player resumes VR with a fresh origin and no old rig or eye generation");
        travel.toggle();travel.track({},true,2400);
        travel.publishPlayerHead(13,24,lowered,playerRoot,headBone,2400);
        expect(!travel.resolve(13,lowered,2400).applied,
            "mission replacement never overrides manual VR disable");
    }
    firstPerson.track({},true,300);firstPerson.toggle();
    expect(!firstPerson.resolve(11,lowered,300).applied,"stale player head cannot survive a fresh headset sample");
    const Pose nativeCamera{{},{10,20,30}};
    camera.track({},true,100);camera.toggle();
    expect(!camera.resolve(1,nativeCamera,100).applied,"native head camera is opt-in");
    camera.configure(true);camera.toggle();
    auto resolved=camera.resolve(1,nativeCamera,100);
    expect(resolved.applied&&same(resolved.nativePose.position,nativeCamera.position),"activation establishes a no-jump head origin");
    camera.track(Pose{{},{1,2,-3}},true,110);
    resolved=camera.resolve(1,nativeCamera,110);
    expect(resolved.applied&&same(resolved.nativePose.position,{9,22,33}),"head translation uses the explicit native camera basis");
    const auto frozen=resolved;
    camera.track(Pose{{},{4,5,-6}},true,120);
    expect(same(frozen.nativePose.position,{9,22,33}),"a captured publication pose remains immutable after newer tracking");
    expect(!camera.resolve(2,nativeCamera,120).applied&&!camera.active(),"camera identity change cancels explicit activation");
    expect(camera.status().reason==HeadCameraStop::cameraChanged,"camera identity cancellation records its reason");
    camera.toggle();camera.resolve(1,nativeCamera,120);
    const auto activationBeforeStall=camera.status().activation;
    expect(!camera.resolve(1,nativeCamera,271).applied&&camera.active()&&camera.status().suspended,"stale tracking suspends camera writes while preserving VR intent");
    expect(camera.status().reason==HeadCameraStop::staleTracking,"stale tracking suspension remains observable");
    camera.track(Pose{{},{5,5,-6}},true,300);resolved=camera.resolve(1,nativeCamera,300);
    expect(resolved.applied&&same(resolved.nativePose.position,{9,20,30})&&!camera.status().suspended
        &&camera.status().activation==activationBeforeStall,"fresh tracking resumes the same origin and stereo activation without a third-person fallback");
    camera.track({},false,301);
    expect(!camera.resolve(1,nativeCamera,301).applied&&camera.active()&&camera.status().suspended,"tracking loss suspends rendering without submitting a theatre view");
    camera.toggle();expect(!camera.active()&&!camera.status().suspended,"manual disable still works while tracking is suspended");
    camera.track({},true,400);camera.toggle();camera.resolve(1,nativeCamera,400);
    camera.track(Pose{{0.70710678f,0,0,0.70710678f},{}},true,410);
    resolved=camera.resolve(1,nativeCamera,410);
    expect(resolved.applied&&same(rotate(resolved.nativePose.orientation,{0,0,1}),{0,1,0}),"head pitch rotates in the native camera basis");
    camera.cancel();expect(!camera.active(),"manual cancellation returns control to the native view");
    camera.configure(false);expect(!camera.available(),"disabled camera experiment does not reserve a native input chord");
    camera.configure(true);camera.track({},true,steadyMilliseconds());camera.toggle();
    expect(camera.resolveCurrent(1,nativeCamera).applied,"live camera clock is sampled atomically with tracking");
    HeadCamera rigidCamera;rigidCamera.configure(true);rigidCamera.track({},true,500);rigidCamera.toggle();
    const auto normalized=rigidCamera.resolve(1,Pose{{0,0.60003f,0,0.80004f},{500,300,1300}},500);
    const auto nq=normalized.nativePose.orientation;
    expect(normalized.applied&&std::abs(nq.y*nq.y+nq.w*nq.w-1)<0.0000002f,"kilometer-scale native camera keeps a normalized rigid rotation");
    HeadCamera handsCamera;handsCamera.configure(true);
    const std::array<EyeView,2> rigEyes{{{Pose{{},{-0.032f,0,0}},EyeFov{-0.7f,0.7f,0.7f,-0.7f}},
                                      {Pose{{},{0.032f,0,0}},EyeFov{-0.7f,0.7f,0.7f,-0.7f}}}};
    {
        HeadCamera demo;demo.configure(true,1,true);
        ControllerFrame input;input.frontEnd=true;input.authoredCamera=true;
        demo.trackStereo({},rigEyes,true,100,input);demo.toggle();
        expect(!demo.resolve(9,nativeCamera,100).applied,"a scripted shot cannot adopt an unverified secondary camera");
        auto root=identityMatrix;root[12]=10;root[13]=20;root[14]=30;
        auto head=identityMatrix;head[13]=1.6f;
        demo.publishPlayerHead(1,2,nativeCamera,root,head,100);
        const auto first=demo.resolve(1,nativeCamera,100);
        demo.trackStereo({},rigEyes,true,150,input);
        expect(!demo.resolve(9,nativeCamera,150).applied&&demo.active(),
            "secondary scripted camera publications cannot cancel the verified primary stereo scene");
        demo.trackStereo({},rigEyes,true,500,input);
        const Pose shot{{.38268343f,0,0,.92387953f},{14,21,32}};
        const auto next=demo.resolve(1,shot,500);
        expect(first.applied&&next.applied&&same(next.nativePose.position,shot.position)
            &&next.stereoTracked,
            "verified scripted camera stays stereo when its decorative player stops publishing");
        demo.trackStereo({},rigEyes,true,1100,input);
        const auto cut=demo.resolve(9,shot,1100);
        expect(cut.applied&&cut.activation>next.activation&&same(cut.nativePose.position,shot.position),
            "native demo camera replaces a retired player camera with a fresh stereo generation");
        input.authoredCamera=false;input.frontEnd=false;
        demo.trackStereo({},rigEyes,true,1110,input);
        expect(!demo.resolve(9,shot,1110).applied,
            "leaving a scripted camera still requires the real gameplay player publication");
    }
    {
        HeadCamera cinematic;cinematic.configure(true,1,true);
        ControllerFrame input;input.scriptedDemo=true;input.authoredCamera=true;input.referenceEpoch=1;
        cinematic.trackStereo({},rigEyes,true,100,input);cinematic.toggle();
        const Pose shot{{.38268343f,0,0,.92387953f},{-41,108,-1717}};
        const auto frame=cinematic.resolve(9,shot,100);
        const Pose animatedShot{{0,.70710678f,0,.70710678f},{-35,112,-1721}};
        cinematic.trackStereo({},rigEyes,true,110,input);
        const auto animated=cinematic.resolve(9,animatedShot,110);
        expect(frame.applied&&frame.playerOwner==0&&frame.stereoTracked
            &&same(frame.nativePose.position,shot.position)&&same(rotate(frame.nativePose.orientation,{0,1,0}),{0,1,0}),
            "a native cutscene enters at its authored position with Coco's level horizon");
        expect(animated.applied&&same(animated.nativePose.position,animatedShot.position)
            &&same(rotate(animated.nativePose.orientation,{0,0,1}),rotate(animatedShot.orientation,{0,0,1})),
            "cutscene camera animation follows the current authored position and rotation");
        const Pose physicalPose{{.13052619f,0,0,.99144486f},{.15f,0,0}};
        cinematic.trackStereo(physicalPose,rigEyes,true,120,input);
        const auto physicalHead=cinematic.resolve(9,animatedShot,120);
        const Pose basis{{0,1,0,0},{}};
        const auto expectedHead=compose(animatedShot,compose(compose(basis,physicalPose),inverse(basis)));
        expect(physicalHead.applied&&same(physicalHead.nativePose.position,expectedHead.position)
            &&same(rotate(physicalHead.nativePose.orientation,{0,0,1}),rotate(expectedHead.orientation,{0,0,1})),
            "physical head translation and rotation remain relative to the current cutscene shot");
        // Tracking recovery must join the current shot, not revive an old
        // cinematic anchor or require a decorative player's skeleton.
        cinematic.trackStereo({},rigEyes,false,121,input);
        cinematic.trackStereo({},rigEyes,true,130,input);
        const auto recovered=cinematic.resolve(9,animatedShot,130);
        expect(recovered.applied&&same(recovered.nativePose.position,animatedShot.position),
            "tracking recovery follows the current cutscene shot");
        expect(!cinematic.resolve(10,shot,130).applied&&cinematic.active(),
            "a secondary camera cannot take over while the current cutscene camera is live");
        cinematic.trackStereo({},rigEyes,true,700,input);
        const Pose nextShot{{0,0,.25881904f,.96592583f},{-20,117,-1730}};
        const auto cameraCut=cinematic.resolve(10,nextShot,700);
        expect(cameraCut.applied&&cameraCut.activation>recovered.activation
            &&same(cameraCut.nativePose.position,nextShot.position)
            &&same(rotate(cameraCut.nativePose.orientation,{0,1,0}),{0,1,0}),
            "a retired cutscene camera hands off immediately while keeping a level horizon");
        expect(cinematic.status().sceneTransitionActivation==cameraCut.activation,
            "only the verified retired-shot adoption authorizes a cinematic image handoff");
        cinematic.setNativeMenuOpen(true);
        cinematic.trackStereo({},rigEyes,true,705,input);
        const auto paused=cinematic.resolve(10,animatedShot,705);
        expect(paused.applied&&paused.menuOpen&&same(paused.nativePose.position,cameraCut.nativePose.position),
            "pausing a cutscene still holds the independently anchored menu view");
        cinematic.setNativeMenuOpen(false);
        input.scriptedDemo=false;
        cinematic.trackStereo({},rigEyes,true,710,input);
        const Pose resumedShot{{0,.25881904f,0,.96592583f},{-20,117,-1730}};
        const auto resumed=cinematic.resolve(10,resumedShot,710);
        expect(resumed.applied&&same(resumed.nativePose.position,resumedShot.position),
            "the native authored camera continues immediately after the demo ends");
        input.authoredCamera=false;
        cinematic.trackStereo({},rigEyes,true,720,input);
        expect(!cinematic.resolve(10,resumedShot,720).applied,
            "returning to gameplay still requires a fresh matching player-head publication");
        auto resumedRoot=identityMatrix;resumedRoot[12]=-20;resumedRoot[13]=115.4f;resumedRoot[14]=-1730;
        auto resumedHead=identityMatrix;resumedHead[13]=1.6f;
        cinematic.publishPlayerHead(10,42,resumedShot,resumedRoot,resumedHead,720);
        const auto gameplay=cinematic.resolve(10,resumedShot,720);
        expect(gameplay.applied&&gameplay.playerOwner==42&&!gameplay.controllers.scriptedDemo,
            "the real player camera resumes after the cinematic without another VR toggle");
        expect(cinematic.status().sceneTransitionActivation==gameplay.activation,
            "the verified player publication authorizes return from a native shot");
        input.referenceEpoch=2;cinematic.trackStereo({},rigEyes,true,730,input);
        expect(cinematic.status().sceneTransitionActivation!=cinematic.status().activation,
            "reference-space changes invalidate a prior cinematic transition authorization");
        cinematic.publishPlayerHead(10,42,resumedShot,resumedRoot,resumedHead,730);
        cinematic.recenter();cinematic.resolve(10,resumedShot,730);
        expect(cinematic.status().sceneTransitionActivation!=cinematic.status().activation,
            "manual recenter does not authorize old cinematic eyes");
    }
    {
        HeadCamera replaced;replaced.configure(true,1,true);
        ControllerFrame input;input.referenceEpoch=42;
        replaced.trackStereo({},rigEyes,true,100,input);replaced.toggle();
        auto root=identityMatrix;auto bone=identityMatrix;bone[13]=1.6f;
        replaced.publishPlayerHead(10,42,{},root,bone,100);
        expect(replaced.resolve(10,{},100).applied,"Mission 1 shot replacement starts with a verified gameplay camera");
        input.authoredCamera=input.scriptedDemo=true;
        replaced.trackStereo({},rigEyes,true,110,input);
        const Pose shot{{},{14,21,32}};
        expect(!replaced.publishedView()&&!replaced.resolve(11,shot,110).applied,
            "entering a movie invalidates gameplay pixels while a live primary still excludes a secondary camera");
        replaced.trackStereo({},rigEyes,true,601,input);
        const auto movie=replaced.resolve(11,shot,601);
        expect(movie.applied&&movie.playerOwner==0&&same(movie.nativePose.position,shot.position),
            "a new movie camera can replace retired gameplay even when the transition cleared its last view");
        expect(replaced.status().sceneTransitionActivation==movie.activation,
            "the replacement movie camera publishes a new authorized stereo generation");
        expect(!replaced.resolve(12,shot,601).applied,
            "another camera cannot replace the newly accepted live movie camera");
    }
    for(const uintptr_t returnCamera:{uintptr_t{11},uintptr_t{12}}){
        HeadCamera terminal;terminal.configure(true,1,true);
        ControllerFrame input;input.referenceEpoch=42;input.handheldMenus=true;
        terminal.trackStereo({},rigEyes,true,100,input);terminal.toggle();
        auto root=identityMatrix;auto bone=identityMatrix;bone[13]=1.6f;
        terminal.publishPlayerHead(10,42,{},root,bone,100);
        expect(terminal.resolve(10,{},100).applied,"tutorial terminal starts with the current player");
        terminal.setNativeMenuOpen(true,true);
        input.authoredCamera=input.scriptedDemo=true;
        terminal.trackStereo({},rigEyes,true,110,input);
        const Pose shot{{},{14,21,32}};
        terminal.resolve(11,shot,110);
        terminal.trackStereo({},rigEyes,true,601,input);
        const auto movie=terminal.resolve(11,shot,601);
        expect(movie.applied&&movie.playerOwner==0,
            "retained tutorial iDroid cannot strand an authored shot on the old player skin");
        input.authoredCamera=input.scriptedDemo=false;
        terminal.trackStereo({},rigEyes,true,2000,input);
        expect(!terminal.resolve(returnCamera,{},2000).applied,
            "retained terminal cannot invent a player publication on movie return");
        terminal.publishPlayerHead(returnCamera,42,{},root,bone,2000);
        const auto resumed=terminal.resolve(returnCamera,{},2000);
        expect(resumed.applied&&resumed.playerOwner==42&&resumed.menuOpen&&resumed.menuIdroid,
            "the exact returning player reacquires a terminal retained across the native movie");
        terminal.trackStereo({},rigEyes,true,2010,input);
        terminal.publishPlayerHead(returnCamera,42,{},root,bone,2010);
        expect(terminal.resolve(returnCamera,{},2010).applied&&!terminal.status().suspended,
            "reacquired terminal continues publishing live player eyes");
    }
    {
        HeadCamera filtered;filtered.configure(true,1,true);
        ControllerFrame input;input.referenceEpoch=42;
        filtered.trackStereo({},rigEyes,true,100,input);filtered.toggle();
        auto root=identityMatrix;auto bone=identityMatrix;bone[13]=1.6f;
        filtered.publishPlayerHead(10,42,{},root,bone,100);
        expect(filtered.resolve(10,{},100).applied,"same-owner handoff fixture starts with a verified player");
        input.authoredCamera=input.scriptedDemo=true;
        filtered.trackStereo({},rigEyes,true,2000,input);
        expect(filtered.resolve(10,{},2000).applied,"native movie may reuse the existing player camera object");
        input.authoredCamera=input.scriptedDemo=false;
        filtered.trackStereo({},rigEyes,true,2010,input);
        expect(!filtered.resolve(10,{},2010).applied&&filtered.status().awaitingPlayer,
            "movie return waits for the existing player's new skin publication");
        filtered.publishPlayerHead(10,42,{},root,bone,2010);
        const auto resumed=filtered.resolve(10,{},2010);
        expect(resumed.applied&&filtered.status().sceneTransitionActivation==resumed.activation,
            "same-owner player resume preserves the explicit cinematic handoff authorization");
        filtered.setNativeMenuOpen(true);
        expect(!filtered.status().sceneTransitionActivation,"opening a native menu retires cinematic handoff authority");
    }
    {
        HeadCamera filtered;filtered.configure(true,1,true);
        ControllerFrame input;input.referenceEpoch=42;
        filtered.trackStereo({},rigEyes,true,100,input);filtered.toggle();
        auto root=identityMatrix;auto bone=identityMatrix;bone[13]=1.6f;
        filtered.publishPlayerHead(10,42,{},root,bone,100);
        expect(filtered.resolve(10,{},100).applied,"shot handoff starts with a verified player");
        input.authoredCamera=input.scriptedDemo=true;
        filtered.trackStereo({},rigEyes,true,2000,input);
        expect(filtered.resolve(10,{},2000).applied,"the existing camera begins the native movie");
        input.authoredCamera=input.scriptedDemo=false;
        filtered.trackStereo({},rigEyes,true,2010,input);
        filtered.publishPlayerHead(11,43,{},root,bone,2010);
        const auto resumed=filtered.resolve(11,{},2010);
        expect(resumed.applied&&resumed.playerOwner==43
            &&filtered.status().sceneTransitionActivation==resumed.activation,
            "a fresh replacement player camera completes an explicitly authorized movie handoff");
        filtered.trackStereo({},rigEyes,true,3100,input);
        filtered.publishPlayerHead(12,44,{},root,bone,3100);
        expect(filtered.resolve(12,{},3100).applied&&!filtered.status().sceneTransitionActivation,
            "ordinary cross-owner travel cannot revive a movie's retired-image authority");
    }
    {
        HeadCamera filtered;filtered.configure(true,1,true);
        ControllerFrame input;input.scriptedDemo=input.authoredCamera=true;input.referenceEpoch=1;
        filtered.trackStereo({},rigEyes,true,100,input);filtered.toggle();filtered.resolve(9,{},100);
        const Pose moving{{0,.04997917f,0,.99875026f},{.2f,0,0}};
        filtered.trackStereo({},rigEyes,true,120,input);const auto smooth=filtered.resolve(9,moving,120);
        expect(smooth.applied&&smooth.nativePose.position.x>0&&smooth.nativePose.position.x<.2f,
            "Coco's cinematic follow filters shake while advancing toward the current shot");
        expect(std::abs(rotate(smooth.nativePose.orientation,{0,0,1}).x)<=.02401f,
            "cinematic camera heading obeys its turn-rate cap");
        filtered.trackStereo({},rigEyes,true,130,input);const auto cut=filtered.resolve(9,Pose{{},{8,0,0}},130);
        expect(cut.applied&&same(cut.nativePose.position,{8,0,0}),"a real camera cut bypasses cinematic smoothing");
        input.scriptedLook=true;
        const Pose lesson{{.38268343f,0,0,.92387953f},{8,0,0}};
        filtered.trackStereo({},rigEyes,true,140,input);const auto looking=filtered.resolve(9,lesson,140);
        expect(looking.applied&&same(rotate(looking.nativePose.orientation,{0,0,1}),rotate(lesson.orientation,{0,0,1})),
            "interactive hospital look lessons retain their native pitch and do not enter cinematic comfort filtering");
        input.scriptedLook=false;input.cinematicMove={0,1};
        const Pose stationaryShot{{},{8,0,0}};
        filtered.trackStereo({},rigEyes,true,150,input);filtered.resolve(9,stationaryShot,150);
        filtered.trackStereo({},rigEyes,true,170,input);const auto spectator=filtered.resolve(9,stationaryShot,170);
        expect(spectator.applied&&near(spectator.nativePose.position.z,.017f)&&same(stationaryShot.position,{8,0,0}),
            "cinematic stick input moves the spectator while retaining the authored source camera");
        input.cinematicFreeMove=false;input.cinematicMove={};
        filtered.trackStereo({},rigEyes,true,190,input);const auto returnedShot=filtered.resolve(9,stationaryShot,190);
        expect(returnedShot.applied&&same(returnedShot.nativePose.position,stationaryShot.position),"live cinematic movement toggle returns to the authored path");
    }
    {
        HeadCamera title;title.configure(true);ControllerFrame input;input.frontEnd=true;input.referenceEpoch=1;
        title.trackStereo({},rigEyes,true,100,input);title.toggle();
        const auto entry=title.resolve(1,nativeCamera,100);
        auto orbit=nativeCamera;orbit.position.x+=2;orbit.orientation={0,.70710678f,0,.70710678f};
        const auto stationary=title.resolve(1,orbit,110);
        expect(entry.applied&&stationary.applied&&same(entry.nativePose.position,stationary.nativePose.position)
            &&same(rotate(entry.nativePose.orientation,{0,0,1}),rotate(stationary.nativePose.orientation,{0,0,1})),
            "Title camera orbit cannot move or turn the tracked player");
        title.trackStereo(Pose{{},{.2f,0,0}},rigEyes,true,120,input);
        const auto lean=title.resolve(1,orbit,120);
        expect(lean.applied&&!same(lean.nativePose.position,entry.nativePose.position),
            "physical head translation remains live in the wrist Title menu");
        input.authoredCamera=true;
        title.trackStereo({},rigEyes,true,125,input);
        const Pose shot{{.38268343f,0,0,.92387953f},{14,21,32}};
        const auto cinematic=title.resolve(1,shot,125);
        expect(cinematic.applied&&same(cinematic.nativePose.position,shot.position)
            &&same(rotate(cinematic.nativePose.orientation,{0,0,1}),rotate(shot.orientation,{0,0,1})),
            "scripted stereo follows the current authored position, pitch and roll instead of freezing its first shot");
        input.authoredCamera=false;
        title.cancel();input.frontEnd=false;title.trackStereo({},rigEyes,true,130,input);title.toggle();
        expect(same(title.resolve(2,orbit,130).nativePose.position,orbit.position),
            "entering gameplay after Title binds the new player camera");
    }
    {
        HeadCamera editor;editor.configure(true,1,true);
        ControllerFrame title;title.frontEnd=true;title.authoredCamera=true;title.referenceEpoch=1;
        editor.trackStereo({},rigEyes,true,100,title);editor.toggle();
        const Pose bed{{.38268343f,0,0,.92387953f},{-41,108,-1717}};
        const std::array<float,16> bedRoot{0,0,-1,0,0,1,0,0,1,0,0,0,-41.1f,106.4f,-1717.1f,1};
        const std::array<float,16> bedHead{1,0,0,0,0,1,0,0,0,0,1,0,0,1.6f,.1f,1};
        editor.publishPlayerHead(1,22,bed,bedRoot,bedHead,100);
        expect(editor.resolve(1,bed,100).applied,"the authored hospital bed scene supplies the accepted backdrop camera");
        ControllerFrame input;input.avatarEditor=true;input.authoredCamera=true;input.referenceEpoch=1;
        editor.trackStereo({},rigEyes,true,110,input);
        const Pose preview{{0,1,0,0},{-.09f,1001.4f,2.3f}};
        const auto frame=editor.resolve(1,preview,110);
        expect(!frame.applied&&frame.controllers.avatarEditor&&!frame.controllers.frontEnd
            &&same(frame.nativePose.position,preview.position),
            "AvatarEdit preserves the real native preview camera and full UI canvas");
        expect(!editor.active()&&editor.status().reason==HeadCameraStop::sceneUnavailable,
            "AvatarEdit uses native quad capture while the compositor retains hospital stereo");
        input.avatarEditor=false;input.scriptedDemo=true;input.frontEnd=false;input.authoredCamera=true;
        editor.trackStereo({},rigEyes,true,120,input);
        const Pose shot{{0,1,0,0},{-40,109,-1718}};
        editor.trackStereo({},rigEyes,true,700,input);
        const auto cutscene=editor.resolve(2,shot,700);
        expect(cutscene.applied&&cutscene.playerOwner==0&&same(cutscene.nativePose.position,shot.position),
            "AvatarEdit hands its retained hospital view to the next authored cutscene camera");
    }
    {
        OpeningSelector opening;const Pose titleHead{};std::array<TrackedHand,2> hands{};
        for(auto& hand:hands)hand.gripTracked=true;
        const OpeningNativeFocus focus{{2,3,5,4,7,8,0},6,0,true};
        auto tick=[&](uint64_t time){return opening.update(true,true,false,titleHead,hands,time,{},focus);};
        auto frame=tick(100);
        expect(frame.active&&!frame.dogPetted&&frame.continueReady,"Native Continue is available without a fabricated animal");
        hands[1].grip=cabinTapeHomes()[0];hands[1].trigger=.8f;frame=tick(150);
        expect(frame.pulse==OpeningPulse::none,"Touching a printed cassette cannot execute a menu action");
        hands[1].squeeze=1;frame=tick(200);
        expect(frame.bench.tapes[0].hand==1,"Grip picks up the cassette with its own identity");
        hands[1].grip=compose(cabinSlotPose(),Pose{{},{0,0,.05f}});frame=tick(250);
        hands[1].grip=cabinSlotPose();frame=tick(300);
        hands[1].squeeze=0;frame=tick(350);
        expect(frame.bench.inserted==0&&frame.bench.tapes[0].state==CabinTapeState::seated,"Release seats a guided cassette");
        bool confirmed=false;
        for(uint64_t time=400;time<=950;time+=50){frame=tick(time);confirmed=confirmed||frame.pulse==OpeningPulse::confirm;}
        expect(confirmed&&frame.pulse==OpeningPulse::none,"Seated Continue generates one bounded native confirmation");
        hands[1].grip.position=cabinEjectButton();hands[1].trigger=0;tick(1000);
        hands[1].grip.position.x+=.10f;
        hands[1].indexTip=cabinEjectButton();hands[1].indexTipTracked=true;
        frame=tick(1050);
        expect(frame.bench.inserted==-1&&frame.bench.ejectPressed,
            "Rendered fingertip contact ejects without trigger or controller-grip proximity");
        frame=tick(1070);
        expect(!frame.bench.ejectPressed,"Resting a fingertip on eject cannot repeatedly press the button");
        for(uint64_t time=1100;time<=1450;time+=50)frame=tick(time);
        expect(frame.bench.tapes[0].state==CabinTapeState::presented,
            "Eject retains the cassette in its carrier rather than throwing it onto the floor");
        const auto presented=frame.bench.tapes[0].pose;
        hands[1].grip.position={4,4,4};hands[1].squeeze=1;frame=tick(1470);
        expect(same(frame.bench.tapes[0].pose.position,presented.position),
            "An ejected cassette remains supported while the player reaches for it");
        hands[1].grip=frame.bench.tapes[0].pose;frame=tick(1500);
        expect(frame.bench.tapes[0].hand==1,"An ejected cassette remains recoverable by hand");
        hands[1].grip.position={-.12f,-.35f,-.59f};frame=tick(1550);
        hands[1].squeeze=0;frame=tick(1600);
        for(uint64_t time=1650;time<=3000;time+=20)frame=tick(time);
        const auto tape=frame.bench.tapes[0];
        expect(tape.hand<0&&tape.pose.position.y>=-1.12f+cabinTapeHalf.z-.001f,"A dropped cassette falls onto cabin support");
        OpeningSelector unknown;hands[1].squeeze=0;hands[1].grip=cabinTapeHomes()[0];
        unknown.update(true,true,false,titleHead,hands,100);
        hands[1].squeeze=1;unknown.update(true,true,false,titleHead,hands,150);
        hands[1].grip=cabinSlotPose();unknown.update(true,true,false,titleHead,hands,200);
        hands[1].squeeze=0;unknown.update(true,true,false,titleHead,hands,250);
        bool blocked=false;
        for(uint64_t time=300;time<=700;time+=50){frame=unknown.update(true,true,false,titleHead,hands,time);blocked|=frame.blocked;expect(frame.pulse==OpeningPulse::none,"Unknown native focus never receives confirmation");}
        expect(blocked,"Insertion reports unavailable native focus instead of guessing its action");
    }
    {
        const Pose pelvis{{0,.38268343f,0,.92387953f},{.5f,.9f,-.5f}};
        const auto hip=compose(pelvis,Pose{{},{-.096f,-.094f,.015f}});
        CabinStudioFrame joined;joined.available=true;
        joined.camera={{},{2,1,3}};
        joinStudioBody(joined,pelvis,hip);
        CabinStudio studio;std::array<StudioHand,2> hands{};
        auto frame=studio.update(true,false,Pose{{},{2,1.7f,3}},Pose{},hands,100,0,false,
            StudioFreeze::live,joined.fieldCameraHolster);
        const auto resting=frame->camera;
        const Pose movedHead{{-.70710678f,0,0,.70710678f},{3,.7f,4}};
        frame=studio.update(true,false,movedHead,Pose{},hands,150,0,false,
            StudioFreeze::live,joined.fieldCameraHolster);
        const bool bodyAnchored=joined.fieldCameraHolster&&same(frame->camera.position,resting.position)
            &&near(compose(inverse(pelvis),resting).position.y,-.094f)
            &&compose(inverse(pelvis),resting).position.x+studioCameraHalf.x<=-.25f
            &&compose(inverse(pelvis),resting).position.z>compose(inverse(pelvis),hip).position.z
            &&same(rotate(resting.orientation,{0,0,-1}),rotate(pelvis.orientation,{0,0,1}));
        hands[1].tracked=true;hands[1].grip=studioCameraPalm(resting,1);hands[1].squeeze=1;
        frame=studio.update(true,false,movedHead,Pose{},hands,200,0,false,
            StudioFreeze::live,joined.fieldCameraHolster);
        expect(bodyAnchored&&frame->cameraHand==1
            &&same(studioCameraPalm(frame->camera,1).position,hands[1].grip.position),
            "The field camera stays on the native right hip through independent HMD motion and picks up at that same authored handle");
    }
    {
        for(size_t side=0;side<2;++side){
            CabinStudio bilateral;std::array<StudioHand,2> contact{};
            contact[side].tracked=true;contact[side].squeeze=1;
            contact[side].grip.position={0,2,0};
            bilateral.update(true,true,Pose{},Pose{},contact,100);
            contact[side].grip.position=studioCameraHome;
            auto picked=bilateral.update(true,true,Pose{},Pose{},contact,150);
            expect(picked->cameraHand==static_cast<int>(side),"Either hand acquires the cabin camera with grip held before contact");
            contact[side].trigger=1;picked=bilateral.update(true,true,Pose{},Pose{},contact,1200);
            expect(picked->shutter==1200,"Either camera hand operates its own shutter");
            contact[side].squeeze=contact[side].trigger=0;
            picked=bilateral.update(true,true,Pose{},Pose{},contact,1250);
            expect(picked->cameraHand<0,"Either hand releases the camera");
            contact[side].grip.position={0,2,0};contact[side].squeeze=1;
            bilateral.update(true,true,Pose{},Pose{},contact,1300);
            contact[side].grip.position=studioNotesHome;
            picked=bilateral.update(true,true,Pose{},Pose{},contact,1350);
            expect(picked->papers.size()==1&&picked->papers[0].hand==static_cast<int>(side),
                "Either hand peels a note with grip held before contact");
            contact[side].squeeze=0;picked=bilateral.update(true,true,Pose{},Pose{},contact,1400);
            expect(picked->papers[0].hand<0,"Either hand releases its note");
        }
        CabinStudio studio;std::array<StudioHand,2> hands{};
        for(auto& hand:hands)hand.tracked=true;
        hands[1].grip.position=studioCameraHome;
        studio.update(true,true,Pose{},Pose{},hands,100);
        hands[1].squeeze=1;auto frame=studio.update(true,true,Pose{},Pose{},hands,150);
        expect(frame->cameraHand==1&&(frame->inputMask&2),"The physical camera owns its held hand's action input");
        hands[1].trigger=1;frame=studio.update(true,true,Pose{},Pose{},hands,1200);
        const auto shot=frame->shutter;
        frame=studio.update(true,true,Pose{},Pose{},hands,1250,1234);
        frame=studio.update(true,true,Pose{},Pose{},hands,1300,1234);
        expect(shot&&frame->shutter==shot&&frame->papers.size()==1&&frame->papers[0].photo==1234,
            "A shutter hold produces one photo identity and one physical print");
        hands[1].squeeze=hands[1].trigger=0;studio.update(true,true,Pose{},Pose{},hands,1350);
        hands[0].grip={{-.70710678f,0,0,.70710678f},studioNotesHome};
        hands[0].squeeze=1;frame=studio.update(true,true,Pose{},Pose{},hands,1400);
        expect(frame->papers.size()==2&&frame->papers.back().hand==0,"A fresh note is peeled from the physical stack");
        hands[0].grip=cabinNativeBoard;studio.update(true,true,Pose{},Pose{},hands,1450);
        hands[0].squeeze=0;frame=studio.update(true,true,Pose{},Pose{},hands,1500);
        expect(frame->papers.back().pinned&&frame->papers.back().hand<0,"Releasing an aligned note on the native board attaches it");
        hands[0].squeeze=1;frame=studio.update(true,true,Pose{},Pose{},hands,1550);
        expect(!frame->papers.back().pinned&&frame->papers.back().hand==0,"A placed note can be pulled back off the board");
        hands[0].squeeze=0;
        CabinStudio restored;restored.restore(frame->papers);frame=restored.update(true,true,Pose{},Pose{},hands,1600);
        expect(frame->papers.size()==2&&frame->papers.front().photo==1234&&frame->papers.back().hand<0,
            "Restored papers retain their photo identity without retaining a stale grip owner");
        CabinStudioFrame held=*frame;held.cameraHand=1;held.markerHand=0;
        held.papers.front().hand=0;held.papers.back().hand=-1;
        held.roomTracking={{0,.38268343f,0,.92387953f},{2,1,-3}};
        held.camera={{},{.2f,-.2f,-.4f}};held.marker={{},{.1f,.2f,.3f}};
        const auto oldCamera=held.camera,oldMarker=held.marker,oldPaper=held.papers.front().pose,wall=held.papers.back().pose;
        std::array<Pose,2> raw{Pose{{},{-.2f,-.2f,-.4f}},Pose{{},{.3f,-.2f,-.4f}}},palms=raw;
        palms[0].position.y+=.05f;palms[1].position.x-=.10f;
        joinStudioHands(held,raw,palms,{true,true});
        const auto shift=compose(palms[0],inverse(raw[0]));
        expect(same(held.camera.position,oldCamera.position+Vec3{-.10f,0,0})
            &&same(compose(held.roomTracking,held.marker).position,compose(shift,compose(held.roomTracking,oldMarker)).position)
            &&same(compose(held.roomTracking,held.papers.front().pose).position,compose(shift,compose(held.roomTracking,oldPaper)).position)
            &&same(held.papers.back().pose.position,wall.position)
            &&same(held.shutterPose.position,compose(held.camera,studioCameraLens).position),
            "Held camera, lens, marker and paper follow solved palms across room frames without moving the pinned wall");
        CabinStudio nearby;StudioPaper print{};print.id=7;print.photo=1234;
        print.pose={{},studioCameraHome+Vec3{-.08f,-.06f,0}};print.ink=std::make_shared<const StudioInk>();
        nearby.restore({print});hands={};hands[0].tracked=hands[0].aimTracked=true;hands[0].grip=print.pose;
        nearby.update(true,true,Pose{},Pose{},hands,1700);hands[0].squeeze=1;
        const auto picked=nearby.update(true,true,Pose{},Pose{},hands,1750);
        expect(picked->cameraHand<0&&picked->papers.front().hand==0,
            "A palm on the print picks the thin paper even inside the camera's former broad grab sphere");
    }
    {
        CabinStudio studio;std::array<StudioHand,2> hands{};uint64_t tick=2000;
        for(auto& hand:hands){hand.tracked=hand.aimTracked=true;}
        hands[1].grip.position=studioCameraHome;
        auto frame=studio.update(true,true,{}, {},hands,tick);
        hands[1].squeeze=1;frame=studio.update(true,true,{}, {},hands,tick+=20);
        frame=studio.update(true,true,{}, {},hands,tick+=20,111);
        frame=studio.update(true,true,{}, {},hands,tick+=20,222);
        const auto click=[&](StudioControl button){
            hands[0].trigger=0;frame=studio.update(true,true,{}, {},hands,tick+=20);
            hands[0].aim=compose(studioControlPose(frame->camera,button),Pose{{},{0,0,.2f}});
            hands[0].trigger=1;frame=studio.update(true,true,{}, {},hands,tick+=20);
        };
        click(StudioControl::back);click(StudioControl::print);
        expect(frame->selectedPhoto==111&&frame->review&&frame->papers.size()==3
            &&frame->papers.front().id!=frame->papers.back().id,
            "Browsing and reprinting an older photograph creates an independent paper copy");
        click(StudioControl::erase);
        expect(frame->confirmPhoto==111&&frame->photos.size()==2,"Photo disposal requires an explicit second trigger edge");
        frame=studio.update(true,true,{}, {},hands,tick+=20);
        expect(frame->photos.size()==2,"Holding Delete cannot confirm the initial request");
        click(StudioControl::live);
        expect(!frame->confirmPhoto&&!frame->review&&frame->photos.size()==2,"Live cancels photo deletion");
        click(StudioControl::erase);click(StudioControl::erase);
        expect(frame->photos==std::vector<uint64_t>{222}&&frame->papers.size()==3&&frame->papers.front().photo==111,
            "Confirmed digital deletion keeps every existing physical copy");
        CabinStudio restored;restored.restore(StudioCollection{frame->papers,frame->photos,frame->selectedPhoto});
        const auto reopened=restored.update(true,true,{}, {},{},tick+=20);
        expect(reopened->photos==std::vector<uint64_t>{222}&&reopened->papers.size()==3,
            "Restoring prints never resurrects a deleted digital library entry");
        hands[0].trigger=hands[1].squeeze=0;studio.update(true,true,{}, {},hands,tick+=20);
        hands[0].grip={Quat{-.70710678f,0,0,.70710678f},studioNotesHome};hands[0].squeeze=1;
        frame=studio.update(true,true,{}, {},hands,tick+=20);const auto note=frame->papers.back().id;
        hands[1].aim=compose(frame->papers.back().pose,Pose{{},{0,0,.2f}});hands[1].trigger=1;
        frame=studio.update(true,true,{}, {},hands,tick+=20);
        const auto key=[&](size_t index){
            hands[1].trigger=0;frame=studio.update(true,true,{}, {},hands,tick+=20);
            hands[1].aim=compose(studioKeyPose(frame->keyboard,index),Pose{{},{0,0,.25f}});
            hands[1].trigger=1;frame=studio.update(true,true,{}, {},hands,tick+=20);
        };
        key(43);expect(frame->confirmPaper==note&&frame->papers.size()==4,"Note disposal asks before removing the object");
        key(33);expect(!frame->confirmPaper&&frame->papers.size()==4,"Cancel preserves the physical note");
        key(43);key(43);
        expect(frame->papers.size()==3&&frame->editor<0&&frame->photos==std::vector<uint64_t>{222},
            "Confirmed note disposal permanently removes only that paper, without digital history");
    }
    {
        OpeningSelector opening;std::array<TrackedHand,2> hands{};
        for(auto& hand:hands)hand.gripTracked=true;
        const Pose entry{{0,.38268343f,0,.92387953f},{1,1.6f,-2}};
        hands[1].grip=compose(entry,Pose{{},openingPropOffsets()[1]});
        auto frame=opening.update(true,true,false,entry,hands,100);
        expect(frame.selection==0,"Continue begins at its cabin anchor");
        const auto moved=compose(entry,Pose{{0,.70710678f,0,.70710678f},{.3f,-.1f,-.4f}});
        frame=opening.update(true,true,false,moved,hands,200);
        expect(frame.selection==0&&same(frame.origin.position,entry.position),
            "head turning and translation do not drag the Continue contact target");
        hands[1].grip=compose(moved,Pose{{},openingPropOffsets()[1]});
        frame=opening.update(true,true,false,moved,hands,250);
        expect(frame.selection==-1,"head-relative empty space cannot select a room-anchored tape");
        hands[1].gripTracked=false;opening.update(true,true,false,moved,hands,300);
        hands[1].gripTracked=true;hands[1].grip=compose(entry,Pose{{},openingPropOffsets()[1]});
        hands[1].trigger=.8f;
        frame=opening.update(true,true,false,moved,hands,350);
        expect(frame.selection==0&&same(frame.origin.position,entry.position),
            "tracking recovery preserves the cabin contact anchor");
        frame=opening.updateCabin(true,moved,hands,400);
        expect(frame.cabinActive&&same(frame.origin.position,entry.position),
            "Title to cabin handoff preserves the animal and tape room anchor");
        HeadCamera roomCamera;roomCamera.configure(true);ControllerFrame input;input.frontEnd=true;
        input.openingOrigin=entry;roomCamera.trackStereo(entry,rigEyes,true,500,input);roomCamera.toggle();
        const auto before=roomCamera.resolve(1,nativeCamera,500);
        roomCamera.trackStereo(moved,rigEyes,true,510,input);
        const auto after=roomCamera.resolve(1,nativeCamera,510);
        const auto roomBefore=nativeTrackedPose(before.nativePose,before.headPose,before.controllers.openingOrigin);
        const auto roomAfter=nativeTrackedPose(after.nativePose,after.headPose,after.controllers.openingOrigin);
        expect(before.applied&&after.applied&&same(roomBefore.position,roomAfter.position)
            &&same(rotate(roomBefore.orientation,{0,0,-1}),rotate(roomAfter.orientation,{0,0,-1})),
            "native rendering and rat containment share a stationary cabin basis across head motion");
    }
    {
        OpeningSelector cabin;std::array<TrackedHand,2> hands{};
        for(auto& hand:hands)hand.gripTracked=true;
        auto frame=cabin.updateCabin(true,Pose{},hands,100);
        frame=cabin.updateCabin(true,Pose{},hands,600);
        expect(frame.cabinActive&&!frame.dogPetted,
            "post-loading cabin does not invent D-Dog or rat actors");
        const NativeRoomSamples samples{{.82f,.82f,.38f,.88f,1.20f,1.90f}};
        auto bounds=nativeRoomBoundsFromSamples(Pose{},1,samples,4.f);
        expect(validNativeRoomBounds(bounds)&&bounds.min.z==-1.90f&&bounds.max.z==1.20f,
            "production room constructor enables valid complete samples with negative rear extent");
        for(size_t axis=0;axis<samples.size();++axis){
            auto missing=samples;missing[axis].reset();
            expect(!validNativeRoomBounds(nativeRoomBoundsFromSamples(Pose{},1,missing,4.f)),
                "every missing native room ray invalidates the enclosing envelope");
            missing[axis]=-.5f;
            expect(!validNativeRoomBounds(nativeRoomBoundsFromSamples(Pose{},1,missing,4.f)),
                "behind-origin native room hits cannot authorize an envelope");
            missing[axis]=4.f;
            expect(!validNativeRoomBounds(nativeRoomBoundsFromSamples(Pose{},1,missing,4.f)),
                "a native room ray endpoint is not a wall");
        }
        expect(!validNativeRoomBounds(nativeRoomBoundsFromSamples(Pose{},0,samples,4.f)),
            "native room bounds require a live scene generation");
        expect(nativeRoomContains(bounds,{-.34f,-.70f,-1.12f})
            &&nativeRoomContains(bounds,{.31f,-.70f,-1.18f})
            &&!nativeRoomContains(bounds,{2,0,0})
            &&nativeRoomDimensions(bounds).x>1.6f,
            "native room bounds constrain real actors and expose dimensions");
        const auto clamped=nativeRoomClamp(bounds,{2,0,-1},.20f);
        expect(nativeRoomContains(bounds,nativeRoomLocal(bounds,clamped),.20f),
            "tracked VR rig is clamped inside the same native room contract");
        bounds.valid=false;
        expect(!nativeRoomContains(bounds,{0,0,0}),
            "missing native room bounds cannot authorize actor contact");
    }
    {
        // The actual wall is at Z=2. The old start-position envelope stopped
        // movement at Z=.15 when a bench was hit by one axial ray.
        const auto wall=[](Vec3 from,Vec3 to){
            if(to.z>1.8f&&to.z>from.z)
                return CabinSweep{true,true,(1.8f-from.z)/(to.z-from.z),{0,0,-1}};
            return CabinSweep{true,false};
        };
        const auto support=[](Vec3 p){return p.x<2.f;};
        const auto resolve=[&](Vec3 a,Vec3 b){return resolveCabinMovement(a,b,wall,support);};
        CabinWalkState walk{};
        auto position=advanceCabinWalk(walk,{},{},{0,1},1000,1,resolve,1.f);
        expect(same(position,{}),"cabin walking starts without a synthetic teleport");
        position=advanceCabinWalk(walk,{},{},{0,1},1050,1,resolve,1.f);
        expect(near(position.z,.05f),"forward stick input advances the native cabin view at a bounded rate");
        position=advanceCabinWalk(walk,{},{},{0,-1},1100,1,resolve,1.f);
        expect(std::abs(position.z)<.0001f,"reverse stick input returns through the same bounded path");

        CabinWalkState turned{};
        const Quat quarterTurn{0,.70710678f,0,.70710678f};
        advanceCabinWalk(turned,{},quarterTurn,{0,1},1000,1,resolve,1.f);
        const auto sideways=advanceCabinWalk(turned,{},quarterTurn,{0,1},1050,1,resolve,1.f);
        expect(near(sideways.x,.05f)&&std::abs(sideways.z)<.0001f,
            "cabin forward follows the tracked horizontal facing");

        CabinWalkState strafe{};
        advanceCabinWalk(strafe,{},{},{1,0},1000,1,resolve,1.f);
        const auto right=advanceCabinWalk(strafe,{},{},{1,0},1050,1,resolve,1.f);
        expect(near(right.x,-.05f)&&near(right.z,0),
            "cabin right stick movement follows FOX camera right, not world positive X");
        const auto left=advanceCabinWalk(strafe,{},{},{-1,0},1100,1,resolve,1.f);
        expect(same(left,{}),"opposite cabin strafe returns to the starting point");

        CabinWalkState seated{};
        advanceCabinWalk(seated,{},{},{0,1},1000,1,resolve);
        const auto seatedMove=advanceCabinWalk(seated,{},{},{0,1},1050,1,resolve);
        expect(near(seatedMove.y,0)&&seatedMove.z>0,
            "measured Quest cabin ceiling does not push a seated viewer down when walking");

        CabinWalkState bounded{};
        position=advanceCabinWalk(bounded,{},{},{0,0},1000,1,resolve,1.f);
        for(uint64_t time=1050;time<=4000;time+=50)
            position=advanceCabinWalk(bounded,{},{},{0,1},time,1,resolve,1.f);
        expect(position.z>1.79f&&position.z<1.8f,
            "cabin walking reaches the actual wall instead of the small initial sample box");
        const auto slide=resolve(position,position+Vec3{.2f,0,.2f});
        expect(slide&&slide->x>.19f&&slide->z<1.8f,"diagonal motion slides along the solid cabin wall");
        expect(!resolve({},{3,0,0}),"missing floor at an open door refuses movement outside the cabin");
        position=advanceCabinWalk(bounded,{}, {},{0,0},4100,2,resolve,1.f);
        expect(same(position,{})&&same(bounded.offset,{}),
            "a new scene generation clears stale cabin locomotion offset");

        CabinWalkState failClosed{};
        const auto unavailable=[](Vec3,Vec3)->std::optional<Vec3>{return {};};
        advanceCabinWalk(failClosed,{},{},{0,1},1000,1,unavailable);
        position=advanceCabinWalk(failClosed,{},{},{0,1},1050,1,unavailable);
        expect(same(position,{}),"unavailable native clearance fails closed without cabin movement");
        CabinWalkState menuWalk{};
        unsigned movementQueries=0;
        const auto countedResolve=[&](Vec3 a,Vec3 b){++movementQueries;return resolve(a,b);};
        advanceCabinWalk(menuWalk,{},{},{0,0},1000,1,countedResolve,1.f);
        const auto menuAnchor=advanceCabinWalk(menuWalk,{},{},{0,1},1050,1,countedResolve,1.f);
        const auto queriesBeforeMenu=movementQueries;
        const auto offsetBeforeMenu=menuWalk.offset;
        const auto menuPosition=advanceCabinWalk(menuWalk,menuAnchor,{},{0,1},5000,1,countedResolve,1.f,true);
        expect(same(menuPosition,menuAnchor)&&same(menuWalk.offset,offsetBeforeMenu)
            &&movementQueries==queriesBeforeMenu,
            "opening an ACC menu preserves the walked anchor without adding the offset twice or querying movement");
        const auto menuNavigation=advanceCabinWalk(menuWalk,menuAnchor,{},{1,-1},10000,1,countedResolve,1.f,true);
        expect(same(menuNavigation,menuAnchor)&&movementQueries==queriesBeforeMenu,
            "ACC menu navigation cannot move or collide the cabin camera");
        const auto stowed=advanceCabinWalk(menuWalk,{},{},{0,0},10010,1,countedResolve,1.f);
        expect(same(stowed,menuAnchor),"stowing an ACC menu restores the single preserved cabin walk offset");
        const auto resumed=advanceCabinWalk(menuWalk,{},{},{0,1},10020,1,countedResolve,1.f);
        expect(near(resumed.z,menuAnchor.z+.01f),"cabin walking resumes without accumulating time spent in a menu");
    }
    {
        CabinWalkState flight{};
        const auto clear=[](Vec3,Vec3 to)->std::optional<Vec3>{return to;};
        const auto blocked=[](Vec3,Vec3)->std::optional<Vec3>{return {};};
        advanceCabinWalk(flight,{10,20,30},{},{},1000,1,clear,1.f,false,true);
        const auto walked=advanceCabinWalk(flight,{10,20,30},{},{0,1},1050,1,clear,1.f,false,true);
        expect(near(walked.z,30.05f),"a passenger can walk inside the moving cabin");
        const auto carried=advanceCabinWalk(flight,{12,21,35},{},{},1100,1,blocked,1.f,false,true);
        expect(same(carried,{12,21,35.05f}),
            "native cabin translation carries the accepted offset even without a new walking step");
        const Quat quarterTurn{0,.70710678f,0,.70710678f};
        const auto turned=advanceCabinWalk(flight,{12,21,35},quarterTurn,{},1150,1,blocked,1.f,false,true);
        expect(same(turned,{12.05f,21,35}),"cabin rotation carries the passenger offset with the helicopter");
        const auto rebased=advanceCabinWalk(flight,{100,21,35},quarterTurn,{},1200,1,clear,1.f,false,true);
        expect(same(rebased,{100,21,35})&&same(flight.offset,{}),
            "a discontinuous native flight transition clears stale passenger offset");
        advanceCabinWalk(flight,{100,21,35},quarterTurn,{0,1},1250,1,clear,1.f,false,true);
        const auto fresh=advanceCabinWalk(flight,{101,21,35},quarterTurn,{},1300,2,clear,1.f,false,true);
        expect(same(fresh,{101,21,35}),"a new cabin owner cannot inherit the previous flight offset");
    }
    {
        CabinTurn turn;
        expect(near(turn.update({1,0},1,30,true,1000,1),0),"entering a cabin with stick held cannot turn");
        turn.update({0,0},1,30,true,1010,1);
        const auto yaw=turn.update({1,0},1,30,true,1060,1);
        expect(yaw<0&&near(yaw,-1.570796327f*.05f),"cabin smooth right turn uses elapsed time and FOX handedness");
        expect(near(turn.update({1,0},1,30,true,2060,1),0),"a stalled frame cannot accumulate a cabin turn jump");
        turn.update({0,0},0,45,true,2070,1);
        expect(near(turn.update({1,0},0,45,true,2080,1),-.785398164f),"cabin snap turn obeys the opt-in angle");
        expect(near(turn.update({1,0},0,45,true,2090,1),0),"a held cabin snap cannot repeat");
        turn.update({0,0},2,30,true,2100,1);
        expect(near(turn.update({1,0},2,30,true,2150,1),0),"turning off disables cabin stick yaw");
        turn.update({0,0},1,30,true,2160,1);
        expect(near(turn.update({1,0},1,30,true,2170,2),0),"new cabin generation discards an armed stick");
    }
    {
        CabinWalkState cabin{};
        const auto clear=[](Vec3,Vec3 to)->std::optional<Vec3>{return to;};
        const Pose carrier{{},{10,20,30}};
        advanceCabinWalk(cabin,carrier.position,{},{},1000,1,clear,1.f,false,true,carrier);
        const auto walked=advanceCabinWalk(cabin,carrier.position,{},{0,1},1050,1,clear,1.f,false,true,carrier);
        const Quat yaw{0,.70710678f,0,.70710678f};
        const auto looked=advanceCabinWalk(cabin,carrier.position,yaw,{},1100,1,clear,1.f,false,true,carrier);
        expect(same(looked,walked),"looking sideways cannot swing a passenger's walk offset around the seat");
        const Quat pitch{.38268343f,0,0,.92387953f};
        const auto pitched=advanceCabinWalk(cabin,carrier.position,pitch,{},1150,1,clear,1.f,false,true,carrier);
        expect(same(pitched,walked),"looking up cannot lift or lower the accepted cabin walking position");
        const auto leaned=advanceCabinWalk(cabin,carrier.position+Vec3{.1f,0,0},yaw,{},1200,1,clear,1.f,false,true,carrier);
        expect(same(leaned,walked+Vec3{.1f,0,0}),"tracked leaning adds translation without rotating the accepted walk offset");
        const Pose turningCarrier{yaw,{12,21,35}};
        const auto carried=advanceCabinWalk(cabin,turningCarrier.position,pitch,{},1250,1,clear,1.f,false,true,turningCarrier);
        expect(same(carried,Vec3{12.05f,21,35}),"the actual helicopter carries and rotates a passenger offset exactly once");
    }
    {
        HeadCamera passenger;passenger.configure(true,1,true);
        ControllerFrame input;input.cabinTransit=true;input.handheldMenus=true;
        input.referenceEpoch=1;input.predictedXrTime=1;
        auto root=identityMatrix;root[12]=100.f;
        auto head=identityMatrix;head[13]=1.2f;
        const Pose orbit{{0,std::sin(.3f),0,std::cos(.3f)},{94,3,-8}};
        passenger.trackStereo({},rigEyes,true,100,input);passenger.toggle();
        expect(passenger.publishPlayerHead(31,42,orbit,root,head,100),"passenger joins the actual native camera and seated player");
        const auto seated=passenger.resolve(31,orbit,100);
        expect(seated.applied&&same(seated.nativePose.position,seated.playerHead)&&seated.cabinBaseTracked,
            "helicopter passengers start at the authored seated head without a standing shoulder/height offset");
        const Quat yaw{0,.70710678f,0,.70710678f};
        const Pose nativeOrbit{{0,std::sin(-1.f),0,std::cos(-1.f)},{92,4,8}};
        head[12]=-.2f;head[13]=1.5f;head[14]=.18f;
        passenger.trackStereo(Pose{yaw,{}},rigEyes,true,110,input);
        passenger.publishPlayerHead(31,42,nativeOrbit,root,head,110);
        const auto looking=passenger.resolve(31,nativeOrbit,110);
        expect(looking.applied&&same(looking.nativePose.position,seated.nativePose.position),
            "HMD turning, native camera orbit and seated idle animation cannot displace the passenger viewpoint");
        expect(same(looking.cabinBasePose.position,seated.cabinBasePose.position)
            &&same(rotate(looking.cabinBasePose.orientation,{0,0,1}),rotate(seated.cabinBasePose.orientation,{0,0,1})),
            "the carrier pose retains the same native seat transaction before HMD rotation");
        root[12]=103;root[13]=2;root[0]=root[10]=0;root[2]=-1;root[8]=1;
        auto movingOrbit=nativeOrbit;movingOrbit.position.x+=3;
        passenger.trackStereo({},rigEyes,true,120,input);
        expect(passenger.publishPlayerHead(31,42,movingOrbit,root,head,120),"the moving camera remains within its native player ownership bound");
        const auto carried=passenger.resolve(31,movingOrbit,120);
        expect(carried.applied&&same(carried.nativePose.position,Vec3{103,3.2f,0}),
            "native helicopter translation and turning carry the authored seat rather than a world anchor or animated head");
        passenger.setNativeMenuOpen(true,true);
        root[12]+=3;
        movingOrbit.position.x+=3;
        passenger.trackStereo({},rigEyes,true,130,input);
        expect(passenger.publishPlayerHead(31,42,movingOrbit,root,head,130),"handheld cabin motion retains native camera/player ownership");
        const auto phone=passenger.resolve(31,movingOrbit,130);
        expect(phone.applied&&phone.menuIdroid&&phone.cabinBaseTracked
            &&same(phone.nativePose.position,carried.nativePose.position+Vec3{3,0,0}),
            "a passenger's handheld iDroid follows the same moving cabin and preserves the seated fit");
    }
    {
        HeadCamera driver;driver.configure(true,1,true);
        ControllerFrame input;input.vehicleControls=true;input.vehicleMounted=true;input.handheldMenus=true;
        input.vehicleView={true,false,true,true,25600,1.570796327f};
        input.referenceEpoch=1;input.predictedXrTime=1;
        auto root=identityMatrix;root[12]=100.f;
        root[0]=root[10]=0;root[2]=-1;root[8]=1;
        auto head=identityMatrix;head[13]=1.6f;
        const Pose chase{{0,1,0,0},{94.f,3.f,-8.f}};
        driver.trackStereo({},rigEyes,true,100,input);driver.toggle();
        expect(driver.publishPlayerHead(31,42,chase,root,head,100),"driver skin joins the native vehicle chase camera");
        const auto seated=driver.resolve(31,chase,100);
        expect(seated.applied&&same(rotate(seated.nativePose.orientation,{0,0,1}),Vec3{1,0,0}),
            "vehicle first-person eyes face the authored seated player rather than the backward chase-camera orbit");
        expect(same(seated.nativePose.position,seated.playerHead),
            "a seated driver uses the native head rather than the standing backward/height fit that places the wheel beyond reach");
        driver.setNativeMenuOpen(true,true);
        root[12]+=3.f;
        Pose movingChase{{0,std::sin(-1.f),0,std::cos(-1.f)},{97.f,3.f,-8.f}};
        driver.trackStereo({},rigEyes,true,110,input);
        driver.publishPlayerHead(31,42,movingChase,root,head,110);
        const auto phone=driver.resolve(31,movingChase,110);
        expect(seated.applied&&phone.applied&&phone.menuIdroid
            &&same(phone.nativePose.position,seated.nativePose.position+Vec3{3.f,0,0})
            &&phone.playerOwner==42&&phone.playerSequence>seated.playerSequence,
            "handheld iDroid follows the live seated head rather than moving VR outside to the vehicle chase boom");
        expect(same(rotate(phone.nativePose.orientation,{0,0,1}),Vec3{1,0,0}),
            "opening the live vehicle iDroid cannot turn the seated eyes toward its unrelated native camera orbit");
        input.vehicleViewInput={.5f,0};
        movingChase.orientation={0,std::sin(-.9f),0,std::cos(-.9f)};
        driver.trackStereo({},rigEyes,true,120,input);
        driver.publishPlayerHead(31,42,movingChase,root,head,120);
        const auto looking=driver.resolve(31,movingChase,120);
        expect(looking.applied&&same(rotate(looking.nativePose.orientation,{0,0,1}),
                Vec3{std::cos(.2f),0,-std::sin(.2f)}),
            "deliberate vehicle right-stick native look retains its angular response in the seated VR view");
        input.vehicleViewInput={};
        root[0]=root[10]=-std::sin(.2f);root[2]=-std::cos(.2f);root[8]=std::cos(.2f);
        input.vehicleView.heading+=.2f;
        movingChase.orientation={0,std::sin(-.8f),0,std::cos(-.8f)};
        driver.trackStereo({},rigEyes,true,130,input);
        driver.publishPlayerHead(31,42,movingChase,root,head,130);
        const auto turning=driver.resolve(31,movingChase,130);
        expect(turning.applied&&same(rotate(turning.nativePose.orientation,{0,0,1}),
                Vec3{std::cos(.4f),0,-std::sin(.4f)}),
            "vehicle heading advances once while native look remains relative to the carrier");
        input.vehicleControls=false;
        movingChase.orientation={0,std::sin(-.2f),0,std::cos(-.2f)};
        driver.trackStereo({},rigEyes,true,390,input);
        driver.publishPlayerHead(31,42,movingChase,root,head,390);
        const auto focusedPhone=driver.resolve(31,movingChase,390);
        expect(focusedPhone.applied&&same(focusedPhone.nativePose.position,focusedPhone.playerHead)
            &&same(rotate(focusedPhone.nativePose.orientation,{0,0,1}),
                rotate(turning.nativePose.orientation,{0,0,1})),
            "iDroid input focus preserves the occupied seat and facing through an unrelated native camera orbit");
        const auto passive=passiveControllerFrame(input,2,1);
        expect(passive.vehicleMounted&&!passive.vehicleControls,
            "a focus overlay retains native vehicle occupancy while releasing vehicle input");
        HeadCamera bodyDriver;bodyDriver.configure(true,1,true);
        ControllerFrame bodyInput;bodyInput.fullBody=true;bodyInput.vehicleMounted=true;
        bodyInput.referenceEpoch=1;
        bodyDriver.trackStereo({},rigEyes,true,150,bodyInput);bodyDriver.toggle();
        bodyDriver.publishPlayerHead(31,42,movingChase,root,head,150);
        const NativeBodyHead seatedSkin{42,99,Pose{{},{103.f,1.2f,.2f}}};
        const auto bodySeat=bodyDriver.resolve(31,movingChase,150,seatedSkin);
        const auto skinEye=compose(seatedSkin.worldFromHead,Pose{{},nativeEyeFromHead}).position;
        expect(bodySeat.applied&&bodySeat.anatomicalEyeModel==99&&same(bodySeat.nativePose.position,skinEye),
            "full-body driver camera joins original seated skin eyes rather than the distinct cached HEAD");
        auto unmatched=movingChase;unmatched.position.x+=1.f;
        driver.trackStereo({},rigEyes,true,400,input);
        expect(!driver.resolve(31,unmatched,400).applied
            &&driver.status().reason==HeadCameraStop::playerHeadUnavailable,
            "moving handheld vehicle eyes reject an unmatched native camera/player transaction");
    }
    {
        HeadCamera handset;handset.configure(true,1,true);
        ControllerFrame input;input.handheldMenus=true;input.snapYaw=.5f;
        const Pose lookingUp{{.258819f,0,0,.965926f},{.1f,0,.2f}};
        handset.trackStereo(lookingUp,rigEyes,true,100,input);handset.toggle();
        handset.publishPlayerHead(31,42,thirdPerson,playerRoot,headBone,100);
        const auto before=handset.resolve(31,thirdPerson,100);
        handset.setNativeMenuOpen(true,true);
        const auto opened=handset.resolve(31,thirdPerson,100);
        expect(before.applied&&opened.applied
            &&same(rotate(before.nativePose.orientation,{0,0,1}),rotate(opened.nativePose.orientation,{0,0,1}))
            &&same(before.nativePose.position,opened.nativePose.position),
            "opening a live handheld iDroid retains physical pitch, snap heading and roomscale position");
        handset.trackStereo({},rigEyes,true,110,input);
        const Pose nativeTilt{{.5f,0,0,.8660254f},thirdPerson.position};
        handset.publishPlayerHead(31,42,nativeTilt,playerRoot,headBone,110);
        const auto level=handset.resolve(31,nativeTilt,110);
        const auto levelForward=rotate(level.nativePose.orientation,{0,0,1});
        expect(level.applied&&near(levelForward.y,0)&&near(levelForward.x,std::sin(.5f)),
            "returning the HMD to level while handheld is open stays level despite native camera pitch");
        handset.setNativeMenuOpen(false,false);
        const auto closed=handset.resolve(31,nativeTilt,110);
        expect(closed.applied&&same(rotate(closed.nativePose.orientation,{0,0,1}),levelForward),
            "stowing the live handset preserves the same gameplay facing");
    }
    {
        HeadCamera guide;guide.configure(true,1,true);
        ControllerFrame input;input.handheldMenus=true;input.referenceEpoch=1;
        input.predictedXrTime=1;input.presentationFocused=true;input.presentationEpoch=1;
        guide.trackStereo({},rigEyes,true,100,input);guide.toggle();
        expect(guide.publishPlayerHead(31,42,thirdPerson,playerRoot,headBone,100),
            "forced-guide fixture starts with the accepted field player");
        const auto field=guide.resolve(31,thirdPerson,100);
        guide.setNativeMenuOpen(true,true);
        const auto phone=guide.resolve(31,thirdPerson,100);
        expect(field.applied&&phone.applied&&phone.menuIdroid&&!phone.menuWorldQuad,
            "ordinary iDroid keeps its existing handheld presentation");
        guide.setNativeMenuOpen(true,true,true);
        input.predictedXrTime=2;
        guide.trackStereo(Pose{{},{.04f,0,0}},rigEyes,true,400,input);
        const auto help=guide.resolve(31,thirdPerson,400);
        expect(help.applied&&help.menuOpen&&help.menuWorldQuad&&!help.menuIdroid
            &&!guide.status().suspended&&help.trackingSequence>phone.trackingSequence,
            "Help over a still-open iDroid keeps fresh head motion after native player skin pauses");
        const PausedRigOwner owner{31,42,phone.activation,input.referenceEpoch,100,phone.menuGeneration};
        expect(mayRefreshPausedRig(owner,31,help,400),
            "forced-guide overlap admits the exact owned paused skin for a fresh hand solve");
        guide.setNativeMenuOpen(true,true,false,true);
        const auto pausedPhone=guide.resolve(31,thirdPerson,400);
        expect(pausedPhone.applied&&pausedPhone.menuIdroid&&!pausedPhone.menuWorldQuad
            &&!guide.status().suspended&&same(pausedPhone.nativePose.position,help.nativePose.position)
            &&mayRefreshPausedRig(owner,31,pausedPhone,400),
            "closing Help preserves the physical handset and fresh rig while the native guide still pauses gameplay");
        expect(guide.publishRigFrame(31,42,thirdPerson,pausedPhone)
            &&guide.publishedRigFrame(400).has_value(),
            "a freshly solved paused guide rig remains available to the native handset publisher");
        expect(!guide.publishedRigFrame(551),
            "a paused tutorial cannot make an old tracked rig current");
        expect(!guide.resolve(99,thirdPerson,400).applied,
            "a tutorial pause cannot lend its accepted anchor to another camera");
        guide.setNativeMenuOpen(true,true,false);
        expect(!guide.resolve(31,thirdPerson,400).applied,
            "returning from Help cannot reuse an old player head as live handheld motion");
        guide.publishPlayerHead(31,42,thirdPerson,playerRoot,headBone,400);
        const auto reopened=guide.resolve(31,thirdPerson,400);
        expect(reopened.applied&&reopened.menuIdroid&&!reopened.menuWorldQuad
            &&!guide.status().suspended,
            "native player resumption returns the same terminal to the tracked LEFT handset");
    }
    for(const bool handheld:{false,true}){
        HeadCamera cabin;cabin.configure(true,1,true);
        ControllerFrame input;input.cabinPlay=true;input.handheldMenus=handheld;
        input.referenceEpoch=1;input.predictedXrTime=1;
        const Pose cabinSource{{0,-.952008f,0,.306075f},{.544129f,1198.19f,.441583f}};
        const Pose phoneSource{{},{.04177f,1198.34f,-1.04204f}};
        auto root=identityMatrix;root[13]=1198.f;
        auto head=identityMatrix;head[13]=.34f;
        cabin.trackStereo({},rigEyes,true,100,input);cabin.toggle();
        cabin.publishPlayerHead(11,22,cabinSource,root,head,100);
        auto walked=cabin.resolve(11,cabinSource,100);
        walked.nativePose.position=walked.nativePose.position+Vec3{.2f,0,.1f};
        expect(cabin.publishRigFrame(11,22,cabinSource,walked),"publish the walked ACC viewpoint before iDroid");
        walked=cabin.resolve(11,cabinSource,100);
        cabin.setNativeMenuOpen(true,true);cabin.trackStereo({},rigEyes,true,110,input);
        const auto opened=cabin.resolve(11,phoneSource,110);
        expect(opened.applied&&same(opened.nativePose.position,walked.nativePose.position)
            &&same(rotate(opened.nativePose.orientation,{0,0,1}),rotate(walked.nativePose.orientation,{0,0,1})),
            "both ACC iDroid modes preserve the walked view when the native phone camera changes position and heading");
        const Pose lean{{0,.13052619f,0,.99144486f},{.12f,0,.03f}};
        cabin.trackStereo(lean,rigEyes,true,120,input);
        const auto moved=cabin.resolve(11,phoneSource,120);
        const Pose basis{{0,1,0,0},{}};
        const auto expected=compose(walked.nativePose,compose(compose(basis,lean),inverse(basis)));
        expect(moved.applied&&same(moved.nativePose.position,expected.position)
            &&same(rotate(moved.nativePose.orientation,{0,0,1}),rotate(expected.orientation,{0,0,1})),
            "anchored ACC menus still follow physical HMD lean and rotation");
    }
    {
        HeadCamera cabinCamera;cabinCamera.configure(true);
        ControllerFrame input;input.frontEnd=true;input.openingSelector=true;
        input.referenceEpoch=1;input.predictedXrTime=1;input.hands[1].gripTracked=true;
        cabinCamera.trackStereo({},rigEyes,true,100,input);cabinCamera.toggle();
        auto frame=cabinCamera.resolve(1,nativeCamera,100);
        HeadCamera enteringCabin;enteringCabin.configure(true);
        enteringCabin.trackStereo({},rigEyes,true,100,input);enteringCabin.toggle();
        enteringCabin.resolve(1,nativeCamera,100);
        const Pose entryTurn{{0,.70710678f,0,.70710678f},{.3f,0,.2f}};
        enteringCabin.trackStereo(entryTurn,rigEyes,true,110,input);
        const auto entryAnchor=enteringCabin.openingTrackingOrigin(110);
        expect(entryAnchor&&same(entryAnchor->position,{})&&same(rotate(entryAnchor->orientation,{0,0,-1}),{0,0,-1}),
            "turning before the first tape skin keeps the original title cabin anchor");
        frame.controllers.openingWorldOrigin=nativeTrackedPose(frame.nativePose,frame.headPose,Pose{});
        frame.controllers.openingWorldAnchored=true;
        const auto fixedAnchor=frame.controllers.openingWorldOrigin;
        frame.nativePose.position.y+=1.f; // A floor constraint changes only the virtual camera.
        expect(cabinCamera.publishRigFrame(1,2,nativeCamera,frame),"publish constrained title rig");
        const auto anchor=cabinCamera.openingTrackingOrigin(100);
        expect(anchor&&same(nativeTrackedPose(frame.nativePose,frame.headPose,*anchor).position,fixedAnchor.position),
            "collision-corrected hand target reconstructs the unchanged native rack anchor");
        OpeningSelector opening;std::array<TrackedHand,2> hands{};hands[1].gripTracked=true;
        hands[1].grip=compose(*anchor,Pose{{},openingPropOffsets()[1]});
        auto selected=opening.update(true,true,false,{},hands,100,anchor);
        expect(selected.selection==0,"corrected physical tape target agrees with fixed native render anchor");
        hands[1].grip=Pose{{},openingPropOffsets()[1]};
        selected=opening.update(true,true,false,{},hands,110,anchor);
        expect(selected.selection==-1,"uncorrected controller space cannot select a displaced phantom tape");
        expect(!cabinCamera.openingTrackingOrigin(251),"stale native anchor cannot steer a current hand target");
        cabinCamera.cancel();expect(!cabinCamera.openingTrackingOrigin(110),"scene cancellation discards the input anchor");
    }
    for(const bool delayedPlayer:{false,true}){
        HeadCamera title;title.configure(true,1,true);ControllerFrame input;input.frontEnd=true;input.referenceEpoch=1;
        title.trackStereo({},rigEyes,true,100,input);title.toggle();
        if(delayedPlayer)expect(!title.resolve(11,thirdPerson,100).applied&&title.status().awaitingPlayer,
            "Title waits for its verified owner without inventing a menu origin");
        title.publishPlayerHead(11,22,thirdPerson,playerRoot,headBone,110);
        title.trackStereo({},rigEyes,true,110,input);
        const auto entry=title.resolve(11,thirdPerson,110);
        const auto expectedPanel=compose(nativeTrackedPose(entry.nativePose,entry.headPose,entry.headPose),Pose{{},{0,-.05f,-1.3f}});
        expect(entry.applied&&entry.playerOwner==22&&same(entry.nativePose.position,thirdPerson.position),
            "Title uses the authored camera rather than the decorative character's bed/head position");
        expect(same(entry.menuPanel.position,expectedPanel.position),
            "Title panel initializes in front of the viewer even after delayed player publication");
        title.cancel();input.frontEnd=false;title.trackStereo({},rigEyes,true,120,input);title.toggle();
        title.publishPlayerHead(11,22,thirdPerson,playerRoot,headBone,120);
        const auto gameplay=title.resolve(11,thirdPerson,120);
        expect(gameplay.applied&&same(gameplay.nativePose.position,{499.75f,300.55f,1300.f}),
            "leaving Title restores the stabilized first-person head attachment");
    }
    {
        HeadCamera snaps;snaps.configure(true);ControllerFrame input;
        snaps.trackStereo({},rigEyes,true,100,input);snaps.toggle();snaps.resolve(1,nativeCamera,100);
        const Pose lean{{},{.3f,.1f,-.2f}};
        snaps.trackStereo(lean,rigEyes,true,110,input);
        const auto before=snaps.resolve(1,nativeCamera,110);
        input.snapYaw=-.523598776f;snaps.trackStereo(lean,rigEyes,true,120,input);
        const auto after=snaps.resolve(1,nativeCamera,120);
        const Quat turn{0,std::sin(input.snapYaw*.5f),0,std::cos(input.snapYaw*.5f)};
        expect(before.applied&&after.applied&&same(before.nativePose.position,after.nativePose.position),
            "snap pivots around the leaned physical head without teleporting it");
        expect(same(rotate(after.nativePose.orientation,{0,0,1}),rotate(turn,rotate(before.nativePose.orientation,{0,0,1}))),
            "snap rotates around world up without adding pitch or roll");
        expect(same(snaps.resolve(1,nativeCamera,121).nativePose.position,after.nativePose.position),
            "repeated native publications do not accumulate the snap pivot correction");
        snaps.setNativeMenuOpen(true);
        const auto menu=snaps.resolve(1,nativeCamera,122);
        expect(menu.menuOpen&&same(menu.nativePose.position,after.nativePose.position)
            &&same(rotate(menu.nativePose.orientation,{0,0,1}),rotate(after.nativePose.orientation,{0,0,1})),
            "opening a native menu does not apply the snap angle a second time");
        snaps.setNativeMenuOpen(false);
        expect(same(snaps.resolve(1,nativeCamera,123).nativePose.position,after.nativePose.position),
            "closing a native menu restores the same snapped gameplay viewpoint");
        const Quat nativeTurn{0,std::sin(.6f),0,std::cos(.6f)};
        auto turnedCamera=compose(Pose{nativeTurn,{}},nativeCamera);
        turnedCamera.position=nativeCamera.position;
        const auto turned=snaps.resolve(1,turnedCamera,124);
        expect(turned.applied&&same(turned.nativePose.position-nativeCamera.position,
            rotate(nativeTurn,after.nativePose.position-nativeCamera.position)),
            "native smooth/vehicle yaw rotates the entire tracking offset including the leaned snap pivot");
        snaps.recenter();
        const auto centered=snaps.resolve(1,turnedCamera,125);
        expect(centered.applied&&same(centered.nativePose.position,turnedCamera.position)
            &&same(rotate(centered.nativePose.orientation,{0,0,1}),rotate(turned.nativePose.orientation,{0,0,1})),
            "recenter clears a leaned snap/native-yaw offset without changing facing");
    }
    {
        HeadCamera seated;seated.configure(true);ControllerFrame input;input.referenceEpoch=1;
        input.seatedMode=true;input.fullBody=true;input.physicalFloorTracked=true;input.physicalFloorY=0;
        Pose head{{},{0,1.05f,0}},stock{{},{10,1.7f,20}};
        seated.trackStereo(head,rigEyes,true,100,input);seated.toggle();
        expect(near(seated.resolve(1,stock,100).nativePose.position.y,1.7f),"seated activation uses native eye height without standing calibration");
        head.position.y+=.1f;seated.trackStereo(head,rigEyes,true,110,input);
        expect(near(seated.resolve(1,stock,110).nativePose.position.y,1.8f),"seated mode retains physical vertical lean");
        seated.recenter();
        expect(near(seated.resolve(1,stock,111).nativePose.position.y,1.8f),"seated recenter preserves calibrated height during a lean");
        seated.trackStereo(head,rigEyes,false,120,input);
        head.position.y+=.1f;seated.trackStereo(head,rigEyes,true,130,input);
        expect(near(seated.resolve(1,stock,130).nativePose.position.y,1.9f),"tracking recovery does not silently recalibrate seated height");
        input.referenceEpoch=2;head.position.y-=1.f;seated.trackStereo(head,rigEyes,true,140,input);
        expect(near(seated.resolve(1,stock,140).nativePose.position.y,1.9f),"runtime reference change transports the seated baseline");
        head.position.y-=.5f;seated.trackStereo(head,rigEyes,true,140,input);seated.recenter();
        head.position.y+=.5f;seated.trackStereo(head,rigEyes,true,141,input);
        expect(near(seated.resolve(1,stock,141).nativePose.position.y,1.9f),
            "rising after a lowered seated recenter does not add permanent height");
        stock.position.y=.4f;
        expect(near(seated.resolve(1,stock,141).nativePose.position.y,.6f),"seated native crouch and prone still lower the viewpoint");
    }
    ControllerFrame hands;hands.referenceEpoch=1;hands.predictedXrTime=9000;
    {
        const auto yaw=[](float a){return Quat{0,std::sin(a*.5f),0,std::cos(a*.5f)};};
        const auto pitch=[](float a){return Quat{std::sin(a*.5f),0,0,std::cos(a*.5f)};};
        const auto roll=[](float a){return Quat{0,0,std::sin(a*.5f),std::cos(a*.5f)};};
        const Pose basis{{0,1,0,0},{}};
        const Pose stock=compose(Pose{yaw(-1.9f),{10,20,30}},Pose{pitch(-.35f),{}});
        Pose initial=compose(Pose{yaw(.3f),{.2f,1.6f,-.1f}},compose(Pose{pitch(-.4f),{}},Pose{roll(.12f),{}}));
        HeadCamera level;level.configure(true);ControllerFrame input;input.referenceEpoch=1;
        level.trackStereo(initial,rigEyes,true,100,input);level.toggle();level.resolve(1,stock,100);
        Pose moved{};
        for(int i=0;i<7;++i){
            moved=compose(Pose{yaw(i*.8f),{.5f,1.65f,-.4f}},Pose{pitch(.2f),{}});
            input.snapYaw=i%2?-.523598776f:0;
            level.trackStereo(moved,rigEyes,true,110+i*10,input);
            const auto frame=level.resolve(1,stock,110+i*10);
            const auto worldFromTracking=compose(compose(frame.nativePose,basis),inverse(moved));
            expect(frame.applied&&same(rotate(worldFromTracking.orientation,{0,1,0}),{0,1,0}),
                "tilted activation and native camera cannot tilt gravity during head or snap turns");
        }
        const auto before=level.resolve(1,stock,171);
        level.recenter();const auto centered=level.resolve(1,stock,172);
        expect(centered.applied&&same(centered.nativePose.position,stock.position)
            &&same(rotate(centered.nativePose.orientation,{0,0,1}),rotate(before.nativePose.orientation,{0,0,1})),
            "recenter brings the player under the head without changing the current facing or physical pitch");
        expect(centered.activation>before.activation,"recenter invalidates earlier eye and rig publications");
        input.referenceEpoch=2;
        const Pose shifted=compose(Pose{yaw(.65f),{2,0,3}},moved);
        level.trackStereo({},rigEyes,false,175,{});
        level.trackStereo(shifted,rigEyes,true,180,input);
        const auto rebased=level.resolve(1,stock,180);
        expect(rebased.applied&&same(rebased.nativePose.position,centered.nativePose.position)
            &&same(rotate(rebased.nativePose.orientation,{0,0,1}),rotate(centered.nativePose.orientation,{0,0,1})),
            "runtime recenter preserves gameplay and snap heading across LOCAL epochs even after lost focus/tracking");
    }
    {
        OpticStabilizer steady;OpticPose optic;optic.tracked=true;optic.rightEyepiece.position={0,0,-.08f};
        Pose grip{{},{.1f,-.02f,-.14f}};
        steady.update({},grip,optic,true,100,1);
        auto jitter=grip;jitter.position.x+=.004f;
        const auto damped=steady.update({},jitter,optic,true,111,1);
        expect(damped.position.x>grip.position.x&&damped.position.x<grip.position.x+.002f,
            "small close-eye grip tremor is damped before housing and scene attachment");
        const Pose head{{0,.258819f,0,.9659258f},{.1f,.03f,.02f}};
        auto attached=compose(head,damped);optic.rightEyepiece=compose(head,Pose{{},{0,0,-.08f}});
        const auto follows=steady.update(head,attached,optic,true,122,1);
        expect(same(follows.position,attached.position),"steady eyepiece follows head movement without filtering the headset");
        optic.rightEyepiece.position={0,0,-.4f};
        expect(same(steady.update({},grip,optic,true,133,1).position,grip.position),"lowered optics immediately return to direct tracked control");
        expect(same(steady.update({},jitter,optic,true,144,2).position,jitter.position),"tracking epoch resets never reuse an old stabilized grip");
    }
    hands.hands[1]={Pose{{},{0.2f,-0.3f,-0.4f}},Pose{{},{0.2f,-0.2f,-0.5f}},true,true};
    HeadCamera menuEpoch;menuEpoch.configure(true,1,true);
    auto menuHands=hands;
    menuEpoch.trackStereo({},rigEyes,true,100,menuHands);
    menuEpoch.publishPlayerHead(11,22,thirdPerson,playerRoot,headBone,100);
    menuEpoch.toggle();menuEpoch.resolve(11,thirdPerson,100);menuEpoch.setNativeMenuOpen(true);
    const Pose menuLean{{},{.2f,.1f,-.3f}};
    menuEpoch.trackStereo(menuLean,rigEyes,true,105,menuHands);
    const auto beforeMenuRebase=menuEpoch.resolve(11,thirdPerson,105);
    menuHands.referenceEpoch=2;menuHands.predictedXrTime=10000;
    menuEpoch.trackStereo(Pose{{0,.258819f,0,.9659258f},{3,1,-2}},rigEyes,true,110,menuHands);
    const auto rebasedMenu=menuEpoch.resolve(11,thirdPerson,110);
    expect(!menuEpoch.status().awaitingPlayer&&!menuEpoch.status().pending&&menuEpoch.active()
        &&rebasedMenu.menuOpen&&same(rebasedMenu.nativePose.position,beforeMenuRebase.nativePose.position)
        &&same(rebasedMenu.menuPanel.position,beforeMenuRebase.menuPanel.position),
           "a reference-space change inside iDroid rebases the live stereo menu viewpoint");
    menuEpoch.setNativeMenuOpen(false);
    menuEpoch.publishPlayerHead(11,22,thirdPerson,playerRoot,headBone,110);
    const auto afterMenuRebase=menuEpoch.resolve(11,thirdPerson,110);
    expect(afterMenuRebase.applied&&same(afterMenuRebase.nativePose.position,beforeMenuRebase.nativePose.position)
        &&same(rotate(afterMenuRebase.nativePose.orientation,{0,0,1}),rotate(beforeMenuRebase.nativePose.orientation,{0,0,1})),
           "closing iDroid after a reference-space change preserves the gameplay viewpoint");
    menuEpoch.setNativeMenuOpen(true,true);
    auto beforeMenuPause=menuEpoch.resolve(11,thirdPerson,110);
    beforeMenuPause.renderedPalms[1]=Pose{{0,0,.258819f,.9659258f},{2,1,3}};
    beforeMenuPause.renderedPalmTracked[1]=true;
    expect(menuEpoch.publishRigFrame(11,22,thirdPerson,beforeMenuPause),"menu accepts its final live skin publication");
    menuEpoch.resolve(11,thirdPerson,110);
    menuHands.predictedXrTime=11000;
    menuHands.presentationFocused=true;menuHands.presentationEpoch=1;
    menuHands.optic.held=true;
    menuEpoch.trackStereo(Pose{{0,.258819f,0,.9659258f},{3.1f,1,-2}},rigEyes,true,400,menuHands);
    const auto afterMenuPause=menuEpoch.resolve(11,thirdPerson,400);
    const auto pausedPublication=menuEpoch.publishedView();
    expect(pausedPublication&&pausedPublication->trackingSequence==afterMenuPause.trackingSequence
        &&pausedPublication->sampleTime==afterMenuPause.sampleTime&&!pausedPublication->rigSequence,
        "diagnostics expose a paused skin as stale even while new head tracking continues");
    expect(afterMenuPause.applied&&afterMenuPause.menuOpen&&!afterMenuPause.rigSequence
        &&!menuEpoch.status().suspended&&afterMenuPause.trackingSequence>beforeMenuPause.trackingSequence,
        "paused skin publication cannot black out a live tracked menu or masquerade as a current rig");
    expect(afterMenuPause.renderedPalmTracked[1]
        &&same(afterMenuPause.renderedPalms[1].position,beforeMenuPause.renderedPalms[1].position)
        &&same(rotate(afterMenuPause.renderedPalms[1].orientation,{0,1,0}),rotate(beforeMenuPause.renderedPalms[1].orientation,{0,1,0}))
        &&!afterMenuPause.controllers.optic.held,
        "paused iDroid keeps its visible palm attachment and hides an optic from an older input publication");
    menuEpoch.setNativeMenuOpen(true,false);
    const auto helpMenu=menuEpoch.resolve(11,thirdPerson,400);
    expect(helpMenu.applied&&helpMenu.menuOpen&&!helpMenu.menuIdroid
        &&same(helpMenu.menuPanel.position,beforeMenuPause.menuPanel.position),
        "iDroid Help keeps the accepted menu anchor while applying the native pause policy");
    {
        auto fresh=helpMenu;
        fresh.controllers.hands[1].gripTracked=true;
        fresh.controllers.presentationFocused=true;fresh.controllers.presentationEpoch=1;
        const PausedRigOwner owner{11,22,fresh.activation,fresh.controllers.referenceEpoch,110,fresh.menuGeneration};
        expect(mayRefreshPausedRig(owner,11,fresh,400),"paused owned animation accepts a fresh tracked skin solve");
        expect(!mayRefreshPausedRig(owner,12,fresh,400),"secondary camera cannot republish the player's paused skin");
        auto changed=fresh;changed.playerOwner=23;
        expect(!mayRefreshPausedRig(owner,11,changed,400),"a replacement player cannot inherit paused animation pointers");
        changed=fresh;++changed.activation;
        expect(!mayRefreshPausedRig(owner,11,changed,400),"retired camera activation cannot refresh a paused skin");
        changed=fresh;++changed.controllers.referenceEpoch;
        expect(!mayRefreshPausedRig(owner,11,changed,400),"reference-space changes require a newly accepted native target");
        changed=fresh;changed.menuOpen=false;
        expect(!mayRefreshPausedRig(owner,11,changed,400),"ordinary gameplay never runs the paused refresh");
        changed.controllers.cameraWorldFrozen=true;
        expect(mayRefreshPausedRig(owner,11,changed,400),"An acknowledged camera freeze refreshes tracked skin without a native menu");
        changed=fresh;changed.controllers.loading=true;
        expect(!mayRefreshPausedRig(owner,11,changed,400),"loading never dereferences a retained paused target");
        changed=fresh;changed.controllers.hands[1].gripTracked=false;
        expect(mayRefreshPausedRig(owner,11,changed,400),"a focused paused owner may attempt a retained presentation solve without raw grip tracking");
        changed.controllers.presentationFocused=false;
        expect(!mayRefreshPausedRig(owner,11,changed,400),"focus loss cannot refresh a paused skin from retained hands");
        changed=fresh;changed.controllers.presentationEpoch=0;
        expect(!mayRefreshPausedRig(owner,11,changed,400),"unidentified presentation cannot refresh a paused skin");
        expect(!mayRefreshPausedRig(owner,11,fresh,551)&&!mayRefreshPausedRig(owner,11,fresh,399),
            "stale and future tracking cannot be passed off as current paused skin");
        auto active=owner;active.nativeUpdateTime=300;
        expect(!mayRefreshPausedRig(active,11,fresh,400),"a running native skin job retains sole publication ownership");
        auto boundary=fresh;++boundary.menuGeneration;
        expect(mayRefreshPausedRig(active,11,boundary,400),
            "menu entry rebuilds the current owned skin before the paused timeout");
        boundary.menuOpen=false;
        expect(mayRefreshPausedRig(active,11,boundary,400),
            "menu exit rebuilds the current owned skin before exposing the gameplay camera");
        auto completed=active;completed.menuGeneration=boundary.menuGeneration;
        expect(!mayRefreshPausedRig(completed,11,boundary,400),
            "completed exit cannot repeatedly refresh ordinary gameplay skin");
        boundary.controllers.presentationFocused=false;
        expect(!mayRefreshPausedRig(active,11,boundary,400),"menu boundary never overrides lost presentation focus");
        boundary=fresh;++boundary.menuGeneration;boundary.playerOwner=23;
        expect(!mayRefreshPausedRig(active,11,boundary,400),"menu boundary never inherits another player's skin");
        boundary=fresh;++boundary.menuGeneration;
        expect(!mayRefreshPausedRig(active,11,boundary,551),"menu boundary never relabels stale tracking as fresh");
        fresh.renderedPalms[1].position.x+=.1f;
        expect(menuEpoch.publishRigFrame(11,22,thirdPerson,fresh),"a completed paused skin solve can publish its new palm");
        const auto refreshed=menuEpoch.resolve(11,thirdPerson,400);
        expect(refreshed.rigSequence&&refreshed.trackingSequence==fresh.trackingSequence
            &&same(refreshed.renderedPalms[1].position,fresh.renderedPalms[1].position),
            "camera and wrist display consume the refreshed paused skin transaction together");
        const auto beforeExit=refreshed;
        menuEpoch.setNativeMenuOpen(false);
        menuEpoch.publishPlayerHead(11,22,thirdPerson,playerRoot,headBone,400);
        const auto afterExit=menuEpoch.resolve(11,thirdPerson,400);
        expect(afterExit.applied&&!afterExit.menuOpen&&!afterExit.rigSequence
            &&afterExit.menuGeneration>beforeExit.menuGeneration,
            "native menu exit demands a skin publication from the new camera generation");
        expect(!menuEpoch.publishRigFrame(11,22,thirdPerson,beforeExit),
            "an in-flight menu skin cannot overwrite the post-exit camera transaction");
        expect(menuEpoch.publishRigFrame(11,22,thirdPerson,afterExit)
            &&menuEpoch.resolve(11,thirdPerson,400).rigSequence,
            "fresh post-exit skin and camera join without accepting the previous menu geometry");
        menuEpoch.setNativeMenuOpen(true,true);
        const auto nextEntry=menuEpoch.resolve(11,thirdPerson,400);
        expect(nextEntry.applied&&nextEntry.menuIdroid&&nextEntry.menuGeneration>afterExit.menuGeneration
            &&!menuEpoch.publishRigFrame(11,22,thirdPerson,afterExit),
            "immediate reopening also rejects a skin solved for the closed camera");
    }
    handsCamera.trackStereo({},rigEyes,true,100,hands);handsCamera.toggle();
    const auto joinedHands=handsCamera.resolve(1,nativeCamera,100);
    expect(joinedHands.applied&&joinedHands.controllers.hands[1].gripTracked&&joinedHands.controllers.predictedXrTime==9000,
           "grip, aim and eyes retain one predicted-time snapshot");
    hands.hands[1].grip.position.x+=0.1f;hands.predictedXrTime=10000;
    handsCamera.trackStereo({},rigEyes,true,110,hands);
    expect(near(joinedHands.controllers.hands[1].grip.position.x,0.2f),"a newer controller sample cannot mutate a source frame");
    expect(!handsCamera.resolve(1,nativeCamera,261).controllers.hands[1].gripTracked,"stale eyes cannot expose a live weapon pose");
    hands.hands[1].grip.orientation.w=0;
    handsCamera.trackStereo({},rigEyes,true,270,hands);
    auto validAimOnly=handsCamera.resolve(1,nativeCamera,270);
    expect(!validAimOnly.controllers.hands[1].gripTracked&&validAimOnly.controllers.hands[1].aimTracked,"grip and aim validity remain independent");
    const Pose held{{},{0.2f,-0.3f,-0.4f}},movingHead{{0,0.258819f,0,0.965926f},{0.1f,0.05f,0}};
    const Pose basis{{0,1,0,0},{}};
    const auto movedNativeHead=compose(nativeCamera,compose(compose(basis,movingHead),inverse(basis)));
    expect(same(nativeTrackedPose(movedNativeHead,movingHead,held).position,nativeTrackedPose(nativeCamera,{},held).position),
           "head motion does not steer a stationary controller");
    const auto nativeRoot=nativeAffinePose(playerRoot);
    HeadCamera rigCamera;rigCamera.configure(true,1,true);
    {
        HeadCamera occluded;occluded.configure(true,1,true);
        occluded.trackStereo({},rigEyes,true,300,{});occluded.toggle();
        occluded.publishPlayerHead(1,22,thirdPerson,playerRoot,headBone,300);
        const auto nativeSkin=occluded.resolve(1,thirdPerson,300);
        expect(nativeSkin.applied&&occluded.publishRigFrame(1,22,thirdPerson,nativeSkin),
            "a native skin publication remains valid when both controllers are occluded");
        const auto visibleWorld=occluded.resolve(1,thirdPerson,300);
        expect(visibleWorld.applied&&visibleWorld.rigSequence&&!visibleWorld.controllers.hands[1].gripTracked,
            "controller tracking loss retains the current head/skin transaction without inventing a tracked hand");
    }
    hands.hands[1].grip.orientation.w=1;hands.predictedXrTime=10000;
    rigCamera.trackStereo({},rigEyes,true,300,hands);rigCamera.toggle();
    rigCamera.publishPlayerHead(1,22,thirdPerson,playerRoot,headBone,300);
    auto rigFrame=rigCamera.resolve(1,thirdPerson,300);rigFrame.nativePose.position.y+=0.1f;
    rigFrame.weaponRig.sampled=true;rigFrame.weaponRig.sampleTime=300;
    rigFrame.weaponRig.resourceHandle=0x1234;rigFrame.weaponRig.component=55;
    rigFrame.weaponRig.muzzleSolved=true;rigFrame.weaponRig.muzzleInGrip.position={.1f,.2f,.3f};
    expect(rigCamera.publishRigFrame(1,22,thirdPerson,rigFrame),"native skin publication latches its exact tracking input");
    const auto skinPublication=rigCamera.publishedRigFrame(300);
    expect(skinPublication&&skinPublication->rigSequence&&skinPublication->controllers.predictedXrTime==10000
        &&same(skinPublication->nativePose.position,rigFrame.nativePose.position),
        "native effects can copy the completed skin transaction before its render-camera publication");
    expect(!rigCamera.publishedRigFrame(299)&&!rigCamera.publishedRigFrame(451),
        "future and stale skin snapshots cannot own native effects");
    hands.predictedXrTime=11000;hands.hands[1].grip.position.x+=0.1f;
    rigCamera.trackStereo({},rigEyes,true,310,hands);
    const auto retainedSkin=rigCamera.publishedRigFrame(310);
    expect(retainedSkin&&retainedSkin->trackingSequence==skinPublication->trackingSequence
        &&retainedSkin->controllers.predictedXrTime==10000,
        "a read-only skin snapshot cannot resample newer controller tracking");
    const auto renderedRig=rigCamera.resolve(1,thirdPerson,310);
    expect(renderedRig.applied&&renderedRig.rigSequence&&renderedRig.controllers.predictedXrTime==10000
        &&same(renderedRig.nativePose.position,rigFrame.nativePose.position),"eyes use the pose that drove the native rig despite newer tracking");
    expect(renderedRig.weaponRig.sampled&&renderedRig.weaponRig.sampleTime==300
        &&renderedRig.weaponRig.resourceHandle==0x1234&&renderedRig.weaponRig.component==55
        &&renderedRig.weaponRig.muzzleSolved&&same(renderedRig.weaponRig.muzzleInGrip.position,{.1f,.2f,.3f}),
        "weapon diagnostics retain their native skin publication despite a newer tracking sample");
    auto audioStatus=rigCamera.status();
    const auto audioPose=trackedListenerPose(renderedRig,audioStatus,310);
    expect(audioPose&&same(audioPose->position,rigFrame.nativePose.position)
        &&same(rotate(audioPose->orientation,{0,0,1}),rotate(rigFrame.nativePose.orientation,{0,0,1})),
        "listener uses the rendered center-head position and orientation despite newer tracking");
    expect(!trackedListenerPose(renderedRig,audioStatus,299)&&!trackedListenerPose(renderedRig,audioStatus,451),
        "future or stale camera publications cannot update the native audio listener");
    ++audioStatus.activation;
    expect(!trackedListenerPose(renderedRig,audioStatus,310),"recenter rejects the previous listener publication");
    audioStatus=rigCamera.status();audioStatus.suspended=true;
    expect(!trackedListenerPose(renderedRig,audioStatus,310),"tracking suspension releases listener ownership");
    audioStatus=rigCamera.status();audioStatus.active=false;
    expect(!trackedListenerPose(renderedRig,audioStatus,310),"theatre mode leaves native audio in control");
    audioStatus=rigCamera.status();audioStatus.pending=true;
    expect(!trackedListenerPose(renderedRig,audioStatus,310),"pending VR activation cannot move audio ahead of the image");
    audioStatus=rigCamera.status();audioStatus.enabled=false;
    expect(!trackedListenerPose(renderedRig,audioStatus,310),"disabled integration cannot retain listener ownership");
    auto badAudioFrame=renderedRig;badAudioFrame.nativePose.orientation.w=std::numeric_limits<float>::quiet_NaN();
    expect(!trackedListenerPose(badAudioFrame,rigCamera.status(),310),"invalid rotation cannot reach the audio engine");
    badAudioFrame=renderedRig;badAudioFrame.applied=false;
    expect(!trackedListenerPose(badAudioFrame,rigCamera.status(),310),"unmodified camera cannot receive a tracked listener");
    badAudioFrame=renderedRig;badAudioFrame.stereoTracked=false;
    expect(!trackedListenerPose(badAudioFrame,rigCamera.status(),310),"missing stereo tracking leaves the native listener unchanged");
    auto changedNativeCamera=thirdPerson;changedNativeCamera.position.x+=0.1f;
    rigCamera.publishPlayerHead(1,22,changedNativeCamera,playerRoot,headBone,311);
    expect(!rigCamera.publishedRigFrame(311),"a changed native source camera rejects the earlier skin snapshot");
    expect(!rigCamera.resolve(1,changedNativeCamera,311).applied&&rigCamera.status().reason==HeadCameraStop::rigFrameMismatch,
           "a rig from another native camera publication is withheld");
    expect(rigCamera.status().rigRejectFlags==2&&!mayReprojectAcceptedStereo(rigCamera.status()),
           "a different source-camera pose is not classified as a render delay");
    rigCamera.trackStereo({},rigEyes,true,600,hands);
    rigCamera.publishPlayerHead(1,22,thirdPerson,playerRoot,headBone,600);
    const auto beforeRigDelay=rigCamera.status().activation;
    expect(!rigCamera.resolve(1,thirdPerson,600).applied&&rigCamera.status().reason==HeadCameraStop::staleRig
           &&rigCamera.status().rigRejectFlags==8&&rigCamera.status().rigAgeMs==300,
           "an aged matching rig is rejected as a producer delay, never published as fresh pixels");
    expect(rigCamera.status().activation==beforeRigDelay&&mayReprojectAcceptedStereo(rigCamera.status()),
           "a delayed matching rig preserves only the already accepted image generation");
    rigCamera.publishPlayerHead(1,22,changedNativeCamera,playerRoot,headBone,600);
    expect(!rigCamera.resolve(1,changedNativeCamera,600).applied&&rigCamera.status().rigRejectFlags==10
           &&mayReprojectAcceptedStereo(rigCamera.status())&&!mayAcceptNewStereoPair(rigCamera.status()),
           "an aged source-camera mismatch keeps new pixels rejected while permitting only prior-pair retention");
    expect(nativeRoot&&same(rotate(nativeRoot->orientation,{1,0,0}),{0,0,-1}),"row affine decoding retains native handedness");
    const Pose chestPose{{.38268343f,0,0,.92387953f},{0,1.2f,0}};
    const Vec3 shoulderCenter{0,1.4f,.03f};
    const Pose uprightHead{{0,.70710678f,0,.70710678f},{2,1.7f,3}};
    const auto torsoPlacement=upperBodyPlacement(chestPose,shoulderCenter,uprightHead);
    expect(torsoPlacement&&same(compose(*torsoPlacement,Pose{{},shoulderCenter}).position,{1.96f,1.47f,3}),
           "shoulders stay behind and below the eyes after native yaw");
    if(torsoPlacement){
        const auto placedChest=compose(*torsoPlacement,chestPose);
        expect(same(rotate(placedChest.orientation,{0,1,0}),{0,1,0}),"native crouch pitch does not tilt the VR shoulder frame");
        const Vec3 a{-.2f,1.4f,.03f},b{.2f,1.4f,.03f};
        const auto movedA=compose(*torsoPlacement,Pose{{},a}).position,movedB=compose(*torsoPlacement,Pose{{},b}).position;
        expect(near(dot(movedA-movedB,movedA-movedB),.16f),"shared upper-body placement preserves shoulder width");
        const auto skinBefore=a*.35f+b*.65f;
        expect(same(movedA*.35f+movedB*.65f,compose(*torsoPlacement,Pose{{},skinBefore}).position),
               "sleeve vertices shared by chest and clavicle keep one rigid placement");
    }
    auto invalidChest=chestPose;invalidChest.orientation.w=std::numeric_limits<float>::quiet_NaN();
    expect(!upperBodyPlacement(invalidChest,shoulderCenter,uprightHead),"invalid torso pose cannot reach skin publication");
    const ArmPose arm{{{},{}},{{},{0.2f,-0.15f,0}},{{},{0.4f,0,0}}};
    const ArmBasis armBasis{arm.elbow.position-arm.shoulder.position,arm.wrist.position-arm.elbow.position};
    for(int i=0;i<100;++i){
        const float angle=static_cast<float>(i)*0.06283185f;
        const Pose target{{0,0,std::sin(angle/2),std::cos(angle/2)},{0.4f*std::cos(angle),0.4f*std::sin(angle),0.1f}};
        const auto solved=solveArm(arm,target,{0,-1,-1},&armBasis);
        expect(solved&&same(solved->pose.wrist.position,target.position),"reachable wrist follows controller translation");
        if(solved){const auto upper=solved->pose.elbow.position-solved->pose.shoulder.position,lower=solved->pose.wrist.position-solved->pose.elbow.position;
            expect(near(dot(upper,upper),0.0625f)&&near(dot(lower,lower),0.0625f),"arm IK preserves both authored segment lengths");
            expect(same(rotate(compose(Pose{solved->pose.elbow.orientation,{}},inverse(Pose{arm.elbow.orientation,{}})).orientation,
                               arm.wrist.position-arm.elbow.position),lower),"forearm twist retains the solved bone axis");}
    }
    const ArmPose alignedArm{{{},{}},{{},{0,-.3f,0}},{{},{0,-.3f,-.25f}}};
    {
        const ArmPose spine{{{},{0,1.003f,0}},{{},{0,1.154f,0}},{{},{0,1.463f,0}}};
        const Pose proneHead{{.70710678f,0,0,.70710678f},{0,.3f,.7f}};
        const Pose viewer{{0,.70710678f,0,.70710678f},{2,.4f,3}};
        const auto eyeFit=fullBodyPlacement(spine.elbow,proneHead,viewer);
        expect(eyeFit&&same(compose(*eyeFit,compose(proneHead,Pose{{},nativeEyeFromHead})).position,viewer.position),
            "body fit anchors the owned eye midpoint with authored prone pitch and viewer yaw");
        const Pose standingHead{{},{0,1.534f,-.015f}};
        const Quat sideways{0,.5f,0,.866025404f};
        const auto standingHeading=anatomicalTorsoHeading({},sideways,false);
        const auto proneHeading=anatomicalTorsoHeading({},sideways,true);
        const auto pronePlacement=proneHeading?fullBodyPlacement(spine.elbow,proneHead,
            Pose{*proneHeading,compose(proneHead,Pose{{},nativeEyeFromHead}).position}):std::nullopt;
        expect(standingHeading&&near(rotate(*standingHeading,{0,0,1}).x,.173648178f)
            &&pronePlacement&&same(compose(*pronePlacement,Pose{{},{.12f,.1f,-.8f}}).position,{.12f,.1f,-.8f}),
            "a 60 degree neck look turns an upright torso only 10 degrees and retains the prone foot footprint");
        expect(!anatomicalTorsoHeading(Quat{0,0,0,0},{},false),
            "untracked locomotion heading cannot authorize a body turn");
        {
            BodyHeadingState follow;
            const RigContinuityKey key{1,2,3,4,5};
            const auto turn=[](float degrees){const float half=degrees*.00872664626f;
                return Quat{0,std::sin(half),0,std::cos(half)};};
            Vec3 previous{0,0,1};uint64_t tick=1;
            for(int direction:{1,-1})for(int step=0;step<=360;++step){
                const auto heading=follow.update({},turn(static_cast<float>(step*direction)),false,++tick,key);
                expect(heading.has_value(),"room-scale body heading remains valid through a full physical turn");
                const auto forward=rotate(*heading,{0,0,1});
                if(step)expect(dot(previous,forward)>.999f,"body follow cannot jump at physical +/-180 degrees");
                previous=forward;
            }
            const auto before=follow.update({},turn(20),false,++tick,key);
            const auto after=follow.update(turn(45),turn(65),false,++tick,key);
            expect(before&&after&&same(rotate(*after,{0,0,1}),rotate(turn(45),rotate(*before,{0,0,1}))),
                "snap turn transports body and head together without spending neck yaw twice");
            expect(!follow.update(Quat{0,0,0,0},{},false,++tick,key),"invalid tracking resets body heading history");
            BodyHeadingState handsFollow;
            BodyHeadingState proneFollow;
            for(int degrees=-360;degrees<=360;degrees+=15){
                const Pose proneChest{compose(Pose{turn(37),{}},Pose{{.70710678f,0,0,.70710678f},{}}).orientation,{}};
                const auto turned=proneFollow.update(turn(static_cast<float>(degrees)),turn(static_cast<float>(degrees+70)),true,++tick,key,turn(-45));
                expect(turned&&same(rotate(*turned,{0,0,1}),rotate(turn(static_cast<float>(degrees)),{0,0,1})),
                    "prone native smooth or snap turning follows locomotion heading, independently of head and hand look");
                if(turned){
                    const auto placement=fullBodyPlacement(proneChest,proneHead,Pose{*turned,viewer.position});
                    expect(placement&&same(compose(*placement,compose(proneHead,Pose{{},nativeEyeFromHead})).position,viewer.position),
                        "prone stick turn pivots at the viewer without moving the anatomical eye");
                }
            }
            for(int step=0;step<=360;++step){
                const auto heading=handsFollow.update({},turn(static_cast<float>(step)),false,++tick,key,
                    turn(static_cast<float>(step)));
                expect(heading&&dot(rotate(*heading,{0,0,1}),rotate(turn(static_cast<float>(step)),{0,0,1}))>.99999f,
                    "whole-body turn keeps shoulders aligned with tracked hands without directional hysteresis");
            }
            handsFollow.reset();
            const auto independentLook=handsFollow.update({},turn(40),false,++tick,key,Quat{});
            expect(independentLook&&same(rotate(*independentLook,{0,0,1}),{0,0,1}),
                "head look does not drag shoulders away from stationary hands");
            const auto freeHand=trackedTorsoHeading(Pose{},{0,-.25f,.45f},turn(180));
            const auto supportHand=trackedTorsoHeading(Pose{},{.2f,-.25f,.45f},turn(180));
            expect(freeHand&&supportHand&&same(rotate(*freeHand,{0,0,1}),{0,0,1})
                &&same(rotate(*freeHand,{0,0,1}),rotate(*supportHand,{0,0,1})),
                "reaching onto the support socket cannot steer the torso or move the primary gun away");
            expect(!trackedTorsoHeading(Pose{},{0,-.25f,-.45f},turn(180))
                &&!trackedTorsoHeading(Pose{},{0,-.25f,.45f},turn(90)),
                "behind-back hands and sideways aim do not claim forward body facing");
            const auto seated=physicalFloorHeightOffset(0,-1.1f,1,1.6f);
            const auto standing=physicalFloorHeightOffset(.5f,-1.1f,1,1.6f);
            const auto rebased=physicalFloorHeightOffset(0,-1.6f,1,1.6f);
            expect(seated&&near(*seated,-.5f)&&standing&&near(*standing,0)&&rebased&&near(*rebased,0),
                "runtime floor keeps seated-to-standing height independent of initial/recentered LOCAL origin");
            expect(!physicalFloorHeightOffset(0,0,1,1.6f)&&!physicalFloorHeightOffset(1.6f,0,0,1.6f),
                "unknown floor height or invalid scale cannot authorize height correction");
            const ArmPose shortLeg{Pose{{},{0,1,0}},Pose{{},{0,.6f,.1f}},Pose{{},{0,.2f,0}}};
            const auto fittedLeg=scaledLegPose(shortLeg,1.25f);
            const auto magnitude=[](Vec3 v){return std::sqrt(dot(v,v));};
            expect(fittedLeg&&same(fittedLeg->shoulder.position,shortLeg.shoulder.position)
                &&same(fittedLeg->wrist.position,{0,0,0})
                &&near(magnitude(fittedLeg->elbow.position-fittedLeg->shoulder.position),
                    1.25f*magnitude(shortLeg.elbow.position-shortLeg.shoulder.position)),
                "floor leg fit preserves hip and proportions while reaching the floor");
            expect(!scaledLegPose(shortLeg,.9f)&&!scaledLegPose(shortLeg,1.6f),
                "floor leg fit rejects shrinking and excessive extension");
            const Pose floorContact{{},{0,.1f,0}};
            const auto neutral=solveGroundedLeg(shortLeg,floorContact,1.1f);
            auto raisedLeg=shortLeg;
            raisedLeg.shoulder.position.y+=.6f;raisedLeg.elbow.position.y+=.6f;raisedLeg.wrist.position.y+=.6f;
            const auto raised=solveGroundedLeg(raisedLeg,floorContact,1.1f);
            const auto returned=solveGroundedLeg(shortLeg,floorContact,1.1f);
            expect(neutral&&raised&&returned&&same(raised->leg.pose.wrist.position,floorContact.position)
                &&same(raised->leg.pose.shoulder.position,raisedLeg.shoulder.position)
                &&raised->scale>1.5f&&near(returned->scale,neutral->scale)
                &&same(returned->leg.pose.elbow.position,neutral->leg.pose.elbow.position),
                "elevated seated/standing legs reach native floor and return without retaining transient size");
            auto crouchedLeg=shortLeg;
            crouchedLeg.shoulder.position.y-=.3f;crouchedLeg.elbow.position.y-=.3f;crouchedLeg.wrist.position.y-=.3f;
            const auto crouched=solveGroundedLeg(crouchedLeg,floorContact,1.1f);
            expect(crouched&&near(crouched->scale,1.1f)
                &&near(magnitude(crouched->leg.pose.elbow.position-crouched->leg.pose.shoulder.position),
                        1.1f*magnitude(shortLeg.elbow.position-shortLeg.shoulder.position)),
                "crouching retains calibrated limb length and bends the knee");
            expect(!solveGroundedLeg(raisedLeg,Pose{{},{0,-1,0}},1.1f)
                &&!solveGroundedLeg(shortLeg,floorContact,.9f),
                "grounded leg rejects unattainable floor and invalid calibration");
        }
        const auto chestBefore=nativeTorsoHeading(Pose{{.70090926f,0,0,.71325045f},{}});
        const auto chestAfter=nativeTorsoHeading(Pose{{.71325045f,0,0,.70090926f},{}});
        expect(chestBefore&&chestAfter&&same(rotate(*chestBefore,{0,0,1}),rotate(*chestAfter,{0,0,1})),
            "a prone chest crossing vertical cannot reverse the body or camera heading");
        const Pose turnedViewer{{0,1,0,0},compose(standingHead,Pose{{},nativeEyeFromHead}).position};
        const auto placement=fullBodyPlacement(spine.elbow,standingHead,turnedViewer);
        std::optional<ArmSolution> placedSpine;
        Pose fittedEye{};
        if(placement){
            const ArmPose placed{compose(*placement,spine.shoulder),compose(*placement,spine.elbow),compose(*placement,spine.wrist)};
            const auto residual=fullBodyPlacement(placed.elbow,compose(*placement,standingHead),turnedViewer);
            if(residual)placedSpine=solveSpine(placed,compose(*residual,placed.wrist));
            if(placedSpine)fittedEye=compose(compose(placedSpine->pose.wrist,inverse(spine.wrist)),
                                            compose(standingHead,Pose{{},nativeEyeFromHead}));
        }
        expect(placedSpine&&dot(fittedEye.position-turnedViewer.position,fittedEye.position-turnedViewer.position)<.002f*.002f,
            "a complete eye-centered body turn leaves only the native 1 mm spine reach margin instead of an eye orbit");
        const auto solved=solveSpine(spine,Pose{{},{.1f,1.44f,.08f}});
        expect(solved&&same(solved->pose.shoulder.position,spine.shoulder.position)
            &&near(dot(solved->pose.elbow.position-solved->pose.shoulder.position,solved->pose.elbow.position-solved->pose.shoulder.position),.022801f)
            &&near(dot(solved->pose.wrist.position-solved->pose.elbow.position,solved->pose.wrist.position-solved->pose.elbow.position),.095481f),
            "body IK retains measured native lumbar/chest/neck lengths and anchor during a tracked lean");
        const auto turned=solveSpine(spine,Pose{{0,.70710678f,0,.70710678f},spine.wrist.position});
        expect(turned&&rotate(turned->pose.elbow.orientation,{0,0,1}).x>.99f,
            "spine orientation retains requested torso yaw around a vertical native chain");
        const auto surface=headClearanceSurface({.03f,0,0},{},.14f);
        const auto safe=surface?outsideArmSurface({.03f,0,0},*surface):std::nullopt;
        expect(safe&&near(safe->x,.14f)&&!headClearanceSurface({.3f,0,0},{},.14f),"camera envelope stops a penetrating limb while retaining an unobstructed hand");
        const Pose coverClavicle{{},{-.081f,-.157f,-.172f}};
        const ArmPose coverArm{Pose{{},{-.116f,-.043f,-.045f}},Pose{{},{-.095f,-.198f,.192f}},Pose{{},{-.068f,-.311f,.419f}}};
        const auto shoulderPlane=headClearanceSurface(coverArm.shoulder.position,{},.18f);
        const auto cleared=shoulderPlane?reachingClavicle(coverClavicle,coverArm,coverArm.wrist,&*shoulderPlane):std::nullopt;
        bool shoulderClear=false,handRetained=false;
        if(cleared){
            const auto delta=compose(*cleared,inverse(coverClavicle));
            const ArmPose shifted{compose(delta,coverArm.shoulder),compose(delta,coverArm.elbow),compose(delta,coverArm.wrist)};
            const auto clearedArm=solveArm(shifted,coverArm.wrist,coverArm.elbow.position-coverArm.shoulder.position);
            shoulderClear=dot(shifted.shoulder.position,shifted.shoulder.position)>=.18f*.18f-1e-5f;
            if(clearedArm){
                const auto error=clearedArm->pose.wrist.position-coverArm.wrist.position;
                handRetained=dot(error,error)<=.002f*.002f;
            }
        }
        expect(shoulderClear&&handRetained,"captured cover reach clears the shoulder from the eyes without losing its hand target");
    }
    // A prone wrist can remain above the floor while the unconstrained elbow
    // bends through it. Exercise contact on rotated slopes, not just flat Y=0.
    for(int i=-4;i<=4;++i){
        const float angle=static_cast<float>(i)*.1f;
        const Pose slope{{0,0,std::sin(angle/2),std::cos(angle/2)},{12,3,-7}};
        const auto transform=[&](Vec3 point){return compose(slope,Pose{{},point}).position;};
        const ArmSurface surface{transform({}),rotate(slope.orientation,{0,1,0}),.06f};
        ArmPose grounded{compose(slope,Pose{{},{0,.10f,0}}),
            compose(slope,Pose{{},{.20f,-.05f,0}}),compose(slope,Pose{{},{.40f,.10f,0}})};
        const Pose target{slope.orientation,transform({.40f,.10f,0})};
        const auto free=solveArm(grounded,target,rotate(slope.orientation,{0,-1,0}));
        const auto constrained=solveArm(grounded,target,rotate(slope.orientation,{0,-1,0}),nullptr,&surface);
        const auto groundedSpine=solveSpine(grounded,target,&surface);
        expect(free&&dot(free->pose.elbow.position-surface.point,surface.normal)<0,
               "prone regression fixture actually places the free elbow below ground");
        expect(constrained&&dot(constrained->pose.elbow.position-surface.point,surface.normal)>=.0599f,
               "native contact keeps the elbow above a sloped surface");
        expect(groundedSpine&&same(groundedSpine->pose.wrist.position,target.position)
            &&dot(groundedSpine->pose.elbow.position-surface.point,surface.normal)>=.0599f,
            "spine contact bends its chest above terrain without lifting the viewer's neck endpoint");
        if(constrained){
            const auto u=constrained->pose.elbow.position-grounded.shoulder.position;
            const auto lowerSegment=constrained->pose.wrist.position-constrained->pose.elbow.position;
            expect(near(dot(u,u),.0625f)&&near(dot(lowerSegment,lowerSegment),.0625f)&&same(constrained->pose.wrist.position,target.position),
                   "contact preserves arm lengths and an unobstructed tracked wrist");
        }
        const auto hand=outsideArmSurface(transform({.4f,-.2f,0}),surface);
        expect(hand&&near(dot(*hand-surface.point,surface.normal),.06f),"penetrating wrist stops at native surface clearance");
        const auto clearHand=outsideArmSurface(target.position,surface);
        expect(clearHand&&same(*clearHand,target.position),"contact does not move an unobstructed wrist");
    }
    expect(!outsideArmSurface({},ArmSurface{{},{},.05f}),"missing collision normal cannot invent a floor");
    const ArmBasis alignedBasis{{0,-.3f,0},{0,0,-.25f},{0,0,-1},{1,0,0}};
    const Quat wristRoll{0,0,.70710678f,.70710678f};
    const auto twisted=solveArm(alignedArm,Pose{wristRoll,alignedArm.wrist.position},{0,-1,0},&alignedBasis);
    expect(twisted&&same(rotate(twisted->pose.elbow.orientation,{1,0,0}),{1,0,0}),
           "tracked wrist roll leaves the native elbow hinge untwisted");
    for(int i=0;i<=720;++i){
        const float angle=static_cast<float>(i)*0.034906585f;
        const Quat roll{0,0,std::sin(angle/2),std::cos(angle/2)};
        auto animated=alignedArm;
        animated.elbow.orientation={0,std::sin(angle*.73f),0,std::cos(angle*.73f)};
        animated.wrist.orientation={std::sin(angle*.21f),0,0,std::cos(angle*.21f)};
        const auto solved=solveArm(animated,Pose{roll,alignedArm.wrist.position},{0,-1,0},&alignedBasis);
        expect(solved&&same(rotate(solved->pose.elbow.orientation,{1,0,0}),{1,0,0})
            &&same(rotate(solved->pose.wrist.orientation,{1,0,0}),rotate(roll,{1,0,0})),
               "repeated wrist turns cannot accumulate twist at the elbow or lose the tracked hand");
    }
    const auto axisRotation=[](unsigned axis,float degrees){
        const float angle=degrees*.00872664626f,s=std::sin(angle);
        return Quat{axis==0?s:0,axis==1?s:0,axis==2?s:0,std::cos(angle)};
    };
    for(const bool right:{false,true}){
        const auto corrections=armCorrectiveRotations(axisRotation(2,20),axisRotation(0,80),
            axisRotation(1,-60),axisRotation(0,100),right);
        expect(same(rotate(corrections[1],{0,1,0}),rotate(axisRotation(0,right?-60.f:-56.f),{0,1,0})),
               "shoulder correctives preserve the native left/right twist weights");
        expect(same(rotate(corrections[3],{1,0,0}),rotate(axisRotation(1,33),{1,0,0})),
               "elbow corrective counter-rotates flexion instead of folding the sleeve");
        expect(same(rotate(corrections[4],{0,1,0}),rotate(axisRotation(0,35),{0,1,0}))
            &&same(rotate(corrections[5],{0,1,0}),rotate(axisRotation(0,75),{0,1,0})),
               "forearm helper joints distribute 100 degrees of wrist twist at 35 and 75 percent");
    }
    const Pose watchElbow{{},{-.3f,-.3f,-.4f}},watchWrist{{},{0,-.3f,-.4f}};
    const auto watch=forearmPanel(watchElbow,watchWrist,{0,1,0});
    expect(watch&&watch->position.x>-.03f&&watch->position.x<0
        &&watch->position.y>watchWrist.position.y&&watch->position.y<watchWrist.position.y+.04f,
        "wrist HUD sits just behind the hand and above the skin, not midway up the sleeve");
    expect(watch&&same(rotate(watch->orientation,{1,0,0}),{1,0,0})
        &&same(rotate(watch->orientation,{0,0,1}),{0,1,0})
        &&same(rotate(watch->orientation,{0,1,0}),{0,0,-1}),
        "left watch reads left to right along the forearm with its top away from the wearer");
    const Pose watchMove{{0,.70710678f,0,.70710678f},{3,1,2}};
    const auto movedWatch=forearmPanel(compose(watchMove,watchElbow),compose(watchMove,watchWrist),rotate(watchMove.orientation,{0,1,0}));
    expect(watch&&movedWatch&&same(movedWatch->position,compose(watchMove,*watch).position),
           "forearm HUD remains attached through character translation and turning");
    expect(!forearmPanel(watchElbow,watchWrist,{1,0,0}),"undefined forearm normal cannot produce a face HUD");
    const auto shortWatch=forearmPanel(Pose{{},{-.15f,-.3f,-.4f}},watchWrist,{0,1,0});
    expect(watch&&shortWatch&&same(watch->position,shortWatch->position),
        "arm length does not move the HUD away from the anatomical wrist");
    SupportContact support;
    const Vec3 heldSeparation{-.08f,0,-.24f},withdrawnSeparation{-.34f,-.06f,.04f},primaryForward{0,0,-1};
    expect(withinSupportCone(heldSeparation,primaryForward),"a hand at the forward rifle grip can guide the barrel");
    expect(!withinSupportCone(withdrawnSeparation,primaryForward),"withdrawing alongside the firing hand releases even when the gun follows it");
    const Quat supportTurn{0,.70710678f,0,.70710678f};
    expect(withinSupportCone(rotate(supportTurn,heldSeparation),rotate(supportTurn,primaryForward))
        &&!withinSupportCone(rotate(supportTurn,withdrawnSeparation),rotate(supportTurn,primaryForward)),
        "support release follows controller aim through body turns");
    expect(!withinSupportCone({},primaryForward)&&!withinSupportCone(heldSeparation,{}),"invalid support directions cannot acquire a hand");
    expect(closeSupportContact({-.045f,-.025f,.01f})&&!withinSupportCone({-.045f,-.025f,.01f},primaryForward),
        "a native pistol cup is a valid close support contact even alongside and below the firing hand");
    expect(!closeSupportContact(heldSeparation)&&!closeSupportContact({0,0,-.25f}),
        "long-gun foregrips retain their existing directional support solve");
    expect(closeSupportContact(rotate(supportTurn,{-.045f,-.025f,.01f}))
        &&!closeSupportContact({})&&!closeSupportContact({std::numeric_limits<float>::quiet_NaN(),0,0}),
        "close-cup classification follows the native weapon frame and rejects invalid contacts");
    expect(!support.update(true,true,.07f,100)&&support.update(true,true,.07f,250)
        &&!support.update(withinSupportCone(withdrawnSeparation,primaryForward),true,.07f,300),
        "a close guided contact cannot retain a sideways withdrawn hand");
    support.reset();
    SupportPose supportPose;
    const Pose acquiredSupport{{},{.25f,0,.12f}},animatedReload{{},{.15f,.2f,.05f}},stowingSupport{{},{-.4f,2.f,-.3f}};
    expect(!supportPose.update(acquiredSupport,false,false),"a free hand has no acquired support pose");
    auto presentedSupport=supportPose.update(acquiredSupport,true,false);
    expect(presentedSupport&&same(presentedSupport->position,acquiredSupport.position),"support acquires the current weapon contact");
    presentedSupport=supportPose.update(stowingSupport,true,false);
    expect(presentedSupport&&same(presentedSupport->position,acquiredSupport.position),"ordinary animation changes cannot move acquired support into the sky");
    presentedSupport=supportPose.update(animatedReload,true,true);
    expect(presentedSupport&&same(presentedSupport->position,animatedReload.position),"an attached native reload can animate contact");
    presentedSupport=supportPose.update(stowingSupport,false,false);
    expect(presentedSupport&&same(presentedSupport->position,animatedReload.position),"release retains the last contact rather than chasing stow animation");
    presentedSupport=supportPose.update(acquiredSupport,true,false);
    expect(presentedSupport&&same(presentedSupport->position,acquiredSupport.position),"new contact reacquires its own weapon pose");
    supportPose.reset();
    expect(!supportPose.update(stowingSupport,false,false),"menu or tracking reset clears support presentation");
    const Pose weaponAtGrab{{},{0,0,.05f}},weaponMoved{{0,.258819f,0,.9659258f},{.02f,0,.1f}};
    supportPose.update(acquiredSupport,true,false,weaponAtGrab);
    const auto movedContact=supportPose.update(stowingSupport,true,false,weaponMoved);
    const auto expectedContact=compose(weaponMoved,compose(inverse(weaponAtGrab),acquiredSupport));
    expect(movedContact&&same(movedContact->position,expectedContact.position),"support stays on the acquired weapon point when its attachment moves relative to the wrist");
    const auto missingAttachment=supportPose.update(stowingSupport,true,false,std::nullopt);
    expect(missingAttachment&&same(missingAttachment->position,expectedContact.position),"missing weapon attachment cannot snap acquired support into the wrist frame");
    expect(!support.update(true,true,.25f,1000)&&!support.update(true,true,.25f,1200),"a free palm away from the weapon support grip stays one-handed");
    expect(!support.update(true,true,.09f,1300)&&!support.update(true,true,.09f,1400)
        &&support.update(true,true,.09f,1450),"support engages only after dwelling at the actual weapon grip");
    expect(support.update(true,true,.19f,1500),"a small movement at the acquired grip retains support");
    expect(support.update(true,true,.25f,1550),"ordinary support movement beyond the old 20 cm boundary remains attached");
    expect(!support.update(true,true,.31f,1600)&&!support.update(true,true,.19f,1800),"pulling away releases support without edge chatter");
    support.update(true,true,.05f,2000);
    expect(support.update(true,true,.05f,2150)&&!support.update(true,false,.05f,2160),"lost hand tracking releases support");
    support.update(true,true,.05f,2300);
    expect(support.update(true,true,.05f,2450)&&!support.update(false,true,.05f,2460),"lowering or inspecting releases support");
    expect(!support.update(true,true,.15f,2600),"selection does not preserve a sticky two-hand latch");
    support.update(true,true,.05f,2700);
    expect(!support.update(true,true,.05f,2800)&&!support.update(true,true,.05f,2750),"a regressed contact clock resets the dwell");
    {
        WeaponTrackingContinuity continuity;
        auto sample=continuity.update(true,true,true,false,false,1,1);
        expect(sample.allowAttack&&!sample.retainReady,"tracked weapon acquires normally");
        sample=continuity.update(true,true,false,true,true,1,1);
        expect(sample.retainReady&&!sample.allowAttack,"held weapon survives tracking loss without firing");
        sample=continuity.update(true,true,false,true,false,1,1);
        expect(sample.retainReady&&!sample.allowAttack,"release during invalid tracking does not arm attack");
        sample=continuity.update(true,true,true,true,true,1,1);
        expect(!sample.allowAttack,"held trigger cannot fire on tracking recovery");
        expect(continuity.update(true,true,true,true,false,1,1).allowAttack,
               "tracked trigger release rearms attack");
        expect(continuity.update(true,true,true,true,true,1,1).allowAttack,
               "deliberate recovered trigger press can fire");
        expect(!continuity.update(true,false,false,true,false,1,1).retainReady,
               "physical grip release lowers weapon during loss");
        expect(!continuity.update(true,true,false,false,false,1,1).retainReady,
               "untracked hand cannot acquire a lowered weapon");
        expect(!continuity.update(true,true,false,true,false,2,1).retainReady,
               "new camera owner cannot inherit weapon readiness");
        expect(!continuity.update(true,true,false,true,false,2,2).retainReady,
               "reference reset cannot inherit weapon readiness");
        sample=continuity.update(false,true,false,true,true,2,2);
        expect(!sample.retainReady&&sample.allowAttack,"other input owners retain their own attack policy");
        expect(!continuity.update(true,true,false,true,true,2,2).retainReady,
               "returning from another owner cannot acquire without tracking");
    }
    RigInput travelInput;
    GamepadSample triggerOnly{};triggerOnly.rightTrigger=255;
    const auto cqc=travelInput.update(triggerOnly,false,false,TravelMode::onFoot);
    expect(cqc.gamepad.rightTrigger==255&&cqc.gamepad.leftTrigger==0&&!cqc.weaponReady,
           "lowered weapon retains the native attack/CQC trigger without aiming");
    const auto readyGun=travelInput.update(triggerOnly,true,true,TravelMode::onFoot);
    expect(readyGun.gamepad.leftTrigger==255&&readyGun.gamepad.rightTrigger==255&&readyGun.weaponReady,
           "right grip readies the firearm; left grip no longer forces support");
    expect(travelInput.update(triggerOnly,true,true,TravelMode::vehicle).gamepad==GamepadSample{},
           "entering a vehicle cannot carry firing input into accelerator or mounted attack");
    travelInput.update({},false,false,TravelMode::vehicle);
    const auto accelerator=travelInput.update(triggerOnly,false,false,TravelMode::vehicle);
    expect(accelerator.gamepad.rightTrigger==255&&accelerator.gamepad.leftTrigger==0&&!accelerator.weaponReady,
           "vehicle accelerator works independently of the weapon-ready grip and brake");
    GamepadSample brakeOnly{};brakeOnly.leftTrigger=255;
    const auto brake=travelInput.update(brakeOnly,false,false,TravelMode::vehicle);
    expect(brake.gamepad.leftTrigger==255&&brake.gamepad.rightTrigger==0&&brake.gamepad.buttons==0,
           "vehicle braking/reversing is not consumed by the equipment modifier");
    expect(travelInput.update({},true,false,TravelMode::vehicle).gamepad.buttons==0,
           "gripping the wheel cannot fire a vehicle weapon");
    expect(travelInput.update(GamepadSample{0x4000},false,false,TravelMode::vehicle).gamepad.buttons==0x0100,
           "X reaches the native vehicle weapon or radio independently of the wheel grip");
    RigInput vehicleEquipment;
    vehicleEquipment.update({},false,false,TravelMode::vehicle);
    const auto vehicleAttack=vehicleEquipment.update(GamepadSample{0x4000},false,true,TravelMode::vehicle);
    expect((vehicleAttack.gamepad.buttons&0x0100)!=0,
           "vehicle weapon or radio remains reachable without reopening the equipment picker");
    {
        RigInput driver;driver.update({},false,false,TravelMode::vehicle,100);
        wheelMailbox().publish({.6f,true,false,100,1,3});
        auto nearWheel=driver.update({},true,true,TravelMode::vehicle,110);
        expect(nearWheel.rightWheelGrip&&!nearWheel.vehicleWeaponIntent&&!nearWheel.weaponReady
            &&driver.equipmentPhase()==0&&nearWheel.gamepad.leftX==static_cast<int16_t>(.6f*32767),
            "right squeeze near an empty wheel joins steering without equipment or weapon actions");
        wheelMailbox().publish({0,false,false,120,0,0});
        nearWheel=driver.update({},false,true,TravelMode::vehicle,125);
        expect(nearWheel.rightWheelGrip&&!nearWheel.vehicleWeaponIntent,
            "pulling away cannot reinterpret the held wheel squeeze as a weapon request");
        driver.update({},false,false,TravelMode::vehicle,130);
        const auto weapon=driver.update({},false,true,TravelMode::vehicle,140);
        expect(!weapon.rightWheelGrip&&weapon.vehicleWeaponIntent&&!weapon.weaponReady&&driver.equipmentPhase()==0,
            "fresh squeeze away from wheel requests only the native firearm adapter, not driving triggers or equipment");
        wheelMailbox().publish({0,false,false,150,0,3});
        const auto heldWeapon=driver.update({},false,true,TravelMode::vehicle,155);
        expect(heldWeapon.vehicleWeaponIntent&&!heldWeapon.rightWheelGrip,
            "a held weapon request cannot snap the right hand onto the wheel");
        driver.update({},false,false,TravelMode::vehicle,160);
        const auto readyBinding=driver.update({},false,true,TravelMode::vehicle,170,0,false,false,true);
        expect(readyBinding.vehicleWeaponIntent&&!readyBinding.rightWheelGrip,
            "remapped ready-only control stays a weapon request even beside the wheel");
        driver.suspend();
        expect(!driver.update({},false,true,TravelMode::vehicle,180).vehicleWeaponIntent,
            "focus loss consumes the held squeeze until the player releases it");
        wheelMailbox().publish({});
    }
    {
        RigInput picker;picker.update({},false,false,TravelMode::vehicle,100);
        GamepadSample controls{0x4000,255,255,20000,18000,0,0};
        const auto opened=picker.update(controls,true,true,TravelMode::vehicle,110,0,false,true,true,true);
        expect(picker.equipmentPhase()==1&&!opened.rightWheelGrip&&!opened.vehicleWeaponIntent
            &&!opened.gamepad.leftTrigger&&!opened.gamepad.rightTrigger
            &&!opened.gamepad.leftX&&!opened.gamepad.leftY&&!(opened.gamepad.buttons&0x300),
            "an explicit vehicle picker binding consumes pedal, wheel, firearm and vehicle attack inputs");
        const auto released=picker.update(controls,true,true,TravelMode::vehicle,120);
        expect(!released.rightWheelGrip&&!released.vehicleWeaponIntent
            &&!released.gamepad.leftTrigger&&!released.gamepad.rightTrigger&&!(released.gamepad.buttons&0x100),
            "releasing a picker chord cannot reinterpret held grip, pedals or X as vehicle actions");
        picker.update({},false,false,TravelMode::vehicle,130);
        expect(picker.update(triggerOnly,false,false,TravelMode::vehicle,140).gamepad.rightTrigger==255,
            "fresh pedal input resumes after the explicit vehicle picker closes");
    }
    expect(mountedViewOwnsRightStick(TravelMode::vehicle,true,false,false)
        &&mountedViewOwnsRightStick(TravelMode::horse,true,false,false)
        &&mountedViewOwnsRightStick(TravelMode::nativeAction,true,false,false)
        &&!mountedViewOwnsRightStick(TravelMode::onFoot,true,false,false)
        &&!mountedViewOwnsRightStick(TravelMode::vehicle,true,false,true)
        &&!mountedViewOwnsRightStick(TravelMode::vehicle,true,true,false),
           "mounted camera and turret axes stay native while menus and native input retain ownership");
    {
        std::array<uint32_t,5> status{};
        const auto set=[&](unsigned bit,bool value=true){
            if(value)status[bit/32]|=1u<<(bit%32);
            else status[bit/32]&=~(1u<<(bit%32));
        };
        set(4);set(145);set(7);
        expect(travelModeFromPlayerStatus(status)==TravelMode::onFoot,"ordinary standing action keeps tracked foot aiming");
        set(7,false);
        expect(travelModeFromPlayerStatus(status)==TravelMode::nativeAction,"observed mortar standing action keeps both native aiming axes");
        for(const auto bit:{134u,87u,88u,83u,58u,56u,59u}){
            set(bit);
            expect(travelModeFromPlayerStatus(status)==TravelMode::onFoot,"carry, CQC, treatment and optics do not acquire stationary aim");
            set(bit,false);
        }
        set(145,false);
        expect(travelModeFromPlayerStatus(status)==TravelMode::onFoot,"inactive character parts cannot acquire stationary aim");
        set(145);set(4,false);
        expect(travelModeFromPlayerStatus(status)==TravelMode::onFoot,"non-standing action cannot acquire stationary aim");
        set(4);set(26);
        expect(travelModeFromPlayerStatus(status)==TravelMode::vehicle,"vehicle ownership remains authoritative");
        set(26,false);set(25);
        expect(travelModeFromPlayerStatus(status)==TravelMode::horse,"horse ownership remains authoritative");
        set(37);
        expect(travelModeFromPlayerStatus(status)==TravelMode::unknown,"unavailable native player status rejects aim ownership");
        RigInput stationaryInput;
        stationaryInput.update({},false,false,TravelMode::nativeAction);
        GamepadSample aim{};aim.rightX=15000;aim.rightY=-17000;
        const auto mountedAim=stationaryInput.update(aim,false,true,TravelMode::nativeAction);
        expect(mountedAim.gamepad.rightX==15000&&mountedAim.gamepad.rightY==-17000&&mountedAim.gamepad.leftTrigger==255,
            "stationary VR ready grip retains azimuth, elevation and native aim trigger");
        expect(stationaryInput.update(aim,false,true,TravelMode::onFoot).gamepad.rightY==-17000,
            "stationary action retains the on-foot button owner through entry and exit");
        RigInput contextInput;
        contextInput.update({},false,false,TravelMode::onFoot);
        GamepadSample interact{};interact.buttons=0x8000;
        expect(contextInput.update(interact,false,false,TravelMode::onFoot).gamepad.buttons==0x8000
            &&contextInput.update(interact,false,false,TravelMode::nativeAction).gamepad.buttons==0x8000,
            "native standing action cannot cancel the Y hold that mounts it");
        expect(contextInput.update(interact,false,false,TravelMode::onFoot).gamepad.buttons==0x8000,
            "native exit completes through the same on-foot interaction binding");
    }
    expect(travelInput.update(brakeOnly,false,true,TravelMode::onFoot).gamepad==GamepadSample{},
           "exiting a vehicle releases brake and grip inputs before restoring foot controls");
    travelInput.update({},false,false,TravelMode::onFoot);
    expect(travelInput.update(triggerOnly,false,true,TravelMode::onFoot).weaponReady,
           "on-foot weapon controls resume after neutral input following exit");
    expect(travelInput.update(triggerOnly,false,true,TravelMode::unknown).gamepad==GamepadSample{},
           "unavailable native travel state cannot guess accelerator versus firearm input");
    travelInput.update({},false,false,TravelMode::vehicle);
    travelInput.suspend();
    expect(travelInput.update(triggerOnly,false,false,TravelMode::vehicle).gamepad==GamepadSample{},
           "regaining XR focus cannot resume a held accelerator");
    travelInput.update({},false,false,TravelMode::vehicle);
    expect(travelInput.update(triggerOnly,false,false,TravelMode::vehicle).gamepad.rightTrigger==255,
           "vehicle input resumes after neutral input following focus loss");
    const auto extended=solveArm(arm,Pose{{},{3,0,0}},{0,-1,0});
    expect(extended&&extended->reachClamped&&near(extended->pose.wrist.position.x,0.499f),"unreachable grip cannot stretch the native limb");
    expect(!solveArm(ArmPose{},Pose{},{}),"missing skeleton geometry cannot produce an arm");
    const Vec3 indexKnuckle{0,0.03f,-0.085f},littleKnuckle{0,-0.03f,-0.075f};
    const auto palm=anatomicalGrip({},indexKnuckle,littleKnuckle);
    expect(palm&&near(palm->position.z,-0.044f),"grip origin is inside the palm rather than at the wrist");
    if(palm){
        const Pose moved{{0,0.70710678f,0,0.70710678f},{3,2,1}};
        const auto movedPalm=anatomicalGrip(moved,compose(moved,Pose{{},indexKnuckle}).position,compose(moved,Pose{{},littleKnuckle}).position);
        const auto expectedPalm=compose(moved,*palm);
        expect(movedPalm&&same(movedPalm->position,expectedPalm.position)
            &&same(rotate(movedPalm->orientation,{0,0,-1}),rotate(expectedPalm.orientation,{0,0,-1})),
            "anatomical grip is invariant under native character or animation rotation");
        const Pose tracked{{0.258819f,0,0,0.9659258f},{0.2f,-0.2f,-0.4f}};
        const auto target=compose(tracked,inverse(*palm));
        expect(same(compose(target,*palm).position,tracked.position),"tracked grip places the palm at the controller, including the wrist offset");
    }
    expect(!anatomicalGrip({},indexKnuckle,indexKnuckle)&&!anatomicalGrip({},Vec3{0,0,2},Vec3{0,1,2}),
           "degenerate or non-hand landmark geometry cannot steer the rig");
    {
        const Pose aim{{},{.2f,-.15f,-.4f}};
        const auto tilted=weaponAimWithPitch(aim,-30);
        expect(tilted&&same(tilted->position,aim.position)
            &&same(rotate(tilted->orientation,{0,0,-1}),{0,-.5f,-.8660254f}),
            "firearm pitch changes pointing around its existing origin");
        for(const auto forward:std::array<Vec3,3>{{{0,0,-1},{0,1,0},{1,0,0}}}){
            const auto wrist=aimedWeaponGrip(aim,*tilted,forward);
            expect(wrist&&same(wrist->position,aim.position)
                &&same(rotate(wrist->orientation,forward),rotate(tilted->orientation,{0,0,-1})),
                "different authored barrel axes retain grip contact and the calibrated muzzle direction");
        }
        const auto neutral=weaponAimWithPitch(aim,0);
        expect(neutral&&same(rotate(neutral->orientation,{0,0,-1}),{0,0,-1}),
            "zero firearm pitch restores the runtime pointing direction");
        const auto vertical=weaponAimWithPitch(aim,-90);
        expect(vertical&&!withinSupportCone({0,-.25f,0},{0,0,-1})
            &&withinSupportCone({0,-.25f,0},rotate(vertical->orientation,{0,0,-1}))
            &&!withinSupportCone({0,.25f,0},rotate(vertical->orientation,{0,0,-1})),
            "support admission uses calibrated primary aim without following a withdrawn hand");
        expect(!weaponAimWithPitch(aim,91)&&!weaponAimWithPitch(aim,std::numeric_limits<float>::quiet_NaN()),
            "invalid firearm pitch cannot steer a native wrist");
    }
    RigEquipment equipment;
    uint64_t equipmentTime=1000;
    const auto equipmentStep=[&](GamepadSample sample,bool modifier,bool rendered=true,uint64_t elapsed=100){
        equipmentTime+=elapsed;
        return equipment.update(sample,modifier,false,equipmentTime,rendered?equipmentTime:0);
    };
    GamepadSample equipmentHeld{0x0040,255,128,17000,30000,0,0};
    auto mapped=equipmentStep(equipmentHeld,true);
    expect(mapped.buttons==0&&mapped.leftX==17000&&mapped.leftY==30000&&equipment.phase()==1,
        "trigger opens the chooser without selecting equipment or leaking a dive click and preserves movement");
    expect(equipmentStep(equipmentHeld,false).buttons==0,"closing an unselected chooser cannot equip or dive");
    for(const auto direction:std::array<GamepadSample,4>{{{0,0,0,-30000,22000,0,30000},{0,0,0,-30000,22000,0,-30000},
            {0,0,0,-30000,22000,30000,0},{0,0,0,-30000,22000,-30000,0}}}){
        equipment.reset();equipmentStep({},true);equipmentStep({},true);
        const uint16_t category=direction.rightY>0?1:direction.rightY<0?2:direction.rightX>0?8:4;
        mapped=equipmentStep(direction,true);
        expect(mapped.buttons==category&&mapped.rightX==0&&mapped.rightY==0&&mapped.leftX==-30000,
            "one category request matches its direction without browsing or interrupting movement");
        const auto openedAt=equipmentTime;
        equipmentTime+=100;
        mapped=equipment.update(direction,true,false,equipmentTime,openedAt);
        expect(mapped.rightX==0&&mapped.rightY==0&&equipment.phase()==2,
            "a picker draw from before the request cannot unlock navigation");
        for(unsigned frame=0;frame<8;++frame){
            mapped=equipmentStep(direction,true,false);
            expect(mapped.buttons==category&&mapped.rightX==0&&mapped.rightY==0,
                "equip/stow delays keep held navigation out of the gameplay camera");
        }
        equipmentStep({},true);equipmentStep({},true);
        expect(equipment.phase()==3,"fresh expanded UI and a centered stick unlock browsing");
        for(const auto browse:std::array<GamepadSample,8>{{{0,0,0,0,0,30000,0},{0,0,0,0,0,23170,23170},
                {0,0,0,0,0,0,30000},{0,0,0,0,0,-23170,23170},{0,0,0,0,0,-30000,0},
                {0,0,0,0,0,-23170,-23170},{0,0,0,0,0,0,-30000},{0,0,0,0,0,23170,-23170}}}){
            mapped=equipmentStep(browse,true);
            expect(mapped.rightX==0&&mapped.rightY==0,"an unsettled flick cannot skip a card");
            mapped=equipmentStep(browse,true,true,60);
            expect(mapped.buttons==category&&mapped.rightX==browse.rightX&&mapped.rightY==browse.rightY,
                "all eight visible card directions retain their axes and signs");
            auto wobble=browse;wobble.rightX=static_cast<int16_t>(-browse.rightX);wobble.rightY=static_cast<int16_t>(-browse.rightY);
            expect(equipmentStep(wobble,true)==mapped,"one continuous gesture cannot jump to another card");
            equipmentStep({},true,true,1);
            expect(equipmentStep(wobble,true,true,1)==mapped,"a one-frame center bounce cannot double-select");
            equipmentStep({},true);equipmentStep({},true);
        }
        mapped=equipmentStep(direction,true,false);
        expect(mapped.rightX==0&&mapped.rightY==0&&equipment.phase()==2,
            "missing UI closes navigation instead of turning the world");
        mapped=equipmentStep(direction,true);
        expect(mapped.rightX==0&&mapped.rightY==0,"UI recovery cannot replay the held direction");
        equipmentStep({},true);equipmentStep({},true);
    }
    equipmentHeld={0x1000};
    expect(equipmentStep(equipmentHeld,true).buttons==0x0084,"A uses the selected item without clicking its navigation stick");
    expect(equipmentStep(equipmentHeld,true).buttons==0x0004,"holding Use does not repeatedly toggle an item");
    expect(equipmentStep(equipmentHeld,false).buttons==0,"held Use cannot become crouch after closing");
    equipmentStep({},false);equipmentStep({},true);equipmentStep({},true);equipmentStep({0,0,0,0,0,-30000,0},true);
    equipmentStep({},true);equipmentStep({},true);
    expect(equipmentStep({0x0080},true).buttons==0x0084,"the existing right-stick item-use click remains available");
    equipmentStep({},true);
    expect(equipmentStep({0x2000},true).buttons==0,"B returns to category choice without quick-equipping anything");
    mapped=equipmentStep({0x2000,0,0,0,0,0,-30000},true);
    expect(mapped.buttons==2&&mapped.rightY==0,"a new downward flick chooses secondary after Back");
    expect(equipmentStep({0x2000,0,0,0,0,0,-30000},false).buttons==0,
        "back and navigation remain consumed on close");
    equipment.reset();equipmentHeld={0x8000};
    expect(equipmentStep(equipmentHeld,true).buttons==0,"reserved optics cannot open a scope or select equipment");
    expect(equipmentStep(equipmentHeld,false).buttons==0,"reserved optics cannot leak an action on release");
    equipmentStep({},false);
    expect(equipmentStep(equipmentHeld,false).buttons==0x8000,"ordinary Y resumes after release");
    equipment.reset();equipmentHeld={0,0,0,30000,0,30000,0};
    expect(equipmentStep(equipmentHeld,true).buttons==0,"opening during a turn cannot choose a category");
    expect(equipmentStep(equipmentHeld,true).rightX==0,"the old turning gesture remains consumed");
    equipmentStep({},true);equipmentStep({},true);
    expect(equipmentStep(equipmentHeld,true).buttons==8,"a fresh right flick chooses support after centering");
    RigInput wristInput;
    GamepadSample walkingSelection{0,255,255,15000,28000,0,0};
    auto selectionInput=wristInput.update(walkingSelection,true,true,TravelMode::onFoot);
    expect(selectionInput.gamepad.leftX==15000&&selectionInput.gamepad.leftY==28000&&selectionInput.gamepad.buttons==0
        &&selectionInput.gamepad.leftTrigger==0&&selectionInput.gamepad.rightTrigger==0&&!selectionInput.weaponReady,
        "selection frees the wrist, lowers the weapon and consumes fire while movement continues");
    walkingSelection.leftTrigger=100;
    expect(wristInput.update(walkingSelection,false,true,TravelMode::onFoot).gamepad.buttons==0,"trigger hysteresis keeps the native selector held through a partial release");
    walkingSelection.leftTrigger=0;
    selectionInput=wristInput.update(walkingSelection,false,true,TravelMode::onFoot);
    expect(selectionInput.gamepad.buttons==0&&selectionInput.gamepad.rightTrigger==0&&selectionInput.weaponReady&&selectionInput.gamepad.leftY==28000,
        "release closes selection and restores the ready grip without firing a held trigger");
    walkingSelection.rightTrigger=0;wristInput.update(walkingSelection,false,true,TravelMode::onFoot);
    walkingSelection.rightTrigger=255;
    expect(wristInput.update(walkingSelection,false,true,TravelMode::onFoot).gamepad.rightTrigger==255,"a fresh trigger pull fires after the picker closes");
    RigInput grenadeInput;
    const GamepadSample grenadeStick{0,0,0,12000,19000,21000,-25000};
    auto grenadeMapped=grenadeInput.update(grenadeStick,false,true,TravelMode::onFoot,1000,0,true);
    expect(grenadeMapped.weaponReady&&grenadeMapped.gamepad.rightY==0&&grenadeMapped.gamepad.rightX==21000
        &&grenadeMapped.gamepad.leftY==19000,"readied grenade uses hand elevation while retaining walking and body turning");
    expect(grenadeInput.update(grenadeStick,false,true,TravelMode::onFoot,1100,0,false).gamepad.rightY==-25000,
        "ordinary weapon look remains available outside grenade mode");
    const Pose primaryGrip{{},{.2f,1.2f,-.3f}},supportGrip{{},{.5f,1.2f,-.6f}};
    const auto guidedGrip=twoHandGrip(primaryGrip,supportGrip,{0,0,-.3f},1);
    expect(guidedGrip&&same(guidedGrip->position,primaryGrip.position)
        &&same(rotate(guidedGrip->orientation,{0,0,-1}),{.70710678f,0,-.70710678f}),
        "support controller guides aim while the primary palm stays fixed");
    const auto unguidedGrip=twoHandGrip(primaryGrip,supportGrip,{0,0,-.3f},0);
    expect(unguidedGrip&&same(rotate(unguidedGrip->orientation,{0,0,-1}),{0,0,-1}),"zero support influence preserves one-handed aim");
    const auto sidewaysBarrel=twoHandGrip(primaryGrip,supportGrip,{1,0,0},1);
    expect(sidewaysBarrel&&same(sidewaysBarrel->position,primaryGrip.position)
        &&same(rotate(sidewaysBarrel->orientation,{1,0,0}),{.70710678f,0,-.70710678f}),
        "a barrel offset from the palm axes follows the support controller without moving the firing grip");
    expect(!twoHandGrip(primaryGrip,primaryGrip,{0,0,-.3f},1)
        &&!twoHandGrip(primaryGrip,Pose{{},{.2f,1.2f,.1f}},{0,0,-.3f},1),"coincident or crossed hands cannot flip the weapon");
    {
        const Pose contact{{},{0,-.1f,-.3f}};
        const Pose atContact{{.38268343f,0,0,.92387953f},compose(primaryGrip,contact).position};
        const auto resting=twoHandSupportGrip(primaryGrip,atContact,contact,1);
        expect(resting&&same(resting->position,primaryGrip.position)
            &&same(rotate(resting->orientation,{0,0,-1}),{0,0,-1})
            &&same(compose(*resting,contact).position,atContact.position),
            "a hand at an off-axis support contact does not swing the weapon away from that contact");
        const auto wrongBore=twoHandGrip(primaryGrip,atContact,{0,0,-1},1);
        expect(wrongBore&&dot(compose(*wrongBore,contact).position-atContact.position,
            compose(*wrongBore,contact).position-atContact.position)>.01f,
            "the off-axis fixture distinguishes contact guidance from erroneous bore-to-palm guidance");
        const Pose movedSupport{{},primaryGrip.position+Vec3{.3f,-.1f,0}};
        const auto moved=twoHandSupportGrip(primaryGrip,movedSupport,contact,1);
        expect(moved&&same(moved->position,primaryGrip.position)
            &&same(compose(*moved,contact).position,movedSupport.position),
            "moving the support hand rotates its authored contact exactly while retaining the primary palm");
        const Pose frame{{0,.38268343f,0,.92387953f},{3,-1,2}};
        const auto transformed=twoHandSupportGrip(compose(frame,primaryGrip),compose(frame,movedSupport),contact,1);
        expect(moved&&transformed&&same(transformed->position,compose(frame,*moved).position)
            &&same(rotate(transformed->orientation,{0,0,-1}),rotate(compose(frame,*moved).orientation,{0,0,-1}))
            &&same(compose(*transformed,contact).position,compose(frame,movedSupport).position),
            "support-contact guidance is invariant under a common world-frame translation and rotation");
        const Pose farther{{},primaryGrip.position+Vec3{.6f,-.2f,0}};
        const auto unscaled=twoHandSupportGrip(primaryGrip,farther,contact,1);
        expect(unscaled&&same(compose(*unscaled,contact).position,primaryGrip.position+Vec3{.3f,-.1f,0}),
            "a farther tracked support hand changes direction without stretching the authored weapon");
        const auto disabled=twoHandSupportGrip(primaryGrip,movedSupport,contact,0);
        expect(disabled&&same(compose(*disabled,contact).position,atContact.position),
            "zero support influence preserves the authored contact and one-handed aim");
        const Pose rightWristFromGrip{{0,0,.38268343f,.92387953f},{.03f,-.02f,.01f}};
        const Pose leftWristFromGrip{{.38268343f,0,0,.92387953f},{-.02f,.015f,-.01f}};
        const auto wristContact=compose(inverse(rightWristFromGrip),compose(contact,leftWristFromGrip));
        const auto fromWrists=compose(rightWristFromGrip,compose(wristContact,inverse(leftWristFromGrip)));
        const auto wristGuided=twoHandSupportGrip(primaryGrip,atContact,fromWrists,1);
        expect(wristGuided&&same(fromWrists.position,contact.position)
            &&same(compose(*wristGuided,fromWrists).position,atContact.position),
            "native wrist contact converts through both anatomical palm offsets before guidance");
        expect(!twoHandSupportGrip(primaryGrip,movedSupport,{},1)
            &&!twoHandSupportGrip(primaryGrip,movedSupport,Pose{{},{std::numeric_limits<float>::quiet_NaN(),0,0}},1)
            &&!twoHandSupportGrip(primaryGrip,movedSupport,Pose{{0,0,0,0},contact.position},1)
            &&!twoHandSupportGrip(primaryGrip,primaryGrip,contact,1),
            "invalid authored contacts and coincident controllers retain one-handed guidance fallback");
    }
    for(unsigned finger=0;finger<5;++finger)for(unsigned joint=0;joint<3;++joint){
        const auto leftOpen=fingerJointRotation(false,finger,joint,0),rightOpen=fingerJointRotation(true,finger,joint,0);
        expect(leftOpen&&rightOpen&&same(rotate(*leftOpen,{1,0,0}),{1,0,0})&&same(rotate(*rightOpen,{-1,0,0}),{-1,0,0}),
            "released controller fingers retain the authored straight bind pose");
        const auto leftCurl=fingerJointRotation(false,finger,joint,1),rightCurl=fingerJointRotation(true,finger,joint,1);
        expect(leftCurl&&rightCurl&&(finger
            ?rotate(*leftCurl,{1,0,0}).y<0&&rotate(*rightCurl,{-1,0,0}).y<0
            :rotate(*leftCurl,{1,0,0}).z<0&&rotate(*rightCurl,{-1,0,0}).z<0),
            "fingers curl into the palm while mirrored thumbs close across it");
    }
    for(const bool rightHand:{false,true}){
        const float side=rightHand?-1.f:1.f;
        Pose thumb{{},{0,0,.03f}};
        const Vec3 segments[]{{side*.04f,-.01f,.008f},{side*.03f,-.01f,0},{side*.02f,0,0}};
        for(unsigned joint=0;joint<3;++joint){
            thumb=compose(thumb,Pose{*fingerJointRotation(rightHand,0,joint,1),{}});
            thumb.position=thumb.position+rotate(thumb.orientation,segments[joint]);
        }
        expect(side*thumb.position.x>.02f&&thumb.position.z<0&&thumb.position.z>-.06f&&near(thumb.position.y,-.02f),
            "closed thumb stays in front of the wrist and crosses the palm without folding backward");
    }
    expect(!fingerJointRotation(false,5,0,1)&&!fingerJointRotation(false,0,3,1)&&!fingerJointRotation(false,0,0,-1),"invalid finger channels are rejected");
    GamepadMailbox gamepad;
    expect(!gamepad.read(100),"unconnected XR gamepad preserves original input path");
    gamepad.publish({0x1000,0,255,100,-100,0,0},true,100);
    auto input=gamepad.read(200);
    expect(input&&input->buttons==0x1000&&input->rightTrigger==255,"active XR input reaches native gamepad sample");
    input=gamepad.read(351);
    expect(input&&input->buttons==0&&input->rightTrigger==0,"stale XR releases held fire and buttons");
    gamepad.publish({0x1000,255,255,0,0,0,0},false,400);
    input=gamepad.read(400);
    expect(input&&*input==GamepadSample{},"focus loss forces neutral state");
    gamepad.publish({0x1000,255,255,0,0,0,0},true,500);
    expect(gamepad.read(499)==GamepadSample{},"clock mismatch never accepts future input");
    bool fresh=true;
    gamepad.read(499,&fresh);expect(!fresh,"future input cannot suppress a physical controller");
    gamepad.read(501,&fresh);expect(fresh,"fresh active sample owns the virtual controller");
    MenuButton menu;
    {
        MenuButton utility;
        expect(utility.update(true,true,100,true)==0&&utility.recentered(),"left grip plus Menu requests recenter without iDroid");
        expect(utility.update(true,true,800,true)==0&&!utility.recentered(),"held recenter chord neither repeats nor opens Pause");
        expect(utility.update(false,true,850)==0,"recenter release cannot leak into iDroid");
        utility.update(true,true,1000);
        expect(utility.update(false,true,1100)==0x10,"ordinary Menu tap still opens iDroid after recenter");
    }
    expect(menu.update(true,true,100)==0,"menu press does not prematurely open iDroid");
    expect(menu.update(false,true,200)==0x10,"short menu release opens iDroid");
    expect(menu.update(false,true,299)==0x10,"short press survives game frame polling");
    expect(menu.update(false,true,300)==0,"iDroid pulse releases");
    menu.update(true,true,400);
    expect(menu.update(true,true,949)==0,"pause hold threshold is enforced");
    expect(menu.update(true,true,950)==0x20,"long menu press maps native Pause");
    expect(menu.update(true,true,1050)==0,"pause pulse releases while held");
    expect(menu.update(false,true,1200)==0,"long menu release never also opens iDroid");
    menu.update(true,true,1300);menu.update(true,false,1400);
    expect(menu.update(true,true,2100)==0,"focus return cannot trigger a held menu");
    menu.update(false,true,2200);menu.update(true,true,2300);
    expect(menu.update(false,true,2400)==0x10,"fresh press after focus recovery works");
    expect(menu.update(true,true,100)==0,"clock reset cancels pending menu input");
    const Panel panel{Pose{{},{0,0,-2}},2,1,1920,1080};
    auto hit=intersectPanel(panel,{},3);
    expect(hit&&hit->x==960&&hit->y==540&&near(hit->distance,2),"ray reaches exact panel center");
    hit=intersectPanel(panel,Pose{{},{1,-0.5f,0}},3);
    expect(hit&&hit->x==1919&&hit->y==1079,"bottom/right boundary clamps to final source pixel");
    expect(!intersectPanel(panel,Pose{{},{1.01f,0,0}},3),"ray outside panel rejected");
    expect(!intersectPanel(panel,{},1),"ray beyond reach rejected");
    expect(!intersectPanel(panel,Pose{{0,1,0,0},{0,0,-3}},3),"back face rejected");
    expect(!intersectPanel(panel,Pose{{0,0.70710678f,0,0.70710678f},{}},3),"parallel ray rejected");
    expect(!intersectPanel(panel,Pose{{0,0,0,0},{}},3),"invalid controller orientation rejected");
    expect(!intersectPanel(Panel{{},0,1,1,1},{},3),"zero-size panel rejected");
    expect(!intersectPanel(Panel{{},1,1,0,1},{},3),"missing pixel dimensions rejected");
    expect(!intersectPanel(panel,{},std::numeric_limits<float>::infinity()),"unbounded invalid reach rejected");
    for(int n=0;n<100;++n){
        const float a=static_cast<float>(n)*0.06283185f;
        const Pose parent{{0,std::sin(a/2),0,std::cos(a/2)},{1.1f,-0.8f,2.3f}};
        const auto transformed=Panel{compose(parent,panel.pose),2,1,1920,1080};
        const auto h=intersectPanel(transformed,parent,3);
        expect(h&&near(h->u,0.5f)&&near(h->v,0.5f),"ray mapping invariant under world rotation/translation");
        expect(same(compose(parent,inverse(parent)).position,{}),"transform and inverse cancel");
    }
    {
        HeadCameraSample frame{};
        frame.applied=true;frame.stereoTracked=true;frame.activation=1;
        frame.nativePose={};frame.headPose={};
        frame.controllers.predictedXrTime=1;frame.controllers.referenceEpoch=1;
        auto& right=frame.controllers.hands[1];
        right.gripTracked=true;right.grip=Pose{{},{0,0,-.35f}};
        right.aimTracked=true;
        constexpr uintptr_t effectBase=0x140000000ull;
        expect(matchesNativeIdroidLightGraph(effectBase,effectBase+0x24e1460u,nativeIdroidLightGraphName),
            "iDroid probe identifies the native Light graph by exact type and authored name");
        expect(!matchesNativeIdroidLightGraph(0,0x24e1460u,nativeIdroidLightGraphName)
            &&!matchesNativeIdroidLightGraph(effectBase,effectBase+0x24e1450u,nativeIdroidLightGraphName)
            &&!matchesNativeIdroidLightGraph(effectBase,effectBase+0x24e1460u,nativeIdroidLightGraphName+1),
            "another graph, base type or absent executable cannot register an iDroid Light");
        expect(matchesNativeIdroidLightRetainedGraph(effectBase,effectBase+0x24e1450u,nativeIdroidLightGraphName),
            "iDroid probe accepts the retained graph's distinct native type and exact Light name");
        expect(!matchesNativeIdroidLightRetainedGraph(effectBase,effectBase+0x24e1460u,nativeIdroidLightGraphName)
            &&!matchesNativeIdroidLightRetainedGraph(effectBase,effectBase+0x24e1450u,nativeIdroidLightGraphName+1)
            &&!matchesNativeIdroidLightRetainedGraph(0,0x24e1450u,nativeIdroidLightGraphName),
            "a temporary prototype, unrelated retained graph or absent executable cannot own runtime Light updates");
        const auto primitiveMatches=[&](uintptr_t properties,uint16_t size,uintptr_t update){
            return matchesNativeIdroidPrimitive(effectBase,effectBase+0x1b6bab0u,update,
                effectBase+0x1b6c0d0u,effectBase+0x1b6c230u,properties,size);
        };
        expect(primitiveMatches(1,nativeIdroidPrimitivePropertySize,effectBase+0x1b6d640u),
            "iDroid primitive registration requires its four verified native callbacks and complete property block");
        expect(!primitiveMatches(0,nativeIdroidPrimitivePropertySize,effectBase+0x1b6d640u)
            &&!primitiveMatches(1,nativeIdroidPrimitivePropertySize-1,effectBase+0x1b6d640u)
            &&!primitiveMatches(1,nativeIdroidPrimitivePropertySize,effectBase+0x1b6d641u),
            "missing, truncated or differently typed primitive cannot join a Light runtime update");
        {
            std::array<std::byte,0xa8> instance{};
            std::array<std::byte,0x70> properties{};
            std::array<std::byte,0x128> model{};
            const auto put=[]<class T,size_t N>(std::array<std::byte,N>& bytes,size_t offset,T value){
                std::memcpy(bytes.data()+offset,&value,sizeof(value));
            };
            put(instance,0,uintptr_t{0x5000});put(instance,0x30,uintptr_t{0x2000});
            put(instance,0x60,uint32_t{1});put(instance,0x68,uint32_t{1});
            put(properties,0x40,uintptr_t{0x3000});put(properties,0x4c,uint32_t{32});
            put(properties,0x50,uint32_t{96});put(properties,0x54,uint32_t{32});
            put(model,0,effectBase+0x20f4d90u);
            put(model,0x118,uintptr_t{0x6000});put(model,0x120,uint64_t{0x84a3182ae4e26449ull});
            put(model,0xe8,uintptr_t{0x4000});put(model,0xf8,uint16_t{1});
            const uint32_t boneName=0x78d68a95u;
            const std::array<float,16> parentMatrix{1,0,0,0,0,1,0,0,0,0,1,0,.5f,.6f,.7f,1};
            unsigned headerReads{},propertyReads{};bool replace{},change{},truncated{},resourceChanged{},argumentChanged{},parentChanged{};
            unsigned parentReads{};
            uintptr_t missing{};
            const auto reader=[&](uintptr_t address,void* output,size_t bytes){
                if(address==missing)return false;
                if(address==0x1000&&bytes==instance.size()){
                    std::memcpy(output,instance.data(),bytes);
                    if(++headerReads==2&&replace){const uintptr_t other=0x2200;std::memcpy(static_cast<std::byte*>(output)+0x30,&other,sizeof(other));}
                    if(headerReads==2&&argumentChanged){const uintptr_t other=0x2300;std::memcpy(static_cast<std::byte*>(output)+8,&other,sizeof(other));}
                    return true;
                }
                if(address==0x2000&&bytes==properties.size()){
                    if(truncated)return false;
                    std::memcpy(output,properties.data(),bytes);
                    if(++propertyReads==2&&change)static_cast<std::byte*>(output)[0]^=std::byte{1};
                    return true;
                }
                if(address==0x3000&&bytes<=model.size()){std::memcpy(output,model.data(),bytes);return true;}
                if(address==0x3118&&bytes==sizeof(uintptr_t)){std::memcpy(output,model.data()+0x118,bytes);return true;}
                if(address==0x3120&&bytes==sizeof(uint64_t)){
                    std::memcpy(output,model.data()+0x120,bytes);
                    if(resourceChanged)static_cast<std::byte*>(output)[0]^=std::byte{1};
                    return true;
                }
                if(address==0x4000&&bytes==sizeof(boneName)){std::memcpy(output,&boneName,bytes);return true;}
                if(address==0x5000&&bytes==sizeof(parentMatrix)){
                    std::memcpy(output,parentMatrix.data(),bytes);
                    if(++parentReads==2&&parentChanged){const float movedParent=1.f;std::memcpy(static_cast<std::byte*>(output)+12*sizeof(float),&movedParent,sizeof(movedParent));}
                    return true;
                }
                return false;
            };
            const auto sample=[&]{headerReads=propertyReads=parentReads=0;return readNativeIdroidPrimitiveCandidate(effectBase,0x1000,reader);};
            const auto current=sample();
            expect(current&&current->model==0x3000&&current->properties==0x2000
                &&current->vertices==32&&current->indices==96&&current->triangles==32
                &&current->modelResource==0x6000&&current->modelResourceCode==0x84a3182ae4e26449ull
                &&current->parentMatrixRead&&current->parentMatrix==parentMatrix
                &&current->boneNamesRead&&current->boneNames[0]==boneName,
                "already-loaded primitive discovery reads current finalized typed model without inventing a graph identity");
            replace=true;expect(!sample(),"replaced finalized property owner rejects the discovery sample");replace=false;
            change=true;expect(!sample(),"changed complete primitive properties reject the discovery sample");change=false;
            resourceChanged=true;expect(!sample(),"changed native model resource code rejects stale asset identity");resourceChanged=false;
            argumentChanged=true;expect(!sample(),"replaced interpreter arguments reject the callback owner snapshot");argumentChanged=false;
            parentChanged=true;const auto tornParent=sample();
            expect(tornParent&&!tornParent->parentMatrixRead,"changing native parent matrix remains unknown instead of fabricating coherent attachment");parentChanged=false;
            truncated=true;expect(!sample(),"incomplete native property reads never yield a discovery candidate");truncated=false;
            missing=0x3000;expect(!sample(),"missing native model rejects its candidate");missing=0;
            put(model,0,effectBase+0x20f4d98u);expect(!sample(),"another native model type cannot borrow the finalized GrModel layout");
            put(model,0,effectBase+0x20f4d90u);
            put(instance,0x68,uint32_t{2});expect(!sample(),"a batch count different from its active range rejects the candidate");
            put(instance,0x68,uint32_t{1});
            put(instance,0x64,uint32_t{3});put(instance,0x68,uint32_t{4});
            expect(sample().has_value(),"a nonzero native task offset is valid when end equals first plus count");
            put(instance,0x64,uint32_t{0});put(instance,0x68,uint32_t{1});
            missing=0x5000;const auto noParent=sample();
            expect(noParent&&!noParent->parentMatrixRead,"unreadable parent matrix remains unknown without fabricated attachment ownership");
            expect(!readNativeIdroidPrimitiveCandidate(effectBase,UINTPTR_MAX-2,reader),
                "overflowing native instance spans are rejected before a read");
            NativeIdroidCandidateBudget budget;
            unsigned admitted{};for(unsigned i=0;i<1000;++i)if(budget.admit(1000))++admitted;
            expect(admitted==128&&!budget.admit(1499)&&budget.admit(1500),
                "absent candidates cannot cause unbounded native reads in one discovery window");
            budget={};for(unsigned i=0;i<8;++i){expect(budget.admit(1000),"bounded candidate report admission");budget.reported();}
            expect(!budget.admit(1001)&&budget.admit(1500),"candidate output has its own strict per-window budget");
            budget={};for(unsigned window=0;window<8;++window)for(unsigned i=0;i<8;++i){
                expect(budget.admit(1000+window*500),"bounded total candidate report admission");budget.reported();
            }
            expect(!budget.admit(10000),"candidate diagnostics stop all reads after their session output budget");
            budget={};admitted=0;for(unsigned i=0;i<1000;++i)if(budget.admit(UINT64_MAX))++admitted;
            expect(admitted==128,"saturated clock cannot restart a discovery window on every callback");
            NativeIdroidCandidateReportState motion;
            auto heldCandidate=*current;
            std::array<float,7> emitter{0,0,0,1,.5f,.6f,.7f};
            expect(motion.changed(heldCandidate,emitter,1,1000),"first finalized candidate preserves a diagnostic identity sample");
            motion.remember(heldCandidate,emitter,1,1000);
            budget={};budget.reported();
            unsigned stationaryReports{};
            for(unsigned i=1;i<=10000;++i){
                if(motion.changed(heldCandidate,emitter,1,1000+i*500u)){
                    ++stationaryReports;budget.reported();motion.remember(heldCandidate,emitter,1,1000+i*500u);
                }
            }
            expect(stationaryReports==0&&budget.total==1,
                "a stationary Help page cannot exhaust changed-pose discovery output");
            auto signEquivalent=emitter;signEquivalent[3]=-1;
            expect(!motion.changed(heldCandidate,signEquivalent,1,2000),"equivalent quaternion signs do not fabricate a changed device pose");
            auto noise=heldCandidate;noise.parentMatrix[12]+=.0001f;
            auto emitterNoise=emitter;emitterNoise[4]+=.0001f;
            expect(!motion.changed(noise,emitterNoise,1,2000),"submillimetre attachment noise does not spend pose reports");
            auto moved=heldCandidate;moved.parentMatrix[12]+=.05f;
            expect(!motion.changed(moved,emitter,1,1499)&&motion.changed(moved,emitter,1,1500),
                "actual parent motion is retained after the bounded half-second output interval");
            auto movedEmitter=emitter;movedEmitter[4]+=.08f;
            expect(motion.changed(heldCandidate,movedEmitter,1,2000),
                "a changed physical device pose is recorded even when an unrelated candidate stays still");
            auto rotatedEmitter=emitter;rotatedEmitter[1]=.1305262f;rotatedEmitter[3]=.9914449f;
            expect(motion.changed(heldCandidate,rotatedEmitter,1,2000),"hand rotation without translation preserves a second emitter comparison");
            auto replacedParent=heldCandidate;replacedParent.parent+=0x100;
            expect(motion.changed(replacedParent,emitter,1,2000)&&motion.changed(heldCandidate,emitter,2,2000),
                "replaced parent and changed activation require fresh candidate evidence");
            auto unreadableParent=heldCandidate;unreadableParent.parentMatrixRead=false;
            expect(motion.changed(unreadableParent,emitter,1,2000),"loss of parent readability is retained as unknown");
            motion.remember(heldCandidate,emitter,1,UINT64_MAX);
            expect(motion.nextReport==UINT64_MAX&&!motion.changed(heldCandidate,emitter,1,UINT64_MAX-1),
                "changed-pose scheduling saturates safely at the clock limit");
            auto drawable=heldCandidate;drawable.batchCount=1;drawable.first=0;drawable.last=1;
            auto emptyBatch=heldCandidate;emptyBatch.batchCount=emptyBatch.first=emptyBatch.last=0;
            motion.remember(emptyBatch,emitter,1,1000);
            expect(motion.changed(drawable,emitter,1,1500),"an active particle range transition survives a preceding empty callback");
            std::array<std::byte,16> clonePool{};
            missing=0;
            std::array<std::byte,0x128> cloneOne=model,cloneTwo=model,cloneThree=model;
            put(clonePool,0,uint32_t{2});put(clonePool,4,uint32_t{4});put(clonePool,8,uintptr_t{0x7100});
            const std::array<float,16> cloneWorld{.1f,0,0,0,0,.1f,0,0,0,0,.1f,0,.5f,.6f,.7f,1};
            put(cloneOne,0x40,cloneWorld);put(cloneTwo,0x40,cloneWorld);put(cloneThree,0x40,cloneWorld);
            put(instance,0x38,uintptr_t{0x7000});put(instance,0x60,uint32_t{2});put(instance,0x68,uint32_t{2});
            auto cloneCandidate=*current;cloneCandidate.particlePool=0x7000;cloneCandidate.batchCount=cloneCandidate.last=2;
            std::array<uintptr_t,3> clonePointers{0x8000,0x9000,0xa000};
            bool replacePool{},replaceClone{},tornWorld{};unsigned poolReads{},pointerReads{},drawWorldReads{};
            const auto cloneReader=[&](uintptr_t address,void* output,size_t bytes){
                if(address==0x7000&&bytes==clonePool.size()){
                    std::memcpy(output,clonePool.data(),bytes);
                    if(++poolReads==2&&replacePool)static_cast<std::byte*>(output)[8]^=std::byte{1};
                    return true;
                }
                if((address==0x7100||address==0x7108||address==0x7110)&&bytes==sizeof(uintptr_t)){
                    const auto index=(address-0x7100)/sizeof(uintptr_t);
                    std::memcpy(output,&clonePointers[index],bytes);
                    if(++pointerReads>2&&replaceClone)static_cast<std::byte*>(output)[0]^=std::byte{1};
                    return true;
                }
                for(const auto [start,data]:std::array<std::pair<uintptr_t,const std::array<std::byte,0x128>*>,3>{{{0x8000,&cloneOne},{0x9000,&cloneTwo},{0xa000,&cloneThree}}}){
                    if(address>=start&&address-start<=data->size()&&bytes<=data->size()-(address-start)){
                        std::memcpy(output,data->data()+(address-start),bytes);
                        if(address==start+0x40&&++drawWorldReads==2&&tornWorld)static_cast<std::byte*>(output)[0]^=std::byte{1};
                        return true;
                    }
                }
                if(address>=0x1000&&address-0x1000<=instance.size()&&bytes<=instance.size()-(address-0x1000)){
                    std::memcpy(output,instance.data()+(address-0x1000),bytes);return true;
                }
                return reader(address,output,bytes);
            };
            const auto cloneSample=[&]{poolReads=pointerReads=drawWorldReads=0;return readNativeIdroidConeDraws(effectBase,0x7000,cloneCandidate,cloneReader);};
            const auto cloneBatch=cloneSample();
            expect(cloneBatch&&cloneBatch->activeCount==2&&cloneBatch->active[0].record==0x8000
                &&cloneBatch->active[0].world==cloneWorld,"post-update particle draw records belong to the exact typed source resource and pool");
            replacePool=true;expect(!cloneSample(),"replaced cone model pool rejects the batch");replacePool=false;
            replaceClone=true;expect(!cloneSample(),"replaced active cone model rejects the batch");replaceClone=false;
            tornWorld=true;expect(!cloneSample(),"changing native cone model matrix cannot certify an endpoint");tornWorld=false;
            clonePointers[1]=clonePointers[0];expect(!cloneSample(),"aliased effect clones cannot be written twice");clonePointers[1]=0x9000;
            clonePointers[0]=cloneCandidate.model;expect(!cloneSample(),"cached property model is never an eligible effect clone");clonePointers[0]=0x8000;
            put(model,0x120,uint64_t{0x84a3182ae4e26448ull});expect(!cloneSample(),"another source resource cannot borrow cone draw layout");
            put(model,0x120,nativeIdroidConeResourceCode);
            put(clonePool,0,uint32_t{1});expect(!cloneSample(),"active range outside the native pool rejects the batch");put(clonePool,0,uint32_t{2});
            put(instance,0x68,uint32_t{1});expect(!cloneSample(),"changed active particle range rejects stale clone ownership");put(instance,0x68,uint32_t{2});
            expect(!readNativeIdroidConeDraws(effectBase,0x7010,cloneCandidate,cloneReader),"callback pool must be the exact native header pool");
            auto emitterCandidate=cloneCandidate;emitterCandidate.parentMatrix=parentMatrix;
            emitterCandidate.parentMatrixRead=emitterCandidate.boneNamesRead=true;emitterCandidate.bones=1;
            emitterCandidate.boneNames[0]=0x78d68a95u;emitterCandidate.flags=1;
            emitterCandidate.interpreterArgument3=emitterCandidate.parent+0x50u;
            const Pose exactEmitter{{},{.5f,.6f,.7f}};
            expect(matchesNativeIdroidConeEmitter(emitterCandidate,exactEmitter),
                "a canonical native cone joins the complete published emitter frame");
            auto differentEmitter=exactEmitter;differentEmitter.orientation={0,.1305262f,0,.9914449f};
            expect(!matchesNativeIdroidConeEmitter(emitterCandidate,differentEmitter),
                "a nearby cone with another rotation cannot own the player's projection");
            differentEmitter=exactEmitter;differentEmitter.position.x+=.001f;
            expect(!matchesNativeIdroidConeEmitter(emitterCandidate,differentEmitter),
                "millimetre proximity cannot replace emitter matrix equality");
            emitterCandidate.interpreterArgument3+=8;
            expect(!matchesNativeIdroidConeEmitter(emitterCandidate,exactEmitter),
                "a replaced native attachment interpreter cannot retain projection ownership");
            const Vec3 projectionEmitter{1,2,3};
            const Pose projectionScreen{{0,0,.258819f,.9659258f},{1.1f,2.2f,2.8f}};
            const auto projectionFit=nativeIdroidConeScreenFit(projectionEmitter,projectionScreen,.45f);
            const auto conePoint=[](const std::array<float,16>& w,Vec3 p){return Vec3{
                p.x*w[0]+p.y*w[4]+p.z*w[8]+w[12],p.x*w[1]+p.y*w[5]+p.z*w[9]+w[13],
                p.x*w[2]+p.y*w[6]+p.z*w[10]+w[14]};};
            expect(projectionFit&&same(conePoint(*projectionFit,{0,0,.01f}),projectionEmitter)
                &&same(conePoint(*projectionFit,{0,0,2}),projectionScreen.position),
                "configured cone keeps the emitter fixed and meets the screen centre");
            bool farPlane=true;
            if(projectionFit)for(unsigned corner=0;corner<16;++corner){
                const float angle=corner*6.28318530718f/16;
                const auto pointOnRing=conePoint(*projectionFit,{.8f*std::cos(angle),.8f*std::sin(angle),2});
                if(std::abs(dot(pointOnRing-projectionScreen.position,rotate(projectionScreen.orientation,{0,0,1})))>.000001f)farPlane=false;
            }
            expect(projectionFit&&farPlane,"every authored far-ring sample terminates on the rotated screen plane");
            const auto fitBounds=projectionFit?nativeIdroidConeBounds(*projectionFit):std::nullopt;
            bool enclosed=fitBounds.has_value();
            if(fitBounds)for(unsigned ring=0;ring<2;++ring)for(unsigned vertex=0;vertex<16;++vertex){
                const float angle=vertex*6.28318530718f/16.f;
                const float radius=ring?.8f:.004f,z=ring?2.f:.01f;
                const auto p=conePoint(*projectionFit,{radius*std::cos(angle),radius*std::sin(angle),z});
                const std::array<float,3> coordinates{p.x,p.y,p.z};
                for(unsigned axis=0;axis<3;++axis)if(coordinates[axis]<(*fitBounds)[axis]
                    ||coordinates[axis]>(*fitBounds)[axis+4])enclosed=false;
            }
            expect(enclosed,"fitted native bounds enclose every authored cone vertex after display rotation and shear");
            auto singleDraw=cloneCandidate;singleDraw.batchCount=singleDraw.last=1;
            put(instance,0x60,uint32_t{1});put(instance,0x68,uint32_t{1});put(clonePool,0,uint32_t{1});
            singleDraw.boneNames[0]=0x78d68a95u;singleDraw.flags=1;
            singleDraw.interpreterArgument3=singleDraw.parent+0x50u;
            put(instance,8,singleDraw.interpreterArgument3);
            unsigned projectionWrites{},projectionAttempts{},failProjectionAttempt{},failRestoreAttempt{};
            size_t partialProjectionBytes{};bool boundsWritable=true,boundsReadable=true;
            const auto fitWritable=[&](uintptr_t address,size_t bytes){
                return (address==0x8040&&bytes==64)||(boundsWritable&&address==0x80a0&&bytes==32);
            };
            const auto fitWriter=[&](uintptr_t address,const void* data,size_t bytes){
                if(!fitWritable(address,bytes))return false;
                const auto attempt=++projectionAttempts;
                if(attempt==failProjectionAttempt){
                    std::memcpy(cloneOne.data()+(address-0x8000),data,std::min(bytes,partialProjectionBytes));
                    return false;
                }
                if(attempt==failRestoreAttempt)return false;
                ++projectionWrites;std::memcpy(cloneOne.data()+(address-0x8000),data,bytes);return true;
            };
            const auto fitReader=[&](uintptr_t address,void* data,size_t bytes){
                return (boundsReadable||address!=0x80a0)&&cloneReader(address,data,bytes);
            };
            const Pose singleScreen{{},{.5f,.8f,.6f}};
            const auto applyProjection=[&]{
                poolReads=pointerReads=drawWorldReads=0;
                return retargetNativeIdroidConeDraw(effectBase,0x7000,singleDraw,exactEmitter,singleScreen,.45f,
                    fitReader,fitWritable,fitWriter);
            };
            boundsWritable=false;
            expect(!applyProjection()&&projectionWrites==0,"unwritable native culling bounds reject the fit before any transform mutation");
            boundsWritable=true;
            boundsReadable=false;
            expect(!applyProjection()&&projectionAttempts==0,"unreadable native bounds cannot be mutated without a rollback snapshot");
            boundsReadable=true;
            const auto originalDraw=cloneOne;
            for(const unsigned failedAttempt:{1u,2u})for(const size_t partialBytes:{size_t{0},size_t{13},size_t{32}}){
                cloneOne=originalDraw;projectionWrites=projectionAttempts=0;
                failProjectionAttempt=failedAttempt;partialProjectionBytes=partialBytes;
                expect(!applyProjection()&&cloneOne==originalDraw
                    &&projectionAttempts==failedAttempt+2,
                    "a partial native transform or bounds write restores both original spans independently");
            }
            cloneOne=originalDraw;projectionWrites=projectionAttempts=0;
            failProjectionAttempt=2;partialProjectionBytes=13;failRestoreAttempt=3;
            expect(!applyProjection()&&projectionAttempts==4
                &&std::memcmp(cloneOne.data()+0xa0,originalDraw.data()+0xa0,32)==0,
                "a failed transform rollback still attempts the original culling bounds restoration");
            cloneOne=originalDraw;projectionWrites=projectionAttempts=0;
            failProjectionAttempt=failRestoreAttempt=0;
            expect(applyProjection()&&projectionWrites==2,"only the exact native draw transform and its culling bounds are written");
            singleDraw.parentMatrix[12]+=.001f;projectionWrites=0;
            expect(!applyProjection()&&projectionWrites==0,"a changed attachment cannot mutate even an already fitted draw record");
            singleDraw.parentMatrix[12]-=.001f;
            clonePointers[0]=singleDraw.model;
            expect(!applyProjection()&&projectionWrites==0,"the shared cached source model cannot become a projection writer");
            clonePointers[0]=0x8000;
            // The native field counterexample publishes one canonical Light
            // with three coincident draw particles. Fit the entire batch or
            // restore all six spans, including a partially written late sibling.
            singleDraw.batchCount=singleDraw.last=3;
            put(instance,0x60,uint32_t{3});put(instance,0x68,uint32_t{3});put(clonePool,0,uint32_t{3});
            const std::array<std::array<std::byte,0x128>,3> originalBatch{originalDraw,cloneTwo,cloneThree};
            bool thirdBoundsWritable=true,replaceGenerationDuringBounds=false;
            const auto resetBatch=[&]{
                cloneOne=originalBatch[0];cloneTwo=originalBatch[1];cloneThree=originalBatch[2];
                put(instance,0x30,singleDraw.properties);projectionWrites=projectionAttempts=0;
                poolReads=pointerReads=drawWorldReads=0;
            };
            const auto batchWritable=[&](uintptr_t address,size_t bytes){
                for(const auto start:clonePointers){
                    if(address==start+0x40&&bytes==64)return true;
                    if(address==start+0xa0&&bytes==32)return start!=0xa000||thirdBoundsWritable;
                }
                return false;
            };
            const auto batchWriter=[&](uintptr_t address,const void* data,size_t bytes){
                if(!batchWritable(address,bytes))return false;
                for(const auto [start,target]:std::array<std::pair<uintptr_t,std::array<std::byte,0x128>*>,3>{{{0x8000,&cloneOne},{0x9000,&cloneTwo},{0xa000,&cloneThree}}}){
                    if(address<start||address-start>target->size()||bytes>target->size()-(address-start))continue;
                    const auto attempt=++projectionAttempts;
                    if(attempt==failProjectionAttempt){
                        std::memcpy(target->data()+(address-start),data,std::min(bytes,partialProjectionBytes));return false;
                    }
                    if(attempt==failRestoreAttempt)return false;
                    ++projectionWrites;std::memcpy(target->data()+(address-start),data,bytes);return true;
                }
                return false;
            };
            const auto batchReader=[&](uintptr_t address,void* data,size_t bytes){
                const bool result=cloneReader(address,data,bytes);
                if(result&&address==0xa0a0&&replaceGenerationDuringBounds)put(instance,0x30,uintptr_t{0x2008});
                return result;
            };
            const auto applyBatch=[&]{return retargetNativeIdroidConeDraw(effectBase,0x7000,singleDraw,
                exactEmitter,singleScreen,.45f,batchReader,batchWritable,batchWriter);};
            resetBatch();thirdBoundsWritable=false;
            expect(!applyBatch()&&projectionAttempts==0,"an unwritable third particle rejects the complete fit before any sibling changes");
            thirdBoundsWritable=true;resetBatch();replaceGenerationDuringBounds=true;
            expect(!applyBatch()&&projectionAttempts==0,"a replaced canonical property generation during batch preflight rejects every write");
            replaceGenerationDuringBounds=false;resetBatch();
            auto separatedWorld=cloneWorld;separatedWorld[12]+=.01f;put(cloneThree,0x40,separatedWorld);
            expect(!applyBatch()&&projectionAttempts==0,"noncoincident source particles cannot borrow the canonical three-particle fit");
            for(unsigned failedAttempt=1;failedAttempt<=6;++failedAttempt){
                resetBatch();failProjectionAttempt=failedAttempt;partialProjectionBytes=13;
                expect(!applyBatch()&&cloneOne==originalBatch[0]&&cloneTwo==originalBatch[1]&&cloneThree==originalBatch[2]
                    &&projectionAttempts==failedAttempt+6,
                    "a partial write to any canonical sibling restores all three original transforms and bounds");
            }
            resetBatch();failProjectionAttempt=6;failRestoreAttempt=7;partialProjectionBytes=13;
            expect(!applyBatch()&&projectionAttempts==12&&cloneTwo==originalBatch[1]&&cloneThree==originalBatch[2],
                "one failed batch rollback never skips later sibling restoration");
            resetBatch();failProjectionAttempt=failRestoreAttempt=0;
            expect(applyBatch()&&projectionWrites==6
                &&std::memcmp(cloneOne.data()+0x40,cloneTwo.data()+0x40,64)==0
                &&std::memcmp(cloneOne.data()+0x40,cloneThree.data()+0x40,64)==0,
                "the observed complete three-particle Light receives one coherent configured endpoint and culling fit");
            resetBatch();singleDraw.batchCount=singleDraw.last=4;
            expect(!applyBatch()&&projectionAttempts==0,"unobserved fourth particles are outside the bounded writer contract");
            singleDraw.batchCount=singleDraw.last=3;singleDraw.first=1;
            expect(!applyBatch()&&projectionAttempts==0,"partial callback ranges cannot independently mutate a sibling subset");
            singleDraw.first=0;
            expect(!nativeIdroidConeScreenFit(projectionEmitter,Pose{{},projectionEmitter},.45f)
                &&!nativeIdroidConeScreenFit(projectionEmitter,projectionScreen,0)
                &&!nativeIdroidConeScreenFit(projectionEmitter,Pose{{0,0,0,0},projectionScreen.position},.45f),
                "coincident, invalid or zero-sized projection targets fail without a native write");
        }
        expect(matchesNativeIdroidMount(0x1c68632c5c53ull,0x0004000038b1433cull,12),
            "observed retail iDroid CNP variant is decoded from its packed native anchor");
        {
            std::array<uint32_t,132> names{};std::array<int32_t,132> parents{};
            names[8]=0x3e2d77e5u;
            std::array<Pose,132> joints{};
            for(const auto& joint:nativeIdroidLeftHand){
                names[joint.bone]=joint.name;parents[joint.bone]=joint.parent;
                joints[joint.bone]=compose(joints[joint.parent],joint.local);
            }
            expect(matchesNativeIdroidLeftHand(names,parents),"cinematic contact admits its exact left-hand skeleton");
            names[8]=0x38b1433cu;
            expect(!matchesNativeIdroidLeftHand(names,parents),"right wrist cannot admit left-hand cinematic contact");
            names[8]=0x3e2d77e5u;parents[34]=8;
            expect(!matchesNativeIdroidLeftHand(names,parents),"ring/little base mismatch cannot authorize authored fingers");
            expect(same(joints[23].position,{.057982051f,-.033873722f,.082693118f})
                &&same(joints[36].position,{.097089701f,-.036415569f,-.036994296f}),
                "left thumb and little fingertip reconstruct the independently measured cinematic palette");
            const auto wristFromGrip=nativeIdroidLeftWristFromGrip();
            const Pose tracked{binocularGripRotation(25,-30,40),{80.2f,331.f,94.5f}};
            const auto wrist=wristFromGrip?compose(tracked,*wristFromGrip):Pose{};
            const auto leftContactPalm=anatomicalGrip(wrist,compose(wrist,joints[24]).position,compose(wrist,joints[34]).position);
            expect(wristFromGrip&&leftContactPalm&&same(leftContactPalm->position,tracked.position)
                &&same(rotate(leftContactPalm->orientation,{0,1,0}),rotate(tracked.orientation,{0,1,0})),
                "cupped cinematic palm keeps the tracked left grip through combined rotation and translation");
            const auto handset=compose(wrist,nativeIdroidLeftBodyInWrist);
            const auto contact=compose(inverse(wrist),handset);
            expect(same(contact.position,{.066569098f,-.021187740f,.004362963f})
                &&same(rotate(contact.orientation,{0,0,1}),{.038529198f,-.999120233f,.016633254f}),
                "left handset uses the captured native contact basis, not a mirrored ordinary right CNP");
            const auto naturalWrist=nativeIdroidLeftHandheldWrist(tracked);
            const auto naturalBody=naturalWrist?compose(*naturalWrist,nativeIdroidLeftBodyInWrist):Pose{};
            const auto naturalPalm=naturalWrist&&wristFromGrip?compose(*naturalWrist,inverse(*wristFromGrip)):Pose{};
            expect(naturalWrist&&same(naturalPalm.position,tracked.position)
                &&same(rotate(naturalPalm.orientation,{0,1,0}),rotate(tracked.orientation,{0,1,0}))
                &&same(rotate(naturalPalm.orientation,{1,0,0}),rotate(tracked.orientation,{1,0,0}))
                &&same(compose(inverse(*naturalWrist),naturalBody).position,nativeIdroidLeftBodyInWrist.position),
                "tracked palm rotation owns the cupped wrist and its fixed handset attachment");
            expect(!nativeIdroidLeftHandheldWrist(tracked,Pose{{0,0,0,0},{}}),
                "invalid explicit fit cannot invent a held cinematic handset orientation");
        }
        expect(!matchesNativeIdroidMount(0x1c68632c5c53ull,0x0003000038b1433cull,12)
            &&!matchesNativeIdroidMount(0x436a694680a8ull,0x0004000038b1433cull,12)
            &&!matchesNativeIdroidMount(0x1c68632c5c53ull,0x0004000038b1433cull,8),
            "another CNP variant, equipment socket or left wrist cannot be an iDroid mount");
        expect(!trackedIdroidPose(frame),"a raw grip cannot replace a missing native iDroid mount");
        const Pose connector{{},{.12f,-.18f,.5f}};
        const auto body=idroidBodyFromConnector(connector);
        expect(body&&same(compose(*body,idroidConnectorInBody).position,connector.position),
            "native iDroid connector remains exactly on the solved controller contact");
        expect(body&&near(body->position.y,-.087803796f)&&near(body->position.z,.500741f),
            "device root retains the measured retail connector offset instead of using the palm center");
        expect(!idroidBodyFromConnector(Pose{{0,0,0,0},{}}),"invalid native connector cannot place a device");
        {
            const auto matrix=[](Pose p){
                const auto x=rotate(p.orientation,{1,0,0}),y=rotate(p.orientation,{0,1,0}),z=rotate(p.orientation,{0,0,1});
                return std::array<float,16>{x.x,x.y,x.z,0,y.x,y.y,y.z,0,z.x,z.y,z.z,0,p.position.x,p.position.y,p.position.z,1};
            };
            const Pose handsetBinding{binocularGripRotation(10,-35,5),{80,330,95}};
            const Pose frozenBody{binocularGripRotation(-25,5,10),{.2f,1.1f,.4f}};
            const Pose antennaLocal{binocularGripRotation(0,0,15),{0,.07f,0}};
            const Pose desired{binocularGripRotation(20,55,-30),{80.3f,331.2f,94.8f}};
            for(const uint8_t flags:{uint8_t{0},uint8_t{1}}){
                const auto old=flags?frozenBody:compose(handsetBinding,frozenBody);
                const auto oldAntenna=compose(old,antennaLocal);
                const auto delta=nativeIdroidAttachmentDelta(matrix(old),matrix(handsetBinding),flags,desired);
                const auto updated=delta?nativeAffinePose(nativeIdroidWorldMatrix(matrix(old),*delta,1)):std::nullopt;
                const auto world=updated?(flags?compose(handsetBinding,*updated):*updated):Pose{};
                expect(updated&&same(world.position,desired.position)
                    &&same(rotate(world.orientation,{0,1,0}),rotate(desired.orientation,{0,1,0})),
                    "paused handset delta moves the native root with the wrist in local and world bindings");
                const auto antenna=delta?nativeAffinePose(nativeIdroidWorldMatrix(matrix(oldAntenna),*delta,1)):std::nullopt;
                const auto relative=antenna&&updated?compose(inverse(*updated),*antenna):Pose{};
                expect(antenna&&same(relative.position,antennaLocal.position)
                    &&same(rotate(relative.orientation,{1,0,0}),rotate(antennaLocal.orientation,{1,0,0})),
                    "handset refresh preserves the native internal antenna animation");
                const auto repeated=updated?nativeIdroidAttachmentDelta(matrix(*updated),matrix(handsetBinding),flags,desired):std::nullopt;
                expect(repeated&&same(nativeAffinePose(*repeated)->position,{})
                    &&same(rotate(nativeAffinePose(*repeated)->orientation,{0,0,1}),{0,0,1}),
                    "repeated paused refresh is an identity delta, not compounded attachment motion");
            }
            expect(!nativeIdroidAttachmentDelta({},matrix(handsetBinding),1,desired)
                &&!nativeIdroidAttachmentDelta(matrix(frozenBody),{},1,desired)
                &&!nativeIdroidAttachmentDelta(matrix(frozenBody),matrix(handsetBinding),1,Pose{{0,0,0,0},{}}),
                "unverified handset or binding transforms never authorize a native delta");
        }
        const Pose cantedGrip{{.5f,0,0,.8660254f},{.1f,-.2f,-.5f}};
        const Pose pointing{{},{.3f,.4f,.5f}};
        const auto naturalContact=idroidGripContact(cantedGrip,pointing);
        expect(naturalContact&&same(naturalContact->position,cantedGrip.position)
            &&same(rotate(naturalContact->orientation,{0,0,-1}),{0,0,-1}),
            "runtime grip cant cannot force an awkward device tilt or substitute the aim origin");
        const Pose gripFit{binocularGripRotation(10,-15,20),{.03f,-.02f,.04f}};
        const auto fittedContact=idroidGripContact(cantedGrip,pointing,gripFit);
        expect(fittedContact&&same(fittedContact->position,compose(cantedGrip,gripFit).position)
            &&same(rotate(fittedContact->orientation,{0,1,0}),rotate(gripFit.orientation,{0,1,0})),
            "iDroid contact fit translates in grip space and rotates in the runtime pointing basis");
        expect(!idroidGripContact(cantedGrip,Pose{{0,0,0,0},{}}),
            "lost controller aim cannot invent a native device holding frame");
        frame.idroidDeviceTracked=true;frame.idroidDevice=body?*body:Pose{};
        const auto idroid=trackedIdroidPose(frame);
        expect(idroid&&same(idroid->screen.position,{.1199944f,.082137202f,.603090601f})
            &&same(rotate(idroid->screen.orientation,{0,1,0}),{0,1,0})
            &&same(rotate(idroid->screen.orientation,{0,0,1}),{0,0,1}),
            "iDroid lower edge meets the projector height instead of centering the menu through the hand");
        const auto nativeProjectionRoot=compose(idroidHologramInBody,Pose{{},{0,0,idroidNativeProjectionDepth}});
        expect(same(nativeProjectionRoot.position,{0,.043378498f,.102349601f})
            &&near(frame.controllers.idroidScreenDepth,idroidNativeProjectionDepth),
            "default projection root matches the recovered cinematic UI transform in handset space");
        const Pose nativeFromLocal{{0,1,0,0},{}};
        if(idroid)right.aim=compose(inverse(nativeFromLocal),compose(idroid->screen,Pose{{},{0,0,.25f}}));
        const auto idroidHit=trackedIdroidRay(frame);
        expect(idroidHit&&near(idroidHit->hit.u,.5f)&&near(idroidHit->hit.v,.5f)
            &&std::abs(int(idroidHit->hit.x)-int(idroidScreenPixelWidth/2))<=1
            &&std::abs(int(idroidHit->hit.y)-int(idroidScreenPixelHeight/2))<=1,
            "iDroid ray lands at the authored projection center");
        if(idroid)right.aim=compose(inverse(nativeFromLocal),compose(idroid->screen,Pose{{},{-.04f,0,.25f}}));
        const auto moved=trackedIdroidRay(frame);
        expect(moved&&moved->hit.u<.5f&&moved->hit.u>0.f,
            "iDroid ray follows aim origin across the display");
        right.gripTracked=false;
        expect(trackedIdroidPose(frame).has_value()&&!trackedIdroidRay(frame),
            "retained iDroid presentation cannot grant a ray when raw grip tracking is lost");
        right.gripTracked=true;
        const auto validGrip=right.grip;right.grip.orientation={0,0,0,0};
        expect(trackedIdroidPose(frame).has_value()&&!trackedIdroidRay(frame),
            "an invalid raw grip cannot grant a ray through a retained device");
        right.grip=validGrip;
        right.aimTracked=false;
        expect(!trackedIdroidRay(frame),"iDroid ray fails closed when the aim pose is not tracked");
        frame.renderedPalmTracked[1]=true;frame.renderedPalms[1]=Pose{{},{7,8,9}};
        right.grip=Pose{binocularGripRotation(35,-20,10),{-1,.4f,-.2f}};
        const auto joined=trackedIdroidPose(frame);
        expect(joined&&idroid&&same(joined->screen.position,idroid->screen.position)
            &&same(rotate(joined->screen.orientation,{0,1,0}),{0,1,0}),
            "raw grip and anatomical palm cannot move the native device's published projection");
        const Pose move{{.3f,.4f,0,.8660254f},{1,2,3}};
        frame.idroidDevice=compose(move,frame.idroidDevice);
        const auto movedDisplay=trackedIdroidPose(frame);
        expect(movedDisplay&&idroid&&same(movedDisplay->screen.position,compose(move,idroid->screen).position)
            &&same(rotate(movedDisplay->screen.orientation,{0,1,0}),rotate(move.orientation,{0,1,0})),
            "projection and native device share translation and rotation during hand motion");
        frame.controllers.idroidScreenWidth=.6f;
        frame.controllers.idroidScreenRotation=binocularGripRotation(35,-20,10);
        const auto fittedDisplay=trackedIdroidPose(frame);
        const auto lowerEdge=[](const IdroidPose& display,float width){
            return compose(display.screen,Pose{{},{0,-width*9.f/32.f,0}}).position;
        };
        expect(fittedDisplay&&movedDisplay&&same(fittedDisplay->body.position,movedDisplay->body.position)
            &&same(lowerEdge(*fittedDisplay,.6f),lowerEdge(*movedDisplay,.45f)),
            "resizing or tilting iDroid preserves its lower emitter attachment and native device");
        frame.controllers.idroidScreenOffset={.06f,-.03f,0};
        const auto shiftedDisplay=trackedIdroidPose(frame);
        const auto mount=compose(frame.idroidDevice,idroidHologramInBody);
        expect(shiftedDisplay&&fittedDisplay&&same(shiftedDisplay->body.position,fittedDisplay->body.position)
            &&same(compose(inverse(mount),shiftedDisplay->screen).position,
                Vec3{.06f,-.03f,.09f}+rotate(frame.controllers.idroidScreenRotation,{0,.6f*9.f/32.f,0})),
            "fit adjustments operate in the native hologram socket frame");
        frame.controllers.idroidScreenWidth=std::numeric_limits<float>::infinity();
        expect(!trackedIdroidPose(frame),"invalid projection size cannot publish a detached or infinite display");
        frame.controllers.idroidScreenWidth=.6f;
        frame.controllers.idroidScreenOffset.x=std::numeric_limits<float>::quiet_NaN();
        expect(!trackedIdroidPose(frame),"invalid iDroid fit cannot publish a broken display transform");
        frame.controllers.idroidScreenOffset={};frame.idroidDeviceTracked=false;
        expect(!trackedIdroidPose(frame),"a stowed or unverified device cannot retain its previous projection");
        frame.idroidDeviceTracked=true;frame.controllers.handheldIdroidStowed=true;
        expect(!trackedIdroidPose(frame)&&!trackedIdroidRay(frame),
            "reload/support suspension hides the projection and its pointer even while the native terminal remains open");
    }
    const Pose tilted{{0.5f,0,0,0.8660254f},{1,1.7f,2}};
    const auto screen=recenteredScreen(tilted,6);
    expect(near(screen.position.y,tilted.position.y)&&near(screen.position.z,-4),"recenter discards head pitch and preserves level screen");
    {
        const std::array<EyeFov,2> fovs{{{-.9f,.65f,.8f,-.75f},{-.65f,.9f,.8f,-.75f}}};
        const auto fitted=fittedScreenWidth(12,6,16.f/9.f,fovs,.032f);
        expect(fitted&&*fitted<9&&*fitted>7,"loading panel fits the narrower side of both asymmetric eyes");
        expect(fittedScreenWidth(2,6,16.f/9.f,fovs,.032f)==2,"already fitted panel retains configured width");
        const auto portrait=fittedScreenWidth(12,6,.5f,fovs,.032f);
        expect(portrait&&*portrait<5,"portrait loading panel fits the vertical frustum too");
        expect(!fittedScreenWidth(12,0,1,fovs,.032f),"screen fit rejects invalid distance");
        expect(!fittedScreenWidth(12,6,0,fovs,.032f),"screen fit rejects invalid source aspect");
        auto invalid=fovs;invalid[1].right=0;
        expect(!fittedScreenWidth(12,6,1,invalid,.032f),"screen fit requires valid forward coverage in both eyes");
    }
    {
        NativeTextKeyboard keyboard;std::array<StudioHand,2> keyboardHands{};
        keyboardHands[1].tracked=keyboardHands[1].aimTracked=true;
        auto frame=keyboard.update(true,{},keyboardHands,1);
        keyboardHands[1].aim=compose(studioKeyPose(frame->keyboard,11),Pose{{},{0,0,.25f}});
        keyboardHands[1].trigger=1;frame=keyboard.update(true,{},keyboardHands,1);
        expect(frame->keyboardOnly&&frame->papers.empty()&&frame->photos.empty()&&keyboard.takeKey().character=='A',
            "Native profile keyboard emits the selected letter without creating studio objects");
        keyboard.update(true,{},keyboardHands,1);
        expect(!keyboard.takeKey().character,"Held text key cannot repeat");
        keyboardHands[1].aimTracked=false;keyboard.update(true,{},keyboardHands,1);
        keyboardHands[1].aimTracked=true;keyboard.update(true,{},keyboardHands,1);
        expect(!keyboard.takeKey().character,"Tracking recovery cannot type a held key");
        keyboardHands[1].trigger=0;keyboard.update(true,{},keyboardHands,1);
        keyboardHands[1].trigger=1;keyboard.update(true,{},keyboardHands,1);
        expect(keyboard.takeKey().character=='A',"Neutral trigger rearms a fresh key edge");
        frame=keyboard.update(true,Pose{{},{1,0,0}},keyboardHands,1,2);
        expect(!keyboard.takeKey().character&&frame->keyboard.position.x==1,
            "Recenter reanchors the keyboard and rejects held input");
        frame=keyboard.update(false,{},keyboardHands,1);
        expect(!frame->available&&!keyboard.takeKey().character,"Losing native profile ownership removes text input");
        expect(nativeTextKeyCharacter(34)=='\b'&&nativeTextKeyCharacter(36)=='\r'
            &&!nativeTextKeyEnabled(35)&&!nativeTextKeyEnabled(43),
            "Native text editing exposes Backspace and Done without note Clear or Delete");
    }
    WristFocus focus;
    expect(focus.update(0,true,true,0.8f,0.5f)==WristState::candidate,"wrist starts dwell");
    expect(focus.update(0.1,true,true,0.8f,0.5f)==WristState::candidate,"wrist cannot open immediately");
    expect(focus.update(0.21,true,true,0.8f,0.5f)==WristState::visible,"wrist opens after dwell");
    expect(focus.update(0.4,true,true,0.6f,0.75f)==WristState::visible,"exit hysteresis prevents boundary flicker");
    expect(focus.update(0.5,true,false,0.8f,0.5f)==WristState::hidden,"blank content immediately hides panel");
    focus.update(1,true,true,0.8f,0.5f);focus.update(1.3,true,true,0.8f,0.5f);
    expect(focus.update(1.5,true,true,0.4f,0.9f)==WristState::cooldown,"leaving expanded bounds closes panel");
    expect(focus.update(1.6,true,true,0.8f,0.5f)==WristState::cooldown,"cooldown rejects immediate reentry");
    expect(focus.update(1.8,true,true,0.8f,0.5f)==WristState::hidden,"cooldown returns to closed state");
    expect(focus.update(1.9,false,true,0.8f,0.5f)==WristState::hidden,"tracking loss is closed");
    expect(focus.update(0.2,true,true,0.8f,0.5f)==WristState::hidden,"clock reset clears dwell");
    PoseHistory history;history.reset(7);
    expect(history.put({{7,1},{},{},{},100}),"pose publication accepted");
    expect(history.find({7,1}).has_value(),"source transaction found");
    expect(!history.find({8,1}),"same sequence in wrong epoch rejected");
    expect(!history.put({{7,1},{},{},{},200}),"duplicate sequence rejected");
    for(uint64_t n=2;n<=65;++n)history.put({{7,n},{},{},{},static_cast<int64_t>(n)*100});
    expect(!history.find({7,1}),"overwritten historical pose rejected instead of using latest");
    expect(history.find({7,65}).has_value(),"newest source transaction retained");
    history.reset(8);expect(!history.find({7,65}),"reset invalidates all prior images");
    expect(!history.put({{7,66},{},{},{},300}),"old producer epoch rejected after reset");
    SkipGate skip;
    expect(!skip.update(Scene::cinematic,1,true,true,true),"held button cannot skip a newly entered cinematic");
    skip.update(Scene::cinematic,1,true,true,false);
    expect(skip.update(Scene::cinematic,1,true,true,true),"fresh press emits one semantic skip");
    expect(!skip.update(Scene::cinematic,1,true,true,true),"held skip does not repeat");
    skip.update(Scene::gameplay,2,true,true,false);
    expect(!skip.update(Scene::gameplay,2,true,true,true),"skip cannot fire during gameplay");
    skip.update(Scene::cinematic,3,false,true,false);
    expect(!skip.update(Scene::cinematic,3,false,true,true),"engine-unready cinematic is not force advanced");
    skip.update(Scene::cinematic,3,true,false,false);
    expect(!skip.update(Scene::cinematic,3,true,false,true),"unfocused controller cannot skip");
    WeaponSockets sockets{Pose{{},{0.1f,-0.02f,0.12f}},Pose{{},{0,0,-0.7f}},true};
    for(int n=0;n<100;++n){
        const float a=static_cast<float>(n)*0.06283185f;
        const Pose grip{{std::sin(a/2),0,0,std::cos(a/2)},{0.2f,1.1f,-0.4f}};
        const auto weapon=solveWeapon(grip,sockets,true);
        expect(weapon&&same(compose(weapon->referenceFromWeapon,sockets.weaponFromGrip).position,grip.position),"weapon authored grip retains contact throughout rotation");
        expect(weapon&&same(compose(weapon->referenceFromWeapon,sockets.weaponFromMuzzle).position,weapon->referenceFromMuzzle.position),"muzzle shares weapon transform chain");
    }
    expect(!solveWeapon({},sockets,false),"tracking loss prevents a fabricated weapon pose");
    sockets.calibrated=false;expect(!solveWeapon({},sockets,true),"uncalibrated weapon cannot silently use guessed sockets");
    {
        MountedGrip grip;
        MountedGripOwner owner{1,2,3,4};
        Pose hand{},contact{},aim{{0,.5f,0,.8660254f},{1,2,3}};
        auto step=[&](bool squeeze,uint64_t time,bool available=true){
            return grip.update(owner,hand,contact,aim,available,squeeze,time,.1f,.5f);
        };
        expect(!step(true,1).held,"entering a mount with held grip cannot acquire it");
        step(false,2);
        const auto acquired=step(true,3);
        expect(acquired.held&&acquired.acquired&&same(acquired.requestedAim.position,aim.position)
            &&near(acquired.requestedAim.orientation.y,aim.orientation.y),"fresh grip acquires without turning or moving the native pivot");
        contact.position={.02f,0,0};
        const auto moved=step(true,4);
        expect(moved.held&&!moved.acquired&&same(moved.snappedHand.position,contact.position),"held hand follows the live authored handle");
        hand.orientation={0,std::sin(.1f),0,std::cos(.1f)};
        const auto turned=step(true,5);
        expect(turned.held&&turned.requestedAim.orientation.y>acquired.requestedAim.orientation.y,"controller rotation requests native aim change");
        expect(!step(true,6,false).held&&!step(true,7).held,"tracking loss drops grip and held recovery cannot reacquire");
        step(false,8);expect(step(true,9).held,"fresh squeeze after recovery can acquire");
        owner.mount=7;expect(!step(true,10).held&&!step(true,11).held,"changed native mount rejects old grip ownership");
        step(false,12);expect(step(true,13).held,"new mount requires a released grip");
        hand.position={.7f,0,0};expect(!step(true,14).held,"excess reach releases snapped hand");
        hand.position={};expect(!step(true,15).held,"returning in reach while squeezed cannot regrab");
        step(false,16);step(true,17);expect(!step(true,300).held,"stale publication releases mounted grip");
    }
    {
        VehicleViewHeading view;
        NativeVehicleView foot{true,true,false,false,65535,0};
        NativeVehicleView jeep{true,false,true,true,25600,2.872148f};
        expect(near(view.update(.584f,.5f,foot,false,100),.584f),"ordinary view retains native heading before entry");
        expect(near(view.update(.219f,0,jeep,false,110),1.084f),"entry retains pre-action yaw and the consumed snap without HMD double composition");
        expect(near(view.update(1.301f,0,jeep,false,120),1.084f),"entry actor and chase-camera swing do not rotate fixed vehicle view");
        jeep.heading+=.2f;
        expect(near(view.update(1.501f,0,jeep,false,130),1.284f),"actual carrier yaw turns seated view");
        expect(near(view.update(1.601f,0,jeep,true,140),1.384f),"deliberate seated native look remains available");
        NativeVehicleView exiting{true,false,false,false,65535,0};
        view.update(1.601f,0,jeep,false,500);
        expect(near(view.update(2.125f,0,exiting,false,510),1.384f),"exit camera auto-orbit cannot turn the retained view");
        expect(near(view.update(2.31f,0,foot,false,600),1.384f),"native normal-action handoff preserves yaw");
        expect(near(view.update(2.51f,0,foot,true,610),1.584f),"ordinary smooth look works after exit with retained heading offset");
        view.reset();
        expect(near(view.update(-.8f,0,foot,false,700),-.8f),"new authored camera owner clears old vehicle offset");
        expect(near(view.update(-1.05f,0,foot,false,710,true),-.8f),
            "Y captures the previous view even when retail alignment precedes the occupancy status");
        expect(near(view.update(-1.2f,0,foot,false,720),-.8f),
            "releasing Y during the early normal-action entry phase retains the captured view");
        expect(near(view.update(-1.4f,0,jeep,false,730),-.8f),
            "later native vehicle occupancy cannot replace the Y-edge heading");
    }
    std::cout<<checks<<" contract checks, "<<failures<<" failures. These tests do not prove in-game or headset behavior.\n";
    return failures?1:0;
}
