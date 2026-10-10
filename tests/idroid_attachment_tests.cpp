#include "mgs5vr/idroid_attachment.hpp"
#include "mgs5vr/idroid_hand_use.hpp"
#include <stdexcept>

int main(){
    using namespace mgs5vr;
    const auto require=[](bool value){if(!value)throw std::runtime_error("iDroid body publication ordering failed");};
    require(idroidHandUseSourceFresh(1,2,3,100,2,3,250));
    require(!idroidHandUseSourceFresh(1,2,3,100,2,3,251));
    require(!idroidHandUseSourceFresh(1,2,3,100,4,3,110));
    require(!idroidHandUseSourceFresh(1,2,3,100,2,4,110));
    require(!idroidHandUseSourceFresh(1,2,3,100,2,3,99));
    require(!idroidHandUseSourceFresh(0,2,3,100,2,3,110));
    const IdroidBodyStamp body{1,2,3,4,5,6,7,8,9,10,11,true};
    require(idroidBodyCompletedBefore(body,100,body,101));
    require(!idroidBodyCompletedBefore(body,102,body,101));
    require(!idroidBodyCompletedBefore(body,0,body,101));
    // Same memory addresses cannot admit another rig transaction or epoch.
    auto changed=body;++changed.rig;require(!idroidBodyCompletedBefore(body,100,changed,101));
    changed=body;++changed.tracking;require(!sameIdroidBodyPublication(body,changed));
    changed=body;++changed.activation;require(!sameIdroidBodyPublication(body,changed));
    changed=body;++changed.referenceEpoch;require(!sameIdroidBodyPublication(body,changed));
    changed=body;++changed.presentationEpoch;require(!sameIdroidBodyPublication(body,changed));
    changed=body;++changed.menuGeneration;require(!sameIdroidBodyPublication(body,changed));
    changed=body;++changed.predictedXrTime;require(!sameIdroidBodyPublication(body,changed));
    changed=body;changed.player=2;require(!sameIdroidBodyPublication(body,changed));
    changed=body;changed.palette=5;require(!sameIdroidBodyPublication(body,changed));
    changed=body;changed.driver=4;require(!sameIdroidBodyPublication(body,changed));
    changed=body;changed.model=3;require(!sameIdroidBodyPublication(body,changed));
    changed=body;changed.focused=false;require(!sameIdroidBodyPublication(body,changed));
    changed=body;changed.rig=0;require(!sameIdroidBodyPublication(changed,changed));
    changed=body;changed.predictedXrTime=0;require(!sameIdroidBodyPublication(changed,changed));
    // Native handle/local-index ownership is independent of a still-valid body.
    const NativeIdroidBinding native{1,2,3,4,5,6,7,8,0,0,(1u<<16)|(499u<<5)|17u};
    require(sameNativeIdroidBinding(native,native));
    auto replaced=native;replaced.model=9;require(!sameNativeIdroidBinding(native,replaced));
    replaced=native;replaced.palette=9;require(!sameNativeIdroidBinding(native,replaced));
    replaced=native;replaced.component=9;require(!sameNativeIdroidBinding(native,replaced));
    replaced=native;replaced.point=9;require(!sameNativeIdroidBinding(native,replaced));
    replaced=native;++replaced.localIndex;require(!sameNativeIdroidBinding(native,replaced));
    replaced=native;++replaced.playerIndex;require(!sameNativeIdroidBinding(native,replaced));
    replaced=native;replaced.handle=0xffffffe0u;require(!sameNativeIdroidBinding(replaced,replaced));
    replaced=native;replaced.row=0;require(!sameNativeIdroidBinding(replaced,replaced));
    auto leased=native;leased.component=leased.row=0;leased.equipmentLease=1;
    require(sameNativeIdroidBinding(leased,leased));
    replaced=leased;replaced.equipmentLease=2;require(!sameNativeIdroidBinding(leased,replaced));
    replaced=leased;replaced.equipmentLease=0;require(!sameNativeIdroidBinding(replaced,replaced));
    replaced=leased;replaced.row=4;require(!sameNativeIdroidBinding(replaced,replaced));
    // Named root in a LOCAL native palette: transform its offset through a
    // rotated/translated binding root. WORLD native palettes must not receive
    // that root a second time. An unrelated binding flag does not mean LOCAL.
    constexpr std::array<float,16> bone{1,0,0,0, 0,1,0,0, 0,0,1,0, 1,2,3,1};
    constexpr std::array<float,16> root{0,1,0,0, -1,0,0,0, 0,0,1,0, 10,20,30,1};
    constexpr std::array<float,16> world{0,1,0,0, -1,0,0,0, 0,0,1,0, 8,21,33,1};
    require(nativeIdroidWorldMatrix(bone,root,1)==world);
    require(nativeIdroidWorldMatrix(bone,root,0)==bone);
    require(nativeIdroidWorldMatrix(bone,root,2)==bone);
    // A paused body refresh without its own A8 cannot inherit a prior display.
    changed=body;++changed.rig;
    require(!idroidBodyCompletedBefore(body,100,changed,103));
    changed=body;++changed.menuGeneration;
    require(!idroidBodyCompletedBefore(body,100,changed,103));
    // A LEFT grip away from the real support socket keeps the device/menu.
    IdroidHandUseController hand;
    IdroidHandUseInput use{true,true,true,true,true,false,false,.45f,.10f,.30f,1,2,3};
    require(hand.update(use,100)==IdroidHandUse::device);
    use.contactDistance=.08f;require(hand.update(use,110)==IdroidHandUse::support);
    use.contactDistance=.20f;require(hand.update(use,120)==IdroidHandUse::support);
    use.supportHeld=false;require(hand.update(use,130)==IdroidHandUse::device);
    use.supportHeld=true;use.contactDistance=.08f;require(hand.update(use,140)==IdroidHandUse::support);
    use.contactTracked=false;require(hand.update(use,150)==IdroidHandUse::device);
    use.contactTracked=true;use.contactDistance=.45f;use.reloadChord=true;
    require(hand.update(use,160)==IdroidHandUse::reload);
    // The effective tap is delivered on release; it must retain ownership
    // while awaiting an actual native reload, not a fixed animation duration.
    use.reloadChord=false;require(hand.update(use,170)==IdroidHandUse::reload);hand.reloadRequested(170);
    use.manipulating=true;require(hand.update(use,180)==IdroidHandUse::reload);
    require(hand.update(use,900)==IdroidHandUse::reload);
    use.manipulating=false;require(hand.update(use,910)==IdroidHandUse::device);
    // A full magazine/non-acknowledged request returns instead of hiding forever.
    use.reloadChord=true;require(hand.update(use,1000)==IdroidHandUse::reload);hand.reloadRequested(1000);
    use.reloadChord=false;require(hand.update(use,1200)==IdroidHandUse::reload);
    require(hand.update(use,1751)==IdroidHandUse::device);
    // A manual close or new native player cannot reopen a former terminal.
    use.reloadChord=true;require(hand.update(use,1800)==IdroidHandUse::reload);
    use.open=false;require(hand.update(use,1810)==IdroidHandUse::device);
    use.open=true;use.reloadChord=false;require(hand.update(use,1820)==IdroidHandUse::device);
    use.contactDistance=.08f;require(hand.update(use,1830)==IdroidHandUse::support);
    ++use.player;require(hand.update(use,1840)==IdroidHandUse::device);
    use.eligible=false;require(hand.update(use,1850)==IdroidHandUse::device);
}
