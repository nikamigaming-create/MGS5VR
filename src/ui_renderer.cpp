#include "mgs5vr/ui_renderer.hpp"
#include "mgs5vr/head_camera.hpp"
#include "mgs5vr/idroid_rig.hpp"
#include "mgs5vr/idroid_ui.hpp"
#include "mgs5vr/idroid_attachment.hpp"
#include "mgs5vr/ui_clip.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/control_prompts.hpp"
#include "mgs5vr/prompt_activation_policy.hpp"
#include "mgs5vr/prompt_trace_budget.hpp"
#include "mgs5vr/log.hpp"
#include <windows.h>
#include <intrin.h>
#include <MinHook.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <mutex>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <filesystem>

namespace mgs5vr {
namespace {
using QueueFn=uintptr_t(*)(void*,void*);
using ExecuteFn=uintptr_t(*)(void*,void*,void*,uint32_t);
using NodeFn=uintptr_t(*)(void*,void*);
QueueFn originalQueue{};ExecuteFn originalExecute{};NodeFn originalNode{};
using ParseTextFn=int(*)(const char*,void*,void*,uint32_t,uint32_t,float,void*,uint32_t);
ParseTextFn originalParseText{};
using PlainTextFn=bool(*)(void*,void*,const char*,bool,bool);
PlainTextFn originalPlainText{};
using MapHelpFn=void(*)(void*);
using UiCaptionFn=void(*)(void*,void*,void*,const char*,bool);
using UiOpacityFn=void(*)(void*,void*,float);
using CommonFooterUpdateFn=uintptr_t(*)(void*);
using CommonFooterCaptionFn=int(*)(void*,void*,void*,uint32_t,const char*);
MapHelpFn originalMapHelp{};
CommonFooterUpdateFn originalCommonFooterUpdate{};
CommonFooterCaptionFn originalCommonFooterCaption{};
bool controlPromptsEnabled{},mapPromptsEnabled{},mapFooterPromptsEnabled{},promptTraceEnabled{};
std::atomic_uint64_t promptCalls{},promptReplacements{},promptUnresolved{},promptPoolFull{};
std::atomic_uint64_t mapPromptReplacements{};
std::atomic_uint64_t mapPromptIconSuppressions{},mapPromptIconRestorations{};
std::atomic_uint64_t mapFooterReplacements{},mapFooterRestorations{},mapFooterUpdates{};
std::atomic_uint64_t mapFooterCaptionCalls{},mapFooterNativeCalls{},mapFooterAccepted{},mapFooterTraceCount{};
std::atomic_uint64_t mapFooterMissingScope{},mapFooterInvalidUnits{};
std::mutex promptMutex;
// The native text unit BORROWS its UTF-8 pointer. Intern immutable replacement
// strings for the module lifetime; no temporary buffers or game allocator frees.
std::unordered_set<std::string> promptTextPool;
size_t promptTextBytes{};
PromptTraceBudget promptTraces;
struct MapPromptState {
    uintptr_t owner{},node{},unit{};
    const char* nativeCaption{};
    const char* appliedCaption{};
    uintptr_t icon{};
    float iconAlpha{1.f};
    bool iconOwned{},traced{};
};
// Native Map updates can migrate between UI worker threads. Keep the owner
// lease together, while only its current setter call receives the TLS scope.
std::mutex mapPromptMutex;
MapPromptState mapPrompt;
thread_local MapPromptState* updatingMapHelp{};
struct MapFooterRow {
    uintptr_t owner{},ui{},node{},units{};
    unsigned slot{},mode{},helpId{};
    const char* nativeCaption{};
    const char* appliedCaption{};
    std::shared_ptr<const ControlBindings> bindings;
    bool vrOwned{};
};
std::mutex mapFooterMutex;
std::array<MapFooterRow,4> mapFooterRows{};
thread_local uintptr_t updatingCommonFooter{};
const char* promptText(const char* content,bool mapFooter=false);
using MarkerDepthFn=void(*)(void*);
MarkerDepthFn originalMarkerDepth{};
std::atomic_uint64_t markerDepthPreserved{};
using ModelParameterFn=void(*)(void*,uint32_t,uint32_t,const float*);
ModelParameterFn setReconParameter{};
std::atomic_uint64_t reconModelGroupsHidden{};
using TitleFn=void(*)(void*);
TitleFn originalTitleShow{},originalTitleUpdate{};
TitleFn originalStartShow{};
TitleFn originalEquipmentUpdate{},openEquipmentOverview{},closeEquipmentOverview{},stopEquipmentCloseAnimation{};
using UiLayerFn=void(*)(void*,void*);
UiLayerFn activateEquipmentLayer{};
std::atomic_uint64_t equipmentPreviewRequestedAt{};
void* equipmentPreviewOwner{}; // Accessed only on the native equipment update.
std::atomic_uintptr_t titleMenu{};
std::atomic_uint64_t titleUpdatedAt{};
std::atomic_bool titleMode{};
std::atomic_bool titleCabinMode{};
std::atomic_bool cabinPlayMode{};
std::atomic_bool sceneMenuMode{};
std::atomic_bool avatarEditMode{};
uintptr_t base{};
std::atomic_bool enabled{};
// Diagnostic source tags travel through the existing native queue join. They
// never authorize a menu draw or prolong a menu's geometry/animation lifetime.
struct UiBoundarySource {
    uintptr_t player{};
    uint64_t menuGeneration{},rigSequence{},referenceEpoch{},presentationEpoch{};
    uint32_t boundary{},sourceIndex{};
};
struct Source {
    EyeFrame eye{};
    uintptr_t camera{};
    std::array<float,16> view{};
    Pose panel{},picker{};
    bool panelTracked{},panelVisible{},choosingCategory{},itemsOpen{},commandsOpen{},menuOpen{},idroidMenu{};
    Pose menuPanel{};
    bool frontEnd{},loading{},openingSelector{},openingBackend{},avatarEditor{};
    std::array<float,16> authoredView{},authoredProjection{};
    float pickerWidth{.42f},idroidScreenWidth{.45f};
    HudMode hudMode{HudMode::binocularsOnly};
    HudView hudView{HudView::world};
    std::array<float,16> projection{};
    bool equipmentOpen{};
    bool menuPanelTracked{};
    float weaponHudSetback{.06f};
    bool firearmReticle{};
    float wristTextScale{1.5f};
    UiBoundarySource boundaryTrace{};
    IdroidUiSource idroidSource{};
    Pose idroidDisplay{};
    float idroidDisplayWidth{};
    bool idroidDisplayTracked{};
};
thread_local Source producing,executing;
std::mutex mutex;
std::unordered_map<uintptr_t,Source> pending;
struct NodeSample {
    uintptr_t node{},camera{},color{},depth{};
    uint64_t calls{},source{};
    uint32_t flags{},eye{},order{};
    std::string name;
};
std::vector<NodeSample> nodes;
std::atomic_uint64_t queued{},executions{},joined{},patched{},cameraMismatch{},viewMismatch{},expired{},overflow{},nodeCalls{};
std::atomic_uint64_t spatialDraws{};
std::atomic_uint64_t suppressedDraws{};
std::array<std::atomic_uint64_t,2> spatialByEye{},hiddenPanelByEye{};
std::filesystem::path settings;
bool wristHudEnabled{};
bool menuReaderVerified{};
bool idroidCloseReaderVerified{};
bool pauseReaderVerified{};
bool popupReaderVerified{};
bool uiBoundaryTraceEnabled{};
constexpr size_t uiBoundaryRecordLimit=8192;
constexpr uint32_t uiBoundaryLimit=4,uiBoundarySourceLimit=12;
using LayoutUpdateFn=void(*)(void*,void*);
LayoutUpdateFn originalLayoutUpdate{};
bool uiBoundaryLayoutHookVerified{};
constexpr size_t uiBoundaryLayoutLimit=512;
constexpr uint64_t uiBoundaryLayoutUpdateLimit=32768;
std::atomic_bool uiBoundaryLayoutWindow{};
std::atomic_uint64_t uiBoundaryLayoutAttempts{},uiBoundaryLayoutRejected{},uiBoundaryLayoutBudgetExhausted{};
// Numeric resource identities are resolved against the user's original pack
// in the private evidence exporter. No order/camera heuristic assigns a role.
struct UiBoundaryLayout {
    uintptr_t owner{},type{},root{},rootType{},packet{},buffer{},bufferType{},stream{},camera{};
    uint64_t resource{},qpc{};
    uint32_t flags{};
    uint8_t cameraFlags{},rootFlags{};
    std::array<unsigned char,0xe0> bytes{};
};
std::array<UiBoundaryLayout,uiBoundaryLayoutLimit> uiBoundaryLayouts{};
size_t uiBoundaryLayoutCount{};
uint64_t uiBoundaryLayoutReplacements{};
// Separate from the optional finite diagnostic trace. Native post-update
// observations identify the layout; the actual draw revalidates its complete
// owner/packet/buffer/camera chain before that identity affects routing.
struct IdroidUiOwner {UiBoundaryLayout layout{};IdroidUiOpenLease open{};};
std::mutex idroidUiMutex;
std::array<IdroidUiOwner,uiBoundaryLayoutLimit> idroidUiOwners{};
size_t idroidUiOwnerCount{};
std::atomic_uint64_t idroidUiRevision{},idroidUiClosingDraws{},idroidUiMissingDisplay{};
struct UiBoundaryGate {
    uintptr_t player{},camera{};
    uint64_t activation{},referenceEpoch{},sequence{},generation{};
    uint32_t boundaries{},sourceIndex{};
    bool initialized{},idroid{},capturing{};
} uiBoundaryGate;
enum class UiObservedRoute:uint32_t {native,menu,leftEquipment,leftHud,leftAnimated,idroidClosing,idroidUnavailable};
struct UiBoundaryRecord {
    UiBoundarySource lineage{};
    uint64_t source{},tracking{},activation{},sampleMs{},qpc{},priorGeneration{},priorSource{};
    uintptr_t node{},nodeType{},camera{},cameraType{},sourceCamera{},terminal{};
    uint32_t eye{},order{},flags{};
    UiObservedRoute route{},priorRoute{};
    bool menuOpen{},idroid{},menuTracked{},idroidDisplayTracked{},priorKnown{},nodeRead{},cameraRead{},terminalRead{};
    std::array<char,161> name{};
    std::array<unsigned char,128> nodeBytes{},cameraBytes{};
    std::array<unsigned char,256> terminalBytes{};
    std::array<float,16> nativeProjection{};
    UiBoundaryLayout layout{};
    uint32_t layoutCandidates{},layoutCurrentCandidates{},layoutCurrentFlags{};
    uint8_t layoutCurrentRootFlags{};
    bool layoutKnown{},layoutCurrentVerified{};
};
struct UiBoundaryPrior {
    uintptr_t node{},nodeType{},camera{};
    uint32_t order{};
    uint64_t generation{},source{};
    UiObservedRoute route{};
};
std::vector<UiBoundaryRecord> uiBoundaryRecords;
std::vector<UiBoundaryPrior> uiBoundaryPrior;
size_t uiBoundaryReported{};
uint64_t uiBoundaryOverflow{};
std::atomic_uint64_t uiBoundaryLayerOverflow{};
using PopupUpdateFn=void(*)(void*);
PopupUpdateFn originalPopupUpdate{};
bool popupChoiceReaderVerified{};
std::mutex popupChoiceMutex;
NativePopupChoiceLeases popupChoiceLeases;
std::atomic_int menuState{-1};
std::atomic_uint64_t pickerDrawTime{};
std::atomic_uint64_t commandsDrawTime{};
template<class T>T field(const void* p,size_t offset){T value{};std::memcpy(&value,static_cast<const unsigned char*>(p)+offset,sizeof(value));return value;}
bool read(uintptr_t p,void* output,size_t size){SIZE_T copied{};return p&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(p),output,size,&copied)&&copied==size;}
bool readBoundaryLayout(uintptr_t owner,UiBoundaryLayout& result){
    UiBoundaryLayout value{};value.owner=owner;
    if(!read(owner,value.bytes.data(),value.bytes.size()))return false;
    const auto* bytes=value.bytes.data();value.type=field<uintptr_t>(bytes,0);
    if(value.type!=base+0x2546380)return false;
    value.root=field<uintptr_t>(bytes,0x80);value.packet=field<uintptr_t>(bytes,0xc8);
    value.buffer=field<uintptr_t>(bytes,0xd8);value.resource=field<uint64_t>(bytes,0x58);
    value.flags=field<uint32_t>(bytes,0x74);value.cameraFlags=field<uint8_t>(bytes,0x4b);
    value.camera=(value.flags&(1u<<14))?0:field<uintptr_t>(bytes,(value.cameraFlags&4)?0x30:0x28);
    std::array<unsigned char,0x50> packet{};
    std::array<unsigned char,0x18> buffer{};
    std::array<unsigned char,0x10> root{};
    if(!value.resource||!read(value.root,root.data(),root.size())
        ||!read(value.packet,packet.data(),packet.size())||!read(value.buffer,buffer.data(),buffer.size())
        ||field<uintptr_t>(packet.data(),0)!=base+0x20f2e40
        ||field<uintptr_t>(packet.data(),0x40)!=value.buffer
        ||field<uintptr_t>(packet.data(),0x48)!=value.camera)return false;
    value.rootType=field<uintptr_t>(root.data(),0);value.rootFlags=field<uint8_t>(root.data(),8);
    value.bufferType=field<uintptr_t>(buffer.data(),0);value.stream=field<uintptr_t>(buffer.data(),0x10);
    // Re-read the owner links after the dependent reads. This is a bounded
    // observation, not synchronization with a native update or source pixels.
    std::array<unsigned char,0xe0> after{};
    if(!read(owner,after.data(),after.size()))return false;
    for(const auto offset:{0u,0x28u,0x30u,0x58u,0x80u,0xc8u,0xd8u})
        if(field<uint64_t>(bytes,offset)!=field<uint64_t>(after.data(),offset))return false;
    if(field<uint32_t>(after.data(),0x74)!=value.flags||field<uint8_t>(after.data(),0x4b)!=value.cameraFlags)return false;
    result=value;return true;
}
bool sameBoundaryLayout(const UiBoundaryLayout& a,const UiBoundaryLayout& b){
    return a.owner==b.owner&&a.type==b.type&&a.resource==b.resource&&a.root==b.root&&a.rootType==b.rootType
        &&a.packet==b.packet&&a.buffer==b.buffer&&a.bufferType==b.bufferType&&a.stream==b.stream&&a.camera==b.camera;
}
IdroidUiIdentity idroidUiIdentity(const UiBoundaryLayout& layout) noexcept {
    return {layout.owner,layout.root,layout.packet,layout.buffer,layout.camera,layout.resource};
}
void rememberIdroidUiOwner(const UiBoundaryLayout& layout){
    std::lock_guard lock(idroidUiMutex);
    const auto begin=idroidUiOwners.begin(),end=begin+idroidUiOwnerCount;
    auto slot=std::find_if(begin,end,[&](const auto& old){return old.layout.owner==layout.owner;});
    if(slot==end){
        if(idroidUiOwnerCount<idroidUiOwners.size())slot=begin+idroidUiOwnerCount++;
        else slot=std::min_element(begin,end,[](const auto& a,const auto& b){return a.layout.qpc<b.layout.qpc;});
    }
    if(!sameBoundaryLayout(slot->layout,layout))slot->open={};
    slot->layout=layout;++idroidUiRevision;
}
struct IdroidUiDraw {IdroidUiRole role{};bool outgoing{};};
IdroidUiDraw identifyIdroidUiDraw(void* state,void* item){
    if(!uiBoundaryLayoutHookVerified||!validIdroidUiSource(executing.idroidSource))return {};
    const auto packet=reinterpret_cast<uintptr_t>(item),buffer=field<uintptr_t>(item,0x40);
    const auto camera=field<uintptr_t>(state,0x308);
    // A native layer may issue hundreds of Map tile draws. Reuse only a
    // decision from this exact queued source eye while no layout was updated.
    struct Cached {uintptr_t packet{},buffer{},camera{};IdroidUiDraw result{};};
    struct Cache {uint64_t source{},activation{},revision{};uint32_t eye{};size_t count{};std::array<Cached,64> rows{};};
    thread_local Cache cache;
    const auto revision=idroidUiRevision.load();
    if(cache.source!=executing.eye.sourceSequence||cache.activation!=executing.eye.activation
        ||cache.eye!=executing.eye.eye||cache.revision!=revision){
        cache.source=executing.eye.sourceSequence;cache.activation=executing.eye.activation;
        cache.eye=executing.eye.eye;cache.revision=revision;cache.count=0;
    }
    for(size_t i=0;i<cache.count;++i)if(cache.rows[i].packet==packet&&cache.rows[i].buffer==buffer
        &&cache.rows[i].camera==camera)return cache.rows[i].result;
    IdroidUiDraw result{};
    {
        std::lock_guard lock(idroidUiMutex);
        IdroidUiOwner* match{};uint32_t candidates{};
        for(size_t i=0;i<idroidUiOwnerCount;++i){
            auto& row=idroidUiOwners[i];
            if(row.layout.packet==packet&&row.layout.buffer==buffer&&row.layout.camera==camera){++candidates;match=&row;}
        }
        UiBoundaryLayout current{};
        if(candidates==1&&readBoundaryLayout(match->layout.owner,current)&&sameBoundaryLayout(match->layout,current)){
            result.role=idroidUiRole(current.resource);
            const auto identity=idroidUiIdentity(current);const auto& source=executing.idroidSource;
            result.outgoing=outgoingIdroidUi(match->open,identity,source,candidates,true);
            if(result.role==IdroidUiRole::device&&source.menuOpen&&source.idroid
                &&(!sameIdroidUiOwner(match->open.source,source)||source.source>=match->open.source.source))
                match->open={identity,source};
        }
    }
    if(cache.count<cache.rows.size())cache.rows[cache.count++]={packet,buffer,camera,result};
    return result;
}
void layoutUpdate(void* object,void* transform){
    // Verified native 1dc1ae0 consumes RCX/RDX and returns void. Let it finish
    // building its draw buffer exactly once; never advance or replay a layout.
    originalLayoutUpdate(object,transform);
    if(!enabled.load())return;
    // Most layout subclasses are not the verified semantic scene type. Reject
    // them with one small read before inspecting any owner links.
    uintptr_t type{};
    if(!read(reinterpret_cast<uintptr_t>(object),&type,sizeof(type))||type!=base+0x2546380)return;
    UiBoundaryLayout value{};
    if(!readBoundaryLayout(reinterpret_cast<uintptr_t>(object),value)){++uiBoundaryLayoutRejected;return;}
    LARGE_INTEGER qpc{};QueryPerformanceCounter(&qpc);value.qpc=static_cast<uint64_t>(qpc.QuadPart);
    try{rememberIdroidUiOwner(value);}catch(...){return;}
    if(!uiBoundaryTraceEnabled||!uiBoundaryLayoutWindow.load())return;
    auto count=uiBoundaryLayoutAttempts.load();
    do{if(count>=uiBoundaryLayoutUpdateLimit){++uiBoundaryLayoutBudgetExhausted;return;}}
    while(!uiBoundaryLayoutAttempts.compare_exchange_weak(count,count+1));
    try{
        std::lock_guard lock(mutex);
        if(!uiBoundaryLayoutWindow.load())return;
        auto begin=uiBoundaryLayouts.begin(),end=begin+uiBoundaryLayoutCount;
        auto slot=std::find_if(begin,end,[&](const auto& old){return old.owner==value.owner;});
        if(slot==end){
            if(uiBoundaryLayoutCount<uiBoundaryLayouts.size())slot=begin+uiBoundaryLayoutCount++;
            else{slot=std::min_element(begin,end,[](const auto& a,const auto& b){return a.qpc<b.qpc;});++uiBoundaryLayoutReplacements;}
        }
        *slot=value;
    }catch(...){++uiBoundaryLayoutRejected;}
}
void popupUpdate(void* object){
    // Exact native 898500 consumes only RCX. Forward its complete update first;
    // this observer does not call a getter, change focus or handle an input.
    originalPopupUpdate(object);
    if(!enabled.load()||!popupChoiceReaderVerified)return;
    const auto owner=reinterpret_cast<uintptr_t>(object);uintptr_t type{};
    if(!read(owner,&type,sizeof(type))||type!=base+0x224ba40)return;
    const auto now=steadyMilliseconds();
    std::lock_guard lock(popupChoiceMutex);
    auto slot=std::find_if(popupChoiceLeases.begin(),popupChoiceLeases.end(),
        [owner](const auto& lease){return lease.owner==owner;});
    if(slot==popupChoiceLeases.end())slot=std::min_element(popupChoiceLeases.begin(),popupChoiceLeases.end(),
        [](const auto& left,const auto& right){return left.sampleMs<right.sampleMs;});
    *slot={owner,now};
}
void equipmentUpdate(void* object){
    const auto requested=equipmentPreviewRequestedAt.load(),now=steadyMilliseconds();
    const bool preview=enabled.load()&&requested&&now>=requested&&now-requested<250
        &&headCamera().active()&&menuState.load()==0;
    // Never retain a pointer across native object replacement. A previous
    // scene's owner may already have been destroyed.
    if(equipmentPreviewOwner&&equipmentPreviewOwner!=object)equipmentPreviewOwner=nullptr;
    if(equipmentPreviewOwner&&(!preview||!field<uint8_t>(object,0x2c4)||field<uint32_t>(object,0x2c0)!=3)){
        // Once native navigation advances out of idle it owns the same layer.
        // Do not tear down a category that opened between XR publications.
        if(field<uint8_t>(object,0x2c4)&&field<uint32_t>(object,0x2c0)==3)closeEquipmentOverview(object);
        equipmentPreviewOwner=nullptr;
        log("Native equipment category preview relinquished");
    }
    // Back has already released the native category. State 9 only waits for
    // its four outgoing card animations; finish those through the game's own
    // animation routine, then let the normal update complete its cleanup.
    if(preview&&stopEquipmentCloseAnimation&&field<uint8_t>(object,0x2c4)
        &&field<uint32_t>(object,0x2c0)==9)stopEquipmentCloseAnimation(object);
    originalEquipmentUpdate(object);
    if(preview&&!equipmentPreviewOwner&&openEquipmentOverview&&closeEquipmentOverview&&activateEquipmentLayer
        &&field<uint8_t>(object,0x2c4)&&field<uint32_t>(object,0x2c0)==3
        &&field<uintptr_t>(object,0x130)&&field<uintptr_t>(object,0x110)
        &&field<uintptr_t>(object,0xc0)&&field<uintptr_t>(object,0xf8)){
        // This native UI routine initializes all four current-equipment cards
        // and their own direction/help artwork. It does not send a pad event
        // or run the native category/item confirmation state machine.
        // The native menu normally sends this activation only after a category
        // input. Without it the card data exists but the UI graph stays hidden.
        activateEquipmentLayer(field<void*>(object,0xf8),field<void*>(object,0xc0));
        openEquipmentOverview(object);equipmentPreviewOwner=object;
        log("Native equipment category preview opened from idle HUD state");
    }
}
bool reconColor(uintptr_t model,uint32_t parameter,std::array<float,4>& color){
    uint32_t count{};uintptr_t materials{};
    if(!read(model+0x130,&count,sizeof(count))||count>64
        ||!read(model+0x138,&materials,sizeof(materials))||!materials)return false;
    for(uint32_t i=0;i<count;++i){
        uintptr_t material{},records{},values{};uint32_t name{},recordCount{},valueCount{};
        if(!read(materials+i*8,&material,sizeof(material))||!material
            ||!read(material+0x10,&name,sizeof(name))||name!=0xd501d11c
            ||!read(material+0x20,&recordCount,sizeof(recordCount))||recordCount>256
            ||!read(material+0x28,&records,sizeof(records))||!records
            ||!read(material+0x30,&valueCount,sizeof(valueCount))||valueCount>256
            ||!read(material+0x38,&values,sizeof(values))||!values)continue;
        for(uint32_t p=0;p<recordCount;++p){
            std::array<uint32_t,2> record{};
            if(!read(records+p*8,record.data(),sizeof(record))||record[0]!=parameter)continue;
            const auto index=record[1]&0x1fffffff;
            return (record[1]&0x20000000)&&index<valueCount
                &&read(values+index*16,color.data(),sizeof(color));
        }
    }
    return false;
}
void markerDepth(void* object){
    // The native marker updater first copies the source actor's world-space
    // bone palette, then this callback compresses only bone zero toward the
    // desktop camera's near plane. Other bones remain at the actor: the
    // triangles joining them become long wedges from a tracked eye camera.
    // Keep the complete world-space palette for skinned recon silhouettes.
    // Single-bone UI markers and non-VR presentation retain native behavior.
    if(enabled.load()&&headCamera().active()){
        const auto address=reinterpret_cast<uintptr_t>(object);
        uintptr_t type{},model{};uint16_t bones{};
        if(read(address,&type,sizeof(type))&&type==base+0x21e0540
            &&read(address+0x38,&model,sizeof(model))&&model
            &&read(model+0xf8,&bones,sizeof(bones))&&bones>1&&bones<=128){
            if(!markerDepthPreserved.fetch_add(1))log("Skinned recon marker palette kept in world space; desktop root-depth compression bypassed");
            return;
        }
    }
    originalMarkerDepth(object);
}
void titleShow(void* object){
    originalTitleShow(object);
    uintptr_t type{};
    if(read(reinterpret_cast<uintptr_t>(object),&type,sizeof(type))&&type==base+0x23d7e58){
        log("Native Title menu opened; spatial Continue panel requested");
    }
}
void titleUpdate(void* object){
    originalTitleUpdate(object);
    const auto address=reinterpret_cast<uintptr_t>(object);uintptr_t type{};
    if(read(address,&type,sizeof(type))&&type==base+0x23d7e58){
        if(!titleMenu.exchange(address))log("Native Title live update observed");
        titleUpdatedAt.store(steadyMilliseconds());
    }
}
void startShow(void* object){
    originalStartShow(object);
    uintptr_t type{};
    if(read(reinterpret_cast<uintptr_t>(object),&type,sizeof(type))&&type==base+0x23d7dd8)
        log("Native Press Start prompt opened");
}
std::string nodeName(uintptr_t node){
    uintptr_t holder{},text{};
    if(!read(node+0x58,&holder,sizeof(holder))||!read(holder,&text,sizeof(text)))return {};
    std::string result;
    for(size_t n=0;n<160;++n){char c{};if(!read(text+n,&c,1)||!c)break;result.push_back(c>=32&&c<=126?c:'?');}
    return result;
}
void tagUiBoundarySource(const HeadCameraSample& rig){
    if(!uiBoundaryTraceEnabled||producing.eye.eye>1||!producing.eye.sourceSequence)return;
    std::lock_guard lock(mutex);
    auto& gate=uiBoundaryGate;
    const bool eligible=!producing.frontEnd&&!producing.loading&&!producing.avatarEditor
        &&rig.playerOwner&&rig.controllers.referenceEpoch;
    const bool same=gate.initialized&&eligible&&gate.player==rig.playerOwner&&gate.camera==producing.camera
        &&gate.activation==producing.eye.activation&&gate.referenceEpoch==rig.controllers.referenceEpoch;
    if(!same){
        gate.initialized=eligible;gate.player=rig.playerOwner;gate.camera=producing.camera;
        gate.activation=producing.eye.activation;gate.referenceEpoch=rig.controllers.referenceEpoch;
        gate.sequence=producing.eye.sourceSequence;gate.generation=rig.menuGeneration;
        gate.idroid=producing.menuOpen&&producing.idroidMenu;gate.capturing=false;
        gate.sourceIndex=0;uiBoundaryPrior.clear();uiBoundaryLayoutCount=0;
        uiBoundaryLayoutWindow.store(false);return;
    }
    const bool idroid=producing.menuOpen&&producing.idroidMenu;
    if(producing.eye.sourceSequence<gate.sequence||rig.menuGeneration<gate.generation)return;
    if(idroid!=gate.idroid){
        // Require an observed generation change, never a wall-clock grace
        // period or the persistent native terminal +0x25 closing byte.
        gate.capturing=rig.menuGeneration>gate.generation&&gate.boundaries<uiBoundaryLimit;
        if(gate.capturing)++gate.boundaries;
        gate.sourceIndex=0;gate.idroid=idroid;
    }else if(producing.eye.sourceSequence!=gate.sequence&&gate.capturing){
        if(++gate.sourceIndex>=uiBoundarySourceLimit)gate.capturing=false;
    }
    gate.sequence=producing.eye.sourceSequence;gate.generation=rig.menuGeneration;
    uiBoundaryLayoutWindow.store(gate.capturing);
    if(gate.capturing)producing.boundaryTrace={rig.playerOwner,rig.menuGeneration,rig.rigSequence,
        rig.controllers.referenceEpoch,rig.controllers.presentationEpoch,gate.boundaries,gate.sourceIndex};
}
void traceUiBoundary(void* state,void* item,UiObservedRoute route) noexcept {
    if(!uiBoundaryTraceEnabled||!executing.boundaryTrace.boundary)return;
    try{
        struct Key {uintptr_t camera{},type{};uint32_t order{};};
        struct DrawBudget {
            uint64_t source{},activation{};uint32_t eye{};size_t count{};
            std::array<Key,64> keys{};
        };
        thread_local DrawBudget budget;
        const auto camera=field<uintptr_t>(state,0x308),address=reinterpret_cast<uintptr_t>(item);
        const auto order=field<uint32_t>(item,0x28);uintptr_t type{};read(address,&type,sizeof(type));
        if(budget.source!=executing.eye.sourceSequence||budget.activation!=executing.eye.activation
            ||budget.eye!=executing.eye.eye){
            budget.source=executing.eye.sourceSequence;budget.activation=executing.eye.activation;
            budget.eye=executing.eye.eye;budget.count=0;
        }
        // Keep one node of each camera/type/order per exact source eye. A Map
        // containing hundreds of tiles cannot consume subsequent layer slots.
        for(size_t i=0;i<budget.count;++i)if(budget.keys[i].camera==camera
            &&budget.keys[i].type==type&&budget.keys[i].order==order)return;
        if(budget.count==budget.keys.size()){++uiBoundaryLayerOverflow;return;}
        budget.keys[budget.count++]={camera,type,order};
        std::lock_guard lock(mutex);
        if(uiBoundaryRecords.size()==uiBoundaryRecordLimit){++uiBoundaryOverflow;return;}
        UiBoundaryRecord r{};r.lineage=executing.boundaryTrace;
        r.source=executing.eye.sourceSequence;r.tracking=executing.eye.trackingSequence;
        r.activation=executing.eye.activation;r.sampleMs=executing.eye.sampleTime;r.eye=executing.eye.eye;
        LARGE_INTEGER qpc{};QueryPerformanceCounter(&qpc);r.qpc=static_cast<uint64_t>(qpc.QuadPart);
        r.node=address;r.nodeType=type;r.camera=camera;r.sourceCamera=executing.camera;
        r.order=order;r.flags=field<uint32_t>(item,0x50);r.route=route;
        r.menuOpen=executing.menuOpen;r.idroid=executing.idroidMenu;r.menuTracked=executing.menuPanelTracked;
        r.idroidDisplayTracked=executing.idroidDisplayTracked;
        r.nodeRead=read(address,r.nodeBytes.data(),r.nodeBytes.size());
        r.cameraRead=read(camera,r.cameraBytes.data(),r.cameraBytes.size());
        read(camera,&r.cameraType,sizeof(r.cameraType));
        r.nativeProjection=field<std::array<float,16>>(state,0x1c0);
        const auto name=nodeName(address);std::copy(name.begin(),name.end(),r.name.begin());
        // Join only the exact packet/buffer/camera tuple observed by the
        // post-native typed-layout hook. Retain failures and ambiguity as
        // evidence; none of these observations affect UI routing.
        if(r.nodeRead)for(size_t i=0;i<uiBoundaryLayoutCount;++i){
            const auto& candidate=uiBoundaryLayouts[i];
            if(candidate.packet!=r.node||candidate.camera!=r.camera
                ||candidate.buffer!=field<uintptr_t>(r.nodeBytes.data(),0x40))continue;
            ++r.layoutCandidates;
            if(!r.layoutKnown){r.layout=candidate;r.layoutKnown=true;}
            UiBoundaryLayout current{};
            if(readBoundaryLayout(candidate.owner,current)&&sameBoundaryLayout(candidate,current)){
                ++r.layoutCurrentCandidates;r.layout=candidate;
                r.layoutCurrentFlags=current.flags;r.layoutCurrentRootFlags=current.rootFlags;
            }
        }
        r.layoutCurrentVerified=r.layoutCandidates==1&&r.layoutCurrentCandidates==1;
        // This is a contemporaneous read-only observation, NOT an atomic join
        // with the queued pixels. The immutable source menu bits remain above.
        uintptr_t system{},systemType{},terminalType{};
        if(menuReaderVerified&&read(base+0x2bf1518,&system,sizeof(system))
            &&read(system,&systemType,sizeof(systemType))&&systemType==base+0x2242d78
            &&read(system+0x7c0,&r.terminal,sizeof(r.terminal))
            &&read(r.terminal,&terminalType,sizeof(terminalType))&&terminalType==base+0x22705a8)
            r.terminalRead=read(r.terminal,r.terminalBytes.data(),r.terminalBytes.size());
        auto prior=std::find_if(uiBoundaryPrior.begin(),uiBoundaryPrior.end(),[&](const auto& p){
            return p.node==r.node&&p.nodeType==r.nodeType&&p.camera==r.camera&&p.order==r.order;
        });
        if(prior!=uiBoundaryPrior.end()&&prior->source<r.source&&prior->generation<=r.lineage.menuGeneration){
            r.priorKnown=true;r.priorRoute=prior->route;r.priorGeneration=prior->generation;r.priorSource=prior->source;
        }
        // History records observed routing only; pointer equality cannot prove
        // native terminal ownership and is never used to change presentation.
        if(r.menuOpen&&r.idroid){
            const UiBoundaryPrior value{r.node,r.nodeType,r.camera,r.order,r.lineage.menuGeneration,r.source,r.route};
            if(prior!=uiBoundaryPrior.end()){if(prior->source<r.source)*prior=value;}
            else if(uiBoundaryPrior.size()<256)uiBoundaryPrior.push_back(value);
        }
        uiBoundaryRecords.push_back(r);
    }catch(...){}
}
std::string readPromptText(const char* content){
    // Read bounded UTF-8 without crossing an allocation on a bad pointer.
    std::string source;
    for(size_t i=0;i<16385;){
        MEMORY_BASIC_INFORMATION region{};
        const auto address=reinterpret_cast<uintptr_t>(content)+i;
        if(!VirtualQuery(reinterpret_cast<void*>(address),&region,sizeof(region))){source.clear();break;}
        const auto remaining=reinterpret_cast<uintptr_t>(region.BaseAddress)+region.RegionSize-address;
        const auto count=std::min({size_t{128},remaining,size_t{16385}-i});
        std::array<char,128> bytes{};
        if(!count||!read(address,bytes.data(),count)){source.clear();break;}
        const auto end=static_cast<const char*>(std::memchr(bytes.data(),0,count));
        source.append(bytes.data(),end?static_cast<size_t>(end-bytes.data()):count);
        if(end)break;
        i+=count;
    }
    return source;
}
void tracePrompt(const char* content,const char* path,PromptTraceOwner owner){
    if(!promptTraceEnabled||!content)return;
    try{
        const auto source=readPromptText(content);
        if(owner.node&&!owner.fontKnown)owner.fontKnown=read(owner.node+0x94,&owner.font,sizeof(owner.font));
        std::lock_guard lock(promptMutex);
        if(!promptTraces.admit(path,owner,source))return;
        void* stack[6]{};const auto frames=RtlCaptureStackBackTrace(1,6,stack,nullptr);
        std::ostringstream message;message<<"VR prompt trace "<<path<<" font=";
        if(owner.fontKnown)message<<owner.font;else message<<"unknown";
        message<<" node=0x"<<std::hex<<owner.node<<" unit=0x"<<owner.unit
               <<" primary=0x"<<owner.primary<<" secondary=0x"<<owner.secondary
               <<" content=0x"<<reinterpret_cast<uintptr_t>(content)
               <<" text="<<std::quoted(source)<<" callers=";
        for(unsigned i=0;i<frames;++i)message<<(reinterpret_cast<uintptr_t>(stack[i])-base)<<',';
        log(message.str());
    }catch(...){}
}
const char* internPrompt(std::string text){
    std::lock_guard lock(promptMutex);
    auto found=promptTextPool.find(text);
    if(found==promptTextPool.end()&&promptTextPool.size()<4096&&promptTextBytes+text.size()+1<=4*1024*1024){
        promptTextBytes+=text.size()+1;
        found=promptTextPool.emplace(std::move(text)).first;
    }
    if(found!=promptTextPool.end())return found->c_str();
    ++promptPoolFull;return nullptr;
}
const char* mapCaptionText(const char* content){
    if(!content||!enabled.load()||!mapPromptsEnabled||nativeGamepadActive()
        ||!nativeMenuOpen().value_or(false))return content;
    try{
        const auto input=controlInputAudit(steadyMilliseconds());
        const auto bindings=controlPromptBindings();
        if(!bindings||!promptVrOwned(false,input&&input->context==ControlContext::nativeButtons))return content;
        const auto source=readPromptText(content);
        auto result=rewriteControlCaption(source,"DECISION",*bindings,ControlContext::menus);
        if(result.replaced)if(const auto replacement=internPrompt(std::move(result.text)))return replacement;
    }catch(...){}
    return content;
}
bool plainText(void* node,void* unit,const char* content,bool scroll,bool alternateFont){
    tracePrompt(content,"plain",{reinterpret_cast<uintptr_t>(node),reinterpret_cast<uintptr_t>(unit)});
    if(updatingMapHelp&&reinterpret_cast<uintptr_t>(node)==updatingMapHelp->node
        &&reinterpret_cast<uintptr_t>(unit)==updatingMapHelp->unit){
        updatingMapHelp->nativeCaption=content;
        const auto replacement=mapCaptionText(content);
        updatingMapHelp->appliedCaption=replacement;
        if(replacement!=content)++mapPromptReplacements;
        return originalPlainText(node,unit,replacement,scroll,alternateFont);
    }
    return originalPlainText(node,unit,content,scroll,alternateFont);
}
void mapHelp(void* object){
    // Scope the plain setter to this exact native Map help owner and its text
    // unit. Other plain captions have no recoverable action identity here.
    const auto address=reinterpret_cast<uintptr_t>(object);
    std::lock_guard ownerLock(mapPromptMutex);
    uintptr_t node{},unit{};
    if(!read(address+0x878,&node,sizeof(node))||!read(address+0x958,&unit,sizeof(unit)))return originalMapHelp(object);
    if(mapPrompt.owner!=address||mapPrompt.node!=node||mapPrompt.unit!=unit)mapPrompt={address,node,unit};
    const auto saved=updatingMapHelp;updatingMapHelp=&mapPrompt;
    originalMapHelp(object);updatingMapHelp=saved;
    uintptr_t holder{},ui{},vtable{},setter{};
    if(!read(address+0x38,&holder,sizeof(holder))||!read(holder+0x20,&ui,sizeof(ui))
        ||!read(ui,&vtable,sizeof(vtable))||vtable!=base+0x2184f90
        ||!read(vtable+0x708,&setter,sizeof(setter))||setter!=base+0x50f9e0)return;
    if(!mapPrompt.nativeCaption)read(address+0x940,&mapPrompt.nativeCaption,sizeof(mapPrompt.nativeCaption));
    // The native owner caches the source caption pointer. A neutral live
    // binding edit must also refresh a page whose native caption is unchanged.
    const auto replacement=mapCaptionText(mapPrompt.nativeCaption);
    if(replacement&&replacement!=mapPrompt.appliedCaption){
        reinterpret_cast<UiCaptionFn>(setter)(reinterpret_cast<void*>(ui),reinterpret_cast<void*>(node),reinterpret_cast<void*>(unit),replacement,true);
        mapPrompt.appliedCaption=replacement;++mapPromptReplacements;
        log("VR Map action caption refreshed: "+readPromptText(replacement).substr(0,240));
    }
    // Native f07e80 binds the Confirm image at +868 (name 6c962274),
    // separately from the translated action caption at +878 and cursor +870.
    // Suppress only that direct image while its replacement label is usable.
    uintptr_t layout{},layoutType{},root{},icon{},iconType{},iconParent{},opacity{};
    uint32_t iconName{};float alpha{};
    if(read(address+0x860,&layout,sizeof(layout))&&read(layout,&layoutType,sizeof(layoutType))
        &&layoutType==base+0x2546380&&read(layout+0x80,&root,sizeof(root))
        &&read(address+0x868,&icon,sizeof(icon))&&read(icon,&iconType,sizeof(iconType))&&iconType==base+0x2546a30
        &&read(icon+0x10,&iconParent,sizeof(iconParent))&&iconParent==root
        &&read(icon+0x6c,&iconName,sizeof(iconName))&&iconName==0x6c962274
        &&read(icon+0x5c,&alpha,sizeof(alpha))&&std::isfinite(alpha)&&alpha>=0.f&&alpha<=1.f
        &&read(vtable+0x2c8,&opacity,sizeof(opacity))&&opacity==base+0x50d420){
        if(mapPrompt.icon!=icon){mapPrompt.icon=icon;mapPrompt.iconOwned=false;}
        const bool replaced=replacement&&replacement!=mapPrompt.nativeCaption;
        if(replaced){
            if(!mapPrompt.iconOwned){mapPrompt.iconAlpha=alpha;mapPrompt.iconOwned=true;}
            reinterpret_cast<UiOpacityFn>(opacity)(reinterpret_cast<void*>(ui),reinterpret_cast<void*>(icon),0.f);
            ++mapPromptIconSuppressions;
        }else if(mapPrompt.iconOwned){
            reinterpret_cast<UiOpacityFn>(opacity)(reinterpret_cast<void*>(ui),reinterpret_cast<void*>(icon),mapPrompt.iconAlpha);
            mapPrompt.iconOwned=false;++mapPromptIconRestorations;
        }
    }
    if(promptTraceEnabled&&!mapPrompt.traced){
        mapPrompt.traced=true;
        std::ostringstream trace;trace<<"VR Map help owner="<<std::hex<<address<<" caption="<<node<<" icon_group="<<mapPrompt.icon;
        log(trace.str());
    }
}
bool mapFooterVrOwned(){
    const auto input=controlInputAudit(steadyMilliseconds());
    return promptVrOwned(nativeGamepadActive(),input&&input->context==ControlContext::nativeButtons)
        &&nativeMenuOpen().value_or(false)
        &&nativeIdroidOpen()&&!nativePauseMenuOpen();
}
struct FooterRowObservation {
    uintptr_t uiType{},setter{},node{};
    uint8_t state{},mode{};uint16_t helpId{};
    std::array<uint8_t,16> metadata{};
    unsigned refusal{};bool identityRead{},metadataRead{};
};
bool takeMapFooterTrace(){
    auto count=mapFooterTraceCount.load(std::memory_order_relaxed);
    while(count<24){
        if(mapFooterTraceCount.compare_exchange_weak(count,count+1,std::memory_order_relaxed))return true;
    }
    return false;
}
bool currentFooterRow(const MapFooterRow& saved, unsigned& mode, unsigned& helpId,
        FooterRowObservation* observed=nullptr){
    FooterRowObservation sample;
    const auto row=saved.owner+0x360+saved.slot*0x88;
    sample.identityRead=read(row+0x81,&sample.mode,sizeof(sample.mode))&&read(row+0x82,&sample.helpId,sizeof(sample.helpId));
    if(observed)sample.metadataRead=read(row+0x78,sample.metadata.data(),sample.metadata.size());
    if(!read(saved.ui,&sample.uiType,sizeof(sample.uiType))||sample.uiType!=base+0x2184f90)sample.refusal=1;
    else if(!read(sample.uiType+0x740,&sample.setter,sizeof(sample.setter))||sample.setter!=base+0x50a110)sample.refusal=2;
    else if(!read(row+0x10,&sample.node,sizeof(sample.node))||sample.node!=saved.node)sample.refusal=3;
    else if(!read(row+0x80,&sample.state,sizeof(sample.state))||sample.state!=1)sample.refusal=4;
    else if(!sample.identityRead)sample.refusal=5;
    if(observed)*observed=sample;
    if(sample.refusal)return false;
    mode=sample.mode;helpId=sample.helpId;return true;
}
__declspec(noinline) int commonFooterCaption(void* ui,void* node,void* units,uint32_t count,const char* content){
    // Exact native common-footer update -> row builder -> caption setter. The
    // unit address and Map page/table row identify the scope; localized prose
    // is never used as an action or page discriminator.
    MapFooterRow row{};
    const auto unitAddress=reinterpret_cast<uintptr_t>(units);
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const bool nativeCaller=caller==base+0x873a4b;
    if(nativeCaller||updatingCommonFooter)++mapFooterCaptionCalls;
    if(nativeCaller)++mapFooterNativeCalls;
    FooterRowObservation observation;
    unsigned refusal=1;
    if(mapFooterPromptsEnabled&&updatingCommonFooter&&content&&caller==base+0x873a4b
        &&unitAddress>=updatingCommonFooter+0xd0){
        refusal=2;
        const auto offset=unitAddress-updatingCommonFooter-0xd0;
        if(offset%0xa0==0&&offset/0xa0<4){
            row={updatingCommonFooter,reinterpret_cast<uintptr_t>(ui),reinterpret_cast<uintptr_t>(node),
                 unitAddress,static_cast<unsigned>(offset/0xa0)};
            refusal=3;
            if(currentFooterRow(row,row.mode,row.helpId,&observation)){
                refusal=4;
                if(mapFooterRowEligible(row.owner,row.node,row.units,count,row.slot,row.mode,row.helpId))refusal=0;
            }
        }
    }
    // A separate finite trace is available without enabling the broad parser.
    // Record the native call and each admission stage before any replacement;
    // a missing report is not a claim that a footer was accepted.
    if(nativeCaller&&refusal==1)++mapFooterMissingScope;
    if(nativeCaller&&refusal==2)++mapFooterInvalidUnits;
    if(nativeCaller&&nativeIdroidOpen()&&takeMapFooterTrace()){
        try{
            std::ostringstream trace;trace<<"VR Map footer admission caller_rva=0x"<<std::hex<<(caller-base)
                <<" owner=0x"<<updatingCommonFooter<<" ui=0x"<<reinterpret_cast<uintptr_t>(ui)
                <<" node=0x"<<reinterpret_cast<uintptr_t>(node)<<" units=0x"<<unitAddress
                <<" ui_type=0x"<<observation.uiType<<" setter=0x"<<observation.setter
                <<" observed_node=0x"<<observation.node<<std::dec<<" count="<<count
                <<" slot="<<row.slot<<" state="<<unsigned(observation.state)
                <<" mode="<<unsigned(observation.mode)<<" help_id="<<observation.helpId
                <<" refusal="<<refusal<<" row_refusal="<<observation.refusal
                <<" identity_read="<<observation.identityRead<<" metadata_read="<<observation.metadataRead
                <<" row_78="<<std::hex<<std::setfill('0');
            for(const auto byte:observation.metadata)trace<<std::setw(2)<<unsigned(byte);
            trace<<" caption="<<std::quoted(readPromptText(content).substr(0,240));
            log(trace.str());
        }catch(...){}
    }
    if(refusal)return originalCommonFooterCaption(ui,node,units,count,content);
    ++mapFooterAccepted;
    try{
        const auto source=readPromptText(content);
        if(source.empty()||source.size()>16384)return originalCommonFooterCaption(ui,node,units,count,content);
        // Keep exact native bytes alive for mode restoration and neutral live
        // remaps. The game's text units borrow their input pointer.
        row.nativeCaption=internPrompt(source);
        if(!row.nativeCaption)return originalCommonFooterCaption(ui,node,units,count,content);
        row.bindings=controlPromptBindings();row.vrOwned=mapFooterVrOwned();
        row.appliedCaption=promptText(row.nativeCaption,true);
    }catch(...){return originalCommonFooterCaption(ui,node,units,count,content);}
    const auto result=originalCommonFooterCaption(ui,node,units,count,row.appliedCaption);
    try{
        {
            std::lock_guard lock(mapFooterMutex);
            for(const auto& saved:mapFooterRows)if(saved.owner&&saved.owner!=row.owner){mapFooterRows={};break;}
            mapFooterRows[row.slot]=row;
        }
        if(row.appliedCaption!=row.nativeCaption)++mapFooterReplacements;
    }catch(...){}
    return result;
}
uintptr_t commonFooterUpdate(void* object){
    struct Scope {
        uintptr_t saved=updatingCommonFooter;
        explicit Scope(uintptr_t owner){updatingCommonFooter=owner;}
        ~Scope(){updatingCommonFooter=saved;}
    } scope{reinterpret_cast<uintptr_t>(object)};
    const auto result=originalCommonFooterUpdate(object);
    if(!enabled.load()||!mapFooterPromptsEnabled)return result;
    const auto updateCount=++mapFooterUpdates;
    if(updateCount==1||updateCount==256||updateCount==1024||updateCount==4096){
        try{
            std::ostringstream trace;trace<<"VR Map footer activity updates="<<updateCount
                <<" caption_calls="<<mapFooterCaptionCalls.load()<<" native_calls="<<mapFooterNativeCalls.load()
                <<" missing_scope="<<mapFooterMissingScope.load()<<" invalid_units="<<mapFooterInvalidUnits.load()
                <<" accepted="<<mapFooterAccepted.load()<<" replacements="<<mapFooterReplacements.load()
                <<" restorations="<<mapFooterRestorations.load();
            log(trace.str());
        }catch(...){}
    }
    try{
        std::array<MapFooterRow,4> rows{};
        {std::lock_guard lock(mapFooterMutex);rows=mapFooterRows;}
        if(std::none_of(rows.begin(),rows.end(),[object](const auto& row){
            return row.owner==reinterpret_cast<uintptr_t>(object)&&row.nativeCaption;
        }))return result;
        const auto bindings=controlPromptBindings();const bool vrOwned=mapFooterVrOwned();
        for(auto row:rows){
            if(row.owner!=reinterpret_cast<uintptr_t>(object)||!row.nativeCaption)continue;
            unsigned mode{},helpId{};
            if(!currentFooterRow(row,mode,helpId))continue;
            const bool sameCaption=helpId==row.helpId;
            const bool mapRow=mapFooterRowEligible(row.owner,row.node,row.units,5,row.slot,mode,helpId);
            // A changed helpId has already been replaced by the native builder.
            // If the same caption is reused on a non-Map page, restore it before
            // relinquishing the scope. No unrelated image node is suppressed.
            if(!sameCaption){
                std::lock_guard lock(mapFooterMutex);
                auto& saved=mapFooterRows[row.slot];
                if(saved.owner==row.owner&&saved.node==row.node&&saved.helpId==row.helpId)saved={};
                continue;
            }
            // Native helpId caching also applies to unchanged binding/mode
            // snapshots. Do not reparse/intern every footer on every frame.
            if(mapRow&&row.bindings==bindings&&row.vrOwned==vrOwned)continue;
            const auto replacement=mapRow?promptText(row.nativeCaption,true):row.nativeCaption;
            if(replacement!=row.appliedCaption){
                originalCommonFooterCaption(reinterpret_cast<void*>(row.ui),reinterpret_cast<void*>(row.node),
                    reinterpret_cast<void*>(row.units),5,replacement);
                if(replacement==row.nativeCaption)++mapFooterRestorations;else ++mapFooterReplacements;
            }
            std::lock_guard lock(mapFooterMutex);
            auto& saved=mapFooterRows[row.slot];
            if(saved.owner==row.owner&&saved.node==row.node&&saved.units==row.units&&saved.helpId==row.helpId){
                if(mapRow){saved.mode=mode;saved.appliedCaption=replacement;saved.bindings=bindings;saved.vrOwned=vrOwned;}
                else saved={};
            }
        }
    }catch(...){}
    return result;
}
const char* promptText(const char* content,bool mapFooter){
    if(!enabled.load()||!promptMarkupAllowed(controlPromptsEnabled,mapFooterPromptsEnabled,mapFooter)||!content)return content;
    const char* replacement=content;
    try{
        const auto bindings=controlPromptBindings();
        const auto input=controlInputAudit(steadyMilliseconds());
        const bool menuOpen=nativeMenuOpen().value_or(false);
        // Native-button mode is an escape to the original game presentation.
        // This also covers the generic parser reentry after a footer restore.
        if(bindings&&(input||menuOpen)
            &&promptVrOwned(nativeGamepadActive(),input&&input->context==ControlContext::nativeButtons)
            &&(!mapFooter||(menuOpen&&nativeIdroidOpen()&&!nativePauseMenuOpen()))){
            const auto source=readPromptText(content);
            if(source.find("<I=G=")!=std::string::npos&&source.size()<=16384){
                auto context=input?input->context:ControlContext::menus;
                if(menuOpen)context=ControlContext::menus;
                auto result=rewriteControlPrompt(source,*bindings,context);
                promptUnresolved.fetch_add(result.unresolved);
                if(result.replaced){
                    std::lock_guard lock(promptMutex);
                    auto found=promptTextPool.find(result.text);
                    if(found==promptTextPool.end()&&promptTextPool.size()<4096&&promptTextBytes+result.text.size()+1<=4*1024*1024){
                        promptTextBytes+=result.text.size()+1;
                        found=promptTextPool.emplace(std::move(result.text)).first;
                        if(promptTextPool.size()<=32)log("VR control prompt ["+std::string(controlContextName(context))+"] "+source.substr(0,160)+" -> "+found->substr(0,240));
                    }
                    if(found!=promptTextPool.end()){
                        replacement=found->c_str();promptReplacements.fetch_add(result.replaced);
                    }else ++promptPoolFull;
                }
            }
        }
    }catch(...){}
    return replacement;
}
int parseText(const char* content,void* primary,void* secondary,uint32_t font,uint32_t flags,float width,void* units,uint32_t capacity){
    // This native entry consumes action markup before measuring and splitting
    // it into text/icon units. The later plain-text setter has already lost
    // those action identities. Preserve all layout arguments and its result.
    ++promptCalls;
    tracePrompt(content,"markup",{0,reinterpret_cast<uintptr_t>(units),reinterpret_cast<uintptr_t>(primary),
                                  reinterpret_cast<uintptr_t>(secondary),font,true});
    return originalParseText(promptText(content),primary,secondary,font,flags,width,units,capacity);
}
__declspec(noinline) uintptr_t queue(void* job,void* state){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    if(!enabled.load()||caller!=base+0x2e7d98)return originalQueue(job,state);
    try{
        std::lock_guard lock(mutex);const auto key=reinterpret_cast<uintptr_t>(state)+0x120;
        // Publish before native dispatch: a worker may start inside the queue
        // call. Publishing afterwards lets it consume the preceding eye tag,
        // including incorrectly hiding the native menu capture pass.
        pending.erase(key);
        if(producing.eye.sourceSequence){
            if(pending.size()<128){pending.emplace(key,producing);++queued;}else ++overflow;
        }
    }catch(...){}
    return originalQueue(job,state);
}
struct ExecutionScope {
    Source saved=executing;
    ~ExecutionScope(){executing=saved;}
};
__declspec(noinline) uintptr_t execute(void* renderer,void* info,void* task,uint32_t worker){
    if(!enabled.load())return originalExecute(renderer,info,task,worker);
    ++executions;ExecutionScope scope;executing={};
    try{
        std::lock_guard lock(mutex);const auto found=pending.find(reinterpret_cast<uintptr_t>(info));
        if(found!=pending.end()){executing=found->second;pending.erase(found);++joined;}
    }catch(...){}
    return originalExecute(renderer,info,task,worker);
}
__declspec(noinline) uintptr_t node(void* state,void* item){
    // Hide native title rows only when the physical cassette selector belongs
    // to a verified helicopter title cabin. A fresh-start hospital backdrop
    // can remain in TitleMode without that replacement surface; keep the
    // native Start Game menu and its input path in that state.
    if(enabled.load()&&executing.eye.sourceSequence&&executing.frontEnd&&executing.openingSelector)return 0;
    if(enabled.load()&&executing.eye.sourceSequence&&!executing.menuOpen){
        const auto order=field<uint32_t>(item,0x28);
        const auto camera=field<uintptr_t>(state,0x308);
        uintptr_t cameraType{};
        const bool sceneCamera=camera==executing.camera;
        const bool layoutCamera=!sceneCamera&&read(camera,&cameraType,sizeof(cameraType))&&cameraType==base+0x20f08c8;
        std::array<float,16> layoutWorld{};
        if(layoutCamera&&read(camera+0x30,layoutWorld.data(),sizeof(layoutWorld))
            &&suppressFlatFirearmReticle(order,layoutWorld[14],executing.firearmReticle)){
            ++suppressedDraws;return 0;
        }
        const bool worldIntel=nativeReconLayer(order,sceneCamera,layoutCamera);
        // Native scene-camera target cues follow the device view. Flat HUD
        // labels are replaced by native world-position labels in the lens.
        if(executing.eye.eye==2)return sceneCamera&&worldIntel&&worldHudVisible(executing.hudMode,executing.hudView)?originalNode(state,item):0;
        if(worldIntel&&!worldHudVisible(executing.hudMode,executing.hudView))return 0;
    }
    if(enabled.load())try{
        ++nodeCalls;
        const auto address=reinterpret_cast<uintptr_t>(item);
        const auto camera=field<uintptr_t>(state,0x308);
        const auto order=field<uint32_t>(item,0x28);
        std::lock_guard lock(mutex);
        // One native layer can contain many transient map tiles. Inventory the
        // camera/layer contract, not every tile, so later menu cameras fit too.
        const bool unjoined=!executing.eye.sourceSequence;
        auto found=nodes.end();for(auto it=nodes.begin();it!=nodes.end();++it)if(it->order==order&&it->camera==camera&&(!it->source)==unjoined){found=it;break;}
        if(found==nodes.end()&&nodes.size()<96){
            nodes.push_back({address,camera,field<uintptr_t>(state,0x340),field<uintptr_t>(state,0x348),0,0,field<uint32_t>(item,0x50),0,order,nodeName(address)});
            found=nodes.end()-1;
            uintptr_t traceType{};
            const bool traceLayoutCamera=!executing.frontEnd&&camera!=executing.camera
                &&read(camera,&traceType,sizeof(traceType))&&traceType==base+0x20f08c8;
            if(traceLayoutCamera){
                std::array<float,16> layoutWorld{};uintptr_t layoutType{};
                if(read(camera,&layoutType,sizeof(layoutType))&&read(camera+0x30,layoutWorld.data(),sizeof(layoutWorld))
                    &&std::isfinite(layoutWorld[14])&&(layoutWorld[14]==100||layoutWorld[14]==135||layoutWorld[14]==150)){
                    std::ostringstream message;message<<"Native gameplay UI node order="<<order<<" depth="<<layoutWorld[14]
                        <<" camera=0x"<<std::hex<<camera<<" source_camera=0x"<<executing.camera<<std::dec
                        <<" scene="<<(camera==executing.camera)<<" eye="<<executing.eye.eye
                        <<" equipment_open="<<executing.choosingCategory<<" items_open="<<executing.itemsOpen
                        <<" commands_open="<<executing.commandsOpen<<" name="<<found->name;
                    log(message.str());
                }
            }
            if(executing.frontEnd||executing.avatarEditor){
                uintptr_t type{};std::array<float,16> world{};
                read(camera,&type,sizeof(type));read(camera+0x30,world.data(),sizeof(world));
                std::ostringstream message;message<<(executing.avatarEditor?"Avatar UI layer":"Title UI layer")
                    <<" order="<<order<<" name="<<found->name
                    <<" camera=0x"<<std::hex<<camera<<" type_rva=0x"<<(type-base)<<std::dec
                    <<" source_camera="<<(camera==executing.camera)<<" panel_tracked="<<executing.panelTracked
                    <<" world=";for(const auto f:world)message<<f<<',';
                message<<" view=";for(const auto f:field<std::array<float,16>>(state,0x200))message<<f<<',';
                message<<" projection=";for(const auto f:field<std::array<float,16>>(state,0x1c0))message<<f<<',';
                if(camera==executing.camera){
                    message<<" authored_view=";for(const auto f:executing.authoredView)message<<f<<',';
                    message<<" authored_projection=";for(const auto f:executing.authoredProjection)message<<f<<',';
                }
                log(message.str());
            }
        }
        if(found!=nodes.end()){++found->calls;found->source=executing.eye.sourceSequence;found->eye=executing.eye.eye;}
    }catch(...){}
    if(enabled.load()&&executing.eye.sourceSequence){
        const auto status=headCamera().status();const auto now=steadyMilliseconds();
        if(status.active&&status.activation==executing.eye.activation&&now>=executing.eye.sampleTime&&now-executing.eye.sampleTime<=150){
            const auto camera=field<uintptr_t>(state,0x308);
            std::array<float,16> world{};
            uintptr_t cameraType{};
            const bool layoutCamera=camera!=executing.camera
                &&read(camera,&cameraType,sizeof(cameraType))&&cameraType==base+0x20f08c8;
            // iDroid's map, tabs and lists use separate animated UI cameras.
            // Their transforms do not equal the fixed HUD layout at Z=100,
            // 135 or 150. Preserve each camera's native clip coordinates and
            // flatten every menu layer onto the same world plane. Testing the
            // HUD transform here left the main map head-locked in both eyes.
            const bool menuCamera=executing.menuOpen&&!executing.frontEnd&&camera!=executing.camera;
            const bool avatarLayout=executing.avatarEditor&&layoutCamera;
            const auto order=field<uint32_t>(item,0x28);
            const bool personalStatus=layoutCamera&&read(camera+0x30,world.data(),sizeof(world))
                &&idroidPersonalStatus(order,world[14],executing.idroidMenu);
            const auto semantic=layoutCamera?identifyIdroidUiDraw(state,item):IdroidUiDraw{};
            const bool closingDevice=semantic.outgoing;
            if((((executing.menuOpen||executing.frontEnd||avatarLayout)&&(layoutCamera||menuCamera))||closingDevice)
                &&!personalStatus){
                // A surviving typed Map draw is still device content after
                // the native terminal clears its open flag. Without an exact
                // current native display publication, discard only that known
                // outgoing content; never reclassify it as left-arm HUD.
                if(idroidUiClosingRoute(closingDevice,executing.idroidDisplayTracked)==IdroidUiClosingRoute::suppress){
                    traceUiBoundary(state,item,UiObservedRoute::idroidUnavailable);
                    ++idroidUiMissingDisplay;++suppressedDraws;return 0;
                }
                traceUiBoundary(state,item,closingDevice?UiObservedRoute::idroidClosing:UiObservedRoute::menu);
                if(executing.menuOpen&&!executing.frontEnd&&!avatarLayout&&!executing.menuPanelTracked){
                    ++suppressedDraws;return 0;
                }
                const auto saved=field<std::array<float,16>>(state,0x1c0);
                const auto layout=selectSpatialUiLayout(executing.frontEnd,executing.avatarEditor,
                    executing.menuOpen||closingDevice,executing.idroidMenu||closingDevice,true);
                const auto canvas=layoutCamera?nativeUiCanvasProjection(saved,executing.projection):saved;
                const auto panelProjection=spatialUiProjection(canvas,layout);
                const auto panel=closingDevice?executing.idroidDisplay:executing.menuPanel;
                const auto width=(executing.frontEnd||executing.avatarEditor)?1.6f
                    :closingDevice?executing.idroidDisplayWidth:executing.idroidScreenWidth;
                const auto mapped=uiPanelProjection(panelProjection,executing.view,executing.eye.view.fov,
                    panel,width,width*9.f/16.f,spatialUiPlaneCenterX(layout));
                if(!mapped){++suppressedDraws;return 0;}
                auto* output=static_cast<unsigned char*>(state)+0x1c0;
                std::memcpy(output,mapped->data(),sizeof(*mapped));
                constexpr std::array<float,16> identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
                const auto bounds=uiPanelProjection(identity,executing.view,executing.eye.view.fov,
                    panel,width,width*9.f/16.f);
                UiClipScope clip(bounds.value_or(std::array<float,16>{}));
                const auto result=originalNode(state,item);
                std::memcpy(output,saved.data(),sizeof(saved));++spatialDraws;
                if(closingDevice)++idroidUiClosingDraws;
                if(executing.eye.eye<2)++spatialByEye[executing.eye.eye];
                return result;
            }
            // The game's initial equipment selector uses the verified Z=100
            // layout camera. Route those native four-way pixels through the
            // same wrist plane so the trigger reveals the real tiles and
            // their real prompts.
            const bool nativeFourWay=executing.equipmentOpen&&!executing.frontEnd
                &&layoutCamera&&order>=133&&order<=139;
            if(nativeFourWay){
                traceUiBoundary(state,item,UiObservedRoute::leftEquipment);
                if(!executing.panelTracked){++suppressedDraws;return 0;}
                const auto saved=field<std::array<float,16>>(state,0x1c0);
                const auto savedView=field<std::array<float,16>>(state,0x200);
                const auto canvas=nativeUiCanvasProjection(saved,executing.projection);
                const auto mapped=uiPanelProjection(canvas,executing.view,executing.eye.view.fov,
                    executing.picker,executing.pickerWidth,executing.pickerWidth*9.f/16.f);
                if(mapped){
                    auto* output=static_cast<unsigned char*>(state)+0x1c0;
                    std::memcpy(output,mapped->data(),sizeof(*mapped));
                    const auto result=originalNode(state,item);
                    // This route also owns the expanded equipment camera.
                    // Publish its successful draw here; returning before the
                    // ordinary HUD route must not leave navigation locked.
                    if(order==135&&read(camera+0x30,world.data(),sizeof(world))&&world[14]==150){
                        auto previous=pickerDrawTime.load();
                        while(previous<executing.eye.sampleTime&&!pickerDrawTime.compare_exchange_weak(previous,executing.eye.sampleTime)){}
                    }
                    std::memcpy(output,saved.data(),sizeof(saved));++spatialDraws;
                    if(executing.eye.eye<2)++spatialByEye[executing.eye.eye];
                    static std::atomic_bool reported{};
                    if(!reported.exchange(true)){
                        std::ostringstream message;message<<"Native four-way equipment selector routed to wrist panel"
                            <<" result="<<result<<" panel="<<executing.picker.position.x<<','<<executing.picker.position.y<<','<<executing.picker.position.z
                            <<" view="<<savedView[0]<<','<<savedView[5]<<','<<savedView[10]<<','<<savedView[12]<<','<<savedView[13]<<','<<savedView[14]
                            <<" projection="<<saved[0]<<','<<saved[5]<<','<<saved[10]<<','<<saved[11]<<','<<saved[14];
                        log(message.str());
                    }
                    return result;
                }
                // A failed wrist projection must not reintroduce the native
                // selector in front of the player.
                ++suppressedDraws;return 0;
            }
            // These native UI cameras inhabit an artificial layout space. World
            // markers use the scene camera and retain their source eye view.
            if(layoutCamera
                &&read(camera+0x30,world.data(),sizeof(world))&&world[0]==-1&&world[5]==1&&world[10]==-1&&world[15]==1
                &&world[1]==0&&world[2]==0&&world[3]==0&&world[4]==0&&world[6]==0&&world[7]==0&&world[8]==0&&world[9]==0&&world[11]==0
                &&world[12]==0&&world[13]==0&&(world[14]==100||world[14]==135||world[14]==150)){
                traceUiBoundary(state,item,UiObservedRoute::leftHud);
                const auto layer=hudLayer(order,world[14],executing.itemsOpen,executing.commandsOpen,executing.choosingCategory);
                const bool contextAction=layer==HudLayer::context;
                // The native equipment carousel has its own layout camera at
                // Z=150. Its cards, tabs and description are orders 133..136;
                // the similarly numbered Z=100 layers are unrelated overlays.
                const bool equipmentPicker=layer==HudLayer::equipment;
                // Call uses the Z=100 layout: choices 135..137, selected action
                // and its help 138..139. Destination marks and status stay separate.
                const bool commandsPicker=layer==HudLayer::commands;
                // The initial trigger-held screen is the game's native
                // four-way selector. Keep its pixels on the same unfolded
                // wrist pose as the later native item cards.
                const bool general=layer==HudLayer::general;
                const bool expanded=equipmentPicker||commandsPicker||general||contextAction;
                // Layer 50 contains preprojected desktop labels. It cannot
                // follow head motion on a face panel. Acquired people and
                // waypoints are instead drawn from native world positions.
                // All personal HUD belongs to this source forearm. Losing the
                // wrist cannot promote prompts or menus into the player's face.
                if(layer==HudLayer::worldLabels
                    ||!executing.panelTracked||(!expanded&&!executing.panelVisible)){
                    ++suppressedDraws;
                    if((contextAction||equipmentPicker||(order>=146&&order<=148))&&executing.eye.eye<2)++hiddenPanelByEye[executing.eye.eye];
                    return 0;
                }
                const auto saved=field<std::array<float,16>>(state,0x1c0);
                const auto panel=expanded?executing.picker:executing.panel;
                // Native bottom-right HUD canvas, kept at its authored aspect.
                constexpr float statusCanvasCenterX=.72f,statusCanvasCenterY=-.70f;
                const float statusCanvasWidth=1.2f*executing.wristTextScale;
                const float layoutWidth=expanded?executing.pickerWidth*((general||contextAction)?executing.wristTextScale:1.f):statusCanvasWidth;
                const auto canvas=nativeUiCanvasProjection(saved,executing.projection);
                if(layer==HudLayer::status&&executing.eye.eye==0){
                    static std::atomic_bool reported{};
                    if(!reported.exchange(true)){
                        std::ostringstream message;message<<"Native forearm status canvas eye_aspect="<<-executing.projection[5]/executing.projection[0]
                            <<" native_xy="<<saved[0]<<','<<saved[5]<<" canvas_xy="<<canvas[0]<<','<<canvas[5]
                            <<" center="<<statusCanvasCenterX<<','<<statusCanvasCenterY
                            <<" panel="<<panel.position.x<<','<<panel.position.y<<','<<panel.position.z;
                        log(message.str());
                    }
                }
                // Positive setback moves only the compact readout toward the
                // elbow. Convert meters to canvas coordinates without moving
                // the shared forearm anchor or any expanded popup.
                const auto mapped=uiPanelProjection(canvas,executing.view,executing.eye.view.fov,panel,layoutWidth,layoutWidth*9.f/16.f,
                    contextAction?.04f:expanded?0.f:statusCanvasCenterX+executing.weaponHudSetback/(statusCanvasWidth*.5f),
                    contextAction?-.52f:expanded?0.f:statusCanvasCenterY);
                if(mapped){
                    auto* output=static_cast<unsigned char*>(state)+0x1c0;
                    std::memcpy(output,mapped->data(),sizeof(*mapped));
                    const auto result=originalNode(state,item);
                    if(commandsPicker&&order==137){
                        auto previous=commandsDrawTime.load();
                        while(previous<executing.eye.sampleTime&&!commandsDrawTime.compare_exchange_weak(previous,executing.eye.sampleTime)){}
                    }
                    if(equipmentPicker&&order==135){
                        auto previous=pickerDrawTime.load();
                        while(previous<executing.eye.sampleTime&&!pickerDrawTime.compare_exchange_weak(previous,executing.eye.sampleTime)){}
                    }
                    std::memcpy(output,saved.data(),sizeof(saved));++spatialDraws;
                    if(executing.eye.eye<2)++spatialByEye[executing.eye.eye];
                    return result;
                }
                // A failed wrist projection must not reintroduce a face HUD.
                ++suppressedDraws;return 0;
            }
            if(layoutCamera){
                traceUiBoundary(state,item,UiObservedRoute::leftAnimated);
                // Animated notification/caption cameras use the same wrist
                // popup as fixed-camera messages. Keep their native pixels.
                if(!executing.panelTracked||order==50){++suppressedDraws;return 0;}
                const auto saved=field<std::array<float,16>>(state,0x1c0);
                const auto canvas=nativeUiCanvasProjection(saved,executing.projection);
                const auto mapped=uiPanelProjection(canvas,executing.view,executing.eye.view.fov,
                    executing.picker,executing.pickerWidth*executing.wristTextScale,executing.pickerWidth*executing.wristTextScale*9.f/16.f);
                if(!mapped){++suppressedDraws;return 0;}
                auto* output=static_cast<unsigned char*>(state)+0x1c0;
                std::memcpy(output,mapped->data(),sizeof(*mapped));
                const auto result=originalNode(state,item);
                std::memcpy(output,saved.data(),sizeof(saved));++spatialDraws;
                if(executing.eye.eye<2)++spatialByEye[executing.eye.eye];
                return result;
            }
        }else{
            // A stale joined UI job may be dropped, but cannot revert to a
            // desktop HUD in the eye image while its wrist pose is unavailable.
            const auto camera=field<uintptr_t>(state,0x308);uintptr_t cameraType{};
            if(camera!=executing.camera&&read(camera,&cameraType,sizeof(cameraType))&&cameraType==base+0x20f08c8){
                ++suppressedDraws;return 0;
            }
        }
    }
    traceUiBoundary(state,item,UiObservedRoute::native);
    return originalNode(state,item);
}
}
void installUiRenderer(uintptr_t moduleBase){
    struct Hook {uintptr_t rva;void* wrapper;void** original;const unsigned char* signature;size_t size;};
    constexpr unsigned char queueEntry[]{0x40,0x57,0x48,0x83,0xec,0x30};
    constexpr unsigned char executeEntry[]{0x48,0x8b,0xc4,0x57,0x48,0x81,0xec,0x80,0x06,0,0};
    constexpr unsigned char nodeEntry[]{0x48,0x89,0x5c,0x24,0x10,0x57,0x48,0x83,0xec,0x20};
    constexpr unsigned char titleShowEntry[]{0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0x41,0x40};
    constexpr unsigned char titleUpdateEntry[]{0x40,0x56,0x48,0x83,0xec,0x40,0x48,0x8b,0xf1};
    constexpr unsigned char startShowEntry[]{0x40,0x57,0x48,0x83,0xec,0x40,0x48,0x8b,0xf9};
    constexpr unsigned char markerDepthEntry[]{0x48,0x89,0x5c,0x24,0x18,0x48,0x89,0x7c,0x24,0x20};
    constexpr unsigned char equipmentUpdateEntry[]{0x40,0x57,0x48,0x83,0xec,0x30,0x80,0xb9,0xc4,0x02,0,0,0};
    constexpr unsigned char parseTextEntry[]{0x40,0x55,0x53,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57};
    const std::array<Hook,8> hooks{{
        {0x2d3380,reinterpret_cast<void*>(&queue),reinterpret_cast<void**>(&originalQueue),queueEntry,sizeof(queueEntry)},
        {0x2e7770,reinterpret_cast<void*>(&execute),reinterpret_cast<void**>(&originalExecute),executeEntry,sizeof(executeEntry)},
        {0x2e6be0,reinterpret_cast<void*>(&node),reinterpret_cast<void**>(&originalNode),nodeEntry,sizeof(nodeEntry)},
        {0x12d73e0,reinterpret_cast<void*>(&titleShow),reinterpret_cast<void**>(&originalTitleShow),titleShowEntry,sizeof(titleShowEntry)},
        {0x12d86a0,reinterpret_cast<void*>(&titleUpdate),reinterpret_cast<void**>(&originalTitleUpdate),titleUpdateEntry,sizeof(titleUpdateEntry)},
        {0x12d6d70,reinterpret_cast<void*>(&startShow),reinterpret_cast<void**>(&originalStartShow),startShowEntry,sizeof(startShowEntry)},
        {0x6bcca0,reinterpret_cast<void*>(&markerDepth),reinterpret_cast<void**>(&originalMarkerDepth),markerDepthEntry,sizeof(markerDepthEntry)},
        {0x8b56f0,reinterpret_cast<void*>(&equipmentUpdate),reinterpret_cast<void**>(&originalEquipmentUpdate),equipmentUpdateEntry,sizeof(equipmentUpdateEntry)}}};
    for(const auto& hook:hooks){std::array<unsigned char,16> bytes{};
        if(!read(moduleBase+hook.rva,bytes.data(),hook.size)||std::memcmp(bytes.data(),hook.signature,hook.size))throw std::runtime_error("Native UI renderer signature mismatch");
    }
    base=moduleBase;
    std::array<wchar_t,32768> executable{};
    if(GetModuleFileNameW(nullptr,executable.data(),static_cast<DWORD>(executable.size())))
        settings=std::filesystem::path(executable.data()).parent_path()/L"mgs5vr.ini";
    wchar_t boundarySwitch[4]{};
    const auto boundaryOverride=GetEnvironmentVariableW(L"MGS5VR_IDROID_UI_BOUNDARY_TRACE",boundarySwitch,4);
    uiBoundaryTraceEnabled=boundaryOverride?boundaryOverride==1&&boundarySwitch[0]==L'1'
        :GetPrivateProfileIntW(L"diagnostics",L"idroid_ui_boundary_trace",0,settings.c_str())==1;
    if(uiBoundaryTraceEnabled){
        uiBoundaryRecords.reserve(uiBoundaryRecordLimit);uiBoundaryPrior.reserve(256);
        log("Read-only iDroid UI boundary trace enabled: four transitions, twelve source pairs each; camera evidence export");
    }
    {
        constexpr std::array<unsigned char,13> layoutEntry{0x4c,0x8b,0xdc,0x56,0x41,0x55,0x48,0x81,0xec,0x68,0x01,0,0};
        constexpr std::array<unsigned char,16> cameraGetter{0xf6,0x41,0x4b,0x04,0x74,0x05,0x48,0x8b,0x41,0x30,0xc3,0x48,0x8b,0x41,0x28,0xc3};
        std::array<unsigned char,13> layoutBytes{};std::array<unsigned char,16> cameraBytes{};
        const auto target=reinterpret_cast<void*>(base+0x1dc1ae0);
        if(read(base+0x1dc1ae0,layoutBytes.data(),layoutBytes.size())&&layoutBytes==layoutEntry
            &&read(base+0x1df5dc0,cameraBytes.data(),cameraBytes.size())&&cameraBytes==cameraGetter
            &&MH_CreateHook(target,reinterpret_cast<void*>(&layoutUpdate),reinterpret_cast<void**>(&originalLayoutUpdate))==MH_OK
            &&MH_EnableHook(target)==MH_OK){
            uiBoundaryLayoutHookVerified=true;
            log("Typed UI scene ownership enabled at native post-update; 512 owners, exact draw-time revalidation");
        }else{MH_DisableHook(target);log("Typed UI scene ownership unavailable; semantic closing route disabled");}
    }
    wchar_t promptSwitch[4]{};
    const auto promptOverride=GetEnvironmentVariableW(L"MGS5VR_VR_BUTTON_PROMPTS",promptSwitch,4);
    const auto promptPolicy=promptActivationPolicy(promptOverride!=0,
        promptOverride==1&&promptSwitch[0]==L'1',
        GetPrivateProfileIntW(L"diagnostics",L"vr_button_prompt_experiment",0,settings.c_str())==1,
        GetPrivateProfileIntW(L"ui",L"map_control_prompts",1,settings.c_str())==1);
    controlPromptsEnabled=promptPolicy.inlineMarkup;
    if(controlPromptsEnabled){
        std::array<unsigned char,sizeof(parseTextEntry)> textBytes{};
        const auto target=reinterpret_cast<void*>(base+0x1dc3aa0);
        if(!read(base+0x1dc3aa0,textBytes.data(),textBytes.size())||std::memcmp(textBytes.data(),parseTextEntry,textBytes.size())
           ||MH_CreateHook(target,reinterpret_cast<void*>(&parseText),reinterpret_cast<void**>(&originalParseText))!=MH_OK
           ||MH_EnableHook(target)!=MH_OK){
            MH_DisableHook(target);controlPromptsEnabled=false;
            log("VR control prompt candidate unavailable; native UI retained");
        }else log("VR control prompt candidate enabled at native input-markup parser");
    }
    wchar_t traceSwitch[4]{};
    const auto traceOverride=GetEnvironmentVariableW(L"MGS5VR_PROMPT_TRACE",traceSwitch,4);
    promptTraceEnabled=controlPromptsEnabled&&(traceOverride?traceOverride==1&&traceSwitch[0]==L'1'
        :GetPrivateProfileIntW(L"diagnostics",L"vr_prompt_trace",0,settings.c_str())==1);
    // The verified Map owner has its own ordinary admission. Failure or opt-out
    // of the unrelated experimental parser must not suppress Map's caption.
    if(promptPolicy.mapCaption||promptTraceEnabled){
        constexpr std::array<unsigned char,10> plainEntry{0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,0x24,0x18};
        std::array<unsigned char,10> bytes{};const auto target=reinterpret_cast<void*>(base+0x5102d0);
        if(!read(base+0x5102d0,bytes.data(),bytes.size())||bytes!=plainEntry
            ||MH_CreateHook(target,reinterpret_cast<void*>(&plainText),reinterpret_cast<void**>(&originalPlainText))!=MH_OK
            ||MH_EnableHook(target)!=MH_OK){MH_DisableHook(target);promptTraceEnabled=false;}
        else if(promptPolicy.mapCaption){
            // The separate Map caption is updated by this verified owner.
            std::array<unsigned char,10> mapBytes{};
            const auto mapTarget=reinterpret_cast<void*>(base+0xf0a110);
            constexpr std::array<unsigned char,16> opacityEntry{0x48,0x85,0xd2,0x74,0x0b,0x0f,0x28,0xca,0x48,0x8b,0xca,0xe9,0x40,0,0x8a,1};
            std::array<unsigned char,16> opacityBytes{};
            if(read(base+0xf0a110,mapBytes.data(),mapBytes.size())&&mapBytes==plainEntry
                &&read(base+0x50d420,opacityBytes.data(),opacityBytes.size())&&opacityBytes==opacityEntry
                &&MH_CreateHook(mapTarget,reinterpret_cast<void*>(&mapHelp),reinterpret_cast<void**>(&originalMapHelp))==MH_OK
                &&MH_EnableHook(mapTarget)==MH_OK){mapPromptsEnabled=true;log("VR Map action-help owner enabled by default; native input retains native prompts");}
            else MH_DisableHook(mapTarget);
        }
    }
    if(promptPolicy.mapCaption){
        // The lower Map footer is authored input markup, separate from the
        // f0a110 Confirm image/caption owner. Keep its caption interception
        // inside the native common-footer update and exact Map table rows.
        constexpr std::array<unsigned char,14> updateEntry{0x40,0x57,0x48,0x83,0xec,0x20,0x48,0x83,0xb9,0xc8,0,0,0,0};
        constexpr std::array<unsigned char,24> captionEntry{0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,0x24,0x18,0x57,0x48,0x83,0xec,0x70,0x49,0x8b,0xe8,0x48,0x8b,0xda,0x48,0x8b,0xf9};
        std::array<unsigned char,14> updateBytes{};std::array<unsigned char,24> captionBytes{};
        const auto updateTarget=reinterpret_cast<void*>(base+0x873d90);
        const auto captionTarget=reinterpret_cast<void*>(base+0x50a110);
        if(read(base+0x873d90,updateBytes.data(),updateBytes.size())&&updateBytes==updateEntry
            &&read(base+0x50a110,captionBytes.data(),captionBytes.size())&&captionBytes==captionEntry
            &&MH_CreateHook(updateTarget,reinterpret_cast<void*>(&commonFooterUpdate),reinterpret_cast<void**>(&originalCommonFooterUpdate))==MH_OK
            &&MH_CreateHook(captionTarget,reinterpret_cast<void*>(&commonFooterCaption),reinterpret_cast<void**>(&originalCommonFooterCaption))==MH_OK
            &&MH_EnableHook(captionTarget)==MH_OK&&MH_EnableHook(updateTarget)==MH_OK){
            mapFooterPromptsEnabled=true;
            log("VR Map footer labels enabled for verified native modes46/47; other footers retain their diagnostic policy");
        }else{
            MH_DisableHook(captionTarget);MH_DisableHook(updateTarget);
            log("VR Map footer adapter unavailable; native footer retained");
        }
    }
    constexpr std::array<unsigned char,11> overviewEntry{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57};
    constexpr std::array<unsigned char,10> overviewCloseEntry{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10};
    constexpr std::array<unsigned char,9> activateLayerEntry{0x48,0x83,0xec,0x28,0x48,0x85,0xd2,0x74,0x15};
    constexpr std::array<unsigned char,10> stopCloseEntry{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20};
    std::array<unsigned char,11> overviewBytes{};std::array<unsigned char,10> overviewCloseBytes{};
    std::array<unsigned char,9> activateLayerBytes{};
    std::array<unsigned char,10> stopCloseBytes{};
    if(read(base+0x8acdc0,overviewBytes.data(),overviewBytes.size())&&overviewBytes==overviewEntry
        &&read(base+0x8ac000,overviewCloseBytes.data(),overviewCloseBytes.size())&&overviewCloseBytes==overviewCloseEntry
        &&read(base+0x50c610,activateLayerBytes.data(),activateLayerBytes.size())&&activateLayerBytes==activateLayerEntry
        &&read(base+0x8b5020,stopCloseBytes.data(),stopCloseBytes.size())&&stopCloseBytes==stopCloseEntry){
        openEquipmentOverview=reinterpret_cast<TitleFn>(base+0x8acdc0);
        closeEquipmentOverview=reinterpret_cast<TitleFn>(base+0x8ac000);
        activateEquipmentLayer=reinterpret_cast<UiLayerFn>(base+0x50c610);
        stopEquipmentCloseAnimation=reinterpret_cast<TitleFn>(base+0x8b5020);
    }
    // Native recon material writes use this setter; group visibility is
    // prepared before scene replay and cannot independently hide each eye.
    constexpr std::array<unsigned char,10> parameterEntry{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20};
    std::array<unsigned char,10> parameterBytes{};
    if(read(base+0x1ce410,parameterBytes.data(),parameterBytes.size())&&parameterBytes==parameterEntry)
        setReconParameter=reinterpret_cast<ModelParameterFn>(base+0x1ce410);
    constexpr std::array<unsigned char,8> menuGetter{0x48,0x8b,0x05,0x11,0x18,0x39,0x02,0xc3};
    constexpr std::array<unsigned char,8> menuOpen{0x80,0x79,0x20,0,0x0f,0x95,0xc0,0xc3};
    std::array<unsigned char,8> getterBytes{},openBytes{};
    menuReaderVerified=read(base+0x85fd00,getterBytes.data(),getterBytes.size())&&getterBytes==menuGetter
        &&read(base+0x934110,openBytes.data(),openBytes.size())&&openBytes==menuOpen;
    // Supported TPP UiSystem/GetPopupSelect/IsShowPopup contract, recovered
    // independently from the owned retail implementation. IsShowPopup's
    // readiness call writes a cache byte: never invoke it from observation.
    constexpr std::array<unsigned char,8> popupSystemGetter{0x48,0x8b,0x05,0x29,0xb0,0x38,0x02,0xc3};
    constexpr std::array<unsigned char,7> popupResultGetter{0x8b,0x81,0x18,0x15,0,0,0xc3};
    constexpr std::array<unsigned char,56> popupParametersGetter{
        0x8b,0x81,0x10,0x15,0,0,0x89,0x02,0x48,0x8b,0x81,0x30,0x15,0,0,0x49,0x89,0x00,
        0x0f,0xb6,0x81,0x78,0x15,0,0,0x41,0x88,0x01,0x0f,0xb6,0x91,0x79,0x15,0,0,
        0x48,0x8b,0x44,0x24,0x28,0x88,0x10,0x48,0x8b,0x44,0x24,0x30,0x8b,0x89,0x28,0x15,0,0,0x89,0x08,0xc3};
    constexpr std::array<unsigned char,28> popupActiveEntry{
        0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xec,0x20,
        0x48,0x8b,0xd9,0x48,0x8b,0x89,0x80,0x0e,0,0,0x40,0x32,0xf6};
    constexpr std::array<unsigned char,7> popupSecondOwner{0x48,0x8b,0x8b,0x88,0x0e,0,0};
    constexpr std::array<unsigned char,13> popupReadinessEntry{
        0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x48,0x8b,0x49,0x10};
    constexpr std::array<unsigned char,6> popupReadinessWrite{0xc6,0x43,0x22,0x01,0xb0,0x01};
    std::array<unsigned char,7> popupResultBytes{};
    std::array<unsigned char,56> popupParameterBytes{};
    std::array<unsigned char,28> popupActiveBytes{};
    std::array<unsigned char,7> popupSecondOwnerBytes{};
    std::array<unsigned char,13> popupReadinessBytes{};
    std::array<unsigned char,6> popupReadinessWriteBytes{};
    popupReaderVerified=menuReaderVerified
        &&read(base+0x866910,getterBytes.data(),getterBytes.size())&&getterBytes==popupSystemGetter
        &&read(base+0x866a70,popupResultBytes.data(),popupResultBytes.size())&&popupResultBytes==popupResultGetter
        &&read(base+0x866aa0,popupParameterBytes.data(),popupParameterBytes.size())&&popupParameterBytes==popupParametersGetter
        &&read(base+0x867460,popupActiveBytes.data(),popupActiveBytes.size())&&popupActiveBytes==popupActiveEntry
        &&read(base+0x86749e,popupSecondOwnerBytes.data(),popupSecondOwnerBytes.size())&&popupSecondOwnerBytes==popupSecondOwner
        &&read(base+0x934120,popupReadinessBytes.data(),popupReadinessBytes.size())&&popupReadinessBytes==popupReadinessEntry
        &&read(base+0x93414d,popupReadinessWriteBytes.data(),popupReadinessWriteBytes.size())&&popupReadinessWriteBytes==popupReadinessWrite;
    // Owned native adapter: 897271 writes popup vtable 224BA40; 897C9E
    // establishes its exact UiSystem backlink. 8985BD copies current dialog
    // parameters, and 897859/897BB6 consume/update the live selected index.
    // Failure disables only choice observation and preserves ordinary UI.
    const auto choiceGuard=[&](uintptr_t rva,const auto& expected){
        auto actual=expected;actual.fill(0);
        return read(base+rva,actual.data(),actual.size())&&actual==expected;
    };
    popupChoiceReaderVerified=popupReaderVerified
        &&choiceGuard(0x898500,std::array<unsigned char,15>{0x80,0xb9,0xc0,0,0,0,0,0x0f,0x85,3,0,0,0,0xf3,0xc3})
        &&choiceGuard(0x897271,std::array<unsigned char,10>{0x48,0x8d,0x05,0xc8,0x47,0x9b,0x01,0x48,0x89,0x03})
        &&choiceGuard(0x897c9e,std::array<unsigned char,12>{0xe8,0x6d,0xec,0xfc,0xff,0x48,0x89,0x85,0xe8,0,0,0})
        &&choiceGuard(0x8985bd,std::array<unsigned char,19>{0x48,0x8d,0x93,0xc8,0,0,0,0x4c,0x8d,0x8b,0xc2,0,0,0,0xe8,0xd0,0xe4,0xfc,0xff})
        &&choiceGuard(0x897859,std::array<unsigned char,19>{0x48,0x63,0x85,0xcc,0,0,0,0x48,0x8d,0x0c,0x80,0x80,0xbc,0xcd,0xb8,0x03,0,0,0})
        &&choiceGuard(0x897bb6,std::array<unsigned char,21>{0x44,0x39,0xa5,0xcc,0,0,0,0x74,0x11,0x44,0x89,0xa5,0xcc,0,0,0,0xb2,0x01,0x48,0x8b,0xcd})
        &&choiceGuard(0x8983e2,std::array<unsigned char,39>{0x89,0x87,0xcc,0,0,0,0x48,0x8b,0x8f,0xd8,0x03,0,0,0x48,0x8d,0x97,0x70,0x01,0,0,0x41,0xb1,0x01,0x4c,0x8b,0xc5,0x88,0x9f,0xb8,0x03,0,0,0x44,0x88,0xb7,0xe0,0x03,0,0});
    if(popupChoiceReaderVerified){
        const auto target=reinterpret_cast<void*>(base+0x898500);
        const auto status=MH_CreateHook(target,reinterpret_cast<void*>(&popupUpdate),reinterpret_cast<void**>(&originalPopupUpdate));
        if(status!=MH_OK)popupChoiceReaderVerified=false;
        else if(MH_EnableHook(target)!=MH_OK){
            MH_DisableHook(target);MH_RemoveHook(target);popupChoiceReaderVerified=false;
        }
    }
    log(popupChoiceReaderVerified?"Native popup choice observer installed; read-only current choice, completed result distinct"
                                  :"Native popup choice observer unavailable; current choice remains unknown");
    // CloseMbDvcTerminal writes the terminal's deferred close byte at +0x25.
    // Back at the root sets this same byte before the stow animation runs.
    constexpr std::array<unsigned char,4> requestClose{0xc6,0x41,0x25,0x01};
    std::array<unsigned char,4> requestCloseBytes{};
    idroidCloseReaderVerified=menuReaderVerified
        &&read(base+0x8cecc7,requestCloseBytes.data(),requestCloseBytes.size())&&requestCloseBytes==requestClose;
    constexpr std::array<unsigned char,8> sequenceGetter{0x48,0x8b,0x05,0xf9,0x98,0x69,0x02,0xc3};
    constexpr std::array<unsigned char,6> closePause{0x89,0x43,0x38,0x89,0x43,0x48};
    std::array<unsigned char,6> closeBytes{};
    pauseReaderVerified=read(base+0x53a280,getterBytes.data(),getterBytes.size())&&getterBytes==sequenceGetter
        &&read(base+0x5391ca,closeBytes.data(),closeBytes.size())&&closeBytes==closePause;
    wristHudEnabled=GetPrivateProfileIntW(L"diagnostics",L"wrist_hud_experiment",0,settings.c_str())==1;
    for(const auto& hook:hooks){const auto result=MH_CreateHook(reinterpret_cast<void*>(base+hook.rva),hook.wrapper,hook.original);
        if(result!=MH_OK)throw std::runtime_error(std::string("Native UI hook: ")+MH_StatusToString(result));
    }
    for(const auto& hook:hooks)if(MH_EnableHook(reinterpret_cast<void*>(base+hook.rva))!=MH_OK){
        for(const auto& installed:hooks)MH_DisableHook(reinterpret_cast<void*>(base+installed.rva));
        throw std::runtime_error("Cannot enable native UI hooks");
    }
    enabled.store(true);log("Native UI worker lineage installed; left-forearm HUD="+std::to_string(wristHudEnabled)+"; menus remain spatial");
}
bool nativeTitleMenuOpen() noexcept {
    if(!enabled.load()||!titleMode.load())return false;
    const auto object=titleMenu.load();uintptr_t type{};
    // The reset callback and state zero also occur during entry. Only a
    // current native update establishes that this menu owns the front end.
    const auto updated=titleUpdatedAt.load(),now=steadyMilliseconds();
    return updated&&now>=updated&&now-updated<=250
        &&object&&read(object,&type,sizeof(type))&&type==base+0x23d7e58;
}
void publishNativeAvatarEdit(bool active) noexcept {avatarEditMode.store(active);}
bool nativeAvatarEditActive() noexcept {return avatarEditMode.load();}
void publishNativeDemoMode(NativeDemoMode mode,uint64_t candidateKey) noexcept {
    publishNativeDemoSnapshot(mode,candidateKey,steadyMilliseconds());
}
NativeDemoMode nativeDemoMode() noexcept {return nativeDemoSnapshot().mode;}
uint64_t nativeDemoCandidateKey() noexcept {return nativeDemoSnapshot().candidateKey;}
uint64_t nativeDemoCandidateGeneration() noexcept {return nativeDemoSnapshot().generation;}
bool nativeScriptedDemoActive() noexcept {
    const auto mode=nativeDemoSnapshot().mode;
    return mode==NativeDemoMode::cinematic||mode==NativeDemoMode::interactiveLook
        ||(mode==NativeDemoMode::staleCandidate&&!headCamera().staleDemoRecoveryReady());
}
void publishNativeTitleMode(bool active) noexcept {titleMode.store(active);}
bool nativeTitleModeActive() noexcept {return titleMode.load();}
void publishNativeTitleCabinMode(bool active) noexcept {titleCabinMode.store(active);}
bool nativeTitleCabinMode() noexcept {return titleCabinMode.load();}
void publishNativeCabinPlay(bool active) noexcept {cabinPlayMode.store(active);}
bool nativeCabinPlay() noexcept {return cabinPlayMode.load();}
void publishNativeSceneMenu(bool active) noexcept {sceneMenuMode.store(active);}
bool nativeLoadingTipsOpen() noexcept {
    if(!enabled.load()||!menuReaderVerified)return false;
    // IsEndLoadingTips obtains this UiSystem and its loading-tip terminal.
    // The terminal's native open flag remains set while Resume Game is waiting.
    uintptr_t system{},type{},terminal{};uint8_t open{};
    return read(base+0x2bf1940,&system,sizeof(system))&&system
        &&read(system,&type,sizeof(type))&&type==base+0x22447e8
        &&read(system+0xe90,&terminal,sizeof(terminal))&&terminal
        &&read(terminal,&type,sizeof(type))&&type==base+0x2273628
        &&read(terminal+0x20,&open,sizeof(open))&&open==1;
}
std::optional<bool> nativeMenuOpen() noexcept {
    if(!enabled.load()||!menuReaderVerified)return {};
    uintptr_t system{},type{},terminal{};uint8_t open{};
    if(!read(base+0x2bf1518,&system,sizeof(system)))return {};
    if(system){
        if(!read(system,&type,sizeof(type))||type!=base+0x2242d78
            ||!read(system+0x7c0,&terminal,sizeof(terminal)))return {};
        if(terminal&&(!read(terminal,&type,sizeof(type))||type!=base+0x22705a8
            ||!read(terminal+0x20,&open,sizeof(open))))return {};
    }
    bool paused=false;
    if(pauseReaderVerified){
        uintptr_t manager{},pause{};uint32_t kind{};
        if(!read(base+0x2bd3b80,&manager,sizeof(manager)))return {};
        if(manager){
            if(!read(manager,&type,sizeof(type))||type!=base+0x218fba0
                ||!read(manager+8,&pause,sizeof(pause)))return {};
            if(pause){
                if(!read(pause,&type,sizeof(type))||type!=base+0x218f648
                    ||!read(pause+0x38,&kind,sizeof(kind)))return {};
                // TppPauseMenu's native open handler assigns the menu kind;
                // its close handler clears it. Menu-button intent is not a
                // substitute: holding that button inside iDroid opens Help.
                paused=kind>0&&kind<=0x80;
            }
        }
    }
    const int next=(open?1:0)|(paused?2:0)|(sceneMenuMode.load()?4:0);
    if(menuState.exchange(next)!=next)try{log("Native menu state="+std::to_string(next)+" (iDroid=1, pause=2, scene menu=4)");}catch(...){}
    return next!=0;
}
bool nativeIdroidOpen() noexcept {
    const auto state=menuState.load();
    return state>=0&&(state&1)!=0;
}
bool nativePauseMenuOpen() noexcept {
    const auto state=menuState.load();
    return state>=0&&(state&2)!=0;
}
bool nativeIdroidClosing() noexcept {
    if(!enabled.load()||!idroidCloseReaderVerified)return true;
    uintptr_t system{},terminal{},type{};uint8_t closing{};
    if(!read(base+0x2bf1518,&system,sizeof(system))||!system
       ||!read(system,&type,sizeof(type))||type!=base+0x2242d78
       ||!read(system+0x7c0,&terminal,sizeof(terminal))||!terminal
       ||!read(terminal,&type,sizeof(type))||type!=base+0x22705a8
       ||!read(terminal+0x25,&closing,sizeof(closing)))return true;
    return closing!=0;
}
std::optional<unsigned> nativeIdroidTutorialMode() noexcept {
    if(!enabled.load()||!menuReaderVerified)return {};
    uintptr_t system{},tutorial{},type{};uint8_t mode{};
    if(!read(base+0x2bf1518,&system,sizeof(system))||!system
       ||!read(system,&type,sizeof(type))||type!=base+0x2242d78
       ||!read(system+0x90,&tutorial,sizeof(tutorial))||!tutorial
       ||!read(tutorial,&type,sizeof(type))||type!=base+0x2272810
       ||!read(tutorial+8,&mode,sizeof(mode))||mode>16)return {};
    return mode;
}
NativePopupSnapshot nativePopupSnapshot() noexcept {
    NativePopupChoiceLeases leases;
    if(enabled.load()&&popupChoiceReaderVerified){std::lock_guard lock(popupChoiceMutex);leases=popupChoiceLeases;}
    return readNativePopupSnapshot(base,enabled.load()&&popupReaderVerified,
        [](uintptr_t address,void* output,size_t size){return read(address,output,size);},leases,steadyMilliseconds());
}
bool releaseNativeIdroidTutorialPause() noexcept {
    const auto mode=nativeIdroidTutorialMode();
    if(!mode||*mode!=0||nativePauseMenuOpen())return false;
    uintptr_t system{},tutorial{},handle{},type{};uint32_t level{};
    if(!read(base+0x2bf1518,&system,sizeof(system))||!system
       ||!read(system+0x90,&tutorial,sizeof(tutorial))||!tutorial
       ||!read(tutorial,&type,sizeof(type))||type!=base+0x2272810
       ||!read(tutorial+0xd0,&handle,sizeof(handle))
       ||handle<base+0x2b65d58||handle>=base+0x2b65d98||(handle-(base+0x2b65d58))%4
       ||!read(base+0x2b65d40,&type,sizeof(type))||type!=base+0x20c56c0
       ||!read(handle,&level,sizeof(level))||(level&0x7fffffff)!=0x7fffffde)return false;
    constexpr std::array<unsigned char,14> signature{0x4c,0x8b,0x81,0xd0,0,0,0,0x41,0x8b,0,0xc1,0xe8,0x1f,0x24};
    std::array<unsigned char,signature.size()> actual{};
    if(!read(base+0x932960,actual.data(),actual.size())||actual!=signature)return false;
    // The native class's pause(bool) changes only its own allocated handle's
    // active bit. The native manager recomputes the combined mask next frame.
    // Keep the handle allocated so a future real lesson can pause normally.
    using Pause=void(*)(void*,bool);
    reinterpret_cast<Pause>(base+0x932960)(reinterpret_cast<void*>(tutorial),false);
    return read(handle,&level,sizeof(level))&&(level&0x80000000)==0;
}
uint64_t nativeEquipmentPickerDrawTime() noexcept {return enabled.load()?pickerDrawTime.load():0;}
void requestNativeEquipmentPreview(bool visible) noexcept {equipmentPreviewRequestedAt.store(visible?steadyMilliseconds():0);}
uint64_t nativeCommandsDrawTime() noexcept {return enabled.load()?commandsDrawTime.load():0;}
std::optional<Pose> wristPickerPose(const HeadCameraSample& rig) noexcept{
    if(!rig.wristPanelTracked)return {};
    const auto head=nativeTrackedPose(rig.nativePose,rig.headPose,rig.headPose);
    return wristPopupPose(rig.wristPanel,head,rig.controllers.wristSelectorHeight);
}
void setUiRenderSource(const EyeFrame& eye,uintptr_t camera,const std::array<float,16>& view,const std::array<float,16>& projection,const HeadCameraSample& rig,
    const std::array<float,16>& authoredView,const std::array<float,16>& authoredProjection,HudView hudView){
    const std::array<Pose,2> eyes{nativeEyePose(rig.nativePose,rig.headPose,rig.views[0].pose),
                                nativeEyePose(rig.nativePose,rig.headPose,rig.views[1].pose)};
    // Keep status flat along the forearm and keep the native picker on that
    // same wrist origin. The iDroid gets its own tracked screen pose.
    const auto picker=wristPickerPose(rig);
    const auto idroid=trackedIdroidPose(rig);
    const bool handMenu=rig.menuOpen&&rig.menuIdroid&&rig.controllers.handheldMenus&&idroid.has_value();
    const bool quadMenu=rig.menuOpen&&rig.menuWorldQuad;
    const bool startup=rig.controllers.frontEnd||rig.controllers.avatarEditor;
    const auto menuPanel=(startup||quadMenu)?rig.menuPanel:handMenu?idroid->screen:picker.value_or(Pose{});
    producing={eye,camera,view,rig.wristPanel,picker.value_or(Pose{}),picker.has_value(),
               rig.wristPanelTracked&&panelFacesBothEyes(rig.wristPanel,eyes),
               rig.controllers.equipmentOpen&&!rig.controllers.equipmentCategory,
               rig.controllers.equipmentCategory==4,rig.controllers.commandControls,
                      rig.menuOpen,rig.menuIdroid,menuPanel,rig.controllers.frontEnd,rig.controllers.loading,
                      rig.controllers.openingSelector,rig.controllers.openingBackend,rig.controllers.avatarEditor,
                      authoredView,authoredProjection,
                rig.controllers.wristPickerWidth,quadMenu?rig.controllers.menuQuadWidth:handMenu?rig.controllers.idroidScreenWidth:rig.controllers.wristPickerWidth};
    producing.loading=rig.controllers.loading;
    producing.openingSelector=rig.controllers.openingSelector;
    producing.hudMode=rig.controllers.hudMode;
    producing.hudView=hudView;
    producing.projection=projection;
    producing.equipmentOpen=rig.controllers.equipmentOpen;
    producing.weaponHudSetback=rig.controllers.weaponHudSetback;
    producing.wristTextScale=rig.controllers.wristTextScale;
    producing.firearmReticle=rig.nativeFirearmActive&&rig.controllers.weaponReady;
    producing.menuPanelTracked=startup||quadMenu||(rig.menuIdroid?handMenu:picker.has_value());
    producing.idroidSource={rig.playerOwner,rig.activation,rig.controllers.referenceEpoch,
        rig.controllers.presentationEpoch,rig.menuGeneration,eye.sourceSequence,
        rig.controllers.presentationFocused,rig.controllers.handheldMenus,rig.menuOpen,rig.menuIdroid};
    if(!rig.menuOpen&&!startup&&!rig.controllers.loading&&validIdroidUiSource(producing.idroidSource)){
        if(const auto native=resolveNativeIdroidDisplay(rig)){
            // Reuse only the display calibration, not the interactive pose or
            // readiness. The helper joins the authored native device to this
            // exact completed rig publication, including its focus epoch.
            auto displayFrame=rig;displayFrame.idroidDevice=native->body;displayFrame.idroidDeviceTracked=true;
            if(const auto display=trackedIdroidPose(displayFrame)){
                producing.idroidDisplay=display->screen;producing.idroidDisplayTracked=true;
                producing.idroidDisplayWidth=rig.controllers.idroidScreenWidth;
            }
        }
    }
    if(!wristHudEnabled){
        // An explicit off-wrist preference changes the HUD mount, never the
        // stereo routing or the independent right-hand iDroid screen.
        const auto head=nativeTrackedPose(rig.nativePose,rig.headPose,rig.headPose);
        const float tilt=rig.controllers.menuQuadTilt*.00872664626f;
        const auto panel=compose(head,Pose{{std::sin(tilt),0,0,std::cos(tilt)},
            {0,0,-rig.controllers.menuQuadDistance}});
        producing.panel=producing.picker=panel;
        producing.panelTracked=producing.panelVisible=valid(panel);
        producing.pickerWidth=rig.controllers.menuQuadWidth;
    }
    tagUiBoundarySource(rig);
}
void clearUiRenderSource() noexcept {producing={};}
ReconModelVisibilityScope::ReconModelVisibilityScope(HudMode mode,HudView view,bool glow) noexcept {
    if(!enabled.load()||!headCamera().active()||!setReconParameter
        ||reconModelVisible(mode,view,glow))return;
    uintptr_t manager{},collection{},type{},data{};uint32_t count{};
    if(!read(base+0x2be4c60,&manager,sizeof(manager))||!manager
        ||!read(manager+0x58,&collection,sizeof(collection))||!collection
        ||!read(collection,&type,sizeof(type))||type!=base+0x21f7f48
        ||!read(collection+0x98,&data,sizeof(data))||!data
        ||!read(collection+0xa8,&count,sizeof(count))||count>128)return;
    std::array<uintptr_t,128> objects{};
    if(!read(data,objects.data(),count*sizeof(uintptr_t)))return;
    for(uint32_t i=0;i<count&&count_<changes_.size();++i){
        const auto owner=objects[i];uintptr_t model{};uint16_t bones{};
        // Never hide the source actor (+0x28), the whole mixed collector, or
        // the occlusion-query proxy (+0x40). Only the verified recon clone.
        if(!owner||!read(owner,&type,sizeof(type))||type!=base+0x21e0540
            ||!read(owner+0x38,&model,sizeof(model))||!model
            ||!read(model,&type,sizeof(type))||type!=base+0x20f4d90
            ||!read(model+0xf8,&bones,sizeof(bones))||bones<2||bones>128)continue;
        // Both native tint vectors carry their opacity in W. Do not change
        // the source actor, native marking state, fade timer or RGB values.
        for(const uint32_t parameter:{0x9224571eu,0x19bdfc21u}){
            alignas(16) std::array<float,4> color{};
            if(count_==changes_.size()||!reconColor(model,parameter,color)||!color[3])continue;
            changes_[count_++]={owner,model,parameter,color};color[3]=0;
            setReconParameter(reinterpret_cast<void*>(model),0xd501d11c,parameter,color.data());
            if(!reconModelGroupsHidden.fetch_add(1))log("Native recon body excluded from unaided scene pass; source actor preserved");
        }
    }
}
ReconModelVisibilityScope::~ReconModelVisibilityScope(){
    for(size_t i=0;i<count_;++i){
        const auto& c=changes_[i];uintptr_t type{},model{};
        if(!read(c.owner,&type,sizeof(type))||type!=base+0x21e0540
            ||!read(c.owner+0x38,&model,sizeof(model))||model!=c.model
            ||!read(model,&type,sizeof(type))||type!=base+0x20f4d90)continue;
        std::array<float,4> current{},hidden=c.color;hidden[3]=0;
        if(!reconColor(model,c.parameter,current)||current!=hidden)continue;
        alignas(16) const auto restored=c.color;
        setReconParameter(reinterpret_cast<void*>(model),0xd501d11c,c.parameter,restored.data());
    }
}
bool applyUiEyeProjection(float* output) noexcept {
    if(!enabled.load()||!executing.eye.sourceSequence||!executing.eye.projected)return false;
    const auto status=headCamera().status();const auto now=steadyMilliseconds();
    if(!status.active||status.activation!=executing.eye.activation||now<executing.eye.sampleTime||now-executing.eye.sampleTime>150){++expired;return false;}
    // Native UI context stores this output at +0x1c0, with its selected
    // GrCamera at +0x308 and the view used for this UI draw at +0x200.
    const auto* state=reinterpret_cast<const unsigned char*>(output)-0x1c0;
    if(field<uintptr_t>(state,0x308)!=executing.camera){++cameraMismatch;return false;}
    // The job belongs to this exact scene/eye and camera, but the shared native
    // GrCamera may already have been restored (or changed to the other eye)
    // before its worker runs. Publish the immutable view captured at enqueue.
    // Never read the current camera here to repair an older UI job.
    const bool restoredView=std::memcmp(state+0x200,executing.view.data(),sizeof(executing.view))!=0;
    // Carry the complete eye projection, including its native depth mapping.
    // Replacing only X/Y FOV on a worker-built matrix retains whichever near
    // plane the shared camera has now (often the restored desktop plane).
    if(restoredView){
        std::memcpy(reinterpret_cast<unsigned char*>(output)-0x1c0+0x200,executing.view.data(),sizeof(executing.view));
        ++viewMismatch;
    }
    std::memcpy(output,executing.projection.data(),sizeof(executing.projection));++patched;return true;
}
void reportUiRenderer(std::ostream& out){
    const auto popupSampleMs=steadyMilliseconds();
    const auto popup=nativePopupSnapshot();
    std::lock_guard lock(mutex);
    if(uiBoundaryTraceEnabled){
        const auto hexBytes=[&](const auto& bytes){
            constexpr char digits[]="0123456789abcdef";out<<'"';
            for(const auto b:bytes)out<<digits[b>>4]<<digits[b&15];out<<'"';
        };
        const auto route=[](UiObservedRoute value){switch(value){
            case UiObservedRoute::menu:return "menu";
            case UiObservedRoute::leftEquipment:return "left_equipment";
            case UiObservedRoute::leftHud:return "left_hud";
            case UiObservedRoute::leftAnimated:return "left_animated";
            case UiObservedRoute::idroidClosing:return "idroid_closing";
            case UiObservedRoute::idroidUnavailable:return "idroid_display_unavailable";
            default:return "native";
        }};
        LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);
        out<<"{\"event\":\"idroid_ui_boundary_trace_status\",\"schema\":2,\"image_base\":"<<base
            <<",\"qpc_frequency\":"<<frequency.QuadPart<<",\"transitions\":"<<uiBoundaryGate.boundaries
            <<",\"records\":"<<uiBoundaryRecords.size()<<",\"overflow\":"<<uiBoundaryOverflow
            <<",\"draws_after_layer_limit\":"<<uiBoundaryLayerOverflow.load()
            <<",\"transition_limit\":"<<uiBoundaryLimit<<",\"source_pair_limit\":"<<uiBoundarySourceLimit
            <<",\"layer_limit_per_source_eye\":64,\"native_snapshots_joined_to_source\":false"
            <<",\"layout_hook_verified\":"<<(uiBoundaryLayoutHookVerified?"true":"false")
            <<",\"layout_update_attempts\":"<<uiBoundaryLayoutAttempts.load()
            <<",\"layout_update_rejected\":"<<uiBoundaryLayoutRejected.load()
            <<",\"layout_update_limit\":"<<uiBoundaryLayoutUpdateLimit
            <<",\"layout_updates_after_limit\":"<<uiBoundaryLayoutBudgetExhausted.load()
            <<",\"layout_owner_count\":"<<uiBoundaryLayoutCount<<",\"layout_owner_limit\":"<<uiBoundaryLayoutLimit
            <<",\"layout_owner_replacements\":"<<uiBoundaryLayoutReplacements
            <<",\"layout_observations_atomic_with_pixels\":false}\n";
        for(;uiBoundaryReported<uiBoundaryRecords.size();++uiBoundaryReported){
            const auto& r=uiBoundaryRecords[uiBoundaryReported];
            out<<"{\"event\":\"idroid_ui_boundary_node\",\"record\":"<<uiBoundaryReported
                <<",\"boundary\":"<<r.lineage.boundary<<",\"boundary_source_index\":"<<r.lineage.sourceIndex
                <<",\"source_sequence\":"<<r.source<<",\"tracking_sequence\":"<<r.tracking
                <<",\"activation\":"<<r.activation<<",\"sample_ms\":"<<r.sampleMs<<",\"observed_qpc\":"<<r.qpc
                <<",\"eye\":"<<r.eye<<",\"player\":"<<r.lineage.player<<",\"rig_sequence\":"<<r.lineage.rigSequence
                <<",\"menu_generation\":"<<r.lineage.menuGeneration<<",\"reference_epoch\":"<<r.lineage.referenceEpoch
                <<",\"presentation_epoch\":"<<r.lineage.presentationEpoch
                <<",\"source_menu_open\":"<<(r.menuOpen?"true":"false")
                <<",\"source_idroid\":"<<(r.idroid?"true":"false")
                <<",\"source_menu_tracked\":"<<(r.menuTracked?"true":"false")
                <<",\"source_idroid_display_tracked\":"<<(r.idroidDisplayTracked?"true":"false")
                <<",\"selected_route\":"<<std::quoted(route(r.route))
                <<",\"prior_same_node_route_known\":"<<(r.priorKnown?"true":"false")
                <<",\"prior_same_node_route\":"<<std::quoted(route(r.priorRoute))
                <<",\"prior_same_node_menu_generation\":"<<r.priorGeneration
                <<",\"prior_same_node_source_sequence\":"<<r.priorSource
                <<",\"node\":"<<r.node<<",\"node_type\":"<<r.nodeType<<",\"camera\":"<<r.camera
                <<",\"camera_type\":"<<r.cameraType<<",\"source_camera\":"<<r.sourceCamera
                <<",\"order\":"<<r.order<<",\"flags\":"<<r.flags<<",\"name\":"<<std::quoted(r.name.data())
                <<",\"node_read\":"<<(r.nodeRead?"true":"false")<<",\"node_bytes\":";hexBytes(r.nodeBytes);
            out<<",\"camera_read\":"<<(r.cameraRead?"true":"false")<<",\"camera_bytes\":";hexBytes(r.cameraBytes);
            out<<",\"observed_terminal\":"<<r.terminal<<",\"terminal_read\":"<<(r.terminalRead?"true":"false")
                <<",\"terminal_bytes\":";hexBytes(r.terminalBytes);
            out<<",\"layout_known\":"<<(r.layoutKnown?"true":"false")
                <<",\"layout_current_verified\":"<<(r.layoutCurrentVerified?"true":"false")
                <<",\"layout_candidates\":"<<r.layoutCandidates<<",\"layout_current_candidates\":"<<r.layoutCurrentCandidates
                <<",\"layout_owner\":"<<r.layout.owner<<",\"layout_type\":"<<r.layout.type
                <<",\"layout_resource_pathcode\":"<<r.layout.resource<<",\"layout_update_qpc\":"<<r.layout.qpc
                <<",\"layout_root\":"<<r.layout.root<<",\"layout_root_type\":"<<r.layout.rootType
                <<",\"layout_root_flags\":"<<unsigned(r.layout.rootFlags)<<",\"layout_flags\":"<<r.layout.flags
                <<",\"layout_camera_flags\":"<<unsigned(r.layout.cameraFlags)
                <<",\"layout_current_flags\":"<<r.layoutCurrentFlags<<",\"layout_current_root_flags\":"<<unsigned(r.layoutCurrentRootFlags)
                <<",\"layout_packet\":"<<r.layout.packet<<",\"layout_buffer\":"<<r.layout.buffer
                <<",\"layout_buffer_type\":"<<r.layout.bufferType<<",\"layout_stream\":"<<r.layout.stream
                <<",\"layout_camera\":"<<r.layout.camera<<",\"layout_update_bytes\":";hexBytes(r.layout.bytes);
            out<<",\"native_projection\":[";
            for(size_t i=0;i<r.nativeProjection.size();++i){if(i)out<<',';
                if(std::isfinite(r.nativeProjection[i]))out<<r.nativeProjection[i];else out<<"null";}
            out<<"]}\n";
        }
    }
    out<<"{\"event\":\"native_ui_renderer\",\"queued\":"<<queued.load()<<",\"executions\":"<<executions.load()
       <<",\"joined\":"<<joined.load()<<",\"projection_patched\":"<<patched.load()<<",\"camera_mismatch\":"<<cameraMismatch.load()
       <<",\"view_restored\":"<<viewMismatch.load()<<",\"spatial_draws\":"<<spatialDraws.load()<<",\"suppressed_draws\":"<<suppressedDraws.load()
       <<",\"expired\":"<<expired.load()<<",\"overflow\":"<<overflow.load()
       <<",\"marker_depth_preserved\":"<<markerDepthPreserved.load()
       <<",\"recon_model_groups_hidden\":"<<reconModelGroupsHidden.load()
       <<",\"idroid_ui_owner_hook\":"<<(uiBoundaryLayoutHookVerified?"true":"false")
       <<",\"idroid_ui_closing_draws\":"<<idroidUiClosingDraws.load()
       <<",\"idroid_ui_missing_display\":"<<idroidUiMissingDisplay.load()
       <<",\"vr_control_prompts\":"<<(controlPromptsEnabled?"true":"false")
       <<",\"prompt_calls\":"<<promptCalls.load()
       <<",\"prompt_replacements\":"<<promptReplacements.load()<<",\"prompt_unresolved\":"<<promptUnresolved.load()
       <<",\"prompt_pool_full\":"<<promptPoolFull.load()
       <<",\"map_control_prompts\":"<<(mapPromptsEnabled?"true":"false")<<",\"map_prompt_replacements\":"<<mapPromptReplacements.load()
       <<",\"map_prompt_icon_suppressions\":"<<mapPromptIconSuppressions.load()<<",\"map_prompt_icon_restorations\":"<<mapPromptIconRestorations.load()
       <<",\"map_footer_prompts\":"<<(mapFooterPromptsEnabled?"true":"false")
       <<",\"map_footer_updates\":"<<mapFooterUpdates.load()<<",\"map_footer_replacements\":"<<mapFooterReplacements.load()
       <<",\"map_footer_restorations\":"<<mapFooterRestorations.load()
       <<",\"map_footer_caption_calls\":"<<mapFooterCaptionCalls.load()<<",\"map_footer_native_calls\":"<<mapFooterNativeCalls.load()
       <<",\"map_footer_accepted\":"<<mapFooterAccepted.load()
       <<",\"map_footer_missing_scope\":"<<mapFooterMissingScope.load()<<",\"map_footer_invalid_units\":"<<mapFooterInvalidUnits.load()
       <<",\"popup_observer\":";
    writeNativePopupSnapshotJson(out,popup,popupSampleMs);
    out<<",\"spatial_by_eye\":["<<spatialByEye[0].load()<<','<<spatialByEye[1].load()<<']'
       <<",\"hidden_panel_by_eye\":["<<hiddenPanelByEye[0].load()<<','<<hiddenPanelByEye[1].load()<<']'
       <<",\"pending\":"<<pending.size()<<",\"node_calls\":"<<nodeCalls.load()<<",\"nodes\":[";
    for(size_t i=0;i<nodes.size();++i){const auto& n=nodes[i];if(i)out<<',';
        out<<"{\"name\":"<<std::quoted(n.name)<<",\"node\":"<<n.node<<",\"camera\":"<<n.camera<<",\"color\":"<<n.color
           <<",\"depth\":"<<n.depth<<",\"flags\":"<<n.flags<<",\"order\":"<<n.order<<",\"calls\":"<<n.calls<<",\"source\":"<<n.source<<",\"eye\":"<<n.eye<<'}';
    }
    out<<"]}\n";
}
void stopUiRenderer() noexcept {
    enabled.store(false);uiBoundaryLayoutWindow.store(false);{std::lock_guard lock(popupChoiceMutex);popupChoiceLeases={};}
    clearUiRenderSource();
}
}
