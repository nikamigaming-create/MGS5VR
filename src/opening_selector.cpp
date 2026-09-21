#include "mgs5vr/opening_selector.hpp"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <atomic>

namespace mgs5vr {

std::array<Vec3,7> openingPropOffsets() noexcept {
    // The native title camera's LOCAL basis is also the physical cabin basis.
    // Keep the rack on the authored bench, below and beyond the face plane so
    // the tapes remain room props while the Continue target stays reachable.
    return {{
        { .60f, -.30f, -1.40f }, // radio/deck
        {-.12f, -.58f, -1.35f }, // Continue
        { .11f, -.58f, -1.35f }, // Download MGSV: GZ Save Data
        { .34f, -.58f, -1.35f }, // Metal Gear Online
        {-.12f, -.76f, -1.35f }, // Options
        { .11f, -.76f, -1.35f }, // Delete Save Data
        { .34f, -.76f, -1.35f }  // Quit Game
    }};
}

std::array<float,7> openingPropScales() noexcept {
    return {{1.45f,1.75f,1.75f,1.75f,1.75f,1.75f,1.75f}};
}

bool openingCabinEnabled() noexcept {
    static const auto ini=[] {
        std::array<wchar_t,32768> module{};
        const auto length=GetModuleFileNameW(nullptr,module.data(),static_cast<DWORD>(module.size()));
        if(!length||length>=module.size())return std::filesystem::path{};
        return std::filesystem::path(module.data()).parent_path()/L"mgs5vr.ini";
    }();
    static std::atomic_bool enabled{};
    static std::atomic_uint64_t checked{};
    const auto now=GetTickCount64();auto prior=checked.load();
    if(!ini.empty()&&(!prior||now-prior>=500)&&checked.compare_exchange_strong(prior,now))
        enabled.store(GetPrivateProfileIntW(L"opening",L"interactive_cabin",0,ini.c_str())==1);
    return enabled.load();
}

bool openingPropsAvailable() noexcept {
    if(!openingCabinEnabled())return false;
    try{
        std::array<wchar_t,32768> module{};
        const auto length=GetModuleFileNameW(nullptr,module.data(),static_cast<DWORD>(module.size()));
        if(!length||length>=module.size())return false;
        const auto root=std::filesystem::path(module.data()).parent_path();
        const auto configured=[&](const wchar_t* key,const wchar_t* fallback){
            std::array<wchar_t,32768> value{};
            const auto count=GetPrivateProfileStringW(L"opening",key,fallback,value.data(),
                static_cast<DWORD>(value.size()),(root/L"mgs5vr.ini").c_str());
            if(!count||count>=value.size())return std::filesystem::path{};
            std::filesystem::path path(value.data());return path.is_absolute()?path:root/path;
        };
        return std::filesystem::is_regular_file(configured(L"radio_fmdl",
                   L"retail-assets\\Assets\\tpp\\item\\rdi\\Scenes\\rdi0_main0_def.fmdl"))
            &&std::filesystem::is_regular_file(configured(L"radio_diffuse_dds",
                   L"retail-assets\\Assets\\tpp\\item\\rdi\\Pictures\\rdi0_main0_def_c00_bsm.dds"))
            &&std::filesystem::is_regular_file(configured(L"cassette_fmdl",
                   L"retail-assets\\Assets\\tpp\\item\\cct\\Scenes\\cct0_main1_def.fmdl"))
            &&std::filesystem::is_regular_file(configured(L"cassette_diffuse_dds",
                   L"retail-assets\\Assets\\tpp\\item\\cct\\Pictures\\cct0_main1_def_c00_bsm.dds"));
    }catch(...){return false;}
}

void OpeningSelector::reset() noexcept {
    phase_=Phase::idle;
    target_=-1;
    confirmUntil_=0;
    previousTrigger_=false;
    initialized_=false;
    origin_={};
}

OpeningSelectorFrame OpeningSelector::update(bool title,bool assetsAvailable,bool dogAvailable,Pose head,
    const std::array<TrackedHand,2>& hands,uint64_t now,std::optional<Pose> anchoredOrigin){
    OpeningSelectorFrame frame{};
    if(!title||!assetsAvailable){
        // Remember a held trigger while disabled so re-entering the title
        // cannot turn an old controller hold into a new native action.
        reset();
        return frame;
    }
    if(!valid(head))return frame;
    if(!initialized_){
        initialized_=true;
        origin_=head;
    }
    if(anchoredOrigin&&valid(*anchoredOrigin))origin_=*anchoredOrigin;
    frame.active=true;
    frame.origin=origin_;
    // A mesh on disk is not a native D-Dog.  Petting is owned by
    // animal_interaction.cpp and is accepted only when TppBuddyDog2 exposes
    // real bones.  This selector must never turn an authored pose into a
    // claimed pet or gate Continue on a synthetic state.
    (void)dogAvailable;
    frame.continueReady=true;

    int handIndex=-1;
    Vec3 relative{};
    const auto offsets=openingPropOffsets();
    int hovered=-1;float nearest=.15f*.15f;
    for(unsigned side=0;side<2;++side){
        const auto& hand=hands[side];
        if(!hand.gripTracked||!valid(hand.grip))continue;
        const auto handRelative=compose(inverse(origin_),hand.grip).position;
        for(size_t i=1;i<offsets.size();++i){
            const auto delta=handRelative-offsets[i];
            const float distance=dot(delta,delta);
            if(std::isfinite(distance)&&distance<=nearest){
                nearest=distance;hovered=static_cast<int>(i-1);handIndex=static_cast<int>(side);relative=handRelative;
            }
        }
    }
    (void)relative;
    const bool pressed=handIndex>=0&&hands[static_cast<size_t>(handIndex)].trigger>.68f;
    const bool rising=pressed&&!previousTrigger_;previousTrigger_=pressed;
    if(phase_==Phase::idle&&rising&&hovered>=0){
        // The native title enters with CONTINUE focused.  Sending a guessed
        // six-Up/six-Down traversal was what routed a bad title state into
        // the GZ/FOB screens.  Keep the only proven route bounded to that
        // initial focus and refuse every other action until its native focus
        // reader is verified.
        if(hovered!=0||!frame.continueReady){
            frame.blocked=true;
            frame.selection=hovered;
            if(hovered>=0)frame.action=static_cast<OpeningTapeAction>(hovered);
            return frame;
        }
        target_=hovered;
        // Hold the native confirm long enough to cross at least one full
        // simulator/native input transaction, but never long enough to
        // become a second menu selection if the save-data prompt appears.
        phase_=Phase::confirm;
        confirmUntil_=now+160;
    }

    frame.selection=phase_==Phase::idle?hovered:target_;
    if(frame.selection>=0)frame.action=static_cast<OpeningTapeAction>(frame.selection);
    if(phase_==Phase::confirm){
        if(now<confirmUntil_)frame.pulse=OpeningPulse::confirm;
        else {phase_=Phase::idle;target_=-1;confirmUntil_=0;}
    }
    return frame;
}

OpeningSelectorFrame OpeningSelector::updateCabin(bool active,Pose head,
    const std::array<TrackedHand,2>& hands,uint64_t now){
    OpeningSelectorFrame frame{};
    if(!active||!valid(head))return frame;
    if(!initialized_){
        initialized_=true;
        origin_=head;
    }
    frame.cabinActive=true;
    frame.origin=origin_;
    (void)hands;(void)now;
    return frame;
}

}
