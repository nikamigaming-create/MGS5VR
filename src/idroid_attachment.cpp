#include "mgs5vr/idroid_attachment.hpp"
#include "mgs5vr/arm_ik.hpp"
#include "mgs5vr/head_camera.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/log.hpp"
#include <windows.h>
#include <intrin.h>
#include <MinHook.h>
#include <array>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>
#include <sstream>

namespace {
using namespace mgs5vr;
using Matrix=std::array<float,16>;
using PaletteApply=void(*)(void*,uint32_t,const float*);
PaletteApply original{};
uintptr_t base{};
std::atomic_bool enabled{},traceEnabled{};
std::atomic_uint64_t eventSequence{},eventCount{},startedMs{};
constexpr uint64_t maximumEvents=4096,maximumDurationMs=45000;
struct Completion {IdroidBodyStamp stamp{};uint64_t qpc{},event{};uint32_t thread{};bool paused{};};
std::mutex completionMutex;
Completion completed{};
bool observationActive() noexcept {
    if(!enabled.load()||!traceEnabled.load()||eventCount.load()>=maximumEvents)return false;
    const auto start=startedMs.load();
    return !start||GetTickCount64()-start<=maximumDurationMs;
}
template<class T> bool read(uintptr_t address,T& value) noexcept {
    SIZE_T copied{};
    return address&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),
        &value,sizeof(value),&copied)&&copied==sizeof(value);
}
template<class T> T get(uintptr_t address) noexcept {T value{};read(address,value);return value;}
uint64_t qpc() noexcept {LARGE_INTEGER value{};QueryPerformanceCounter(&value);return static_cast<uint64_t>(value.QuadPart);}
bool finite(const Matrix& matrix) noexcept {
    return std::all_of(matrix.begin(),matrix.end(),[](float value){return std::isfinite(value);});
}
struct Owned {
    uintptr_t character{},bodyInterface{},holder{},body{},driver{},bodyPalette{};
    uintptr_t service{},component{},row{},nativeInterface{},model{},palette{},point{};
    uint32_t playerIndex{},localIndex{},handle{},flags{},pointFlags{};
    uint16_t pointState{},pointVariant{};
    uint8_t state{},recordFlags{},bindingFlags{},handsetBindingFlags{};
    Matrix wrist{},cnp{},bindingRoot{},bodyRoot{},handsetRoot{},handsetBindingRoot{},handsetRootBone{};
};
struct DisplayPublication {NativeIdroidDisplay value{};Matrix rootBone{},bindingRoot{};uint8_t bindingFlags{};};
std::optional<DisplayPublication> display;
NativeIdroidBinding binding(const Owned& owned) noexcept {
    return {owned.character,owned.service,owned.component,owned.row,owned.nativeInterface,
        owned.model,owned.palette,owned.point,owned.playerIndex,owned.localIndex,owned.handle};
}
bool inspect(uintptr_t owner,Owned& value) noexcept {
    if(get<uintptr_t>(owner)!=base+0x23b8218)return false;
    value.character=get<uintptr_t>(owner+0x370);
    if(get<uintptr_t>(value.character)!=base+0x2295210)return false;
    const auto weapon=get<uintptr_t>(value.character+0x80);
    if(get<uintptr_t>(weapon)!=base+0x23b3e80||get<uintptr_t>(weapon+8)!=value.character)return false;
    value.service=get<uintptr_t>(weapon+0x60);
    if(get<uintptr_t>(value.service)!=base+0x234b840)return false;
    value.playerIndex=get<uint32_t>(owner+0x3a0);
    std::array<uintptr_t,4> callbacks{},contexts{};
    if(!read(value.service+0x1f0,callbacks)||!read(value.service+0x210,contexts))return false;
    for(size_t i=0;i<callbacks.size();++i){
        if(callbacks[i]!=base+0x11fae20||get<uintptr_t>(contexts[i])!=base+0x23c7bc8
            ||get<uintptr_t>(contexts[i]+8)!=value.character)continue;
        if(value.component)return false; // Ambiguous current-character registration.
        value.component=contexts[i];
    }
    if(!value.component)return false;
    const auto instances=get<uintptr_t>(value.component+0x38),rows=get<uintptr_t>(value.component+0x78);
    const auto first=get<uint32_t>(instances+0x24);
    if(!instances||!rows||value.playerIndex<first||value.playerIndex-first>15)return false;
    value.row=rows+static_cast<uintptr_t>(value.playerIndex-first)*0x60;
    value.handle=get<uint32_t>(value.row+0x18);value.flags=get<uint32_t>(value.row+0x50);
    value.state=get<uint8_t>(value.row+0x10);
    const auto slot=value.handle>>16;
    if((value.handle&31u)!=17u||((value.handle>>5)&0x7ffu)!=499u||slot>255)return false;
    const auto records=get<uintptr_t>(value.service+0x140);
    if(!records||!read(records+static_cast<uintptr_t>(slot)*6+5,value.recordFlags)||!(value.recordFlags&1))return false;
    const auto count=get<uint32_t>(value.service+0x10c);
    const auto mapping=get<uintptr_t>(value.service+0x180);
    if(!count||count>256||!mapping)return false;
    bool mapped=false;
    for(uint32_t i=0;i<count;++i){
        uint8_t index{};if(!read(mapping+i,index))return false;
        if(index==slot){if(mapped)return false;value.localIndex=i;mapped=true;}
    }
    if(!mapped)return false;
    value.nativeInterface=get<uintptr_t>(value.service+0x190);
    if(get<uintptr_t>(value.nativeInterface)!=base+0x22e2970)return false;
    const auto models=get<uintptr_t>(value.nativeInterface+0x58);
    value.model=get<uintptr_t>(models+static_cast<uintptr_t>(value.localIndex)*8);
    if(!models||get<uintptr_t>(value.model)!=base+0x20f4d90
        ||get<uint64_t>(value.model+0x120)!=0x84a3472eb6970ed4ull
        ||get<int16_t>(value.model+0xf8)!=4)return false;
    constexpr std::array<uint32_t,4> expectedNames{0xf08b256e,0xf812a09e,0x3e2998c0,0x11ce7757};
    std::array<uint32_t,4> names{};
    if(!read(get<uintptr_t>(value.model+0xe8),names)||names!=expectedNames)return false;
    value.palette=get<uintptr_t>(value.model+0xe0);
    if(!read(value.model+0xfa,value.handsetBindingFlags))return false;
    value.bodyInterface=get<uintptr_t>(value.character+0x10);
    const auto fade=get<uintptr_t>(value.character+0x78);
    if(get<uintptr_t>(fade)!=base+0x23b0c70||get<uintptr_t>(fade+0x408)!=value.character
        ||get<uintptr_t>(fade+0x238)!=value.bodyInterface
        ||get<uintptr_t>(value.bodyInterface)!=base+0x22e6070)return false;
    const auto bodyFirst=get<uint32_t>(value.bodyInterface+0xc),bodyCount=get<uint32_t>(value.bodyInterface+8);
    const auto holders=get<uintptr_t>(value.bodyInterface+0x10);
    if(!holders||!bodyCount||bodyCount>16||value.playerIndex<bodyFirst||value.playerIndex-bodyFirst>=bodyCount)return false;
    value.holder=holders+static_cast<uintptr_t>(value.playerIndex-bodyFirst)*0xc0;
    value.body=get<uintptr_t>(value.holder+0x68);value.driver=get<uintptr_t>(value.holder+0x70);
    if(get<uintptr_t>(value.body)!=base+0x20f4d90||get<uintptr_t>(value.driver)!=base+0x24c1988)return false;
    const auto bones=get<int16_t>(value.body+0xf8);
    if(bones<13||bones>512)return false;
    value.bodyPalette=get<uintptr_t>(value.body+0xe0);value.bindingFlags=get<uint8_t>(value.body+0xfa);
    const auto wrappers=get<uintptr_t>(value.holder+0x80);
    const auto wrapperCount=get<uint8_t>(value.holder+0xb9);
    const uint32_t selected=value.state==3?2u:1u;
    if(!wrappers||wrapperCount<=selected||wrapperCount>128)return false;
    const auto wrapper=wrappers+selected*16;
    if(get<uintptr_t>(wrapper)!=base+0x22f6300)return false;
    value.point=get<uintptr_t>(wrapper+8);
    const auto pointCount=get<uint32_t>(value.driver+0x38);
    const auto points=get<uintptr_t>(value.driver+0x40);
    if(!points||!pointCount||pointCount>128)return false;
    bool member=false;
    for(uint32_t i=0;i<pointCount;++i)if(get<uintptr_t>(points+static_cast<uintptr_t>(i)*8)==value.point){member=true;break;}
    if(!member||get<uintptr_t>(value.point)!=base+0x24c37e8
        ||get<uint64_t>(value.point+0x10)!=0x1c68632c5c53ull
        ||get<uint32_t>(value.point+0xa0)!=0x38b1433cu||get<int16_t>(value.point+0xa8)!=12)return false;
    value.pointState=get<uint16_t>(value.point+0xa4);value.pointVariant=get<uint16_t>(value.point+0xa6);
    value.pointFlags=get<uint32_t>(value.point+0xac);
    if(value.pointVariant!=(selected==1?4u:6u))return false;
    if(!value.palette||!value.bodyPalette||!read(value.bodyPalette+12*64,value.wrist)
        ||!read(value.point+0x20,value.cnp)||!read(value.body+0xa0,value.bindingRoot)
        ||!read(value.body+0x40,value.bodyRoot)||!read(value.model+0x40,value.handsetRoot)
        ||!read(value.model+0xa0,value.handsetBindingRoot)||!read(value.palette,value.handsetRootBone))return false;
    return finite(value.wrist)&&finite(value.cnp)&&finite(value.bindingRoot)&&finite(value.bodyRoot)&&finite(value.handsetRoot)
        &&finite(value.handsetBindingRoot)&&finite(value.handsetRootBone)
        &&get<uintptr_t>(owner+0x370)==value.character&&get<uint32_t>(value.row+0x18)==value.handle
        &&get<uintptr_t>(models+static_cast<uintptr_t>(value.localIndex)*8)==value.model
        &&get<uintptr_t>(value.holder+0x68)==value.body&&get<uintptr_t>(value.holder+0x70)==value.driver
        &&get<uintptr_t>(value.body+0xe0)==value.bodyPalette&&get<uintptr_t>(value.model+0xe0)==value.palette;
}
IdroidBodyStamp stamp(const HeadCameraSample& frame,const Owned& owned) noexcept {
    return {frame.playerOwner,owned.body,owned.driver,owned.bodyPalette,frame.activation,
        frame.controllers.referenceEpoch,frame.controllers.presentationEpoch,frame.trackingSequence,
        frame.rigSequence,frame.menuGeneration,frame.controllers.predictedXrTime,frame.controllers.presentationFocused};
}
bool reserveEvent() noexcept {
    const auto now=GetTickCount64();uint64_t zero{};
    startedMs.compare_exchange_strong(zero,now);
    if(now-startedMs.load()>maximumDurationMs)return false;
    return eventCount.fetch_add(1)<maximumEvents;
}
void printMatrix(std::ostringstream& stream,const char* label,const Matrix& matrix){
    stream<<' '<<label<<'=';
    for(size_t i=0;i<matrix.size();++i){if(i)stream<<',';stream<<matrix[i];}
}
void emit(const char* kind,const Owned& owned,const IdroidBodyStamp& current,
          uint64_t eventQpc,uint64_t event,const Completion& before,const Matrix* input){
    if(!observationActive()||!reserveEvent())return;
    std::ostringstream stream;stream.precision(9);
    stream<<"Idroid attachment observe kind="<<kind<<" event="<<event<<" qpc="<<eventQpc
        <<" thread="<<GetCurrentThreadId()<<" owner="<<current.player<<" body="<<current.model
        <<" driver="<<current.driver<<" palette="<<current.palette<<" rig="<<current.rig
        <<" tracking="<<current.tracking<<" activation="<<current.activation
        <<" reference="<<current.referenceEpoch<<" focus_epoch="<<current.presentationEpoch
        <<" focused="<<current.focused<<" predicted="<<current.predictedXrTime<<" menu="<<current.menuGeneration
        <<" completed_event="<<before.event<<" completed_qpc="<<before.qpc<<" completed_rig="<<before.stamp.rig
        <<" completed_thread="<<before.thread<<" completed_paused="<<before.paused
        <<" body_completed_before="<<idroidBodyCompletedBefore(before.stamp,before.qpc,current,eventQpc)
        <<" component="<<owned.component<<" row="<<owned.row<<" state="<<static_cast<unsigned>(owned.state)
        <<" flags="<<owned.flags<<" handle="<<owned.handle<<" native_flags="<<static_cast<unsigned>(owned.recordFlags)
        <<" interface="<<owned.nativeInterface<<" local="<<owned.localIndex<<" model="<<owned.model
        <<" handset_palette="<<owned.palette<<" point="<<owned.point<<" point_state="<<owned.pointState
        <<" point_variant="<<owned.pointVariant<<" point_flags="<<owned.pointFlags
        <<" binding_flags="<<static_cast<unsigned>(owned.bindingFlags)
        <<" handset_binding_flags="<<static_cast<unsigned>(owned.handsetBindingFlags)
        <<" observation_only=1 matrix_snapshot_atomic=0";
    if(input)printMatrix(stream,"input",*input);
    printMatrix(stream,"wrist",owned.wrist);printMatrix(stream,"cnp",owned.cnp);
    printMatrix(stream,"body_root",owned.bodyRoot);printMatrix(stream,"binding_root",owned.bindingRoot);
    printMatrix(stream,"handset_root",owned.handsetRoot);
    printMatrix(stream,"handset_binding_root",owned.handsetBindingRoot);
    printMatrix(stream,"handset_root_bone",owned.handsetRootBone);log(stream.str());
}
void paletteApply(void* object,uint32_t index,const float* matrix){
    const auto returnAddress=reinterpret_cast<uintptr_t>(_ReturnAddress());
    Owned owned;Completion before;IdroidBodyStamp current{};bool admitted=false;
    if(enabled.load()&&returnAddress==base+0xdbd404)try{
        const auto eventQpc=qpc(),event=eventSequence.fetch_add(1)+1;
        const auto frame=headCamera().publishedRigFrame(steadyMilliseconds());
        Matrix input{};
        {std::lock_guard lock(completionMutex);before=completed;}
        if(frame&&inspect(frame->playerOwner,owned)&&owned.nativeInterface==reinterpret_cast<uintptr_t>(object)
            &&owned.localIndex==index&&read(reinterpret_cast<uintptr_t>(matrix),input)&&finite(input)){
            current=stamp(*frame,owned);
            admitted=idroidBodyCompletedBefore(before.stamp,before.qpc,current,eventQpc);
            emit("a8_before",owned,current,eventQpc,event,before,&input);
        }
    }catch(...){}
    original(object,index,matrix);
    if(admitted&&enabled.load())try{
        // The native call above is the only transform writer. Copy its named
        // root bone, using the exact native binding-space branch, before any
        // later body generation can be mistaken for this device publication.
        const auto frame=headCamera().publishedRigFrame(steadyMilliseconds());
        Owned after;
        if(!frame||!inspect(current.player,after)||!sameIdroidBodyPublication(current,stamp(*frame,after))
            ||!sameNativeIdroidBinding(binding(owned),binding(after))
            ||owned.state!=after.state||owned.flags!=after.flags)return;
        const auto pose=nativeAffinePose(nativeIdroidWorldMatrix(after.handsetRootBone,
            after.handsetBindingRoot,after.handsetBindingFlags));
        if(!pose)return;
        Matrix rootBone{},bindingRoot{};
        if(!read(after.palette,rootBone)||rootBone!=after.handsetRootBone
            ||!read(after.model+0xa0,bindingRoot)||bindingRoot!=after.handsetBindingRoot
            ||get<uint8_t>(after.model+0xfa)!=after.handsetBindingFlags)return;
        const auto publishedQpc=qpc(),event=eventSequence.fetch_add(1)+1;
        {std::lock_guard lock(completionMutex);
            if(completed.event!=before.event||!sameIdroidBodyPublication(completed.stamp,current))return;
            display=DisplayPublication{{*pose,current,binding(after),before.qpc,publishedQpc},
                rootBone,bindingRoot,after.handsetBindingFlags};}
        emit("a8_after",after,current,publishedQpc,event,before,nullptr);
    }catch(...){}
}
}
namespace mgs5vr {
bool idroidAttachmentDiagnosticsActive() noexcept {return enabled.load();}
std::optional<NativeIdroidDisplay> resolveNativeIdroidDisplay(const HeadCameraSample& frame) noexcept {
    if(!enabled.load()||!frame.applied||!frame.stereoTracked)return {};
    try{
        std::optional<DisplayPublication> snapshot;
        {std::lock_guard lock(completionMutex);snapshot=display;}
        if(!snapshot)return {};
        // Reject unmatched generations before doing native reads. This also
        // keeps ordinary non-iDroid rendering off the expensive owner walk.
        auto requested=snapshot->value.stamp;
        requested.player=frame.playerOwner;requested.activation=frame.activation;
        requested.referenceEpoch=frame.controllers.referenceEpoch;
        requested.presentationEpoch=frame.controllers.presentationEpoch;
        requested.tracking=frame.trackingSequence;requested.rig=frame.rigSequence;
        requested.menuGeneration=frame.menuGeneration;
        requested.predictedXrTime=frame.controllers.predictedXrTime;
        requested.focused=frame.controllers.presentationFocused;
        if(!idroidBodyCompletedBefore(snapshot->value.stamp,snapshot->value.bodyCompleteQpc,
            requested,snapshot->value.publishedQpc))return {};
        Owned owned;
        if(!inspect(frame.playerOwner,owned)||!sameIdroidBodyPublication(snapshot->value.stamp,stamp(frame,owned))
            ||!sameNativeIdroidBinding(snapshot->value.binding,binding(owned))
            ||snapshot->bindingFlags!=owned.handsetBindingFlags||snapshot->rootBone!=owned.handsetRootBone
            ||snapshot->bindingRoot!=owned.handsetBindingRoot)return {};
        std::lock_guard lock(completionMutex);
        if(!display||display->value.publishedQpc!=snapshot->value.publishedQpc
            ||!sameIdroidBodyPublication(completed.stamp,requested))return {};
        return snapshot->value;
    }catch(...){return {};}
}
void installIdroidAttachmentDiagnostics(uintptr_t moduleBase) noexcept {
    const auto requested=[](const wchar_t* name){
        wchar_t value[2]{};
        return GetEnvironmentVariableW(name,value,2)==1&&value[0]==L'1';
    };
    if(enabled.load())return;
    traceEnabled.store(requested(L"MGS5VR_IDROID_ATTACHMENT_TRACE")||requested(L"MGS5VR_IDROID_UI_BOUNDARY_TRACE"));
    base=moduleBase;
    constexpr std::array<uint8_t,22> expected{0x48,0x83,0xec,0x58,0x48,0x8b,0x41,0x58,0x8b,0xd2,
        0x48,0x8b,0x04,0xd0,0x48,0x0f,0xbf,0x88,0xf8,0,0,0};
    std::array<uint8_t,expected.size()> actual{};
    if(!read(base+0xadae00,actual)||actual!=expected
        ||get<uintptr_t>(base+0x22e2970+0xa8)!=base+0xadae00){log("Idroid attachment observation rejected native signature");return;}
    const auto address=reinterpret_cast<void*>(base+0xadae00);
    const auto created=MH_CreateHook(address,reinterpret_cast<void*>(&paletteApply),reinterpret_cast<void**>(&original));
    if(created!=MH_OK){log("Idroid attachment observation hook create failed");return;}
    if(MH_EnableHook(address)!=MH_OK){MH_RemoveHook(address);log("Idroid attachment observation hook enable failed");return;}
    enabled.store(true);
    LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);
    log("Idroid attachment read-only publication enabled qpc_frequency="+std::to_string(frequency.QuadPart)
        +" trace="+std::to_string(traceEnabled.load())+" max_events=4096 max_duration_ms=45000 transforms_unchanged=1");
}
void noteIdroidBodyComplete(uintptr_t driver,uintptr_t binding,const HeadCameraSample& frame,bool pausedRefresh) noexcept {
    if(!enabled.load()||!frame.applied||!frame.rigSequence)return;
    try{
        const auto eventQpc=qpc(),event=eventSequence.fetch_add(1)+1;
        Owned owned;
        if(!inspect(frame.playerOwner,owned)||owned.driver!=driver||owned.body+0xa0!=binding)return;
        const auto current=stamp(frame,owned);
        Completion before;
        {std::lock_guard lock(completionMutex);before=completed;
            completed={current,eventQpc,event,GetCurrentThreadId(),pausedRefresh};display.reset();}
        emit("body_complete",owned,current,eventQpc,event,before,nullptr);
    }catch(...){}
}
void stopIdroidAttachmentDiagnostics() noexcept {
    enabled.store(false);
    traceEnabled.store(false);
    std::lock_guard lock(completionMutex);completed={};display.reset();
}
}
