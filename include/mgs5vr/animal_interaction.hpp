#pragma once
#include "head_camera.hpp"
#include <string>
namespace mgs5vr {
// Contact is measured from the rendered palms and the native buddy's bones.
using HandContacts=std::array<std::array<Vec3,6>,2>;
void publishAnimalHands(const HeadCameraSample& frame,const std::array<Pose,2>& palms,const HandContacts& contacts);
void consumeAnimalTouch();
// Private native-action queue helpers; never synthesize player action buttons.
std::string inspectAnimalTouch();
std::string requestNativeDogResponse();
}
