#include "mgs5vr/ui_renderer.hpp"
#include "mgs5vr/head_camera.hpp"
#include "mgs5vr/idroid_rig.hpp"
#include "mgs5vr/input_bridge.hpp"
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
#include <vector>
#include <filesystem>

namespace mgs5vr {
namespace {
using QueueFn=uintptr_t(*)(void*,void*);
using ExecuteFn=uintptr_t(*)(void*,void*,void*,uint32_t);
using NodeFn=uintptr_t(*)(void*,void*);
QueueFn originalQueue{};ExecuteFn originalExecute{};NodeFn originalNode{};
using MarkerDepthFn=void(*)(void*);
MarkerDepthFn originalMarkerDepth{};
std::atomic_uint64_t markerDepthPreserved{};
using ModelParameterFn=void(*)(void*,uint32_t,uint32_t,const float*);
ModelParameterFn setReconParameter{};
std::atomic_uint64_t reconModelGroupsHidden{};
using TitleFn=void(*)(void*);
TitleFn originalTitleShow{},originalTitleUpdate{};
TitleFn originalStartShow{};
std::atomic_uintptr_t titleMenu{};
std::atomic_uint64_t titleUpdatedAt{};
uintptr_t base{};
std::atomic_bool enabled{};
struct Source {
    EyeFrame eye{};
    uintptr_t camera{};
    std::array<float,16> view{};
    Pose panel{},picker{};
    bool panelTracked{},panelVisible{},choosingCategory{},itemsOpen{},commandsOpen{},menuOpen{};
    Pose menuPanel{};
    bool frontEnd{};
    std::array<float,16> authoredView{},authoredProjection{};
    float pickerWidth{.75f};
    HudMode hudMode{HudMode::binocularsOnly};
    HudView hudView{HudView::world};
    std::array<float,16> projection{};
    bool equipmentOpen{};
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
bool spatialEnabled{};
bool menuReaderVerified{};
bool pauseReaderVerified{};
std::atomic_int menuState{-1};
std::atomic_uint64_t pickerDrawTime{};
std::atomic_uint64_t commandsDrawTime{};
// Opt-in isolation for native HUD-layer diagnosis. Never used by gameplay
// defaults; refreshed by the bounded observer, not by every render-node call.
std::atomic_int diagnosticHiddenOrder{-1};

template<class T>T field(const void* p,size_t offset){T value{};std::memcpy(&value,static_cast<const unsigned char*>(p)+offset,sizeof(value));return value;}
bool read(uintptr_t p,void* output,size_t size){SIZE_T copied{};return p&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(p),output,size,&copied)&&copied==size;}
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
    if(enabled.load()&&executing.eye.sourceSequence&&!executing.menuOpen&&!executing.frontEnd
        &&diagnosticHiddenOrder.load()==static_cast<int>(field<uint32_t>(item,0x28)))return 0;
    // Title's complete native menu is captured once and placed on a cabin
    // surface after each eye. Its individual layers must not reappear here.
    if(enabled.load()&&executing.eye.sourceSequence&&executing.frontEnd)return 0;
    if(enabled.load()&&executing.eye.sourceSequence&&!executing.menuOpen){
        const auto order=field<uint32_t>(item,0x28);
        const auto camera=field<uintptr_t>(state,0x308);
        uintptr_t cameraType{};
        const bool sceneCamera=camera==executing.camera;
        const bool layoutCamera=!sceneCamera&&read(camera,&cameraType,sizeof(cameraType))&&cameraType==base+0x20f08c8;
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
            if(executing.frontEnd){
                uintptr_t type{};std::array<float,16> world{};
                read(camera,&type,sizeof(type));read(camera+0x30,world.data(),sizeof(world));
                std::ostringstream message;message<<"Title UI layer order="<<order<<" name="<<found->name
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
    if(enabled.load()&&spatialEnabled&&executing.eye.sourceSequence){
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
            const bool titleWorldUi=executing.frontEnd&&camera==executing.camera;
            const bool menuCamera=executing.menuOpen&&!executing.frontEnd&&camera!=executing.camera;
            if(((executing.menuOpen||executing.frontEnd)&&(layoutCamera||menuCamera))||titleWorldUi){
                const auto saved=field<std::array<float,16>>(state,0x1c0);
                const auto savedView=field<std::array<float,16>>(state,0x200);
                const auto mapped=uiPanelProjection(titleWorldUi?executing.authoredProjection:saved,executing.view,executing.eye.view.fov,
                    executing.menuPanel,executing.frontEnd?1.6f:idroidScreenWidth,
                    (executing.frontEnd?1.6f:idroidScreenWidth)*9.f/16.f);
                if(!mapped){++suppressedDraws;return 0;}
                auto* output=static_cast<unsigned char*>(state)+0x1c0;
                if(titleWorldUi)std::memcpy(static_cast<unsigned char*>(state)+0x200,executing.authoredView.data(),sizeof(executing.authoredView));
                std::memcpy(output,mapped->data(),sizeof(*mapped));
                const auto result=originalNode(state,item);
                if(titleWorldUi)std::memcpy(static_cast<unsigned char*>(state)+0x200,savedView.data(),sizeof(savedView));
                std::memcpy(output,saved.data(),sizeof(saved));++spatialDraws;
                if(executing.eye.eye<2)++spatialByEye[executing.eye.eye];
                return result;
            }
            const auto order=field<uint32_t>(item,0x28);
            // The game's initial equipment selector uses the verified Z=100
            // layout camera. Route those native four-way pixels through the
            // same wrist plane so the trigger reveals the real tiles and
            // their real prompts.
            const bool nativeFourWay=executing.equipmentOpen&&!executing.frontEnd
                &&layoutCamera&&order>=133&&order<=139;
            if(nativeFourWay){
                const auto saved=field<std::array<float,16>>(state,0x1c0);
                const auto savedView=field<std::array<float,16>>(state,0x200);
                const auto mapped=uiPanelProjection(nativeUiCanvasProjection(saved,executing.authoredProjection,executing.projection),executing.view,executing.eye.view.fov,
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
                const bool expanded=equipmentPicker||commandsPicker;
                const bool general=layer==HudLayer::general;
                // Layer 50 contains preprojected desktop labels. It cannot
                // follow head motion on a face panel. Acquired people and
                // waypoints are instead drawn from native world positions.
                // General gameplay HUD is intentionally suppressed in tracked
                // first-person VR. It must never fall back to a head-locked
                // panel when a weapon is drawn. The remaining layers are
                // limited to the authored forearm/picker poses.
                if(layer==HudLayer::worldLabels||general
                    ||(!executing.panelTracked||(!expanded&&!executing.panelVisible))){
                    ++suppressedDraws;
                    if((contextAction||equipmentPicker||(order>=146&&order<=148))&&executing.eye.eye<2)++hiddenPanelByEye[executing.eye.eye];
                    return 0;
                }
                const auto saved=field<std::array<float,16>>(state,0x1c0);
                const auto panel=expanded?executing.picker:
                    contextAction?compose(executing.panel,Pose{{},{0,.075f,.001f}}):executing.panel;
                const float layoutWidth=commandsPicker?.6f:equipmentPicker?executing.pickerWidth:1.2f;
                const auto mapped=uiPanelProjection(nativeUiCanvasProjection(saved,executing.authoredProjection,executing.projection),executing.view,executing.eye.view.fov,panel,layoutWidth,layoutWidth*9.f/16.f,
                    expanded?0.f:contextAction?.04f:.72f,
                    expanded?0.f:contextAction?-.52f:-.70f);
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
                // Captions, notices, and other non-authored layout-camera
                // elements are general HUD. Suppress them in tracked
                // first-person VR instead of projecting them onto the face.
                ++suppressedDraws;return 0;
            }
        }
    }
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
    const std::array<Hook,7> hooks{{
        {0x2d3380,reinterpret_cast<void*>(&queue),reinterpret_cast<void**>(&originalQueue),queueEntry,sizeof(queueEntry)},
        {0x2e7770,reinterpret_cast<void*>(&execute),reinterpret_cast<void**>(&originalExecute),executeEntry,sizeof(executeEntry)},
        {0x2e6be0,reinterpret_cast<void*>(&node),reinterpret_cast<void**>(&originalNode),nodeEntry,sizeof(nodeEntry)},
        {0x12d73e0,reinterpret_cast<void*>(&titleShow),reinterpret_cast<void**>(&originalTitleShow),titleShowEntry,sizeof(titleShowEntry)},
        {0x12d86a0,reinterpret_cast<void*>(&titleUpdate),reinterpret_cast<void**>(&originalTitleUpdate),titleUpdateEntry,sizeof(titleUpdateEntry)},
        {0x12d6d70,reinterpret_cast<void*>(&startShow),reinterpret_cast<void**>(&originalStartShow),startShowEntry,sizeof(startShowEntry)},
        {0x6bcca0,reinterpret_cast<void*>(&markerDepth),reinterpret_cast<void**>(&originalMarkerDepth),markerDepthEntry,sizeof(markerDepthEntry)}}};
    for(const auto& hook:hooks){std::array<unsigned char,16> bytes{};
        if(!read(moduleBase+hook.rva,bytes.data(),hook.size)||std::memcmp(bytes.data(),hook.signature,hook.size))throw std::runtime_error("Native UI renderer signature mismatch");
    }
    base=moduleBase;
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
    constexpr std::array<unsigned char,8> sequenceGetter{0x48,0x8b,0x05,0xf9,0x98,0x69,0x02,0xc3};
    constexpr std::array<unsigned char,6> closePause{0x89,0x43,0x38,0x89,0x43,0x48};
    std::array<unsigned char,6> closeBytes{};
    pauseReaderVerified=read(base+0x53a280,getterBytes.data(),getterBytes.size())&&getterBytes==sequenceGetter
        &&read(base+0x5391ca,closeBytes.data(),closeBytes.size())&&closeBytes==closePause;
    std::array<wchar_t,32768> executable{};
    if(GetModuleFileNameW(nullptr,executable.data(),static_cast<DWORD>(executable.size()))){
        settings=std::filesystem::path(executable.data()).parent_path()/L"mgs5vr.ini";
        spatialEnabled=GetPrivateProfileIntW(L"diagnostics",L"wrist_hud_experiment",0,settings.c_str())==1;
    }
    for(const auto& hook:hooks){const auto result=MH_CreateHook(reinterpret_cast<void*>(base+hook.rva),hook.wrapper,hook.original);
        if(result!=MH_OK)throw std::runtime_error(std::string("Native UI hook: ")+MH_StatusToString(result));
    }
    for(const auto& hook:hooks)if(MH_EnableHook(reinterpret_cast<void*>(base+hook.rva))!=MH_OK){
        for(const auto& installed:hooks)MH_DisableHook(reinterpret_cast<void*>(base+installed.rva));
        throw std::runtime_error("Cannot enable native UI hooks");
    }
    enabled.store(true);log("Native UI worker lineage installed; experimental left-forearm weapon HUD="+std::to_string(spatialEnabled));
}
bool nativeTitleMenuOpen() noexcept {
    if(!enabled.load())return false;
    const auto object=titleMenu.load();uintptr_t type{};
    // The reset callback and state zero also occur during entry. Only a
    // current native update establishes that this menu owns the front end.
    const auto updated=titleUpdatedAt.load(),now=steadyMilliseconds();
    return updated&&now>=updated&&now-updated<=250
        &&object&&read(object,&type,sizeof(type))&&type==base+0x23d7e58;
}
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
    const int next=(open?1:0)|(paused?2:0);
    if(menuState.exchange(next)!=next)try{log("Native menu state="+std::to_string(next)+" (iDroid=1, pause=2)");}catch(...){}
    return next!=0;
}
bool nativeIdroidOpen() noexcept {
    const auto state=menuState.load();
    return state>=0&&(state&1)!=0;
}
uint64_t nativeEquipmentPickerDrawTime() noexcept {return enabled.load()?pickerDrawTime.load():0;}
uint64_t nativeCommandsDrawTime() noexcept {return enabled.load()?commandsDrawTime.load():0;}
Pose wristPickerPose(const HeadCameraSample& rig) noexcept{
    const auto head=nativeTrackedPose(rig.nativePose,rig.headPose,rig.headPose);
    // Unfold the real cards above their forearm anchor. Fit the complete native
    // panel into both eyes instead of clipping its text at close wrist range.
    const auto anchor=rig.wristPanel.position+rotate(head.orientation,{0,rig.controllers.wristSelectorHeight,0});
    // Frustum fitting uses tracked -Z-forward poses. Native camera poses are
    // +Z-forward and make every corner appear behind the eye, forcing fallback.
    const std::array<EyeView,2> eyes{{
        {nativeTrackedPose(rig.nativePose,rig.headPose,rig.views[0].pose),rig.views[0].fov},
        {nativeTrackedPose(rig.nativePose,rig.headPose,rig.views[1].pose),rig.views[1].fov}}};
    const auto width=rig.controllers.wristPickerWidth;
    return fitWristPanel(head,anchor,eyes,width,width*9.f/16.f).value_or(Pose{head.orientation,anchor});
}
void setUiRenderSource(const EyeFrame& eye,uintptr_t camera,const std::array<float,16>& view,const std::array<float,16>& projection,const HeadCameraSample& rig,
    const std::array<float,16>& authoredView,const std::array<float,16>& authoredProjection,HudView hudView){
    const std::array<Pose,2> eyes{nativeEyePose(rig.nativePose,rig.headPose,rig.views[0].pose),
                                nativeEyePose(rig.nativePose,rig.headPose,rig.views[1].pose)};
    // Keep status flat along the forearm and keep the native picker on that
    // same wrist origin. The iDroid gets its own tracked screen pose.
    const auto picker=wristPickerPose(rig);
    const auto idroid=trackedIdroidPose(rig);
    const auto menuPanel=rig.menuOpen&&idroid?idroid->screen:
        rig.menuOpen?picker:rig.menuPanel;
    producing={eye,camera,view,rig.wristPanel,picker,rig.wristPanelTracked,
               rig.wristPanelTracked&&panelFacesBothEyes(rig.wristPanel,eyes),
               rig.controllers.equipmentOpen&&!rig.controllers.equipmentCategory,
               rig.controllers.equipmentCategory==4,rig.controllers.commandControls,
               rig.menuOpen,menuPanel,rig.controllers.frontEnd,authoredView,authoredProjection,rig.controllers.wristPickerWidth};
    producing.hudMode=rig.controllers.hudMode;
    producing.hudView=hudView;
    producing.projection=projection;
    producing.equipmentOpen=rig.controllers.equipmentOpen;
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
    if(executing.frontEnd){
        // Title choices and their shading are authored in the scene camera's
        // world space. Recover their native clip layout before placing that
        // complete layout on the spatial panel. Using an eye camera here leaves the
        // title backdrop crossing the cabin and its text outside the view.
        std::memcpy(reinterpret_cast<unsigned char*>(output)-0x1c0+0x200,executing.authoredView.data(),sizeof(executing.authoredView));
        std::memcpy(output,executing.authoredProjection.data(),sizeof(executing.authoredProjection));
        return true;
    }
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
    const auto hidden=static_cast<int>(GetPrivateProfileIntW(L"diagnostics",L"ui_hide_order",-1,settings.c_str()));
    diagnosticHiddenOrder.store(hidden>=0&&hidden<=255?hidden:-1);
    std::lock_guard lock(mutex);
    out<<"{\"event\":\"native_ui_renderer\",\"queued\":"<<queued.load()<<",\"executions\":"<<executions.load()
       <<",\"joined\":"<<joined.load()<<",\"projection_patched\":"<<patched.load()<<",\"camera_mismatch\":"<<cameraMismatch.load()
       <<",\"view_restored\":"<<viewMismatch.load()<<",\"spatial_draws\":"<<spatialDraws.load()<<",\"suppressed_draws\":"<<suppressedDraws.load()
       <<",\"expired\":"<<expired.load()<<",\"overflow\":"<<overflow.load()
       <<",\"marker_depth_preserved\":"<<markerDepthPreserved.load()
       <<",\"recon_model_groups_hidden\":"<<reconModelGroupsHidden.load()
       <<",\"spatial_by_eye\":["<<spatialByEye[0].load()<<','<<spatialByEye[1].load()<<']'
       <<",\"hidden_panel_by_eye\":["<<hiddenPanelByEye[0].load()<<','<<hiddenPanelByEye[1].load()<<']'
       <<",\"pending\":"<<pending.size()<<",\"node_calls\":"<<nodeCalls.load()<<",\"nodes\":[";
    for(size_t i=0;i<nodes.size();++i){const auto& n=nodes[i];if(i)out<<',';
        out<<"{\"name\":"<<std::quoted(n.name)<<",\"node\":"<<n.node<<",\"camera\":"<<n.camera<<",\"color\":"<<n.color
           <<",\"depth\":"<<n.depth<<",\"flags\":"<<n.flags<<",\"order\":"<<n.order<<",\"calls\":"<<n.calls<<",\"source\":"<<n.source<<",\"eye\":"<<n.eye<<'}';
    }
    out<<"]}\n";
}
void stopUiRenderer() noexcept {enabled.store(false);clearUiRenderSource();}
}
