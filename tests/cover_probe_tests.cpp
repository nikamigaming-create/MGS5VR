#include "mgs5vr/cover_probe.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
void require(bool result,const char* reason){if(!result)throw std::runtime_error(reason);}
}
int main(){
    using namespace mgs5vr;
    try {
        const std::string prefix="41:5000:2:100:7:1000:10,20,30:";
        const auto parsed=parseCoverProbeRequest(prefix+"0,10,20,30;7,11,20,32");
        require(parsed&&parsed->count==2&&parsed->nodes[0].node==0&&parsed->nodes[1].node==7,
            "bounded native NAV node zero is valid and IDs/points retain their order");
        for(const auto& input:std::array<std::string,12>{
            prefix+"0,nan,20,30",prefix+"0,inf,20,30",prefix+"0,10,20,30;",
            prefix+"0,10,20,30;0,11,20,32",prefix+"0,71,20,30",prefix+"0,10,20,30,1",
            prefix+"-1,10,20,30",prefix+"0, 10,20,30",prefix+"0,10,20",
            "0:5000:2:100:7:1000:10,20,30:0,10,20,30",
            "41:5001:2:100:7:1000:10,20,30:0,10,20,30",
            "41:99:2:100:7:1000:10,20,30:0,10,20,30"})
            require(!parseCoverProbeRequest(input),"malformed/unbounded request must be rejected");
        std::string many;
        for(unsigned i=0;i<8;++i){if(i)many+=';';many+=std::to_string(i)+",10,20,30";}
        require(parseCoverProbeRequest(prefix+many)->count==8,"eight floor candidates are admitted");
        require(!parseCoverProbeRequest(prefix+many+";8,10,20,30"),"ninth candidate is rejected");
        require(!parseCoverProbeRequest(std::string(1601,'1')),"wire body has a finite byte bound");
        require(!parseCoverProbeRequest("18446744073709551616:100:2:100:7:1000:10,20,30:0,10,20,30"),
            "overflow cannot alias a previous nonce");
        auto request=*parsed;request.issuedMs=1000;request.expiresMs=6000;
        CoverProbeFrame frame{{2,100,7},1000,9,{10,21.7f,30},true};
        require(coverProbeFrameReason(request,frame,1000)==CoverProbeReason::none,"same-owner fresh native eye is admitted");
        require(coverProbeFrameReason(request,frame,1150)==CoverProbeReason::none,"150ms frame boundary is admitted");
        require(coverProbeFrameReason(request,frame,1151)==CoverProbeReason::staleFrame,"stale native frame is refused");
        frame.sampleMs=1250;
        require(coverProbeFrameReason(request,frame,1250)==CoverProbeReason::none,"250ms root boundary is admitted");
        frame.sampleMs=1251;
        require(coverProbeFrameReason(request,frame,1251)==CoverProbeReason::staleRoot,"stale observed root is refused");
        frame.sampleMs=1001;
        require(coverProbeFrameReason(request,frame,1000)==CoverProbeReason::staleFrame,"future native clock is refused");
        frame.sampleMs=1000;
        require(coverProbeFrameReason(request,frame,6000)==CoverProbeReason::lease,"lease endpoint does not extend availability");
        for(unsigned i=0;i<3;++i){auto other=frame;
            if(i==0)++other.owner.activation;else if(i==1)++other.owner.playerOwner;else ++other.owner.presentationEpoch;
            require(coverProbeFrameReason(request,other,1000)==CoverProbeReason::owner,"all three owner fields gate evidence");
        }
        auto unfocused=frame;unfocused.usable=false;
        require(coverProbeFrameReason(request,unfocused,1000)==CoverProbeReason::frame,"presentation focus is required");
        auto distant=frame;distant.nativeEye={10,23,30};
        require(coverProbeFrameReason(request,distant,1000)==CoverProbeReason::none,"three-metre eye reach boundary is admitted");
        distant.nativeEye.y=23.01f;
        require(coverProbeFrameReason(request,distant,1000)==CoverProbeReason::eyeReach,"camera/root mismatch cannot invent an eye height");
        auto corrupt=request;corrupt.observedPlayer.x=std::numeric_limits<float>::quiet_NaN();
        require(!validCoverProbeRequest(corrupt),"programmatic requests cannot bypass finite root validation");
        require(coverProbeTerrainObstruction(true,9,1,10,5,0),"valid terrain hit before the endpoint is obstruction");
        require(!coverProbeTerrainObstruction(true,3,0,10,0,0),"valid miss is clear, not cover");
        require(!coverProbeTerrainObstruction(false,9,1,10,5,0),"unavailable query cannot mean cover");
        require(!coverProbeTerrainObstruction(true,8,1,10,5,.05f),"off-ray hit cannot mean cover");
        require(!coverProbeTerrainObstruction(true,9,1,10,9.7f,0),"endpoint tolerance cannot count as cover");
        require(!coverProbeTerrainObstruction(true,9,1,10,.01f,0),"near-origin hit cannot count as cover");
        require(!coverProbeTerrainObstruction(true,9,1,10,11,0),"beyond-segment hit cannot mean cover");
        require(!coverProbeTerrainObstruction(true,9,1,10,5,std::numeric_limits<float>::quiet_NaN()),"nonfinite ray evidence is refused");
        std::cout<<"cover probe pure contracts passed\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
