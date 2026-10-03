#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <ostream>

namespace mgs5vr {
enum class NativeIdroidRecovery { refuse, ordinary, completedGuideOnly };
// A missing native read is not evidence that a tutorial is inactive. Active
// modes still require the existing native completed-guide checks before Stop.
inline NativeIdroidRecovery nativeIdroidRecoveryForTutorialMode(std::optional<unsigned> mode) noexcept {
    if(!mode||*mode>16)return NativeIdroidRecovery::refuse;
    return *mode==0?NativeIdroidRecovery::ordinary:NativeIdroidRecovery::completedGuideOnly;
}
inline void writeNativeIdroidTutorialModeJson(std::ostream& out,std::optional<unsigned> mode){
    if(nativeIdroidRecoveryForTutorialMode(mode)==NativeIdroidRecovery::refuse)out<<"null";
    else out<<*mode;
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
