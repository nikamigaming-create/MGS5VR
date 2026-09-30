#include "mgs5vr/stale_demo_recovery.hpp"
#include "mgs5vr/head_camera.hpp"
#include "mgs5vr/input_bridge.hpp"

#include <stdexcept>
#include <string>

namespace {
void require(bool condition,const char* message) {
    if(!condition)throw std::runtime_error(message);
}
}

int main() {
    using mgs5vr::StaleDemoRecoveryDwell;
    constexpr uint64_t key=11,generation=1,camera=101,owner=202,poll=1000;
    StaleDemoRecoveryDwell dwell;

    // Fresh matching publications every 100 ms satisfy exactly two seconds.
    require(!dwell.observe(true,key,generation,camera,owner,1000,poll,false),"first sample cannot recover");
    for(uint64_t t=1100;t<3000;t+=100)
        require(!dwell.observe(true,key,generation,camera,owner,t,t,false),"dwell must remain conservative before 2 s");
    require(dwell.observe(true,key,generation,camera,owner,3000,3000,false),"continuous fresh stream recovers at 2 s");
    require(dwell.ready(3050,key,generation,3050,false),"ready remains fresh");
    require(!dwell.ready(3301,key,generation,3000,false),"candidate heartbeat expires");
    require(!dwell.ready(3151,key,generation,3151,false),"publication freshness expires");

    // A >150 ms camera gap, repeated timestamp, or stale Lua heartbeat resets.
    require(!dwell.observe(true,key,generation,camera,owner,4000,4000,false),"new dwell begins");
    require(!dwell.observe(true,key,generation,camera,owner,4100,4100,false),"new dwell advances");
    require(!dwell.observe(true,key,generation,camera,owner,4251,4251,false),"gap resets dwell");
    require(!dwell.observe(true,key,generation,camera,owner,4251,4251,false),"repeated publication resets dwell");
    require(!dwell.observe(true,key,generation,camera,owner,4400,4000,false),"old candidate poll resets dwell");

    // Candidate generation, camera, owner, menu, and candidate loss each reset.
    require(!dwell.observe(true,key,generation,camera,owner,5000,5000,false),"candidate restarts");
    require(!dwell.observe(true,key,generation+1,camera,owner,5100,5100,false),"generation change resets");
    require(!dwell.observe(true,key,generation+1,camera+1,owner,5200,5200,false),"camera change resets");
    require(!dwell.observe(true,key,generation+1,camera+1,owner+1,5300,5300,false),"owner change resets");
    require(!dwell.observe(true,key,generation,camera,owner,5400,5400,true),"menu blocks and resets");
    require(!dwell.observe(false,key,generation,camera,owner,5500,5500,false),"candidate loss resets");
    require(!dwell.ready(5600,key,generation,5600,false),"reset state cannot recover");

    // Exercise the actual camera integration, including authored shots which
    // deliberately have no attached gameplay owner. Merely receiving another
    // actor's head publication must not release such a shot.
    const auto cameraCase=[&](mgs5vr::NativeDemoMode mode,bool matching){
        using namespace mgs5vr;
        HeadCamera headCamera;headCamera.configure(true,1,true);
        ControllerFrame frame;frame.authoredCamera=frame.scriptedDemo=true;
        frame.referenceEpoch=1;frame.predictedXrTime=1;
        const std::array<EyeView,2> eyes{{{Pose{{},{-.032f,0,0}},{-.7f,.7f,.7f,-.7f}},
                                        {Pose{{},{.032f,0,0}},{-.7f,.7f,.7f,-.7f}}}};
        const std::array<float,16> root{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        auto bone=root;bone[13]=1.6f;
        const Pose source{{},{0,1.6f,0}};
        const auto end=steadyMilliseconds(),start=end-2200;
        publishNativeDemoSnapshot(NativeDemoMode::cinematic,0,start);
        headCamera.trackStereo({},eyes,true,start,frame);headCamera.toggle();
        const auto opening=headCamera.resolve(camera,source,start);
        require(opening.applied&&!opening.playerOwner,"authored shot has no gameplay owner");
        for(uint64_t t=start+100;t<=end;t+=100){
            publishNativeDemoSnapshot(mode,key,t);
            require(headCamera.publishPlayerHead(matching?camera:camera+1,owner,source,root,bone,t),"valid native publication accepted");
            headCamera.trackStereo({},eyes,true,t,frame);
            require(headCamera.resolve(camera,source,t).applied,"authored shot keeps rendering throughout dwell");
        }
        const bool recovered=headCamera.staleDemoRecoveryReady();
        publishNativeDemoSnapshot(NativeDemoMode::cinematic,0,steadyMilliseconds());
        require(!headCamera.staleDemoRecoveryReady(),"positive demo immediately revokes recovery");
        return recovered;
    };
    require(cameraCase(mgs5vr::NativeDemoMode::staleCandidate,true),"ownerless shot recovers only after matching rendered player stream");
    require(!cameraCase(mgs5vr::NativeDemoMode::staleCandidate,false),"unrelated player camera cannot release accepted shot");
    require(!cameraCase(mgs5vr::NativeDemoMode::cinematic,true),"fresh skeleton does not interrupt real demo");
    mgs5vr::publishNativeDemoSnapshot(mgs5vr::NativeDemoMode::none,0,0);
    return 0;
}
