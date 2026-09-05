#include "mgs5vr/stereo.hpp"
#include <cmath>

namespace mgs5vr {
bool valid(EyeFov f){
    return std::isfinite(f.left)&&std::isfinite(f.right)&&std::isfinite(f.up)&&std::isfinite(f.down)
        &&f.left<0&&f.right>0&&f.down<0&&f.up>0&&f.left>-1.56f&&f.right<1.56f&&f.down>-1.56f&&f.up<1.56f;
}
bool setEyeProjection(std::array<float,16>& m,EyeFov f){
    if(!valid(f)||std::abs(m[11]-1)>0.00001f||std::abs(m[15])>0.00001f)return false;
    for(float v:m)if(!std::isfinite(v))return false;
    const float l=std::tan(f.left),r=std::tan(f.right),u=std::tan(f.up),d=std::tan(f.down);
    m[0]=-2/(r-l);m[5]=2/(u-d);m[8]=-(r+l)/(r-l);m[9]=-(u+d)/(u-d);
    return true;
}
Pose nativeEyePose(Pose nativeHead,Pose sourceHead,Pose sourceEye,float units){
    const Pose basis{{0,1,0,0},{}};
    auto relative=compose(inverse(sourceHead),sourceEye);relative.position=relative.position*units;
    auto result=compose(nativeHead,compose(compose(basis,relative),inverse(basis)));
    auto& q=result.orientation;
    const double norm=std::sqrt(static_cast<double>(q.x)*q.x+static_cast<double>(q.y)*q.y+static_cast<double>(q.z)*q.z+static_cast<double>(q.w)*q.w);
    if(norm>0.5){q={static_cast<float>(q.x/norm),static_cast<float>(q.y/norm),static_cast<float>(q.z/norm),static_cast<float>(q.w/norm)};}
    return result;
}
bool readyEyePair(const std::array<EyeFrame,2>& eyes,uint64_t activation,uint64_t now){
    // Both images must be drawn from one native scene/tracking transaction.
    // Consecutive game frames, including alternating-eye rendering, are rejected.
    if(!activation||eyes[0].sourceSequence!=eyes[1].sourceSequence
        ||eyes[0].trackingSequence!=eyes[1].trackingSequence)return false;
    for(size_t n=0;n<eyes.size();++n){const auto& e=eyes[n];
        if(!e.projected||!e.joined||e.eye!=n||!e.sourceSequence||e.activation!=activation
            ||!valid(e.view.pose)||!valid(e.view.fov)||now<e.sampleTime||now-e.sampleTime>150)return false;
    }
    return true;
}
}
