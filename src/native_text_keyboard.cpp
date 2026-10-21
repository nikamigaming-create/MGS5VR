#include "mgs5vr/native_text_input.hpp"

namespace mgs5vr {
bool nativeTextKeyEnabled(size_t key) noexcept {
    return key<44&&key!=35&&key!=43;
}
uint16_t nativeTextKeyCharacter(size_t key) noexcept {
    if(!nativeTextKeyEnabled(key))return 0;
    if(key==33)return ' ';
    if(key==34)return '\b';
    if(key==36)return '\r';
    const auto label=studioKey(key);
    return label.size()==1?static_cast<uint16_t>(label.front()):0;
}
std::shared_ptr<const CabinStudioFrame> NativeTextKeyboard::update(bool allowed,Pose head,
        const std::array<StudioHand,2>& hands,uint64_t generation,uint64_t referenceEpoch,uint64_t activation){
    key_={};frame_.inputMask=frame_.hapticMask=0;frame_.hoverKey=-1;
    const bool neutral=hands[0].trigger<.2f&&hands[1].trigger<.2f;
    if(!allowed||!generation||!referenceEpoch||!activation||!valid(head)){
        frame_={};ready_=false;generation_=referenceEpoch_=activation_=0;pressed_={};
    }
    else{
        if(generation_!=generation||referenceEpoch_!=referenceEpoch||activation_!=activation){
            frame_={};frame_.available=frame_.keyboardOnly=true;frame_.editor=0;
            frame_.keyboard=compose(head,Pose{{},{0,-.28f,-.7f}});
            generation_=generation;referenceEpoch_=referenceEpoch;activation_=activation;ready_=false;pressed_={};
        }
        if(!ready_&&neutral)ready_=true;
        for(size_t side=0;side<2;++side){
            const auto& hand=hands[side];const bool pressed=hand.trigger>.55f;
            if(ready_&&hand.tracked&&hand.aimTracked){
                for(size_t key=0;key<44;++key){
                    if(!nativeTextKeyEnabled(key))continue;
                    if(!intersectPanel(Panel{studioKeyPose(frame_.keyboard,key),.043f,.043f,1,1},hand.aim,1.5f))continue;
                    frame_.hoverKey=static_cast<int>(key);frame_.inputMask|=static_cast<uint8_t>(1u<<side);
                    if(pressed&&!pressed_[side]&&!key_.character){
                        key_.character=nativeTextKeyCharacter(key);frame_.hapticMask|=static_cast<uint8_t>(1u<<side);
                    }
                    break;
                }
            }
            // Recovered tracking cannot turn an already held trigger into a key.
            pressed_[side]=pressed||!hand.tracked||!hand.aimTracked;
        }
    }
    return std::make_shared<const CabinStudioFrame>(frame_);
}
NativeTextKey NativeTextKeyboard::takeKey() noexcept {const auto value=key_;key_={};return value;}
}
