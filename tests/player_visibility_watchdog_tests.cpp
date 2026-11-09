#include "mgs5vr/player_visibility.hpp"
#include "mgs5vr/paused_rig.hpp"
#include <stdexcept>

// The owned adult D-Dog body variants use distinct resource identities.
// main3 was observed at native alpha .01 despite a valid live dog owner;
// main4 was the sole previously admitted outfit. Puppies/other actors remain
// outside this verified adult-buddy publisher path.
static_assert(mgs5vr::cabinDogCameraResource(0x84a2627e55f4658dull)); // main3, reproduced failure
static_assert(mgs5vr::cabinDogCameraResource(0x84a0e671b02ebb56ull)); // main4, prior accepted resource
static_assert(mgs5vr::cabinDogCameraResource(0x84a0d026d7938ff5ull)); // main0
static_assert(mgs5vr::cabinDogCameraResource(0x84a1d561958eb109ull)); // main1
static_assert(mgs5vr::cabinDogCameraResource(0x84a03f16dbbc41b6ull)); // main2
static_assert(!mgs5vr::cabinDogCameraResource(0x84a23d954f15159eull)); // puppy
static_assert(!mgs5vr::cabinDogCameraResource(0x84a3894e7b2bce61ull)); // puppy low-detail
static_assert(!mgs5vr::cabinDogCameraResource(0x84a1add434a40c96ull)); // unrelated dog
static_assert(!mgs5vr::cabinDogCameraResource(0));
static_assert(!mgs5vr::cabinDogCameraResource(0x84a2627e55f4658cull));

constexpr bool dogProximityPolicyIsCabinInteractionOnly(){
    const mgs5vr::CabinDogCameraContext active{0x84a2627e55f4658dull,true,true,false,false,false,false};
    if(!mgs5vr::suppressCabinDogCameraNearFade(active))return false;
    // Every exclusion independently restores native distance policy. A
    // current frame is supplied only by the existing fresh owner/epoch join.
    auto excluded=active;excluded.currentImmersiveFrame=false;
    if(mgs5vr::suppressCabinDogCameraNearFade(excluded))return false;
    excluded=active;excluded.cabinPlay=false;
    if(mgs5vr::suppressCabinDogCameraNearFade(excluded))return false;
    excluded=active;excluded.frontEnd=true;
    if(mgs5vr::suppressCabinDogCameraNearFade(excluded))return false;
    excluded=active;excluded.authoredCamera=true;
    if(mgs5vr::suppressCabinDogCameraNearFade(excluded))return false;
    excluded=active;excluded.scriptedDemo=true;
    if(mgs5vr::suppressCabinDogCameraNearFade(excluded))return false;
    excluded=active;excluded.nativeGamepad=true;
    if(mgs5vr::suppressCabinDogCameraNearFade(excluded))return false;
    for(const auto resource:{0ull,0x84a23d954f15159eull,0x84a1add434a40c96ull,0x84a2627e55f4658cull}){
        excluded=active;excluded.resource=resource;
        if(mgs5vr::suppressCabinDogCameraNearFade(excluded))return false;
    }
    return true;
}
static_assert(dogProximityPolicyIsCabinInteractionOnly());
constexpr bool dogProximityPolicyAdmitsOnlyInteractiveTitle(){
    mgs5vr::CabinDogCameraContext title{0x84a2627e55f4658dull,true,false,true,false,false,false};
    title.nativeTitleCabin=title.openingBackend=title.openingSelector=true;
    if(!mgs5vr::suppressCabinDogCameraNearFade(title))return false;
    auto excluded=title;excluded.nativeTitleCabin=false;
    if(mgs5vr::suppressCabinDogCameraNearFade(excluded))return false;
    excluded=title;excluded.openingBackend=false;
    if(mgs5vr::suppressCabinDogCameraNearFade(excluded))return false;
    excluded=title;excluded.openingSelector=false;
    if(mgs5vr::suppressCabinDogCameraNearFade(excluded))return false;
    excluded=title;excluded.openingNativeMenu=true;
    if(mgs5vr::suppressCabinDogCameraNearFade(excluded))return false;
    excluded=title;excluded.loading=true;
    if(mgs5vr::suppressCabinDogCameraNearFade(excluded))return false;
    excluded=title;excluded.avatarEditor=true;
    if(mgs5vr::suppressCabinDogCameraNearFade(excluded))return false;
    excluded=title;excluded.currentImmersiveFrame=false;
    if(mgs5vr::suppressCabinDogCameraNearFade(excluded))return false;
    excluded=title;excluded.authoredCamera=true;
    if(mgs5vr::suppressCabinDogCameraNearFade(excluded))return false;
    excluded=title;excluded.scriptedDemo=true;
    if(mgs5vr::suppressCabinDogCameraNearFade(excluded))return false;
    excluded=title;excluded.nativeGamepad=true;
    return !mgs5vr::suppressCabinDogCameraNearFade(excluded);
}
static_assert(dogProximityPolicyAdmitsOnlyInteractiveTitle());

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
int main() {
    using namespace mgs5vr;
    const auto require=[](bool value,const char* message){
        if(!value)throw std::runtime_error(message);
    };
    const PausedRigOwner nativeBody{12,34,2,7,1000,3};
    HeadCameraSample frame{};
    frame.applied=true;frame.stereoTracked=true;frame.playerOwner=34;
    frame.activation=2;frame.menuGeneration=4;frame.menuOpen=true;frame.menuIdroid=true;
    frame.controllers.referenceEpoch=7;frame.controllers.presentationEpoch=9;
    frame.controllers.predictedXrTime=1234;frame.controllers.presentationFocused=true;
    frame.controllers.handheldMenus=true;frame.sampleTime=1001;

    // Reproduced sequence: native body/A8 completed, then the opening camera
    // observes a new menu generation. A body-only rebuild would detach the
    // handset. Wait for native publication without advancing animation.
    require(!mayRefreshPausedRig(nativeBody,12,frame,1001),"fresh handheld opening rebuilt body after device");
    frame.sampleTime=1150;
    require(!mayRefreshPausedRig(nativeBody,12,frame,1150),"freshness boundary admitted premature body rebuild");
    frame.sampleTime=1151;
    require(mayRefreshPausedRig(nativeBody,12,frame,1151),"existing long-paused refresh was lost");

    // The opposite boundary must preserve the already-fixed tracked stow;
    // native closing alone never authorizes suppressing the handset.
    frame.sampleTime=1001;frame.menuOpen=false;frame.menuIdroid=false;
    require(mayRefreshPausedRig(nativeBody,12,frame,1001),"close boundary no longer refreshes immediately");
    frame.menuOpen=true;
    require(mayRefreshPausedRig(nativeBody,12,frame,1001),"spatial Pause/Help boundary was deferred");
    frame.menuIdroid=true;frame.controllers.handheldMenus=false;
    require(mayRefreshPausedRig(nativeBody,12,frame,1001),"explicit spatial iDroid boundary was deferred");

    // Once normal native publication reaches the menu generation, ordinary
    // timeout behavior is unchanged. No repeated boundary refresh or latch.
    frame.controllers.handheldMenus=true;frame.menuGeneration=3;
    require(!mayRefreshPausedRig(nativeBody,12,frame,1001),"fresh existing menu unexpectedly rebuilt");
    frame.sampleTime=1151;
    require(mayRefreshPausedRig(nativeBody,12,frame,1151),"ordinary paused handheld refresh was lost");

    // Deferral must not weaken the original ownership, focus or clock guards.
    frame.playerOwner=35;
    require(!mayRefreshPausedRig(nativeBody,12,frame,1151),"different player admitted paused refresh");
    frame.playerOwner=34;frame.controllers.referenceEpoch=8;
    require(!mayRefreshPausedRig(nativeBody,12,frame,1151),"different reference space admitted paused refresh");
    frame.controllers.referenceEpoch=7;frame.controllers.presentationFocused=false;
    require(!mayRefreshPausedRig(nativeBody,12,frame,1151),"unfocused source admitted paused refresh");
    frame.controllers.presentationFocused=true;frame.sampleTime=1000;
    require(!mayRefreshPausedRig(nativeBody,12,frame,1151),"stale tracked frame admitted paused refresh");
    frame.sampleTime=999;
    require(!mayRefreshPausedRig(nativeBody,12,frame,999),"reversed native clock admitted paused refresh");
    return 0;
}
