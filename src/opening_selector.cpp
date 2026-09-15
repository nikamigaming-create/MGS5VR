#include "mgs5vr/opening_selector.hpp"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <filesystem>

namespace mgs5vr {

std::array<Vec3,7> openingPropOffsets() noexcept {
    // The native title camera's LOCAL basis is also the physical cabin basis.
    // Keep the rack in front of the authored bench, high enough to be read in
    // the first eye capture and far enough away to avoid a hand-sized overlay.
    return {{
        { .60f, -.22f, -1.05f }, // radio/deck
        {-.12f, -.48f, -1.00f }, // Continue
        { .11f, -.48f, -1.00f }, // Download MGSV: GZ Save Data
        { .34f, -.48f, -1.00f }, // Metal Gear Online
        {-.12f, -.67f, -1.00f }, // Options
        { .11f, -.67f, -1.00f }, // Delete Save Data
        { .34f, -.67f, -1.00f }  // Quit Game
    }};
}

std::array<float,7> openingPropScales() noexcept {
    return {{1.45f,1.75f,1.75f,1.75f,1.75f,1.75f,1.75f}};
}

bool openingPropsAvailable() noexcept {
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
    upRemaining_=downRemaining_=0;
    nextPulseAt_=0;
    previousTrigger_=false;
}

OpeningSelectorFrame OpeningSelector::update(bool title,bool assetsAvailable,Pose head,
    const TrackedHand& hand,uint64_t now){
    OpeningSelectorFrame frame{};
    const bool pressed=hand.trigger>.68f;
    if(!title||!assetsAvailable||!valid(head)||!hand.gripTracked||!valid(hand.grip)){
        // Remember a held trigger while disabled so re-entering the title
        // cannot turn an old controller hold into a new native action.
        reset();previousTrigger_=pressed;return frame;
    }
    frame.active=true;

    const auto relative=compose(inverse(head),hand.grip).position;
    const auto offsets=openingPropOffsets();
    int hovered=-1;float nearest=.15f*.15f;
    for(size_t i=1;i<offsets.size();++i){
        const auto delta=relative-offsets[i];
        const float distance=dot(delta,delta);
        if(std::isfinite(distance)&&distance<=nearest){nearest=distance;hovered=static_cast<int>(i-1);}
    }
    const bool rising=pressed&&!previousTrigger_;previousTrigger_=pressed;
    if(phase_==Phase::idle&&rising&&hovered>=0){
        target_=hovered;
        // The screenshot-verified native English Title list has six rows.
        // Reset to the top first, so the physical label maps to the native
        // action even if a previous title input left another row selected.
        phase_=Phase::resetToTop;upRemaining_=6;
        downRemaining_=static_cast<unsigned>(target_);nextPulseAt_=now;
    }

    frame.selection=phase_==Phase::idle?hovered:target_;
    if(frame.selection>=0)frame.action=static_cast<OpeningTapeAction>(frame.selection);
    if(phase_!=Phase::idle&&now>=nextPulseAt_){
        if(phase_==Phase::resetToTop&&upRemaining_){
            --upRemaining_;frame.pulse=OpeningPulse::up;nextPulseAt_=now+90;
            if(!upRemaining_)phase_=Phase::moveDown;
        }else if(phase_==Phase::moveDown&&downRemaining_){
            --downRemaining_;frame.pulse=OpeningPulse::down;nextPulseAt_=now+90;
            if(!downRemaining_)phase_=Phase::confirm;
        }else if(phase_==Phase::confirm){
            frame.pulse=OpeningPulse::confirm;phase_=Phase::idle;nextPulseAt_=now+300;
        }else if(phase_==Phase::resetToTop){
            phase_=Phase::moveDown;nextPulseAt_=now;
        }else if(phase_==Phase::moveDown){
            phase_=Phase::confirm;nextPulseAt_=now;
        }
    }
    return frame;
}

}
