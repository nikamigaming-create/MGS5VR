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
    std::array<Vec3,7> offsets{};offsets[0]=cabinPlayerPose().position;
    const auto homes=cabinTapeHomes();for(size_t i=0;i<6;++i)offsets[i+1]=homes[i].position;
    return offsets;
}

std::array<float,7> openingPropScales() noexcept {
    return {{1,1,1,1,1,1,1}};
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
        return std::filesystem::is_regular_file(configured(L"cassette_fmdl",
                   L"retail-assets\\Assets\\tpp\\item\\cct\\Scenes\\cct0_main1_def.fmdl"))
            &&std::filesystem::is_regular_file(configured(L"cassette_diffuse_dds",
                   L"retail-assets\\Assets\\tpp\\item\\cct\\Pictures\\cct0_main1_def_c00_bsm.dds"));
    }catch(...){return false;}
}

void OpeningSelector::reset() noexcept {
    phase_=Phase::idle;
    target_=-1;
    confirmUntil_=0;
    routeUntil_=pulseUntil_=0;direction_=OpeningPulse::none;
    initialized_=false;
    origin_={};
    props_.reset();
}

OpeningSelectorFrame OpeningSelector::update(bool title,bool assetsAvailable,bool dogAvailable,Pose head,
    const std::array<TrackedHand,2>& hands,uint64_t now,std::optional<Pose> anchoredOrigin,OpeningNativeFocus focus,CabinLayout layout){
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
    frame.continueReady=focus.ready&&std::find(focus.actions.begin(),focus.actions.begin()+focus.count,2)
        !=focus.actions.begin()+focus.count;

    std::array<CabinHand,2> cabinHands{};
    for(size_t side=0;side<2;++side){const auto& hand=hands[side];
        cabinHands[side]={compose(inverse(origin_),hand.grip),hand.gripTracked,hand.squeeze,hand.trigger,
            compose(inverse(origin_),Pose{{},hand.indexTip}).position,hand.indexTipTracked};}
    frame.bench=props_.update(cabinHands,now,layout);
    const int hovered=frame.bench.hovered;
    if(phase_==Phase::idle&&frame.bench.activated>=0){
        const int inserted=frame.bench.activated;
        constexpr std::array<uint8_t,6> nativeActions{2,3,5,4,7,8};
        if(!focus.ready||std::find(focus.actions.begin(),focus.actions.begin()+focus.count,nativeActions[inserted])
           ==focus.actions.begin()+focus.count){
            frame.blocked=true;
            frame.selection=inserted;
            frame.action=static_cast<OpeningTapeAction>(inserted);
            return frame;
        }
        target_=inserted;
        // Hold the native confirm long enough to cross at least one full
        // simulator/native input transaction, but never long enough to
        // become a second menu selection if the save-data prompt appears.
        phase_=Phase::navigate;routeUntil_=now+5000;
    }

    if(phase_!=Phase::idle&&phase_!=Phase::confirm){
        constexpr std::array<uint8_t,6> nativeActions{2,3,5,4,7,8};
        if(!focus.ready||now>=routeUntil_||frame.bench.inserted!=target_){
            frame.blocked=true;phase_=Phase::idle;target_=-1;
        }else if(phase_==Phase::release){
            // Let the native input see a release, and observe the resulting
            // focus before generating another edge. Never count guessed rows.
            if(now>=pulseUntil_+140&&focus.row!=priorRow_)phase_=Phase::navigate;
        }else if(pulseUntil_&&now<pulseUntil_)frame.pulse=direction_;
        else {
            const auto end=focus.actions.begin()+focus.count;
            const auto desired=std::find(focus.actions.begin(),end,nativeActions[target_]);
            if(desired==end){frame.blocked=true;phase_=Phase::idle;target_=-1;}
            else if(focus.row==static_cast<size_t>(desired-focus.actions.begin())){
                phase_=Phase::confirm;confirmUntil_=now+160;
            }else{
                priorRow_=focus.row;direction_=focus.row<desired-focus.actions.begin()?OpeningPulse::down:OpeningPulse::up;
                pulseUntil_=now+80;frame.pulse=direction_;phase_=Phase::release;
            }
        }
        if(phase_==Phase::release&&now<pulseUntil_)frame.pulse=direction_;
    }

    frame.selection=phase_==Phase::idle?hovered:target_;
    if(frame.selection>=0)frame.action=static_cast<OpeningTapeAction>(frame.selection);
    if(phase_==Phase::confirm){
        constexpr std::array<uint8_t,6> nativeActions{2,3,5,4,7,8};
        if(now<confirmUntil_&&focus.ready&&focus.actions[focus.row]==nativeActions[target_]
           &&frame.bench.inserted==target_)frame.pulse=OpeningPulse::confirm;
        else {phase_=Phase::idle;target_=-1;confirmUntil_=0;}
    }
    return frame;
}

OpeningSelectorFrame OpeningSelector::updateCabin(bool active,Pose head,
    const std::array<TrackedHand,2>& hands,uint64_t now,std::optional<Pose> anchoredOrigin,CabinLayout layout){
    OpeningSelectorFrame frame{};
    if(!active||!valid(head))return frame;
    if(!initialized_){
        initialized_=true;
        origin_=head;
    }
    frame.cabinActive=true;
    if(anchoredOrigin&&valid(*anchoredOrigin))origin_=*anchoredOrigin;
    frame.origin=origin_;
    std::array<CabinHand,2> cabinHands{};
    for(size_t side=0;side<2;++side){const auto& hand=hands[side];
        cabinHands[side]={compose(inverse(origin_),hand.grip),hand.gripTracked,hand.squeeze,hand.trigger,
            compose(inverse(origin_),Pose{{},hand.indexTip}).position,hand.indexTipTracked};}
    frame.bench=props_.update(cabinHands,now,layout);
    frame.selection=frame.bench.hovered;
    return frame;
}

}
