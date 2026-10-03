#include "mgs5vr/idroid_attachment.hpp"
#include <stdexcept>

int main(){
    using namespace mgs5vr;
    const auto require=[](bool value){if(!value)throw std::runtime_error("iDroid body publication ordering failed");};
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
}
