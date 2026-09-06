#include "mgs5vr/stereo.hpp"
#include <cmath>

namespace mgs5vr {
std::optional<std::array<float,16>> uiPanelProjection(const std::array<float,16>& uiProjection,
    const std::array<float,16>& eyeView,EyeFov fov,Pose panel,float width,float height,float centerX,float centerY){
    if(!valid(panel)||!valid(fov)||!std::isfinite(width)||!std::isfinite(height)||width<=0||height<=0
        ||width>4||height>4||!std::isfinite(centerX)||!std::isfinite(centerY))return {};
    for(float f:uiProjection)if(!std::isfinite(f))return {};
    for(float f:eyeView)if(!std::isfinite(f))return {};
    const auto x=rotate(panel.orientation,{width/2,0,0}),y=rotate(panel.orientation,{0,height/2,0});
    const auto p=panel.position-x*centerX-y*centerY;
    // Flatten the native UI's clip coordinates onto an actual world plane.
    // Its original homogeneous W retains perspective within native UI layouts.
    std::array<float,16> plane{x.x,x.y,x.z,0,y.x,y.y,y.z,0,0,0,0,0,p.x,p.y,p.z,1};
    const auto multiply=[](const auto& a,const auto& b){std::array<float,16> out{};
        for(size_t r=0;r<4;++r)for(size_t c=0;c<4;++c){double sum=0;
            for(size_t k=0;k<4;++k)sum+=static_cast<double>(a[r*4+k])*b[k*4+c];out[r*4+c]=static_cast<float>(sum);}
        return out;};
    std::array<float,16> projection{};projection[11]=1;projection[14]=0.03f;
    if(!setEyeProjection(projection,fov))return {};
    return multiply(uiProjection,multiply(multiply(plane,eyeView),projection));
}
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
Pose nativeTrackedPose(Pose nativeHead,Pose sourceHead,Pose trackedPose,float units){
    auto relative=compose(inverse(sourceHead),trackedPose);relative.position=relative.position*units;
    return compose(compose(nativeHead,Pose{{0,1,0,0},{}}),relative);
}
}
