#pragma once
#include "head_camera.hpp"
#include <algorithm>
#include <cmath>
#include <string>
namespace mgs5vr {
struct AnimalStrokeDelta {float speed{},travel{};};
// Rendered contact establishes where the hand touches the animal. Deliberate
// motion must also exist in raw tracking and relative to head translation;
// native animation, a moving animal, walking and head-only motion cannot pet.
inline AnimalStrokeDelta animalStrokeDelta(Vec3 contact,Vec3 tracked,Vec3 headRelative,float seconds){
    if(!std::isfinite(seconds)||seconds<=0)return {};
    const auto distance=[](Vec3 v){return std::sqrt(dot(v,v));};
    const float a=distance(contact),b=distance(tracked),c=distance(headRelative);
    if(!std::isfinite(a)||!std::isfinite(b)||!std::isfinite(c))return {1.f,0};
    return {std::max({a,b,c})/seconds,std::min(b,c)/seconds>.025f?std::min({a,b,c}):0.f};
}
// Contact is measured from the rendered palms and the native buddy's bones.
using HandContacts=std::array<std::array<Vec3,6>,2>;
void publishAnimalHands(const HeadCameraSample& frame,const std::array<Pose,2>& palms,const HandContacts& contacts);
// Call only from the native Lua transaction, where buddy command responses
// are available. Rechecks fresh XR ownership before consuming a hand frame.
void consumeAnimalTouch();
// Private native-action queue helpers; never synthesize player action buttons.
std::string inspectAnimalTouch();
std::string requestNativeDogResponse();
}
