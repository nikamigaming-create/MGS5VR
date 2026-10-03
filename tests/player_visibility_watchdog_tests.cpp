#include "mgs5vr/player_visibility.hpp"
#include "mgs5vr/paused_rig.hpp"

static_assert(!mgs5vr::shouldRestoreStalePlayerVisibility(249, false, false, false));
static_assert(mgs5vr::shouldRestoreStalePlayerVisibility(250, false, false, false));
static_assert(mgs5vr::shouldRestoreStalePlayerVisibility(500, false, false, true));
static_assert(!mgs5vr::shouldRestoreStalePlayerVisibility(500, true, false, false));
static_assert(mgs5vr::shouldRestoreStalePlayerVisibility(500, true, true, false));
static_assert(!mgs5vr::shouldRestoreStalePlayerVisibility(500, true, true, true));

constexpr bool opacityCommandPreservesNativeState() {
    // A native transition may start with any draw/dirty/hidden bit pattern.
    // The arm opacity command must retain every unrelated bit, including
    // native-hidden state, and stay idempotent until native consumption.
    for(unsigned flags=0;flags<256;++flags){
        const auto command=mgs5vr::firstPersonOpaquePublication(static_cast<uint8_t>(flags));
        if(command.opacity!=255||(command.flags&0x20u)==0
           ||(command.flags&~0x20u)!=(flags&~0x20u))return false;
        const auto repeated=mgs5vr::firstPersonOpaquePublication(command.flags);
        if(repeated.opacity!=command.opacity||repeated.flags!=command.flags)return false;
    }
    return true;
}
static_assert(opacityCommandPreservesNativeState());
static_assert(mgs5vr::nativeVisibilityRecordAddress(0x1000,3,100,100)==0x1000);
static_assert(mgs5vr::nativeVisibilityRecordAddress(0x1000,3,100,102)==0x1100);
static_assert(!mgs5vr::nativeVisibilityRecordAddress(0x1000,3,100,99));
static_assert(!mgs5vr::nativeVisibilityRecordAddress(0x1000,3,100,103));
static_assert(!mgs5vr::nativeVisibilityRecordAddress(0x1000,33,100,100));
static_assert(!mgs5vr::nativeVisibilityRecordAddress(0,3,100,100));
static_assert(!mgs5vr::nativeVisibilityRecordAddress((std::numeric_limits<uintptr_t>::max)()-16,2,100,101));
constexpr bool visibilityOwnerIsExact(){
    const mgs5vr::NativeVisibilityIdentity owned{1,2,3,4,5,6,0x1000,0x1080,101,100,3};
    if(!mgs5vr::sameNativeVisibilityIdentity(owned,owned))return false;
    auto changed=owned;changed.character=9;
    if(mgs5vr::sameNativeVisibilityIdentity(owned,changed))return false;
    changed=owned;changed.model=9;
    if(mgs5vr::sameNativeVisibilityIdentity(owned,changed))return false;
    changed=owned;changed.pool=9;
    if(mgs5vr::sameNativeVisibilityIdentity(owned,changed))return false;
    changed=owned;changed.record=0x1000;
    if(mgs5vr::sameNativeVisibilityIdentity(changed,changed))return false;
    changed=owned;changed.owner=0;
    return !mgs5vr::sameNativeVisibilityIdentity(changed,changed);
}
static_assert(visibilityOwnerIsExact());
constexpr bool visibilityTraceIsBounded(){
    mgs5vr::VisibilityBoundaryBudget budget;
    if(budget.admit(1,2,0,false,1))return false;
    uint64_t source=2;
    for(uint64_t boundary=1;boundary<=4;++boundary){
        for(unsigned i=0;i<12;++i){
            if(!budget.admit(1,2,boundary,(boundary&1u)!=0,source))return false;
            if(budget.admit(1,2,boundary,(boundary&1u)!=0,source))return false;
            ++source;
        }
        if(budget.admit(1,2,boundary,(boundary&1u)!=0,source++))return false;
    }
    return budget.complete()&&!budget.admit(1,2,5,true,source);
}
static_assert(visibilityTraceIsBounded());
constexpr bool visibilityTraceRejectsUnownedOrStaleSources(){
    mgs5vr::VisibilityBoundaryBudget budget;
    if(budget.admit(0,2,0,false,1)||budget.admit(1,0,0,false,1))return false;
    if(budget.admit(1,2,0,false,1))return false;
    if(!budget.admit(1,2,1,true,2))return false;
    if(budget.admit(1,2,0,false,3))return false; // Older menu generation.
    if(budget.admit(1,2,1,true,2))return false; // Older source.
    if(budget.admit(9,2,1,true,4))return false; // Owner replacement is a new baseline.
    if(budget.admit(9,3,1,true,5))return false; // Activation replacement cannot inherit the window.
    return !budget.admit(9,3,1,true,6);
}
static_assert(visibilityTraceRejectsUnownedOrStaleSources());
constexpr bool failedVisibilityReadsAreBounded(){
    mgs5vr::VisibilityInspectionBudget budget;
    if(budget.reserve(1000))return false;
    budget.arm(1000);
    // The ownership read after every reservation can fail. That outcome must
    // never defer the limit or reserve another uncounted attempt.
    for(unsigned i=0;i<2048;++i)if(!budget.reserve(1001))return false;
    return !budget.reserve(1001)&&!budget.reserve(1002);
}
static_assert(failedVisibilityReadsAreBounded());
constexpr bool visibilityExpiryDoesNotNeedAMenuBoundary(){
    mgs5vr::VisibilityInspectionBudget budget;budget.arm(1000);
    if(budget.reserve(999)||!budget.reserve(46000)||budget.reserve(46001))return false;
    budget.arm(46001); // Repeated source/failed-owner arming cannot extend it.
    return !budget.reserve(46002);
}
static_assert(visibilityExpiryDoesNotNeedAMenuBoundary());
constexpr mgs5vr::PausedRigOwner menuBefore{12,34,2,7,1000,3};
static_assert(mgs5vr::hasOwnedRigMenuBoundary(menuBefore,{12,34,2,7,1010,4}));
static_assert(!mgs5vr::hasOwnedRigMenuBoundary(menuBefore,{12,34,2,7,1010,3}));
static_assert(!mgs5vr::hasOwnedRigMenuBoundary(menuBefore,{12,34,2,7,1010,2}));
static_assert(!mgs5vr::hasOwnedRigMenuBoundary(menuBefore,{13,34,2,7,1010,4}));
static_assert(!mgs5vr::hasOwnedRigMenuBoundary(menuBefore,{12,35,2,7,1010,4}));
static_assert(!mgs5vr::hasOwnedRigMenuBoundary(menuBefore,{12,34,3,7,1010,4}));
static_assert(!mgs5vr::hasOwnedRigMenuBoundary(menuBefore,{12,34,2,8,1010,4}));
static_assert(!mgs5vr::hasOwnedRigMenuBoundary({},{12,34,2,7,1010,4}));
int main() { return 0; }
