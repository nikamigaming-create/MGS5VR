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
// The initial native four-way selector uses the Z=100 command-style draw
// orders 135..137; its trigger-held state is the additional discriminator.
constexpr HudLayer hudLayer(uint32_t order,float cameraDepth,bool items,bool commands,bool category=false) noexcept {
    if(order==50)return HudLayer::worldLabels;
    if(order==52)return HudLayer::context;
    if((cameraDepth==150&&order>=133&&order<=136)||(items&&cameraDepth==100&&order==133))return HudLayer::equipment;
    if(category&&cameraDepth==100&&order>=135&&order<=137)return HudLayer::equipment;
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
constexpr bool reconModelVisible(HudMode mode,HudView view,bool glow) noexcept {
    return glow&&view==HudView::binoculars&&worldHudVisible(mode,view);
}
// Acquired enemy labels use the current eye's world projection. Native recon
// body shaders remain in the physical binocular view, independently of labels.
constexpr bool worldMarkerVisible(HudMode mode,bool enemyMarkers,uint8_t letter) noexcept {
    return mode!=HudMode::off&&(letter?mode==HudMode::full:enemyMarkers);
}
// Native scene silhouettes and the independent layout-camera person cue.
// Draw-order numbers alone are not enough: other UI cameras reuse them.
constexpr bool nativeReconLayer(uint32_t order,bool sceneCamera,bool layoutCamera) noexcept {
    return (sceneCamera&&(order==2||order==3))||(layoutCamera&&order==23);
}
// GZ's owned priority table separates character cues (23/24) from terrain
// markers (25/26). Apply only to joined gameplay cameras, never iDroid maps.
constexpr bool gzWorldMarkerVisible(uint32_t order,HudMode mode,HudView view,bool enemies) noexcept {
    if(order<23||order>26)return true;
    if(view!=HudView::world)return worldHudVisible(mode,view);
    return order<25?worldMarkerVisible(mode,enemies,0):worldHudVisible(mode,view);
}
// Retail status/ammo uses the Z=135 layout. iDroid's own animated map cameras
// reuse priorities, so only this observed layout stays on the personal HUD.
constexpr bool idroidPersonalStatus(uint32_t order,float cameraDepth,bool idroid) noexcept {
    return idroid&&cameraDepth==135&&order>=146&&order<=148;
}
// Owned TPP UiDrawPriorityTable: RETICLE_LOW/RETICLE/RETICLE_HIGH (53..57)
// and SIGHT_RETICLE/SIGHT_RETICLE_ADD/SIGHT_STRONG (130..132). These desktop
// aiming layers must not be mapped onto the forearm beside a physical gun.
// Placement/throwing and mounted aiming keep their native feedback.
constexpr bool suppressFlatFirearmReticle(uint32_t order,float cameraDepth,bool firearm) noexcept {
    return firearm&&cameraDepth==100&&((order>=53&&order<=57)||(order>=130&&order<=132));
}
}
