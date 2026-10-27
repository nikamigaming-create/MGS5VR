#include "mgs5vr/gear_contact.hpp"
#include "mgs5vr/cabin_studio.hpp"
#include <cstdlib>
#include <iostream>
using namespace mgs5vr;
void require(bool value,const char* message){if(!value){std::cerr<<message<<'\n';std::exit(1);}}
int main(){
    for(const uint64_t delay:{0ull,500ull,999ull,1000ull,1001ull}){
        GearContact contact;
        require(contact.update(1,true,true,true,false,100).pulse,"touch must pulse");
        for(uint64_t elapsed=100;elapsed<delay;elapsed+=100)
            require(!contact.update(1,true,false,true,false,100+elapsed).pulse,"grace must be silent");
        require(contact.update(1,true,false,true,true,100+delay).grab==(delay<1000),"one second deadline");
    }
    GearContact held;
    require(!held.update(1,true,true,true,true,100).grab,"already held grip must not acquire");
    held.update(1,true,true,true,false,110);
    require(held.update(1,true,true,true,true,120).grab,"fresh squeeze acquires");
    const auto repeated=held.update(1,true,true,true,true,130);
    require(!repeated.grab&&!repeated.pulse,"held grip cannot repeat or buzz again");
    GearContact changed;
    changed.update(1,true,true,true,false,100);
    require(!changed.update(2,true,false,true,true,110).grab,"loadout change cancels latch");
    GearContact lost;
    lost.update(1,true,true,true,false,100);
    require(!lost.update(1,true,false,true,true,400).grab,"stale source cancels latch");
    GearContact reach;
    reach.update(1,true,true,true,false,100);
    require(!reach.update(1,true,false,false,true,110).grab,"no distant pickup");
    GearContact clock;
    clock.update(1,true,true,true,false,100);
    require(!clock.update(1,true,true,true,true,90).grab,"clock regression cancels");
    GearContact tracking;
    tracking.update(1,true,true,true,false,100);
    tracking.update(1,false,false,true,true,110);
    require(!tracking.update(1,true,true,true,true,120).grab,"tracking recovery needs release");
    CabinRackCue rack;
    require(!rack.update(0.f,1.f),"initial mount contact must not repeat a return cue");
    require(!rack.update(.14f,1.f),"leaving the mount only arms a return cue");
    require(!rack.update(.07f,.95f),"a misaligned weapon must not signal a dock");
    require(rack.update(.07f,.99f),"returning close and aligned to the mount must cue once");
    require(!rack.update(.04f,1.f),"remaining at the mount must not repeat the cue");
    require(!rack.update(.2f,1.f)&&rack.update(.04f,1.f),"a later removal and return rearms the cue");
    require(!rack.update(-1.f,1.f)&&!rack.update(0.f,1.f),"invalid distance resets the cue");
    CabinStudio studio;
    std::array<StudioHand,2> hands{};
    const Pose holster{{},{1,1,-.5f}};
    hands[1].tracked=true;hands[1].grip=holster;
    auto camera=studio.update(true,false,{}, {},hands,100,0,false,StudioFreeze::live,holster);
    require(camera->cameraHand==-1&&(camera->hapticMask&2),"mounted camera cues before grab");
    hands[1].grip.position.x+=studioCameraHalf.x+.08f;
    for(uint64_t at=200;at<1099;at+=100)
        studio.update(true,false,{}, {},hands,at,0,false,StudioFreeze::live,holster);
    hands[1].squeeze=1;
    camera=studio.update(true,false,{}, {},hands,1099,0,false,StudioFreeze::live,holster);
    require(camera->cameraHand==1,"mounted camera accepts late squeeze after drift");
    const Pose movedHolster{{},{1.3f,1,-.5f}};
    hands[1].squeeze=0;
    camera=studio.update(true,false,{}, {},hands,1110,0,false,StudioFreeze::live,movedHolster);
    require(camera->cameraHand==-1&&camera->camera.position.x==movedHolster.position.x,
        "release returns camera to current mount in same update");
    std::cout<<"Gear contact boundaries passed\n";
}
