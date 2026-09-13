#pragma once
#include <cstdint>

namespace mgs5vr {
enum class HudMode { full, binocularsOnly, off };
// A lens render is not necessarily a recon device: firearm scopes and a
// binocular held away from the eye must not gain enhanced target information.
enum class HudView { world, binoculars, otherOptic };
enum class HudLayer { general, worldLabels, context, status, equipment, commands };

// TPP's fixed layout cameras share draw-order numbers. Camera depth and the
// active picker distinguish equipment cards from unrelated HUD content.
constexpr HudLayer hudLayer(uint32_t order,float cameraDepth,bool items,bool commands) noexcept {
    if(order==50)return HudLayer::worldLabels;
    if(order==52)return HudLayer::context;
    if((cameraDepth==150&&order>=133&&order<=136)||(items&&cameraDepth==100&&order==133))return HudLayer::equipment;
    if(commands&&cameraDepth==100&&order>=135&&order<=139)return HudLayer::commands;
    if(order>=146&&order<=148)return HudLayer::status;
    return HudLayer::general;
}
constexpr HudView hudViewForPass(unsigned eye,bool binocularAtEye) noexcept {
    return eye<2?HudView::world:eye==2&&binocularAtEye?HudView::binoculars:HudView::otherOptic;
}
constexpr bool worldHudVisible(HudMode mode,HudView view) noexcept {
    return (view==HudView::world&&mode==HudMode::full)
        ||(view==HudView::binoculars&&(mode==HudMode::full||mode==HudMode::binocularsOnly));
}
}
