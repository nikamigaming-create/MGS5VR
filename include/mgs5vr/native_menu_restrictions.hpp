#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <ostream>

namespace mgs5vr {
enum class NativeIdroidRecovery { refuse, ordinary, requestedStow };
// A missing native read cannot authorize terminal recovery. A deliberate held
// Back may stow a live lesson's terminal through its native lifecycle without
// marking the lesson complete. Automatic root-Back cleanup remains mode-zero.
inline NativeIdroidRecovery nativeIdroidRecoveryForTutorialMode(std::optional<unsigned> mode) noexcept {
    if(!mode||*mode>16)return NativeIdroidRecovery::refuse;
    return *mode==0?NativeIdroidRecovery::ordinary:NativeIdroidRecovery::requestedStow;
}
inline void writeNativeIdroidTutorialModeJson(std::ostream& out,std::optional<unsigned> mode){
    if(nativeIdroidRecoveryForTutorialMode(mode)==NativeIdroidRecovery::refuse)out<<"null";
    else out<<*mode;
}

inline bool nativeIdroidTutorialPauseReleaseAllowed(std::optional<unsigned> mode,bool pauseMenu,
    bool closing,bool requestedStow) noexcept {
    if(!mode||*mode>16||pauseMenu)return false;
    return *mode==0||(requestedStow&&closing);
}

inline bool nativeIdroidStowHelpCloseAllowed(std::optional<unsigned> mode,bool idroidOpen,
    unsigned pauseKind,bool ownedRequest) noexcept {
    return ownedRequest&&mode&&*mode==0&&!idroidOpen&&pauseKind==32;
}

struct NativeIdroidStowHelpRequest {
    uintptr_t owner{};
    uint64_t time{},revision{};
    bool acknowledged{};
};
// Only the informational Help created by an explicit owned stow may receive
// one native Confirm. A new owner/request, timeout or cancellation wins.
class NativeIdroidStowHelpLease {
public:
    void begin(uintptr_t owner,uint64_t time) noexcept {request_={owner,time,++revision_,false};}
    NativeIdroidStowHelpRequest pending(uint64_t time) const noexcept {
        return request_.owner&&request_.time&&time>=request_.time&&time-request_.time<=2000?request_:NativeIdroidStowHelpRequest{};
    }
    bool acknowledge(NativeIdroidStowHelpRequest expected,uint64_t time) noexcept {
        const auto current=pending(time);
        if(!current.owner||current.acknowledged||current.owner!=expected.owner
           ||current.time!=expected.time||current.revision!=expected.revision)return false;
        request_.acknowledged=true;return true;
    }
    bool finish(NativeIdroidStowHelpRequest expected) noexcept {
        if(!request_.acknowledged||request_.owner!=expected.owner||request_.revision!=expected.revision)return false;
        request_={};return true;
    }
private:
    NativeIdroidStowHelpRequest request_{};
    uint64_t revision_{};
};

enum class NativeCabinIdroidStow { unchanged, wait, requestClose, finishStop };
// An ordinary ACC terminal is also owned by the native seated player's
// subjective/device action. Stop alone hides the UI but leaves that action
// open; the next Menu tap then exits it instead of reopening the terminal.
// Let native Close release that owner before completing the UI shutdown.
inline NativeCabinIdroidStow nativeCabinIdroidStow(bool cabin,bool uiOnly,
    std::optional<unsigned> mode,bool open,std::optional<bool> closing,bool requested,
    bool playerVerified,std::optional<bool> subject) noexcept {
    if(!cabin||uiOnly||mode!=std::optional<unsigned>{0})return NativeCabinIdroidStow::unchanged;
    if(!open||!closing.has_value()||(!requested&&!*closing)||!playerVerified||!subject.has_value())
        return NativeCabinIdroidStow::wait;
    if(!*closing)return NativeCabinIdroidStow::requestClose;
    return *subject?NativeCabinIdroidStow::wait:NativeCabinIdroidStow::finishStop;
}

// A native tutorial borrows the menu's disabled bit. Keep the prior values
// from its ordered begin transaction; progression and other flag bits remain
// owned by the game. A later independent disable invalidates that entry.
class NativeMenuRestrictionLease {
public:
    static constexpr unsigned entries=78;
    void beginEntry(uintptr_t menu,uintptr_t tutorial,unsigned index,bool disabled) noexcept {
        if(index==0){reset();menu_=menu;tutorial_=tutorial;}
        if(!menu||!tutorial||menu!=menu_||tutorial!=tutorial_||index!=count_||index>=entries){reset();return;}
        prior_[index]=disabled;owned_[index]=true;++count_;
    }
    void independentDisable(uintptr_t menu,unsigned index) noexcept {
        if(menu==menu_&&index<entries)owned_[index]=false;
    }
    bool ready(uintptr_t menu,uintptr_t tutorial) const noexcept {
        return menu&&tutorial&&menu==menu_&&tutorial==tutorial_&&count_==entries;
    }
    std::optional<bool> prior(unsigned index) const noexcept {
        if(count_!=entries||index>=entries||!owned_[index])return {};
        return prior_[index];
    }
    void reset() noexcept {menu_=tutorial_=0;count_=0;owned_.fill(false);}
private:
    uintptr_t menu_{},tutorial_{};
    unsigned count_{};
    std::array<bool,entries> prior_{},owned_{};
};
}
