#include "mgs5vr/stereo.hpp"
#include <algorithm>
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
std::optional<EyeFov> enclosingEyeFov(EyeFov f){
    if(!valid(f))return {};
    const float x=std::max(-f.left,f.right),y=std::max(f.up,-f.down);
    return EyeFov{-x,x,y,-y};
}
std::optional<EyeFov> opticalFov(EyeFov f,float magnification){
    if(!valid(f)||!std::isfinite(magnification)||magnification<.25f||magnification>4.f)return {};
    if(magnification==1)return f;
    const auto angle=[&](float a){return std::atan(std::tan(a)/magnification);};
    const EyeFov result{angle(f.left),angle(f.right),angle(f.up),angle(f.down)};
    if(!valid(result))return {};
    return result;
}
std::optional<EyeImageRegion> eyeImageRegion(EyeFov rendered,EyeFov requested,uint32_t width,uint32_t height){
    if(!valid(rendered)||!valid(requested)||!width||!height||width>16384||height>16384
        ||requested.left<rendered.left||requested.right>rendered.right
        ||requested.down<rendered.down||requested.up>rendered.up)return {};
    const double l=std::tan(double(rendered.left)),r=std::tan(double(rendered.right));
    const double u=std::tan(double(rendered.up)),d=std::tan(double(rendered.down));
    const auto pixelX=[&](float angle){return (std::tan(double(angle))-l)/(r-l)*width;};
    const auto pixelY=[&](float angle){return (u-std::tan(double(angle)))/(u-d)*height;};
    const int32_t x0=std::clamp(int32_t(std::floor(pixelX(requested.left))),0,int32_t(width));
    const int32_t x1=std::clamp(int32_t(std::ceil(pixelX(requested.right))),0,int32_t(width));
    const int32_t y0=std::clamp(int32_t(std::floor(pixelY(requested.up))),0,int32_t(height));
    const int32_t y1=std::clamp(int32_t(std::ceil(pixelY(requested.down))),0,int32_t(height));
    if(x1<=x0||y1<=y0)return {};
    return EyeImageRegion{x0,y0,x1-x0,y1-y0,
        {float(std::atan(l+(r-l)*x0/width)),float(std::atan(l+(r-l)*x1/width)),
         float(std::atan(u-(u-d)*y0/height)),float(std::atan(u-(u-d)*y1/height))}};
}
bool setEyeProjection(std::array<float,16>& m,EyeFov f){
    if(!valid(f)||std::abs(m[11]-1)>0.00001f||std::abs(m[15])>0.00001f)return false;
    for(float v:m)if(!std::isfinite(v))return false;
    const float l=std::tan(f.left),r=std::tan(f.right),u=std::tan(f.up),d=std::tan(f.down);
    m[0]=-2/(r-l);m[5]=2/(u-d);m[8]=-(r+l)/(r-l);m[9]=-(u+d)/(u-d);
    return true;
}
bool widenVisibilityProjection(std::array<float,16>& matrix,Pose head,const std::array<EyeView,2>& views){
    if(!valid(head)||std::abs(matrix[0])<.01f||std::abs(matrix[5])<.01f)return false;
    for(float v:matrix)if(!std::isfinite(v))return false;
    float x=0,y=0;
    for(const auto& eye:views){
        if(!valid(eye.pose)||!valid(eye.fov))return false;
        const auto relative=compose(inverse(head),eye.pose);
        for(float horizontal:{eye.fov.left,eye.fov.right})for(float vertical:{eye.fov.down,eye.fov.up}){
            const auto ray=rotate(relative.orientation,{std::tan(horizontal),std::tan(vertical),-1});
            if(ray.z>=-.01f)return false;
            x=std::max(x,std::atan2(std::abs(ray.x),-ray.z));
            y=std::max(y,std::atan2(std::abs(ray.y),-ray.z));
        }
    }
    // Angular coverage only; object bounds still supply native near-field
    // visibility. Do not increase the render FOV, resolution, or LOD distances.
    constexpr float margin=.12f;
    x=std::max(x+margin,std::atan((1+std::abs(matrix[8]))/std::abs(matrix[0])));
    y=std::max(y+margin,std::atan((1+std::abs(matrix[9]))/std::abs(matrix[5])));
    auto widened=matrix;
    if(x>=1.5f||y>=1.5f||!setEyeProjection(widened,{-x,x,y,-y}))return false;
    matrix=widened;return true;
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
bool readyEyePair(const std::array<EyeFrame,2>& eyes,uint64_t activation,uint64_t now,uint64_t maximumAgeMs){
    // Both images must be drawn from one native scene/tracking transaction.
    // Consecutive game frames, including alternating-eye rendering, are rejected.
    if(!activation||!maximumAgeMs||maximumAgeMs>500||eyes[0].sourceSequence!=eyes[1].sourceSequence
        ||eyes[0].trackingSequence!=eyes[1].trackingSequence||eyes[0].magnification!=eyes[1].magnification)return false;
    for(size_t n=0;n<eyes.size();++n){const auto& e=eyes[n];
        if(!e.projected||!e.joined||e.eye!=n||!e.sourceSequence||e.activation!=activation
            ||!valid(e.view.pose)||!valid(e.view.fov)||!valid(e.displayFov)||!std::isfinite(e.magnification)
            ||e.magnification<1||e.magnification>4||now<e.sampleTime||now-e.sampleTime>maximumAgeMs)return false;
    }
    return true;
}
Pose nativeTrackedPose(Pose nativeHead,Pose sourceHead,Pose trackedPose,float units){
    auto relative=compose(inverse(sourceHead),trackedPose);relative.position=relative.position*units;
    return compose(compose(nativeHead,Pose{{0,1,0,0},{}}),relative);
}
bool panelFacesBothEyes(Pose panel,const std::array<Pose,2>& eyes){
    if(!valid(panel))return false;
    const auto normal=rotate(panel.orientation,{0,0,1});
    for(const auto& eye:eyes){
        if(!valid(eye))return false;
        const auto toward=eye.position-panel.position;
        const float distance=std::sqrt(dot(toward,toward));
        if(distance<.05f||dot(normal,toward)<.15f*distance)return false;
    }
    return true;
}
}
