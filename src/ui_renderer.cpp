#include "mgs5vr/ui_renderer.hpp"
#include "mgs5vr/head_camera.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/log.hpp"
#include <windows.h>
#include <intrin.h>
#include <MinHook.h>
#include <atomic>
#include <cstring>
#include <iomanip>
#include <mutex>
#include <ostream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace mgs5vr {
namespace {
using QueueFn=uintptr_t(*)(void*,void*);
using ExecuteFn=uintptr_t(*)(void*,void*,void*,uint32_t);
using NodeFn=uintptr_t(*)(void*,void*);
QueueFn originalQueue{};ExecuteFn originalExecute{};NodeFn originalNode{};
uintptr_t base{};
std::atomic_bool enabled{};
struct Source {EyeFrame eye{};uintptr_t camera{};std::array<float,16> view{};};
thread_local Source producing,executing;
std::mutex mutex;
std::unordered_map<uintptr_t,Source> pending;
struct NodeSample {
    uintptr_t node{},camera{},color{},depth{};
    uint64_t calls{},source{};
    uint32_t flags{},eye{};
    std::string name;
};
std::vector<NodeSample> nodes;
std::atomic_uint64_t queued{},executions{},joined{},patched{},cameraMismatch{},viewMismatch{},expired{},overflow{},nodeCalls{};
template<class T>T field(const void* p,size_t offset){T value{};std::memcpy(&value,static_cast<const unsigned char*>(p)+offset,sizeof(value));return value;}
bool read(uintptr_t p,void* output,size_t size){SIZE_T copied{};return p&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(p),output,size,&copied)&&copied==size;}
std::string nodeName(uintptr_t node){
    uintptr_t holder{},text{};
    if(!read(node+0x58,&holder,sizeof(holder))||!read(holder,&text,sizeof(text)))return {};
    std::string result;
    for(size_t n=0;n<160;++n){char c{};if(!read(text+n,&c,1)||!c)break;result.push_back(c>=32&&c<=126?c:'?');}
    return result;
}
__declspec(noinline) uintptr_t queue(void* job,void* state){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const auto result=originalQueue(job,state);
    if(!enabled.load()||caller!=base+0x2e7d98)return result;
    try{
        std::lock_guard lock(mutex);const auto key=reinterpret_cast<uintptr_t>(state)+0x120;
        // Remove recycled state even when it was queued outside an eye draw.
        pending.erase(key);
        if(producing.eye.sourceSequence){
            if(pending.size()<128){pending.emplace(key,producing);++queued;}else ++overflow;
        }
    }catch(...){}
    return result;
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
    if(enabled.load()&&executing.eye.sourceSequence)try{
        ++nodeCalls;
        const auto address=reinterpret_cast<uintptr_t>(item);
        const auto camera=field<uintptr_t>(state,0x308);
        std::lock_guard lock(mutex);
        auto found=nodes.end();for(auto it=nodes.begin();it!=nodes.end();++it)if(it->node==address&&it->camera==camera){found=it;break;}
        if(found==nodes.end()&&nodes.size()<96){
            nodes.push_back({address,camera,field<uintptr_t>(state,0x340),field<uintptr_t>(state,0x348),0,0,field<uint32_t>(item,0x50),0,nodeName(address)});
            found=nodes.end()-1;
        }
        if(found!=nodes.end()){++found->calls;found->source=executing.eye.sourceSequence;found->eye=executing.eye.eye;}
    }catch(...){}
    return originalNode(state,item);
}
}
void installUiRenderer(uintptr_t moduleBase){
    struct Hook {uintptr_t rva;void* wrapper;void** original;const unsigned char* signature;size_t size;};
    constexpr unsigned char queueEntry[]{0x40,0x57,0x48,0x83,0xec,0x30};
    constexpr unsigned char executeEntry[]{0x48,0x8b,0xc4,0x57,0x48,0x81,0xec,0x80,0x06,0,0};
    constexpr unsigned char nodeEntry[]{0x48,0x89,0x5c,0x24,0x10,0x57,0x48,0x83,0xec,0x20};
    const std::array<Hook,3> hooks{{
        {0x2d3380,reinterpret_cast<void*>(&queue),reinterpret_cast<void**>(&originalQueue),queueEntry,sizeof(queueEntry)},
        {0x2e7770,reinterpret_cast<void*>(&execute),reinterpret_cast<void**>(&originalExecute),executeEntry,sizeof(executeEntry)},
        {0x2e6be0,reinterpret_cast<void*>(&node),reinterpret_cast<void**>(&originalNode),nodeEntry,sizeof(nodeEntry)}}};
    for(const auto& hook:hooks){std::array<unsigned char,16> bytes{};
        if(!read(moduleBase+hook.rva,bytes.data(),hook.size)||std::memcmp(bytes.data(),hook.signature,hook.size))throw std::runtime_error("Native UI renderer signature mismatch");
    }
    base=moduleBase;
    for(const auto& hook:hooks){const auto result=MH_CreateHook(reinterpret_cast<void*>(base+hook.rva),hook.wrapper,hook.original);
        if(result!=MH_OK)throw std::runtime_error(std::string("Native UI hook: ")+MH_StatusToString(result));
    }
    for(const auto& hook:hooks)if(MH_EnableHook(reinterpret_cast<void*>(base+hook.rva))!=MH_OK){
        for(const auto& installed:hooks)MH_DisableHook(reinterpret_cast<void*>(base+installed.rva));
        throw std::runtime_error("Cannot enable native UI hooks");
    }
    enabled.store(true);log("Native UI worker lineage installed; HUD pixel extraction and wrist display remain incomplete");
}
void setUiRenderSource(const EyeFrame& eye,uintptr_t camera,const std::array<float,16>& view){producing={eye,camera,view};}
void clearUiRenderSource() noexcept {producing={};}
bool applyUiEyeProjection(float* output) noexcept {
    if(!enabled.load()||!executing.eye.sourceSequence||!executing.eye.projected)return false;
    const auto status=headCamera().status();const auto now=steadyMilliseconds();
    if(!status.active||status.activation!=executing.eye.activation||now<executing.eye.sampleTime||now-executing.eye.sampleTime>150){++expired;return false;}
    // Native UI context stores this output at +0x1c0, with its selected
    // GrCamera at +0x308 and the view used for this UI draw at +0x200.
    const auto* state=reinterpret_cast<const unsigned char*>(output)-0x1c0;
    if(field<uintptr_t>(state,0x308)!=executing.camera){++cameraMismatch;return false;}
    if(std::memcmp(state+0x200,executing.view.data(),sizeof(executing.view))){++viewMismatch;return false;}
    std::array<float,16> matrix{};std::memcpy(matrix.data(),output,sizeof(matrix));
    if(!setEyeProjection(matrix,executing.eye.view.fov))return false;
    std::memcpy(output,matrix.data(),sizeof(matrix));++patched;return true;
}
void reportUiRenderer(std::ostream& out){
    std::lock_guard lock(mutex);
    out<<"{\"event\":\"native_ui_renderer\",\"queued\":"<<queued.load()<<",\"executions\":"<<executions.load()
       <<",\"joined\":"<<joined.load()<<",\"projection_patched\":"<<patched.load()<<",\"camera_mismatch\":"<<cameraMismatch.load()
       <<",\"view_mismatch\":"<<viewMismatch.load()<<",\"expired\":"<<expired.load()<<",\"overflow\":"<<overflow.load()
       <<",\"pending\":"<<pending.size()<<",\"node_calls\":"<<nodeCalls.load()<<",\"nodes\":[";
    for(size_t i=0;i<nodes.size();++i){const auto& n=nodes[i];if(i)out<<',';
        out<<"{\"name\":"<<std::quoted(n.name)<<",\"node\":"<<n.node<<",\"camera\":"<<n.camera<<",\"color\":"<<n.color
           <<",\"depth\":"<<n.depth<<",\"flags\":"<<n.flags<<",\"calls\":"<<n.calls<<",\"source\":"<<n.source<<",\"eye\":"<<n.eye<<'}';
    }
    out<<"]}\n";
}
void stopUiRenderer() noexcept {enabled.store(false);clearUiRenderSource();}
}
