#pragma once
#include "animal_interaction.hpp"
namespace mgs5vr {
void installSmallAnimalInteraction(uintptr_t imageBase);
void stopSmallAnimalInteraction() noexcept;
void publishSmallAnimalHands(const HeadCameraSample&,const std::array<Pose,2>&,const HandContacts&);
void applySmallAnimalSkin(uintptr_t binding);
std::string inspectSmallAnimals();
}
