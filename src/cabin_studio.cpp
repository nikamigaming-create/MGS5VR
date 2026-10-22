#include "mgs5vr/cabin_studio.hpp"
#include <algorithm>
#include <cmath>
#include <atomic>
#include <chrono>
#include <mutex>
#include <limits>

namespace mgs5vr {
namespace {
std::mutex freezeMutex;
CameraFreezeRequest freezeRequest{};
std::atomic<StudioFreeze> freezeStatus{StudioFreeze::live};
std::atomic_bool freezeOwned{};
constexpr Quat flat{-.70710678f,0,0,.70710678f};
constexpr std::array<std::string_view,44> keys{
    "Q","W","E","R","T","Y","U","I","O","P","7",
    "A","S","D","F","G","H","J","K","L","8","9",
    "Z","X","C","V","B","N","M",".",",","5","6",
    "SPACE","BACK","CLEAR","DONE","0","1","2","3","4","-","TRASH"};
bool near(Vec3 a,Vec3 b,float radius){const auto delta=a-b;return dot(delta,delta)<radius*radius;}
float surfaceDistance(Pose object,Vec3 half,Vec3 point){
    const auto local=compose(inverse(object),Pose{{},point}).position;
    const Vec3 outside{std::max(0.f,std::abs(local.x)-half.x),
        std::max(0.f,std::abs(local.y)-half.y),std::max(0.f,std::abs(local.z)-half.z)};
    return dot(outside,outside);
}
float span(float a,float b,float c){return std::sqrt(a*a+b*b+c*c);}
Pose rigid(Pose pose){
    auto& q=pose.orientation;const auto length=std::sqrt(static_cast<double>(q.x)*q.x+static_cast<double>(q.y)*q.y
        +static_cast<double>(q.z)*q.z+static_cast<double>(q.w)*q.w);
    if(std::isfinite(length)&&length>.5)q={static_cast<float>(q.x/length),static_cast<float>(q.y/length),
        static_cast<float>(q.z/length),static_cast<float>(q.w/length)};
    return pose;
}
void settle(Pose& pose,Vec3& velocity,float dt,Vec3 half,
    float support=-std::numeric_limits<float>::infinity()){
    velocity.y-=9.81f*dt;pose.position=pose.position+velocity*dt;
    const auto p=pose.position;
    const float floor=std::max(cabinNativeSupportHeight(p),support);
    if(p.y-half.y<floor){pose.position.y=floor+half.y;
        velocity.y=std::abs(velocity.y)<.12f?0:std::abs(velocity.y)*.08f;
        velocity.x*=std::max(0.f,1-12*dt);velocity.z*=std::max(0.f,1-12*dt);}
    pose.position.x=std::clamp(pose.position.x,-.95f+half.x,.95f-half.x);
    pose.position.z=std::clamp(pose.position.z,-1.34f+half.z,2.4f-half.z);
}
Vec3 paperExtent(Pose pose,Vec3 half){
    const auto x=rotate(pose.orientation,{1,0,0}),y=rotate(pose.orientation,{0,1,0}),z=rotate(pose.orientation,{0,0,1});
    return {std::abs(x.x)*half.x+std::abs(y.x)*half.y+std::abs(z.x)*half.z,
        std::abs(x.y)*half.x+std::abs(y.y)*half.y+std::abs(z.y)*half.z,
        std::abs(x.z)*half.x+std::abs(y.z)*half.y+std::abs(z.z)*half.z};
}
Vec3 paperSupportExtent(const StudioPaper& paper){
    auto half=studioPaperHalf(paper.photo!=0);
    // The ink quad sits 1 mm above the paper centre, outside the thinner
    // backing mesh. Include it so an upper backing cannot cut through ink.
    half.z=std::max(half.z,.001f);
    return paperExtent(paper.pose,half);
}
}
void requestCameraFreeze(bool requested,uint64_t activation,uint64_t referenceEpoch) noexcept{
    try{std::lock_guard lock(freezeMutex);
        const bool changed=requested!=freezeRequest.requested;
        const auto now=static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
        freezeRequest={requested,activation,referenceEpoch,now};
        if(changed&&requested&&!freezeOwned.load())freezeStatus.store(StudioFreeze::pending);
    }catch(...){}
}
CameraFreezeRequest cameraFreezeRequest() noexcept{std::lock_guard lock(freezeMutex);return freezeRequest;}
void acknowledgeCameraFreeze(StudioFreeze status,bool owned) noexcept{freezeOwned.store(owned);freezeStatus.store(status);}
StudioFreeze cameraFreezeState() noexcept{return freezeStatus.load();}
bool cameraWorldFrozen() noexcept{return freezeOwned.load();}
Vec3 studioPaperHalf(bool photo) noexcept{return photo?Vec3{.044f,.0535f,.0007f}:Vec3{.038f,.038f,.0006f};}
Pose studioCameraPalm(Pose camera,size_t side) noexcept{
    // Anatomical +X points into the right palm and out of the left palm.
    // Turn the socket so both palms cup the body from its respective side.
    return compose(camera,Pose{{0,1,0,0},side?studioCameraRightGrip:studioCameraLeftGrip});
}
std::optional<Pose> studioFieldCameraHolster(Pose pelvis,Pose rightHip) noexcept{
    if(!valid(pelvis)||!valid(rightHip))return {};
    const auto hip=compose(inverse(pelvis),rightHip).position;
    // The admitted Snake chain places the right hip on native -X. Reject a
    // foreign/mirrored anchor instead of putting the camera inside the body.
    if(hip.x>=-.015f||hip.x<-.25f||std::abs(hip.y)>.25f||std::abs(hip.z)>.2f)return {};
    // The first anatomical-eye review showed the rear-side placement buried
    // behind Snake's belt. Keep the camera outside that captured silhouette,
    // toward the front of the hip, while retaining its native hip height.
    const Vec3 offset{std::min(-.35f,hip.x-studioCameraHalf.x),hip.y,hip.z+.04f};
    return rigid(compose(pelvis,Pose{{0,1,0,0},offset}));
}
void joinStudioBody(CabinStudioFrame& frame,Pose pelvis,Pose rightHip){
    frame.fieldCameraHolster.reset();
    if(!frame.available||frame.cabin)return;
    frame.fieldCameraHolster=studioFieldCameraHolster(pelvis,rightHip);
    if(!frame.fieldCameraHolster||frame.cameraHand>=0)return;
    const auto correction=compose(*frame.fieldCameraHolster,inverse(frame.camera));
    frame.camera=*frame.fieldCameraHolster;
    // Only the currently offered field output follows its camera. Persistent
    // cabin copies and held papers retain their own existing object poses.
    for(auto& paper:frame.papers)if(paper.field&&paper.hand<0)
        paper.fieldPose=compose(correction,paper.fieldPose);
}
std::string_view studioKey(size_t index) noexcept{return index<keys.size()?keys[index]:std::string_view{};}
Pose studioKeyPose(Pose keyboard,size_t index) noexcept{
    return compose(keyboard,Pose{{},{(static_cast<float>(index%11)-5)*.045f,
        (.5f-static_cast<float>(index/11))*.045f,0}});
}
Pose studioControlPose(Pose camera,StudioControl control) noexcept{
    const auto index=static_cast<unsigned>(control);
    return compose(camera,Pose{{},{index%2?.029f:-.029f,-.025f-.019f*(index/2),.041f}});
}
std::string_view studioControlLabel(StudioControl control) noexcept{
    constexpr std::array<std::string_view,6> labels{"BACK","NEXT","PRINT","DELETE","LIVE","FREEZE"};
    const auto index=static_cast<size_t>(control);return index<labels.size()?labels[index]:std::string_view{};
}
std::string studioWristHint(const CabinStudioFrame& frame){
    if(!frame.available)return {};
    const auto held=std::find_if(frame.papers.begin(),frame.papers.end(),[](const auto& p){return p.hand>=0;});
    if(frame.markerHand>=0)
        return std::string(frame.markerHand==0?"LEFT":"RIGHT")
            +" TRIGGER + PEN TIP ON PAPER : WRITE\n"
            +(held!=frame.papers.end()&&frame.cabin
                ?"PAPER ON BOARD + RELEASE GRIP : PIN"
                :"RELEASE GRIP TO PUT THE PEN DOWN");
    if(held!=frame.papers.end())
        return std::string(held->photo?"PHOTO : HOLD GRIP TO CARRY\n":"NOTE : PICK UP THE PEN TO WRITE\n")
            +(frame.cabin?"PLACE ON BOARD + RELEASE GRIP TO PIN":"RELEASE GRIP TO STORE IN THE CABIN");
    if(frame.cameraHand>=0){
        if(frame.confirmPhoto)return "DELETE PHOTO? PRESS DELETE AGAIN\nLIVE CANCELS / PRINTED COPIES ARE KEPT";
        if(frame.review)return "GALLERY : BACK / NEXT TO BROWSE\nPRINT MAKES A COPY / LIVE RETURNS TO CAMERA";
        return std::string(frame.cameraHand==0?"LEFT":"RIGHT")
            +" TRIGGER : TAKE PHOTO\nBACK / NEXT : GALLERY   PRINT : COPY";
    }
    return {};
}
void joinStudioHands(CabinStudioFrame& frame,const std::array<Pose,2>& inputGrips,
    const std::array<Pose,2>& renderedPalms,const std::array<bool,2>& tracked){
    if(!frame.available)return;
    for(size_t side=0;side<2;++side){
        if(!tracked[side]||!valid(inputGrips[side])||!valid(renderedPalms[side]))continue;
        const auto correction=compose(renderedPalms[side],inverse(inputGrips[side]));
        const auto roomCorrection=compose(inverse(frame.roomTracking),compose(correction,frame.roomTracking));
        if(frame.cameraHand==static_cast<int>(side)){
            frame.camera=compose(correction,frame.camera);
            frame.shutterPose=compose(frame.camera,studioCameraLens);
            for(auto& paper:frame.papers)if(paper.hand<0){
                if(paper.field)paper.fieldPose=compose(correction,paper.fieldPose);
                else if(paper.id==frame.outputPaper)paper.pose=compose(roomCorrection,paper.pose);
            }
        }
        if(frame.markerHand==static_cast<int>(side))frame.marker=compose(roomCorrection,frame.marker);
        for(auto& paper:frame.papers)if(paper.hand==static_cast<int>(side)){
            if(paper.field)paper.fieldPose=compose(correction,paper.fieldPose);
            else paper.pose=compose(roomCorrection,paper.pose);
        }
    }
}
void CabinStudio::restore(std::vector<StudioPaper> papers){
    StudioCollection collection;collection.papers=std::move(papers);
    for(const auto& paper:collection.papers)if(paper.photo&&std::find(collection.photos.begin(),collection.photos.end(),paper.photo)==collection.photos.end())
        collection.photos.push_back(paper.photo);
    restore(std::move(collection));
}
void CabinStudio::restore(StudioCollection collection){
    frame_.outputPaper=0;
    auto& papers=collection.papers;
    if(papers.size()>studioPaperLimit)papers.resize(studioPaperLimit);
    frame_.papers.clear();
    for(auto& paper:papers)if(paper.id&&valid(paper.pose)&&paper.ink){
        paper.hand=-1;paper.field=false;paper.velocity={};nextId_=std::max(nextId_,paper.id+1);frame_.papers.push_back(std::move(paper));}
    frame_.photos.clear();
    for(const auto photo:collection.photos)if(photo&&frame_.photos.size()<studioPhotoLimit
        &&std::find(frame_.photos.begin(),frame_.photos.end(),photo)==frame_.photos.end())frame_.photos.push_back(photo);
    frame_.selectedPhoto=std::find(frame_.photos.begin(),frame_.photos.end(),collection.selectedPhoto)!=frame_.photos.end()
        ?collection.selectedPhoto:frame_.photos.empty()?0:frame_.photos.back();
}
void CabinStudio::print(uint64_t photo,bool field){
    if(!photo||frame_.papers.size()>=studioPaperLimit)return;
    // Each copy owns its ID, location and ink. It is never a gallery entry.
    for(auto& p:frame_.papers)if(p.field&&p.hand<0)p.field=false;
    StudioPaper paper{};paper.id=nextId_++;paper.photo=photo;paper.field=field;
    paper.fieldPose=compose(frame_.camera,studioCameraPrintPose);
    paper.pose=field?Pose{flat,studioPrintHome+Vec3{0,.0015f*static_cast<float>(frame_.papers.size()),0}}
        :compose(inverse(frame_.roomTracking),paper.fieldPose);
    frame_.outputPaper=paper.id;
    paper.ink=std::make_shared<const StudioInk>();frame_.papers.push_back(std::move(paper));++frame_.revision;
}
void CabinStudio::control(StudioControl button,uint64_t now){
    if(button==StudioControl::erase){
        if(!frame_.selectedPhoto)return;
        if(frame_.confirmPhoto!=frame_.selectedPhoto){frame_.confirmPhoto=frame_.selectedPhoto;return;}
        const auto selected=std::find(frame_.photos.begin(),frame_.photos.end(),frame_.selectedPhoto);
        if(selected!=frame_.photos.end()){
            const auto index=static_cast<size_t>(selected-frame_.photos.begin());frame_.photos.erase(selected);
            frame_.selectedPhoto=frame_.photos.empty()?0:frame_.photos[std::min(index,frame_.photos.size()-1)];
            ++frame_.revision;
        }
        frame_.confirmPhoto=0;frame_.review=true;return;
    }
    frame_.confirmPhoto=0;
    if(button==StudioControl::back||button==StudioControl::next){
        frame_.review=true;
        if(!frame_.photos.empty()){
            const auto selected=std::find(frame_.photos.begin(),frame_.photos.end(),frame_.selectedPhoto);
            const auto index=selected==frame_.photos.end()?0:static_cast<size_t>(selected-frame_.photos.begin());
            frame_.selectedPhoto=frame_.photos[button==StudioControl::back?(index?index-1:0):std::min(index+1,frame_.photos.size()-1)];
            ++frame_.revision;
        }
    }else if(button==StudioControl::print)print(frame_.selectedPhoto,!frame_.cabin);
    else if(button==StudioControl::live)frame_.review=false;
    else if(button==StudioControl::freeze&&frame_.freezeSupported){
        frame_.freezeRequested=!frame_.freezeRequested;
        frame_.freezeState=frame_.freezeRequested?StudioFreeze::pending:StudioFreeze::live;
    }
    (void)now;
}
std::shared_ptr<const CabinStudioFrame> CabinStudio::update(bool allowed,bool cabin,Pose head,Pose room,
    const std::array<StudioHand,2>& hands,uint64_t now,uint64_t newPhoto,bool freezeSupported,StudioFreeze freeze,
    std::optional<Pose> fieldCameraHolster){
    frame_.available=allowed;frame_.cabin=cabin;frame_.roomTracking=room;
    frame_.fieldCameraHolster=allowed&&!cabin&&fieldCameraHolster&&valid(*fieldCameraHolster)
        ?fieldCameraHolster:std::nullopt;
    // Round trips through a native cabin's tolerance-accepted quaternion must
    // not accumulate scale in a resting prop, then invalidate its palm solve.
    frame_.camera=rigid(frame_.camera);frame_.marker=rigid(frame_.marker);
    for(auto& paper:frame_.papers){paper.pose=rigid(paper.pose);paper.fieldPose=rigid(paper.fieldPose);}
    frame_.hapticMask=frame_.inputMask=0;frame_.hoverKey=frame_.hoverControl=-1;
    frame_.cameraCandidateMask=0;
    frame_.capacityReached=frame_.papers.size()>=studioPaperLimit;
    frame_.freezeSupported=freezeSupported;frame_.freezeState=freeze;
    if(!freezeSupported||frame_.cameraHand<0||freeze==StudioFreeze::unavailable)frame_.freezeRequested=false;
    const float dt=last_&&now>=last_?std::min(.05f,static_cast<float>(now-last_)*.001f):0;last_=now;
    if(!initialized_){initialized_=true;frame_.camera=compose(room,Pose{{},studioCameraHome});frame_.marker={{},studioMarkerHome};
        for(size_t i=0;i<2;++i){squeezed_[i]=hands[i].squeeze>.55f;triggered_[i]=hands[i].trigger>.65f;}}
    if(!allowed){frame_.editor=-1;frame_.cameraHand=frame_.markerHand=-1;frame_.outputPaper=0;
        cameraContact_={};
        frame_.freezeRequested=false;frame_.confirmPhoto=0;frame_.confirmPaper=0;
        for(auto& paper:frame_.papers){paper.hand=-1;paper.field=false;}
        for(size_t i=0;i<2;++i){squeezed_[i]=hands[i].squeeze>.55f;triggered_[i]=hands[i].trigger>.65f;}
        return std::make_shared<const CabinStudioFrame>(frame_);}
    if(frame_.cameraHand<0){
        if(cabin){if(!cameraDropped_)frame_.camera=compose(room,Pose{{},studioCameraHome});}
        else {
            cameraDropped_=false;
            if(frame_.fieldCameraHolster)frame_.camera=*frame_.fieldCameraHolster;
            else {const auto forward=rotate(head.orientation,{0,0,-1});const float yaw=std::atan2(-forward.x,-forward.z);
                frame_.camera=compose(Pose{{0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)},head.position},Pose{{},{.27f,-.40f,.08f}});}
        }
    }
    if(newPhoto&&newPhoto!=processedPhoto_&&std::find(frame_.photos.begin(),frame_.photos.end(),newPhoto)==frame_.photos.end()
        &&frame_.photos.size()<studioPhotoLimit){
        frame_.photos.push_back(newPhoto);frame_.lastPhoto=frame_.selectedPhoto=newPhoto;
        frame_.review=false;frame_.confirmPhoto=0;++frame_.revision;print(newPhoto,!cabin);
    }
    if(newPhoto)processedPhoto_=newPhoto;
    if(cabin)for(auto& paper:frame_.papers)if(paper.field){paper.field=false;paper.hand=-1;}
    for(size_t side=0;side<2;++side){
        const auto& hand=hands[side];const bool tracking=hand.tracked&&valid(hand.grip);
        const bool squeeze=tracking&&hand.squeeze>(squeezed_[side]?.25f:.55f);
        const bool trigger=tracking&&hand.trigger>.65f,edge=trigger&&!triggered_[side];
        const auto local=compose(inverse(room),hand.grip);
        auto paperIt=std::find_if(frame_.papers.begin(),frame_.papers.end(),[&](const auto& p){return p.hand==static_cast<int>(side);});
        const bool hasPaper=paperIt!=frame_.papers.end();
        const float mountedCameraDistance=surfaceDistance(frame_.camera,studioCameraHalf,hand.grip.position);
        // The field camera already has a source-bound body mount. Contact
        // cues precede pickup; a short hand drift retains one second to grab.
        // Cabin table props retain their established handling policy.
        const auto cameraContact=cameraContact_[side].update(1,
            !cabin&&tracking&&frame_.fieldCameraHolster.has_value()&&frame_.cameraHand<0
                &&!hasPaper&&frame_.markerHand!=static_cast<int>(side),
            mountedCameraDistance<.035f*.035f,mountedCameraDistance<.15f*.15f,squeeze,now);
        if(cameraContact.pulse)frame_.hapticMask|=uint8_t(1u<<side);
        if(cameraContact.item)frame_.cameraCandidateMask|=uint8_t(1u<<side);
        if(frame_.cameraHand>=0&&frame_.cameraHand!=static_cast<int>(side)&&!hasPaper
                &&frame_.markerHand!=static_cast<int>(side)&&hand.aimTracked&&valid(hand.aim)){
            for(size_t i=0;i<static_cast<size_t>(StudioControl::count);++i){
                const auto button=static_cast<StudioControl>(i);
                const auto pose=studioControlPose(frame_.camera,button);
                if(!intersectPanel(Panel{pose,studioControlHalf.x*2,studioControlHalf.y*2,1,1},hand.aim,.8f))continue;
                frame_.hoverControl=static_cast<int>(i);frame_.inputMask|=uint8_t(1u<<side);
                if(edge){control(button,now);frame_.hapticMask|=uint8_t(1u<<side);}
                break;
            }
        }
        if(frame_.cameraHand==static_cast<int>(side)){
            frame_.inputMask|=uint8_t(1u<<side);
            if(tracking){const auto next=compose(hand.grip,attachment_[side]);
                if(dt>0)cameraVelocity_=(next.position-frame_.camera.position)*(1/dt);frame_.camera=next;}
            if(edge&&now-lastShot_>1000&&frame_.photos.size()<studioPhotoLimit&&!frame_.review&&!frame_.confirmPhoto){
                lastShot_=now;frame_.shutter=now;frame_.shutterPose=compose(frame_.camera,studioCameraLens);
                frame_.hapticMask|=uint8_t(1u<<side);}
            if(!squeeze){frame_.cameraHand=-1;cameraDropped_=cabin;frame_.freezeRequested=false;
                if(!cabin&&frame_.fieldCameraHolster){frame_.camera=*frame_.fieldCameraHolster;cameraVelocity_={};}
                frame_.confirmPhoto=0;cameraVelocity_=rotate(inverse(room).orientation,cameraVelocity_);
                for(auto& p:frame_.papers)if(p.field&&p.hand<0)p.field=false;}
        }else if(frame_.markerHand==static_cast<int>(side)){
            frame_.inputMask|=uint8_t(1u<<side);
            if(tracking){const auto next=compose(local,attachment_[side]);
                if(dt>0)markerVelocity_=(next.position-frame_.marker.position)*(1/dt);frame_.marker=next;}
            if(trigger&&tracking){
                const auto tip=compose(frame_.marker,Pose{{},{0,0,-.08f}}).position;
                bool wrote=false;
                for(auto& paper:frame_.papers){
                    const auto p=compose(inverse(paper.pose),Pose{{},tip}).position;const auto half=studioPaperHalf(paper.photo!=0);
                    if(std::abs(p.z)>.015f||std::abs(p.x)>half.x||std::abs(p.y)>half.y)continue;
                    auto ink=std::make_shared<StudioInk>(*paper.ink);
                    if((!writing_||ink->strokes.empty())&&ink->strokes.size()<128)ink->strokes.emplace_back();
                    if(ink->strokes.empty())continue;
                    auto& points=ink->strokes.back().points;
                    const StudioPoint point{.5f+p.x/(2*half.x),.5f-p.y/(2*half.y)};
                    size_t total{};for(const auto& stroke:ink->strokes)total+=stroke.points.size();
                    if(total<8192&&points.size()<1024&&(points.empty()||span(point.u-points.back().u,point.v-points.back().v,0)>.002f)){
                        points.push_back(point);++ink->revision;paper.ink=ink;++frame_.revision;}
                    wrote=true;break;
                }
                writing_=wrote;
            }else writing_=false;
            if(!squeeze){frame_.markerHand=-1;markerDropped_=true;writing_=false;}
        }else if(hasPaper){
            frame_.inputMask|=uint8_t(1u<<side);
            auto& paperPose=paperIt->field?paperIt->fieldPose:paperIt->pose;
            if(tracking){const auto next=compose(paperIt->field?hand.grip:local,attachment_[side]);
                if(dt>0)paperIt->velocity=(next.position-paperPose.position)*(1/dt);paperPose=next;}
            if(!squeeze){
                paperIt->hand=-1;const auto half=studioPaperHalf(paperIt->photo!=0);
                const auto boardLocal=compose(inverse(cabinNativeBoard),paperIt->pose);
                const auto p=boardLocal.position,normal=rotate(boardLocal.orientation,{0,0,1});
                if(cabin&&std::abs(p.z)<.045f&&std::abs(p.x)<cabinBoardHalf.x-half.x&&std::abs(p.y)<cabinBoardHalf.y-half.y&&normal.z>.75f){
                    paperIt->pose=compose(cabinNativeBoard,Pose{{},{p.x,p.y,.003f}});
                    paperIt->pinned=true;paperIt->velocity={};frame_.hapticMask|=uint8_t(1u<<side);}
                if(paperIt->field){paperIt->field=false;
                    if(frame_.editor==static_cast<int>(paperIt->id)){frame_.editor=-1;frame_.confirmPaper=0;}}
                ++frame_.revision;
            }
        }else if(squeeze&&(cabin||!squeezed_[side])){
            StudioPaper* selected{};float paperDistance=.035f*.035f;
            for(auto& paper:frame_.papers){if(paper.hand>=0||(!cabin&&!paper.field))continue;
                const auto distance=surfaceDistance(paper.field?paper.fieldPose:paper.pose,studioPaperHalf(paper.photo!=0),
                    paper.field?hand.grip.position:local.position);
                if(distance<paperDistance){paperDistance=distance;selected=&paper;}}
            const auto cameraDistance=frame_.cameraHand<0
                ?surfaceDistance(frame_.camera,studioCameraHalf,hand.grip.position):100.f;
            const auto markerDistance=cabin&&frame_.markerHand<0
                ?surfaceDistance(frame_.marker,{.008f,.008f,.08f},local.position):100.f;
            const bool cameraPickup=cabin||!frame_.fieldCameraHolster
                ?cameraDistance<.035f*.035f&&cameraDistance<paperDistance&&cameraDistance<=markerDistance
                :cameraContact.grab&&!selected;
            if(cameraPickup){
                frame_.cameraHand=static_cast<int>(side);cameraDropped_=false;
                attachment_[side]=rigid(compose(inverse(hand.grip),frame_.camera));
                // Retain the picked-up orientation and place the authored side
                // handle in the palm, leaving the lens and rear finder visible.
                attachment_[side].position=rotate(attachment_[side].orientation,
                    (side?studioCameraRightGrip:studioCameraLeftGrip)*-1.f);
                frame_.camera=compose(hand.grip,attachment_[side]);
                frame_.inputMask|=uint8_t(1u<<side);}
            else if(markerDistance<.035f*.035f&&markerDistance<paperDistance){
                frame_.markerHand=static_cast<int>(side);markerDropped_=false;
                attachment_[side]=compose(inverse(local),frame_.marker);
                // The closed pen grip surrounds its barrel. Keeping an arbitrary
                // pickup offset could leave the fingers beside the pen or at
                // its writing tip; retain orientation but seat the barrel.
                attachment_[side].position={};
                frame_.marker=compose(local,attachment_[side]);}
            else if(selected||cabin){
                if(cabin&&!selected&&near(local.position,studioNotesHome,.055f)&&frame_.papers.size()<studioPaperLimit){
                    StudioPaper paper{};paper.id=nextId_++;paper.pose={flat,studioNotesHome};paper.ink=std::make_shared<const StudioInk>();
                    frame_.papers.push_back(std::move(paper));selected=&frame_.papers.back();++frame_.revision;
                }
                if(selected){selected->hand=static_cast<int>(side);selected->pinned=false;
                    if(selected->id==frame_.outputPaper)frame_.outputPaper=0;
                    const auto grip=selected->field?hand.grip:local;
                    auto& paperPose=selected->field?selected->fieldPose:selected->pose;
                    attachment_[side]=compose(inverse(grip),paperPose);
                    const auto half=studioPaperHalf(selected->photo!=0);
                    const Vec3 paperEdge{side?-half.x-.006f:half.x+.006f,0,0};
                    attachment_[side].position=rotate(attachment_[side].orientation,paperEdge*-1.f);
                    paperPose=compose(grip,attachment_[side]);}
            }
            if(frame_.cameraHand==static_cast<int>(side)||frame_.markerHand==static_cast<int>(side)
                ||std::any_of(frame_.papers.begin(),frame_.papers.end(),[&](const auto& p){return p.hand==static_cast<int>(side);})){
                frame_.hapticMask|=uint8_t(1u<<side);frame_.inputMask|=uint8_t(1u<<side);}
        }
        // An empty hand points at a held paper to open the physical text editor.
        if(frame_.hoverControl<0&&frame_.cameraHand!=static_cast<int>(side)&&frame_.markerHand!=static_cast<int>(side)
            &&!hasPaper&&hand.aimTracked&&valid(hand.aim)){
            const auto aim=compose(inverse(room),hand.aim);
            if(frame_.editor>=0){
                frame_.inputMask|=uint8_t(1u<<side);
                for(size_t key=0;key<keys.size();++key){
                    const auto hit=intersectPanel(Panel{studioKeyPose(frame_.keyboard,key),.043f,.043f,1,1},aim,1.5f);
                    if(!hit)continue;frame_.hoverKey=static_cast<int>(key);
                    if(edge){auto found=std::find_if(frame_.papers.begin(),frame_.papers.end(),[&](const auto& p){return p.id==static_cast<uint32_t>(frame_.editor);});
                        if(found==frame_.papers.end()){frame_.editor=-1;break;}
                        if(frame_.confirmPaper){
                            if(key==43&&found->id==frame_.confirmPaper){
                                frame_.papers.erase(found);frame_.editor=-1;frame_.confirmPaper=0;++frame_.revision;
                            }else if(key==33||key==36)frame_.confirmPaper=0;
                            frame_.hapticMask|=uint8_t(1u<<side);break;
                        }
                        auto ink=std::make_shared<StudioInk>(*found->ink);const auto text=keys[key];
                        if(text=="TRASH"){frame_.confirmPaper=found->id;break;}
                        if(text=="DONE")frame_.editor=-1;
                        else if(text=="CLEAR")ink->text.clear();
                        else if(text=="BACK"){if(!ink->text.empty())ink->text.pop_back();}
                        else if(ink->text.size()<studioTextLimit)ink->text+=text=="SPACE"?" ":text;
                        ++ink->revision;found->ink=ink;++frame_.revision;frame_.hapticMask|=uint8_t(1u<<side);
                    }break;
                }
            }else if(edge){
                for(const auto& paper:frame_.papers){if(paper.hand<0)continue;
                    const auto half=studioPaperHalf(paper.photo!=0);
                    if(intersectPanel(Panel{paper.field?paper.fieldPose:paper.pose,half.x*2,half.y*2,1,1},paper.field?hand.aim:aim,1.2f)){
                        frame_.editor=static_cast<int>(paper.id);
                        frame_.confirmPaper=0;
                        const auto localHead=compose(inverse(room),head);
                        frame_.keyboard=compose(localHead,Pose{{},{0,-.08f,-.55f}});frame_.inputMask|=uint8_t(1u<<side);break;
                    }}
            }
        }
        squeezed_[side]=squeeze;triggered_[side]=trigger;
    }
    if(cabin){
        if(cameraDropped_&&frame_.cameraHand<0){auto local=compose(inverse(room),frame_.camera);
            settle(local,cameraVelocity_,dt,paperExtent(local,studioCameraHalf));frame_.camera=rigid(compose(room,local));}
        if(markerDropped_&&frame_.markerHand<0)settle(frame_.marker,markerVelocity_,dt,{.008f,.008f,.08f});
        // Settle lower sheets first so a print stack has physical thickness.
        // Previously every copy fell onto the same seat plane and overlapped.
        std::vector<StudioPaper*> loose;
        for(auto& paper:frame_.papers)if(paper.hand<0&&!paper.pinned){
            if(paper.id==frame_.outputPaper){
                paper.pose=compose(inverse(room),compose(frame_.camera,studioCameraPrintPose));
                paper.velocity={};
            }else loose.push_back(&paper);
        }
        std::stable_sort(loose.begin(),loose.end(),[](const auto* a,const auto* b){return a->pose.position.y<b->pose.position.y;});
        for(size_t i=0;i<loose.size();++i){
            auto& paper=*loose[i];const auto half=paperSupportExtent(paper);
            float support=-std::numeric_limits<float>::infinity();
            const auto next=paper.pose.position+paper.velocity*dt;
            if(std::abs(rotate(paper.pose.orientation,{0,0,1}).y)>.98f){
                for(size_t j=0;j<i;++j){
                    const auto& below=*loose[j];
                    if(std::abs(rotate(below.pose.orientation,{0,0,1}).y)<=.98f)continue;
                    const auto extent=paperSupportExtent(below);
                    const auto delta=next-below.pose.position;
                    if(std::abs(delta.x)<half.x+extent.x&&std::abs(delta.z)<half.z+extent.z
                       &&paper.pose.position.y>=below.pose.position.y-extent.y)
                        support=std::max(support,below.pose.position.y+extent.y);
                }
            }
            settle(paper.pose,paper.velocity,dt,half,support);
        }
    }else {
        for(auto& paper:frame_.papers)if(paper.field&&paper.hand<0)
            paper.fieldPose=compose(frame_.camera,studioCameraPrintPose);
        cameraDropped_=false;
    }
    return std::make_shared<const CabinStudioFrame>(frame_);
}
}
