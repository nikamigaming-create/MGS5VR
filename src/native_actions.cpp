#include "mgs5vr/native_actions.hpp"
#include "mgs5vr/native_controls.hpp"
#include "mgs5vr/log.hpp"
#include "mgs5vr/animal_interaction.hpp"
#include "mgs5vr/small_animal.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/controller_rig.hpp"
#include "mgs5vr/controls.hpp"
#include "mgs5vr/ui_renderer.hpp"
#include "mgs5vr/render_camera.hpp"
#include "mgs5vr/native_menu_restrictions.hpp"
#include "mgs5vr/tracking_fault.hpp"
#include "mgs5vr/opening_selector.hpp"
#include "mgs5vr/optic_events.hpp"
#include "native_cabin_script.hpp"
#include "native_presentation_script.hpp"
#include <windows.h>
#include <MinHook.h>
#include <intrin.h>
#include <array>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <fstream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sstream>
#include <thread>
#include <charconv>
#include <unordered_set>

namespace mgs5vr {
namespace {
using Call=int(*)(void*,int,int,int);
using Load=int(*)(void*,const char*,size_t,const char*);
using Top=int(*)(void*);
using SetTop=void(*)(void*,int);
using String=const char*(*)(void*,int,size_t*);
Call original{};Load load{};Top getTop{};SetTop setTop{};String getString{};
using Send=void*(*)(void*,int32_t*,uint32_t,void*);
Send originalSend{};
uintptr_t imageBase{};
std::atomic_uint64_t traceUntil{};
std::mutex traceMutex;
std::unordered_set<uint64_t> traced;
std::atomic_bool enabled{};
std::atomic_bool nativeLuaReady{};
std::mutex requestMutex;
std::filesystem::path requestPath,resultPath,tracePath;
bool externalCommands{};
uint64_t checkedAt{};
thread_local unsigned depth{};
// Explicitly armed, bounded observations of the retail UI input and stow
// handshake. These hooks never alter the native result or dispatch input.
using UiButton=bool(*)(void*,uint32_t,uint32_t,bool);
using UiClose=bool(*)(void*);
UiButton originalUiButton{};UiClose originalUiClose{};
using UiDisable=void(*)(void*,uint8_t,bool);
UiDisable originalUiDisable{};
struct MenuDisableRead {uintptr_t owner{},caller{};unsigned mode{},index{},before{},after{};bool disabled{};};
std::deque<MenuDisableRead> menuDisableReads;
NativeMenuRestrictionLease tutorialMenuLease;
bool menuRestrictionAbi{};
std::atomic_uint64_t menuTraceUntil{},menuButtonCalls{},menuCloseCalls{};
std::mutex menuTraceMutex;
struct MenuInputRead {uint64_t time{};uint32_t index{},bit{},held{},pressed{};uint16_t xr{};bool result{},bypass{};};
std::deque<MenuInputRead> menuInputReads;
constexpr wchar_t actionPipeName[]=L"\\\\.\\pipe\\MGS5VR.NativeActions";
constexpr uint32_t maxPipeScript=1024*1024;
constexpr size_t maxQueuedActions=256;
constexpr size_t maxActionsPerTransaction=8;
uint64_t candidateKey(const char* value,size_t length){
    uint64_t hash=14695981039346656037ull;
    for(size_t i=0;i<length;++i){hash^=static_cast<unsigned char>(value[i]);hash*=1099511628211ull;}
    return hash?hash:1;
}
#pragma pack(push,1)
struct PipeRequestHeader { uint64_t id{}; uint32_t length{}; };
struct PipeResponseHeader {
    uint64_t id{};
    int32_t status{};
    uint32_t length{};
    uint64_t queuedMilliseconds{};
    uint64_t executionMicroseconds{};
};
#pragma pack(pop)
static_assert(sizeof(PipeRequestHeader)==12);
static_assert(sizeof(PipeResponseHeader)==32);
struct ActionConnection {
    HANDLE pipe{INVALID_HANDLE_VALUE};
    std::mutex writeMutex;
    std::condition_variable responseReady;
    PipeResponseHeader response{};
    std::string responseText;
    bool hasResponse{};
    std::atomic_bool open{true};
};
struct ActionRequest {
    uint64_t id{};
    std::string script;
    std::chrono::steady_clock::time_point queuedAt{};
    std::shared_ptr<ActionConnection> connection;
    std::shared_ptr<std::atomic_bool> cancelled{std::make_shared<std::atomic_bool>(false)};
};
std::mutex fastQueueMutex,connectionMutex;
std::deque<ActionRequest> fastQueue;
std::shared_ptr<ActionConnection> activeConnection;
std::thread actionPipeThread;
std::atomic_bool actionPipeStopping{true};
template<class T> T read(uintptr_t address){
    T value{};SIZE_T size{};
    if(address)ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),&value,sizeof(value),&size);
    return size==sizeof(value)?value:T{};
}
bool uiButton(void* object,uint32_t index,uint32_t bit,bool bypass){
    const auto result=originalUiButton(object,index,bit,bypass);
    if(steadyMilliseconds()<menuTraceUntil.load()&&nativeIdroidOpen()){
        ++menuButtonCalls;
        const auto audit=controlInputSnapshot();
        if(result||audit.nativeButtons){
            using Pad=void*(*)(uint32_t);
            const auto pad=index<4?reinterpret_cast<Pad>(imageBase+0x1b8a0)(index):nullptr;
            MenuInputRead v{steadyMilliseconds(),index,bit,read<uint32_t>(reinterpret_cast<uintptr_t>(pad)+0x20),
                read<uint32_t>(reinterpret_cast<uintptr_t>(pad)+0x28),audit.nativeButtons,result,bypass};
            std::lock_guard lock(menuTraceMutex);
            if(menuInputReads.size()>=128)menuInputReads.pop_front();
            menuInputReads.push_back(v);
        }
    }
    return result;
}
bool uiClose(void* object){
    const auto result=originalUiClose(object);
    if(steadyMilliseconds()<menuTraceUntil.load()&&nativeIdroidOpen())++menuCloseCalls;
    return result;
}
void uiDisable(void* object,uint8_t index,bool disabled){
    const auto owner=reinterpret_cast<uintptr_t>(object);
    const auto mode=nativeIdroidTutorialMode();
    const auto before=read<uint8_t>(owner+index*40+8);
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    if(index<78&&read<uintptr_t>(owner)==imageBase+0x227ee50){
        std::lock_guard lock(menuTraceMutex);
        const auto ui=read<uintptr_t>(imageBase+0x2bf1518);
        const auto tutorial=read<uintptr_t>(ui+0x90);
        const bool typed=read<uintptr_t>(ui)==imageBase+0x2242d78
            &&read<uintptr_t>(tutorial)==imageBase+0x2272810;
        // Verified GuiMbTutorial begin disables all entries before assigning
        // the mode. Its FOB handler then permits entries 33/72. Observe the
        // begin's prior values, not the already-disabled mode-10 snapshot.
        if(typed&&disabled&&caller==imageBase+0xf4285b)
            tutorialMenuLease.beginEntry(owner,tutorial,index,(before&0x40)!=0);
        else if(disabled)tutorialMenuLease.independentDisable(owner,index);
        originalUiDisable(object,index,disabled);
        if(steadyMilliseconds()<menuTraceUntil.load()){
            if(menuDisableReads.size()>=128)menuDisableReads.pop_front();
            menuDisableReads.push_back({owner,caller,mode.value_or(255),index,before,read<uint8_t>(owner+index*40+8),disabled});
        }
    }else originalUiDisable(object,index,disabled);
}
bool restoreCompletedGuideRestrictions(){
    const auto mode=nativeIdroidTutorialMode();
    if(!menuRestrictionAbi||!originalUiDisable||!mode||*mode!=0||nativePauseMenuOpen())return false;
    const auto ui=read<uintptr_t>(imageBase+0x2bf1518);
    const auto tutorial=read<uintptr_t>(ui+0x90);
    const auto common=read<uintptr_t>(ui+0x70);
    const auto menu=read<uintptr_t>(common+0xca0);
    if(read<uintptr_t>(ui)!=imageBase+0x2242d78||read<uintptr_t>(tutorial)!=imageBase+0x2272810
       ||read<uintptr_t>(menu)!=imageBase+0x227ee50)return false;
    std::lock_guard lock(menuTraceMutex);
    if(!tutorialMenuLease.ready(menu,tutorial))return false;
    for(unsigned index=0;index<NativeMenuRestrictionLease::entries;++index){
        const auto prior=tutorialMenuLease.prior(index);
        if(prior)originalUiDisable(reinterpret_cast<void*>(menu),static_cast<uint8_t>(index),*prior);
    }
    tutorialMenuLease.reset();return true;
}
bool haveGuideRestrictionLease(){
    if(!menuRestrictionAbi||!originalUiDisable)return false;
    const auto ui=read<uintptr_t>(imageBase+0x2bf1518);
    const auto tutorial=read<uintptr_t>(ui+0x90);
    const auto common=read<uintptr_t>(ui+0x70);
    const auto menu=read<uintptr_t>(common+0xca0);
    if(read<uintptr_t>(ui)!=imageBase+0x2242d78||read<uintptr_t>(tutorial)!=imageBase+0x2272810
       ||read<uintptr_t>(menu)!=imageBase+0x227ee50)return false;
    std::lock_guard lock(menuTraceMutex);return tutorialMenuLease.ready(menu,tutorial);
}
void* send(void* context,int32_t* result,uint32_t object,void* command){
    if((object>>9)==19&&GetTickCount64()<traceUntil.load()){
        const auto bytes=read<std::array<unsigned char,64>>(reinterpret_cast<uintptr_t>(command));
        uint64_t hash{};std::memcpy(&hash,bytes.data(),8);
        std::lock_guard lock(traceMutex);
        if(traced.size()<128&&traced.insert(hash).second){
            std::ostringstream s;s<<"Native D-Dog command="<<std::hex<<hash<<" caller="
                <<reinterpret_cast<uintptr_t>(_ReturnAddress())<<" bytes=";
            constexpr char digits[]="0123456789abcdef";
            for(auto b:bytes)s<<digits[b>>4]<<digits[b&15];
            log(s.str());
        }
    }
    return originalSend(context,result,object,command);
}
void traceCommands(){
    std::error_code error;
    if(!std::filesystem::exists(tracePath,error))return;
    if(!originalSend){
        const auto services=read<uintptr_t>(read<uintptr_t>(imageBase+0x2c3a8a0)+8);
        const auto objects=read<uintptr_t>(services+0x60);
        const auto function=read<uintptr_t>(read<uintptr_t>(objects)+0x38);
        if(function<imageBase+0x1000||function>=imageBase+0x2090000)return;
        auto* address=reinterpret_cast<void*>(function);
        if(MH_CreateHook(address,reinterpret_cast<void*>(&send),reinterpret_cast<void**>(&originalSend))!=MH_OK
           ||MH_EnableHook(address)!=MH_OK)return;
    }
    {std::lock_guard lock(traceMutex);traced.clear();}
    traceUntil.store(GetTickCount64()+15000);
    std::filesystem::remove(tracePath,error);
    log("Native D-Dog command observation enabled for 15 seconds");
}
bool readPipe(HANDLE pipe,void* output,uint32_t length){
    auto* bytes=static_cast<unsigned char*>(output);uint32_t offset{};
    while(offset<length){DWORD read{};if(!ReadFile(pipe,bytes+offset,length-offset,&read,nullptr)||!read)return false;offset+=read;}
    return true;
}
bool writePipe(HANDLE pipe,const void* input,uint32_t length){
    const auto* bytes=static_cast<const unsigned char*>(input);uint32_t offset{};
    while(offset<length){DWORD written{};if(!WriteFile(pipe,bytes+offset,length-offset,&written,nullptr)||!written)return false;offset+=written;}
    return true;
}
void closeConnection(const std::shared_ptr<ActionConnection>& connection){
    if(!connection)return;
    std::lock_guard lock(connection->writeMutex);
    if(!connection->open.exchange(false))return;
    if(connection->pipe!=INVALID_HANDLE_VALUE){
        DisconnectNamedPipe(connection->pipe);CloseHandle(connection->pipe);connection->pipe=INVALID_HANDLE_VALUE;
    }
}
void wakeActionPipe(){
    const auto pipe=CreateFileW(actionPipeName,GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr);
    if(pipe!=INVALID_HANDLE_VALUE)CloseHandle(pipe);
}
bool publishFastInput(const std::string& request,std::string& result);
void sendFastResponse(const ActionRequest& request,int32_t status,
    uint64_t queuedMilliseconds,uint64_t executionMicroseconds,const std::string& result);
bool flushFastResponse(const ActionRequest& request){
    const auto& connection=request.connection;
    std::unique_lock lock(connection->writeMutex);
    const auto ready=connection->responseReady.wait_for(lock,std::chrono::seconds(15),[&]{
        return connection->hasResponse||actionPipeStopping.load()||!connection->open.load();
    });
    if(actionPipeStopping.load()||!connection->open.load())return false;
    if(!ready){
        request.cancelled->store(true);
        connection->responseText="native action timed out waiting for Lua transaction";
        connection->response={request.id,4,static_cast<uint32_t>(connection->responseText.size()),15000,0};
    }
    const auto header=connection->response;
    auto body=std::move(connection->responseText);
    connection->hasResponse=false;
    lock.unlock();
    // This pipe was opened synchronously. Only its I/O thread may read/write
    // it: writing from the game's Lua thread while ReadFile waits for another
    // request deadlocks the native update until the client disconnects.
    return writePipe(connection->pipe,&header,sizeof(header))
        &&writePipe(connection->pipe,body.data(),header.length);
}
void actionPipeLoop(){
    while(!actionPipeStopping.load()){
        const auto pipe=CreateNamedPipeW(actionPipeName,PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT,PIPE_UNLIMITED_INSTANCES,1<<20,1<<20,0,nullptr);
        if(pipe==INVALID_HANDLE_VALUE){log("Native action pipe creation failed error="+std::to_string(GetLastError()));std::this_thread::yield();continue;}
        auto connection=std::make_shared<ActionConnection>();connection->pipe=pipe;
        const bool connected=ConnectNamedPipe(pipe,nullptr)||GetLastError()==ERROR_PIPE_CONNECTED;
        if(!connected){CloseHandle(pipe);continue;}
        {std::lock_guard lock(connectionMutex);activeConnection=connection;}
        log("Native action pipe connected");
        while(!actionPipeStopping.load()&&connection->open.load()){
            PipeRequestHeader header{};
            if(!readPipe(pipe,&header,sizeof(header)))break;
            if(!header.length||header.length>maxPipeScript)break;
            ActionRequest request;request.id=header.id;request.script.resize(header.length);
            if(!readPipe(pipe,request.script.data(),header.length))break;
            request.queuedAt=std::chrono::steady_clock::now();request.connection=connection;
            std::string immediateResult;
            if(publishFastInput(request.script,immediateResult)){
                const auto finished=std::chrono::steady_clock::now();
                const auto execution=std::chrono::duration_cast<std::chrono::microseconds>(finished-request.queuedAt).count();
                const auto status=immediateResult.rfind("unknown input:",0)==0?1:0;
                sendFastResponse(request,status,0,static_cast<uint64_t>(std::max<int64_t>(0,execution)),immediateResult);
                log("Native fast input completed id="+std::to_string(request.id)+" exec_us="+std::to_string(execution));
                if(!flushFastResponse(request))break;
                continue;
            }
            bool queued=false;
            {std::lock_guard lock(fastQueueMutex);
                if(fastQueue.size()<maxQueuedActions){fastQueue.push_back(request);queued=true;}}
            if(queued)log("Native action pipe queued id="+std::to_string(header.id)+" bytes="+std::to_string(header.length));
            if(!queued){
                const std::string result="action queue full";
                sendFastResponse(request,3,0,0,result);
            }
            if(!flushFastResponse(request))break;
        }
        {std::lock_guard lock(connectionMutex);if(activeConnection==connection)activeConnection.reset();}
        {
            std::lock_guard lock(fastQueueMutex);
            fastQueue.erase(std::remove_if(fastQueue.begin(),fastQueue.end(),
                [&](const ActionRequest& request){return request.connection==connection;}),fastQueue.end());
        }
        closeConnection(connection);log("Native action pipe disconnected");
    }
}
void sendFastResponse(const ActionRequest& request,int32_t status,
    uint64_t queuedMilliseconds,uint64_t executionMicroseconds,const std::string& result){
    const auto connection=request.connection;if(!connection||!connection->open.load())return;
    const PipeResponseHeader response{request.id,status,static_cast<uint32_t>(std::min<size_t>(result.size(),maxPipeScript)),
        queuedMilliseconds,executionMicroseconds};
    std::lock_guard lock(connection->writeMutex);
    if(!connection->open.load()||connection->pipe==INVALID_HANDLE_VALUE)return;
    if(request.cancelled->load())return;
    connection->response=response;
    connection->responseText=result.substr(0,response.length);
    connection->hasResponse=true;
    connection->responseReady.notify_one();
}
bool luaReady(void* state){
    const int top=getTop(state);constexpr char check[]="return type(TppMain)=='table' and type(GameObject)=='table' and type(gvars)=='table' and type(vars)=='table' and type(TppMission)=='table' and type(TppBuddyService)=='table' and 'ready' or 'wait'";
    int status=load(state,check,sizeof(check)-1,"@mgs5vr-state-check");if(!status)status=original(state,0,1,0);
    size_t length{};const auto* value=getString(state,-1,&length);const bool ready=!status&&value&&length==5&&std::memcmp(value,"ready",5)==0;
    setTop(state,top);nativeLuaReady.store(ready);return ready;
}
bool publishFastInput(const std::string& request,std::string& result){
    constexpr std::string_view faultPrefix="diagnostics:controller-tracking-loss:";
    if(request.starts_with(faultPrefix)){
        auto& probe=controllerTrackingFaultProbe();bool armed=false;
        const auto arguments=std::string_view(request).substr(faultPrefix.size());
        if(arguments=="off"){probe.clear();armed=true;}
        else if(const auto separator=arguments.find(':');separator!=std::string_view::npos){
            unsigned mask{};uint64_t duration{};
            const auto first=std::from_chars(arguments.data(),arguments.data()+separator,mask);
            const auto last=std::from_chars(arguments.data()+separator+1,arguments.data()+arguments.size(),duration);
            const auto status=headCamera().status();
            if(first.ec==std::errc{}&&first.ptr==arguments.data()+separator&&last.ec==std::errc{}
               &&last.ptr==arguments.data()+arguments.size()&&status.active&&!status.suspended)
                armed=probe.arm(mask,duration,steadyMilliseconds());
        }
        result=std::string("{\"schema\":1,\"synthetic_tracking_fault\":true,\"enabled\":")
            +(probe.enabled()?"true":"false")+",\"armed\":"+(armed?"true":"false")
            +",\"mask\":"+std::to_string(probe.mask(steadyMilliseconds()))+"}";
        return true;
    }
    if(request=="inspect-native-exposure"){result=renderCameraExposureDiagnostics();return true;}
    if(request=="diagnostics:scope-exposure-isolation:on"||request=="diagnostics:scope-exposure-isolation:off"){
        if(!setRenderCameraExposureIsolation(request.ends_with(":on")))
            result="unsupported scope exposure diagnostic build";
        else result=renderCameraExposureDiagnostics();
        return true;
    }
    if(request=="diagnostics:optic-lens-render:on"||request=="diagnostics:optic-lens-render:off"){
        if(!setRenderCameraLensRendering(request.ends_with(":on")))
            result="unsupported optic lens diagnostic build";
        else result=renderCameraExposureDiagnostics();
        return true;
    }
    if(request=="trace-native-menu-input"||request=="inspect-native-menu-input"){
        if(request=="trace-native-menu-input"){
            std::lock_guard lock(menuTraceMutex);menuInputReads.clear();menuDisableReads.clear();
            menuButtonCalls=0;menuCloseCalls=0;menuTraceUntil=steadyMilliseconds()+15000;
        }
        const auto ui=read<uintptr_t>(imageBase+0x2bf1518);
        const auto terminal=read<uintptr_t>(ui+0x7c0);
        const auto tutorial=read<uintptr_t>(ui+0x90);
        const bool valid=read<uintptr_t>(ui)==imageBase+0x2242d78
            &&read<uintptr_t>(terminal)==imageBase+0x22705a8
            &&read<uintptr_t>(tutorial)==imageBase+0x2272810;
        std::ostringstream out;out<<std::boolalpha;
        out<<"{\"schema\":1,\"verified\":"<<valid<<",\"installed\":"<<(originalUiButton&&originalUiClose)
           <<",\"armed\":"<<(steadyMilliseconds()<menuTraceUntil.load())
           <<",\"button_calls\":"<<menuButtonCalls.load()<<",\"close_getter_calls\":"<<menuCloseCalls.load();
        if(valid)out<<",\"terminal_open\":"<<unsigned(read<uint8_t>(terminal+0x20))
            <<",\"deferred_close\":"<<unsigned(read<uint8_t>(terminal+0x25))
            <<",\"active_tutorial_mode\":"<<unsigned(read<uint8_t>(tutorial+8));
        out<<",\"reads\":[";
        std::lock_guard lock(menuTraceMutex);bool first=true;
        for(const auto& v:menuInputReads){if(!first)out<<',';first=false;
            out<<"{\"ms\":"<<v.time<<",\"index\":"<<v.index<<",\"bit\":"<<v.bit
               <<",\"held\":"<<v.held<<",\"pressed\":"<<v.pressed<<",\"xr\":"<<v.xr
               <<",\"result\":"<<v.result<<",\"bypass\":"<<v.bypass<<'}';}
        out<<"],\"disabled_entries\":[";first=true;
        for(const auto& v:menuDisableReads){if(!first)out<<',';first=false;
            out<<"{\"owner\":"<<v.owner<<",\"caller_rva\":"<<(v.caller-imageBase)<<",\"mode\":"<<v.mode
               <<",\"index\":"<<v.index<<",\"before\":"<<v.before<<",\"after\":"<<v.after<<",\"disabled\":"<<v.disabled<<'}';}
        out<<"]}";result=out.str();return true;
    }
    if(request=="inspect-bot-state"){
        // Read-only transport/scene diagnostics must also work while Lua is
        // suspended at title/loading/Pause. These are individually sampled
        // publications, not a claimed coherent native simulation transaction.
        const auto rendered=headCamera().publishedView();
        const auto controlSample=controlInputSnapshot();
        const auto camera=headCamera().status();
        const auto menu=nativeMenuOpen();
        const auto tutorialMode=nativeIdroidTutorialMode();
        const auto popupSampleMs=steadyMilliseconds();
        const auto popup=nativePopupSnapshot();
        const auto now=steadyMilliseconds();
        std::ostringstream out;out<<std::boolalpha;
        out<<"{\"schema\":1,\"now_ms\":"<<now
           <<",\"lua_ready\":"<<nativeLuaReady.load()
           <<",\"title\":"<<nativeTitleModeActive()<<",\"title_menu\":"<<nativeTitleMenuOpen()
           <<",\"title_cabin\":"<<nativeTitleCabinMode()<<",\"cabin\":"<<nativeCabinPlay()
           <<",\"loading\":"<<nativeLoadingTipsOpen()<<",\"demo\":"<<nativeScriptedDemoActive()
           <<",\"demo_candidate\":"<<(nativeDemoMode()==NativeDemoMode::staleCandidate)
           <<",\"demo_recovery_ready\":"<<headCamera().staleDemoRecoveryReady()
           <<",\"demo_interactive_look\":"<<(nativeDemoMode()==NativeDemoMode::interactiveLook)
           <<",\"menu\":"<<(menu?(*menu?"true":"false"):"null")
           <<",\"pause\":"<<(menu?(nativePauseMenuOpen()?"true":"false"):"null")
           <<",\"idroid\":"<<nativeIdroidOpen()<<",\"gamepad\":"<<nativeGamepadActive()
           <<",\"idroid_closing\":"<<nativeIdroidClosing()<<",\"idroid_tutorial_mode\":";
        writeNativeIdroidTutorialModeJson(out,tutorialMode);
        out<<",\"tracking_fault_mask\":"<<controllerTrackingFaultProbe().mask(steadyMilliseconds())
           <<",\"idroid_handheld\":"<<handheldMenusSelected()
           <<",\"idroid_menu_input_ready\":"<<handheldMenuInputReady()
           <<",\"camera_active\":"<<camera.active<<",\"camera_available\":"<<headCamera().available()
           <<",\"camera_pending\":"<<camera.pending<<",\"camera_suspended\":"<<camera.suspended
           <<",\"camera_awaiting_player\":"<<camera.awaitingPlayer
           <<",\"camera_reason\":"<<static_cast<unsigned>(camera.reason)
           <<",\"activation\":"<<camera.activation<<",\"opening_assets\":"<<openingPropsAvailable();
        out<<",\"popup_observer\":";writeNativePopupSnapshotJson(out,popup,popupSampleMs);
        if(freshControlInputAudit(controlSample,now)){
            const auto* audit=&controlSample;
            out<<",\"controls\":{\"context\":\""<<controlContextName(audit->context)
               <<"\",\"age_ms\":"<<now-audit->time<<",\"sample_ms\":"<<audit->time
               <<",\"rig_input\":"<<audit->rigInput<<",\"travel_mode\":"<<audit->travelMode
               <<",\"native_packet_source\":\"xr_runtime_final_mapped_packet\",\"native_buttons\":"<<audit->nativeButtons
               <<",\"native_axes\":["<<audit->nativeAxes[0]<<','<<audit->nativeAxes[1]<<','<<audit->nativeAxes[2]<<','<<audit->nativeAxes[3]
               <<"],\"native_triggers\":["<<static_cast<unsigned>(audit->nativeTriggers[0])<<','<<static_cast<unsigned>(audit->nativeTriggers[1])
               <<"],\"physical\":[";
            for(size_t i=0;i<11;++i){if(i)out<<',';out<<audit->physical.buttons[i];}
            out<<"],\"sticks\":["<<audit->physical.leftStick[0]<<','<<audit->physical.leftStick[1]
               <<','<<audit->physical.rightStick[0]<<','<<audit->physical.rightStick[1]<<"],\"touches\":{";
            constexpr std::array<const char*,10> touchNames{"left_thumbrest","right_thumbrest","a_touch","b_touch","x_touch","y_touch",
                "left_stick_touch","right_stick_touch","left_trigger_touch","right_trigger_touch"};
            for(size_t i=0;i<touchNames.size();++i){if(i)out<<',';out<<'"'<<touchNames[i]<<"\":"<<audit->physical.buttons[19+i];}
            out<<"}}";
        }else out<<",\"controls\":null";
        if(rendered){
            const auto& frame=*rendered;
            out<<",\"rendered\":{\"source\":\"last_camera_publication\",\"tracking_sequence\":"<<frame.trackingSequence
               <<",\"rig_sequence\":"<<frame.rigSequence<<",\"activation\":"<<frame.activation
               <<",\"sample_ms\":"<<frame.sampleTime<<",\"player_sequence\":"<<frame.playerSequence
               <<",\"player_owner\":"<<frame.playerOwner<<",\"player_head\":["<<frame.playerHead.x<<','<<frame.playerHead.y<<','<<frame.playerHead.z
               <<"],\"left_palm_tracked\":"<<frame.renderedPalmTracked[0]
               <<",\"right_palm_tracked\":"<<frame.renderedPalmTracked[1];
            out<<",\"raw_left_grip_tracked\":"<<frame.controllers.hands[0].gripTracked
               <<",\"presentation_focused\":"<<frame.controllers.presentationFocused
               <<",\"presentation_epoch\":"<<frame.controllers.presentationEpoch
               <<",\"raw_left_aim_tracked\":"<<frame.controllers.hands[0].aimTracked
               <<",\"raw_right_grip_tracked\":"<<frame.controllers.hands[1].gripTracked
               <<",\"raw_right_aim_tracked\":"<<frame.controllers.hands[1].aimTracked;
            const auto pose=[&](const char* name,Pose value){
                const auto p=value.position;const auto q=value.orientation;
                out<<",\""<<name<<"\":{\"position\":["<<p.x<<','<<p.y<<','<<p.z
                   <<"],\"orientation\":["<<q.x<<','<<q.y<<','<<q.z<<','<<q.w<<"]}";
            };
            pose("native_pose",frame.nativePose);
            pose("head_pose",frame.headPose);
            pose("left_palm",frame.renderedPalms[0]);
            pose("left_grip",frame.controllers.hands[0].grip);
            pose("right_palm",frame.renderedPalms[1]);
            pose("right_grip",frame.controllers.hands[1].grip);
            pose("idroid_device",frame.idroidDevice);
            pose("scope_ocular",frame.weaponScope.ocular);
            pose("scope_objective",frame.weaponScope.objective);
            pose("weapon_support_grip",frame.weaponSupportGrip);
            pose("binocular_ocular",frame.controllers.optic.pose.rightEyepiece);
            out<<",\"scope_tracked\":"<<frame.weaponScope.tracked
               <<",\"idroid_device_tracked\":"<<frame.idroidDeviceTracked
               <<",\"scope_magnification\":"<<frame.weaponScope.magnification
               <<",\"scope_radius\":"<<frame.weaponScope.radius
               <<",\"native_firearm\":"<<frame.nativeFirearmActive
               <<",\"weapon_support_tracked\":"<<frame.weaponSupportGripTracked
               <<",\"weapon_support_attached\":"<<frame.weaponSupportAttached
               <<",\"binocular_held\":"<<frame.controllers.optic.held
               <<",\"binocular_magnification\":"<<frame.controllers.magnification;
            const auto& weapon=frame.weaponRig;
            out<<",\"weapon_native_observer\":{\"source\":\"retained_native_skin_sampling\",\"sample_ms\":"<<weapon.sampleTime
               <<",\"sampled\":"<<weapon.sampled<<",\"character\":"<<weapon.character<<",\"component\":"<<weapon.component
               <<",\"component_accepted\":"<<weapon.componentAccepted<<",\"instances\":"<<weapon.instances
               <<",\"instance_first\":"<<weapon.instanceFirst<<",\"player_index\":"<<weapon.playerIndex
               <<",\"instance_index_matched\":"<<weapon.instanceIndexMatched<<",\"state\":"<<weapon.state
               <<",\"body_model\":"<<weapon.bodyModel<<",\"weapon_model\":null,\"weapon_definition\":null"
               <<",\"resource_handle\":"<<weapon.resourceHandle<<",\"resource_accepted\":"<<weapon.resourceAccepted
               <<",\"support_identity\":"<<weapon.supportIdentity<<",\"scope_identity\":"<<frame.weaponScope.weaponIdentity
               <<",\"flags\":"<<weapon.flags<<",\"mode\":"<<weapon.mode<<",\"muzzle_attempted\":"<<weapon.muzzleAttempted
               <<",\"muzzle_matrix_read\":"<<weapon.muzzleMatrixRead<<",\"attachment_valid\":"<<weapon.attachmentValid
               <<",\"muzzle_socket_valid\":"<<weapon.muzzleSocketValid<<",\"muzzle_solved\":"<<weapon.muzzleSolved
               <<",\"optical_descriptor_read\":"<<weapon.opticalRead<<",\"optical_descriptor\":[";
            for(size_t i=0;i<weapon.optical.size();++i){if(i)out<<',';out<<static_cast<unsigned>(weapon.optical[i]);}
            out<<']';pose("muzzle_in_grip",weapon.muzzleInGrip);out<<'}';
            out<<'}';
        }else out<<",\"rendered\":null";
        if(const auto anchor=headCamera().openingTrackingOrigin(now)){
            const auto p=anchor->position;const auto q=anchor->orientation;
            out<<",\"opening_position\":["<<p.x<<','<<p.y<<','<<p.z
               <<"],\"opening_orientation\":["<<q.x<<','<<q.y<<','<<q.z<<','<<q.w<<']';
        }
        out<<'}';result=out.str();return true;
    }
    constexpr std::string_view prefix="input:";
    if(request.rfind(prefix,0)!=0)return false;
    const auto key=request.substr(prefix.size());GamepadSample sample{};
    if(key=="a")sample.buttons=0x1000;
    else if(key=="b")sample.buttons=0x2000;
    else if(key=="x")sample.buttons=0x4000;
    else if(key=="y")sample.buttons=0x8000;
    else if(key=="start")sample.buttons=0x0010;
    else if(key=="back")sample.buttons=0x0020;
    else if(key!="release"){result="unknown input: "+key;return true;}
    if(key=="a")requestLoadingPromptConfirm();
    gamepadMailbox().publishExternal(sample,key!="release",steadyMilliseconds());
    result="input published: "+key;return true;
}
bool executeFast(void* state,const std::string& request,int& status,std::string& result){
    if(publishFastInput(request,result)){status=result.rfind("unknown input:",0)==0?1:0;return true;}
    if(!luaReady(state))return false;
    if(request=="inspect-animal-touch"){result=inspectAnimalTouch();status=0;return true;}
    if(request=="native-dog-response"){result=requestNativeDogResponse();status=0;return true;}
    if(request=="inspect-small-animals"){result=inspectSmallAnimals();status=0;return true;}
    const int top=getTop(state);const std::string script="if type(TppMain)~='table' or type(GameObject)~='table' or type(gvars)~='table' or type(vars)~='table' or type(TppMission)~='table' or type(TppBuddyService)~='table' then return '__MGS5VR_WAIT__' end\n"+request;
    status=load(state,script.data(),script.size(),"@mgs5vr-native-actions");if(!status)status=original(state,0,1,0);
    size_t length{};const auto* text=getString(state,-1,&length);result=text?std::string(text,std::min(length,size_t{32768})):"(no return value)";setTop(state,top);
    return !(status==0&&result=="__MGS5VR_WAIT__");
}
void drainFastActions(void* state){
    for(size_t count=0;count<maxActionsPerTransaction;++count){
        ActionRequest request;
        {std::lock_guard lock(fastQueueMutex);if(fastQueue.empty())return;request=std::move(fastQueue.front());fastQueue.pop_front();}
        if(request.cancelled->load()||!request.connection->open.load())continue;
        const auto started=std::chrono::steady_clock::now();int status{};std::string result;
        if(!executeFast(state,request.script,status,result)){
            static uint64_t lastWaitingLog{};
            const auto now=GetTickCount64();
            if(now-lastWaitingLog>=1000){lastWaitingLog=now;log("Native action pipe waiting for Lua readiness");}
            std::lock_guard lock(fastQueueMutex);fastQueue.push_front(std::move(request));return;
        }
        const auto finished=std::chrono::steady_clock::now();
        const auto queued=std::chrono::duration_cast<std::chrono::milliseconds>(started-request.queuedAt).count();
        const auto execution=std::chrono::duration_cast<std::chrono::microseconds>(finished-started).count();
        sendFastResponse(request,status,static_cast<uint64_t>(std::max<int64_t>(0,queued)),static_cast<uint64_t>(std::max<int64_t>(0,execution)),result);
        log("Native fast action completed id="+std::to_string(request.id)+" queue_ms="+std::to_string(queued)+" exec_us="+std::to_string(execution));
    }
}
void pump(void* state){
    drainFastActions(state);
    std::unique_lock lock(requestMutex,std::try_to_lock);
    const auto now=GetTickCount64();
    if(!lock.owns_lock()||now-checkedAt<100)return;
    checkedAt=now;
    if(controllerRigEnabled()&&luaReady(state)){
        const int top=getTop(state);
        const auto cameraStatus=headCamera().status();
        if(cameraStatus.active&&!cameraStatus.suspended&&!cameraStatus.nativeMenuOpen){
            while(const auto event=takeOpticEvent(steadyMilliseconds(),cameraStatus.activation)){
                std::ostringstream script;
                script.precision(9);
                // The retail queue owns delivery and resend generations.
                // Calling TppMain.OnMessage directly loses a RESEND result.
                script<<"if type(Mission.SendMessageToSubscribers)=='function' and TppSequence.IsMissionPrepareFinished() then "
                    <<"Mission.SendMessageToSubscribers('Player','"
                    <<(event->kind==OpticEventKind::raised?"OnBinocularsMode":"PutMarkerWithBinocle")<<"'";
                if(event->kind==OpticEventKind::waypoint)
                    script<<','<<event->position.x<<','<<event->position.y<<','<<event->position.z;
                script<<");return 'queued' end return 'unavailable'";
                const auto text=script.str();
                int status=load(state,text.data(),text.size(),"@mgs5vr-physical-optic-event");
                if(!status)status=original(state,0,1,0);
                size_t length{};const auto* value=getString(state,-1,&length);
                if(!status&&value&&std::string_view(value,length)=="queued")acknowledgeOpticUse(*event);
                log("Physical optic player notification kind="+std::to_string(static_cast<int>(event->kind))
                    +" status="+std::to_string(status)+" result="+(value?std::string(value,std::min(length,size_t{180})):"no result"));
                setTop(state,top);
            }
        }
        // The retail game's named player-pad exclusion blocks character
        // actions without pausing skin, animation or terminal updates. Own a
        // separate registration so another tutorial's exclusion is preserved.
        static bool idroidPadRegistered{},accFobGuideHelpSeen{};
        static void* idroidPadState{};
        if(idroidPadState!=state){idroidPadRegistered=accFobGuideHelpSeen=false;idroidPadState=state;}
        const bool handheldIdroid=nativeIdroidOpen()&&!nativeIdroidClosing()&&handheldMenusSelected();
        // ACC already fixes the character in its seat and releases the native
        // player-pad exclusion for terminal controls. Retain that ownership;
        // field iDroid still excludes player movement. Resolve the mission in
        // this Lua transaction rather than an expiring render-side heartbeat.
        bool idroidCabin{},idroidPadPolicyKnown=true;
        if(handheldIdroid){
            constexpr std::string_view ownerScript=
                "if type(TppMission)=='table' and type(TppMission.IsHelicopterSpace)=='function' "
                "and type(vars.missionCode)=='number' then return "
                "TppMission.IsHelicopterSpace(vars.missionCode) and 'cabin' or 'field' end return 'unavailable'";
            int ownerStatus=load(state,ownerScript.data(),ownerScript.size(),"@mgs5vr-idroid-pad-owner");
            if(!ownerStatus)ownerStatus=original(state,0,1,0);
            size_t length{};const auto* owner=getString(state,-1,&length);
            idroidPadPolicyKnown=!ownerStatus&&owner&&length==5
                &&(std::memcmp(owner,"cabin",5)==0||std::memcmp(owner,"field",5)==0);
            idroidCabin=idroidPadPolicyKnown&&std::memcmp(owner,"cabin",5)==0;
            setTop(state,top);
        }
        const bool guardIdroid=handheldIdroid&&!idroidCabin;
        if(idroidPadPolicyKnown&&guardIdroid!=idroidPadRegistered){
            const std::string script=std::string("if type(TppGameStatus)=='table' and type(TppGameStatus.")
                +(guardIdroid?"Set":"Reset")+")== 'function' then TppGameStatus."
                +(guardIdroid?"Set":"Reset")+"('MGS5VR_iDroid','S_DISABLE_PLAYER_PAD'); return 'ok' end return 'unavailable'";
            int padStatus=load(state,script.data(),script.size(),"@mgs5vr-idroid-player-pad");
            if(!padStatus)padStatus=original(state,0,1,0);
            size_t length{};const auto* result=getString(state,-1,&length);
            if(!padStatus&&result&&length==2&&std::memcmp(result,"ok",2)==0){
                idroidPadRegistered=guardIdroid;
                log(guardIdroid?"Handheld iDroid: player pad excluded; hand and UI updates retained"
                    :"Handheld iDroid: owned player-pad exclusion released");
            }
            setTop(state,top);
        }
        setHandheldMenuInputReady(handheldIdroid&&idroidPadPolicyKnown&&guardIdroid==idroidPadRegistered);
        // This is the retail pause API used by TppMain and TppException.
        // Own one named registration and release only that registration;
        // another menu, loading transition or script may also hold a pause.
        static bool idroidPauseRegistered{};
        // A native close request precedes IsMbDvcTerminalOpened becoming
        // false: the character must run its stow animation in between.
        // Keeping our pause until the open bit clears deadlocks that exit.
        const bool pauseIdroid=nativeIdroidOpen()&&!nativeIdroidClosing()&&!handheldMenusSelected();
        if(pauseIdroid!=idroidPauseRegistered){
            // The ordinary menu mask also freezes the iDroid's own update:
            // opening stops halfway and Back/tab input never runs. Retail's
            // iDroid tutorial mask pauses gameplay while leaving UI active.
            const std::string script=pauseIdroid
                ?"if type(TppPause)=='table' and type(TppPause.RegisterPause)=='function' "
                 "and type(TppPause.PAUSE_LEVEL_MB_DVC_TUTORIAL)=='number' then "
                 "TppPause.RegisterPause('MGS5VR_iDroid',TppPause.PAUSE_LEVEL_MB_DVC_TUTORIAL); return 'ok' end return 'unavailable'"
                :"if type(TppPause)=='table' and type(TppPause.UnregisterPause)=='function' then "
                 "TppPause.UnregisterPause('MGS5VR_iDroid'); return 'ok' end return 'unavailable'";
            int pauseStatus=load(state,script.data(),script.size(),"@mgs5vr-idroid-pause");
            if(!pauseStatus)pauseStatus=original(state,0,1,0);
            size_t length{};const auto* result=getString(state,-1,&length);
            if(!pauseStatus&&result&&length==2&&std::memcmp(result,"ok",2)==0){
                idroidPauseRegistered=pauseIdroid;
                log(pauseIdroid?"iDroid quad: native gameplay paused; stereo retained":"iDroid quad: owned pause released");
            }
            setTop(state,top);
        }
        // Mode 10 is the native FOB_MISSION restriction (name hash f835f633).
        // Cold ACC can retain it after the campaign reports FINISH. Let the
        // player dismiss its informational Help first; then release only that
        // completed guide's input restriction and separately owned world pause.
        const auto guideMode=nativeIdroidTutorialMode();
        if(!nativeCabinPlay()||!nativeIdroidOpen()||!guideMode||*guideMode!=10)accFobGuideHelpSeen=false;
        else if(nativePauseMenuOpen())accFobGuideHelpSeen=true;
        if(nativeCabinPlay()&&nativeIdroidOpen()
           &&!nativePauseMenuOpen()&&accFobGuideHelpSeen&&guideMode&&*guideMode==10&&haveGuideRestrictionLease()){
            constexpr std::string_view guideScript=
                "if vars.missionCode==40010 and TppSequence.GetCurrentSequenceName()=='Seq_Game_MainGame' "
                "and gvars.trm_fobTutorialState==127 and not mvars.heliSpace_nowMissionListGuidance "
                "and type(TppUiCommand.SetTutorialMode)=='function' and not TppUiCommand.IsShowPopup() then "
                "TppUiCommand.SetTutorialMode(false); return 'released' end return 'active guide'";
            int guideStatus=load(state,guideScript.data(),guideScript.size(),"@mgs5vr-acc-completed-guide");
            if(!guideStatus)guideStatus=original(state,0,1,0);
            size_t length{};const auto* result=getString(state,-1,&length);
            if(!guideStatus&&result&&length==8&&std::memcmp(result,"released",8)==0){
                const auto restored=restoreCompletedGuideRestrictions();
                log(std::string("Completed ACC FOB guide released; native tutorial pause released=")
                    +(releaseNativeIdroidTutorialPause()?"1":"0")+"; prior menu restrictions restored="+(restored?"1":"0"));
            }
            setTop(state,top);
        }
        const bool heldBackClose=takeNativeIdroidClose();
        const auto tutorialMode=nativeIdroidTutorialMode();
        const auto recovery=nativeIdroidRecoveryForTutorialMode(tutorialMode);
        // ACC has no ordinary player terminal/stow task to consume +0x25.
        // Finish only a close already requested by native root Back. Other
        // game modes retain their native player animation/terminal lifecycle.
        const bool cabinClose=nativeCabinPlay()&&nativeIdroidOpen()&&nativeIdroidClosing()
            &&recovery==NativeIdroidRecovery::ordinary&&!nativePauseMenuOpen();
        if((heldBackClose||cabinClose)&&recovery!=NativeIdroidRecovery::refuse&&!nativePauseMenuOpen()){
            // Stop is the terminal's own cleanup path. CloseMbDvcTerminal only
            // sets a deferred close flag, which ACC does not consume.
            // Do not mark any tutorial complete or write progression variables.
            const std::string closeScript=
                "if type(TppUiCommand)=='table' and type(TppUiCommand.IsMbDvcTerminalOpened)=='function' "
                "and type(TppUiCommand.StopMbDvcTerminal)=='function' and TppUiCommand.IsMbDvcTerminalOpened() "
                "and type(TppUiCommand.IsShowPopup)=='function' and not TppUiCommand.IsShowPopup() then local completedGuide=false; "
                +std::string(cabinClose
                    ?"if not (vars.missionCode==40010 and TppSequence.GetCurrentSequenceName()=='Seq_Game_MainGame') then return 'different owner' end "
                    :"")
                // A player-requested recovery may release the reproduced stale
                // completed ACC guide restriction. Never complete a tutorial,
                // change save/progression variables, or cancel a live lesson.
                +std::string(heldBackClose&&nativeCabinPlay()&&tutorialMode&&*tutorialMode==10&&haveGuideRestrictionLease()
                    ?"if vars.missionCode==40010 and TppSequence.GetCurrentSequenceName()=='Seq_Game_MainGame' "
                     "and gvars.trm_fobTutorialState==127 and not mvars.heliSpace_nowMissionListGuidance "
                     "and type(TppUiCommand.SetTutorialMode)=='function' then TppUiCommand.SetTutorialMode(false); completedGuide=true end "
                    :"")
                +std::string(heldBackClose&&recovery==NativeIdroidRecovery::completedGuideOnly
                    ?"if not completedGuide then return 'live tutorial retained' end ":"")
                +"TppUiCommand.StopMbDvcTerminal(); return completedGuide and 'completed guide closed' or 'requested' end return 'already closed or overlay'";
            int closeStatus=load(state,closeScript.data(),closeScript.size(),"@mgs5vr-idroid-back-recovery");
            if(!closeStatus)closeStatus=original(state,0,1,0);
            size_t length{};const auto* result=getString(state,-1,&length);
            constexpr std::string_view completedGuide="completed guide closed";
            if(!closeStatus&&result&&length==completedGuide.size()&&std::memcmp(result,completedGuide.data(),length)==0){
                const auto restored=restoreCompletedGuideRestrictions();
                log(std::string("Requested ACC recovery: native tutorial pause released=")+(releaseNativeIdroidTutorialPause()?"1":"0")
                    +"; prior menu restrictions restored="+(restored?"1":"0"));
            }
            log(std::string(cabinClose?"Native ACC iDroid root-Back cleanup status=":"Native iDroid held-Back recovery status=")+std::to_string(closeStatus)+": "
                +(result?std::string(result,std::min(length,size_t{300})):"no result"));
            setTop(state,top);
        }
        int presentationStatus=load(state,nativePresentationScript.data(),nativePresentationScript.size(),"@mgs5vr-presentation-state");
        if(!presentationStatus)presentationStatus=original(state,0,1,0);
        size_t presentationLength{};const auto* presentationValue=getString(state,-1,&presentationLength);
        const bool avatarEditorActive=!presentationStatus&&presentationValue&&presentationLength==11
            &&std::memcmp(presentationValue,"avatar-edit",11)==0;
        const bool titleCabinActive=!presentationStatus&&presentationValue&&presentationLength==11
            &&std::memcmp(presentationValue,"title-cabin",11)==0;
        const bool scriptedDemoTitleActive=!presentationStatus&&presentationValue&&presentationLength==19
            &&std::memcmp(presentationValue,"scripted-demo-title",19)==0;
        constexpr std::string_view staleCandidatePrefix="stale-demo-candidate:";
        const bool staleDemoCandidate=!presentationStatus&&presentationValue
            &&presentationLength>staleCandidatePrefix.size()
            &&std::memcmp(presentationValue,staleCandidatePrefix.data(),staleCandidatePrefix.size())==0;
        const auto staleCandidateIdentity=staleDemoCandidate
            ?candidateKey(presentationValue+staleCandidatePrefix.size(),presentationLength-staleCandidatePrefix.size()):0;
        const bool scriptedLookTitleActive=!presentationStatus&&presentationValue&&presentationLength==19
            &&std::memcmp(presentationValue,"scripted-look-title",19)==0;
        const bool scriptedLookActive=scriptedLookTitleActive||(!presentationStatus&&presentationValue&&presentationLength==13
            &&std::memcmp(presentationValue,"scripted-look",13)==0);
        const bool titleActive=titleCabinActive||scriptedDemoTitleActive||scriptedLookTitleActive||(!presentationStatus&&presentationValue&&presentationLength==5
            &&std::memcmp(presentationValue,"title",5)==0);
        const bool cabinActive=!presentationStatus&&presentationValue&&presentationLength==5
            &&std::memcmp(presentationValue,"cabin",5)==0;
        const bool sceneMenuActive=!presentationStatus&&presentationValue
            &&((presentationLength==10&&std::memcmp(presentationValue,"cabin-menu",10)==0)
                ||(presentationLength==9&&std::memcmp(presentationValue,"game-over",9)==0));
        const bool scriptedDemoActive=scriptedDemoTitleActive||(!presentationStatus&&presentationValue&&presentationLength==13
            &&std::memcmp(presentationValue,"scripted-demo",13)==0);
        publishNativeAvatarEdit(avatarEditorActive);
        const bool knownDemoPresentation=scriptedLookActive||scriptedDemoActive||staleDemoCandidate
            ||(presentationValue&&((presentationLength==5&&std::memcmp(presentationValue,"title",5)==0)
                ||(presentationLength==5&&std::memcmp(presentationValue,"cabin",5)==0)
                ||(presentationLength==6&&std::memcmp(presentationValue,"closed",6)==0)
                ||avatarEditorActive||titleCabinActive||sceneMenuActive));
        // A failed or malformed Lua query is not a negative demo signal. Keep
        // the previous conservative mode; its candidate heartbeat expires.
        if(!presentationStatus&&knownDemoPresentation)
            publishNativeDemoMode(scriptedLookActive?NativeDemoMode::interactiveLook:
                scriptedDemoActive?NativeDemoMode::cinematic:
                staleDemoCandidate?NativeDemoMode::staleCandidate:NativeDemoMode::none,
                staleCandidateIdentity);
        publishNativeTitleMode(titleActive);
        publishNativeTitleCabinMode(titleCabinActive);
        publishNativeCabinPlay(cabinActive);
        publishNativeSceneMenu(sceneMenuActive);
        setTop(state,top);
        const bool requested=openingCabinEnabled()&&nativeTitleMenuOpen()&&headCamera().status().active;
        const std::string script=std::string("local requested=")+(requested?"true\n":"false\n")+std::string(nativeCabinScript);
        int status=load(state,script.data(),script.size(),"@mgs5vr-native-cabin");
        if(!status)status=original(state,0,1,0);
        size_t length{};const auto* value=getString(state,-1,&length);
        const std::string result=value?std::string(value,std::min(length,size_t{500})):"no result";
        setTop(state,top);
        static std::string previous;
        const auto report=std::to_string(status)+":"+result;
        if(report!=previous){log("Native cabin lifecycle "+report);previous=report;}
    }
    if(!externalCommands)return;
    traceCommands();
    std::error_code error;
    if(!std::filesystem::exists(requestPath,error))return;
    const auto size=std::filesystem::file_size(requestPath,error);
    if(error||!size||size>65536)return;
    std::ifstream input(requestPath,std::ios::binary);
    std::string request(static_cast<size_t>(size),'\0');
    if(!input.read(request.data(),static_cast<std::streamsize>(size)))return;
    input.close();
    if(request=="inspect-animal-touch"||request=="native-dog-response"||request=="inspect-small-animals"){
        const int top=getTop(state);
        constexpr char check[]="return type(TppMain)=='table' and type(GameObject)=='table' and type(gvars)=='table' and type(vars)=='table' and type(TppMission)=='table' and type(TppBuddyService)=='table' and 'ready' or 'wait'";
        int status=load(state,check,sizeof(check)-1,"@mgs5vr-state-check");
        if(!status)status=original(state,0,1,0);
        size_t length{};const auto* value=getString(state,-1,&length);
        const bool ready=!status&&value&&length==5&&std::memcmp(value,"ready",5)==0;
        setTop(state,top);
        if(!ready)return;
        const auto result=request=="inspect-animal-touch"?inspectAnimalTouch():request=="inspect-small-animals"?inspectSmallAnimals():requestNativeDogResponse();
        std::ofstream output(resultPath,std::ios::binary|std::ios::trunc);output<<"OK\n"<<result;output.close();
        std::filesystem::remove(requestPath,error);return;
    }
    const int top=getTop(state);
    if(top<0||top>4096)return;
    const std::string script="if type(TppMain)~='table' or type(GameObject)~='table' or type(gvars)~='table' or type(vars)~='table' or type(TppMission)~='table' or type(TppBuddyService)~='table' then return '__MGS5VR_WAIT__' end\n"+request;
    int status=load(state,script.data(),script.size(),"@mgs5vr-native-actions");
    if(!status)status=original(state,0,1,0);
    size_t length{};const auto* text=getString(state,-1,&length);
    const std::string result=text?std::string(text,(std::min)(length,size_t{32768})):"(no return value)";
    setTop(state,top);
    if(!status&&result=="__MGS5VR_WAIT__")return;
    std::ofstream output(resultPath,std::ios::binary|std::ios::trunc);
    output<<(status?"ERROR\n":"OK\n")<<result;output.close();
    std::filesystem::remove(requestPath,error);
    log("Native action request completed status="+std::to_string(status)+" result="+result.substr(0,500));
}
int call(void* state,int arguments,int results,int handler){
    ++depth;
    const auto status=original(state,arguments,results,handler);
    if(enabled.load()&&depth==1)try{pump(state);}catch(const std::exception& e){log(std::string("Native action request failed: ")+e.what());}
    --depth;return status;
}
template<size_t N> bool matches(uintptr_t address,const std::array<unsigned char,N>& expected){
    std::array<unsigned char,N> bytes{};SIZE_T count{};
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),bytes.data(),N,&count)
        &&count==N&&bytes==expected;
}
}
void installNativeActions(uintptr_t base,const std::filesystem::path& folder,bool allowExternalCommands){
    if(!matches(base+0x1a116c0,std::array<unsigned char,13>{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x40,0x41,0x8b,0xf8})
       ||!matches(base+0x1a178e0,std::array<unsigned char,9>{0x48,0x83,0xec,0x38,0x48,0x89,0x54,0x24,0x20})
       ||!matches(base+0x1a112e0,std::array<unsigned char,13>{0x48,0x8b,0x41,0x10,0x48,0x2b,0x41,0x18,0x48,0xc1,0xf8,0x04,0xc3})
       ||!matches(base+0x1a11f70,std::array<unsigned char,7>{0x85,0xd2,0x78,0x42,0x4c,0x63,0xc2})
       ||!matches(base+0x1a12150,std::array<unsigned char,10>{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10}))
        throw std::runtime_error("Native Lua action ABI differs");
    load=reinterpret_cast<Load>(base+0x1a178e0);getTop=reinterpret_cast<Top>(base+0x1a112e0);
    setTop=reinterpret_cast<SetTop>(base+0x1a11f70);getString=reinterpret_cast<String>(base+0x1a12150);
    requestPath=folder/L"mgs5vr-native-actions.lua";resultPath=folder/L"mgs5vr-native-actions-result.txt";
    imageBase=base;tracePath=folder/L"mgs5vr-animal-trace.txt";
    auto* address=reinterpret_cast<void*>(base+0x1a116c0);
    if(MH_CreateHook(address,reinterpret_cast<void*>(&call),reinterpret_cast<void**>(&original))!=MH_OK||MH_EnableHook(address)!=MH_OK)
        throw std::runtime_error("Cannot install native action queue");
    externalCommands=allowExternalCommands;
    if(externalCommands){
        const bool abi=matches(base+0x1dd7ba0,std::array<unsigned char,10>{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20})
            &&matches(base+0x92a160,std::array<unsigned char,12>{0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x48,0x8b,0x49});
        if(abi){
            auto* button=reinterpret_cast<void*>(base+0x1dd7ba0);
            auto* close=reinterpret_cast<void*>(base+0x92a160);
            if(MH_CreateHook(button,reinterpret_cast<void*>(&uiButton),reinterpret_cast<void**>(&originalUiButton))==MH_OK
               &&MH_EnableHook(button)==MH_OK
               &&MH_CreateHook(close,reinterpret_cast<void*>(&uiClose),reinterpret_cast<void**>(&originalUiClose))==MH_OK
               &&MH_EnableHook(close)==MH_OK)log("Bounded native menu input/stow diagnostics available");
            else log("Native menu diagnostics unavailable; native behavior retained");
        }else log("Native menu diagnostic ABI differs; observation hooks disabled");
    }
    const bool restrictionAbi=matches(base+0x9683d0,std::array<unsigned char,9>{0x4c,0x8b,0xc9,0x0f,0xb6,0xc2,0x45,0x84,0xc0})
        &&matches(base+0xf42840,std::array<unsigned char,11>{0x48,0x8b,0x46,0x28,0x41,0xb0,0x01,0x40,0x0f,0xb6,0xd7})
        &&matches(base+0xf42855,std::array<unsigned char,9>{0xff,0x90,0xa0,0,0,0,0x40,0xfe,0xc7});
    if(restrictionAbi){
        auto* disable=reinterpret_cast<void*>(base+0x9683d0);
        menuRestrictionAbi=MH_CreateHook(disable,reinterpret_cast<void*>(&uiDisable),reinterpret_cast<void**>(&originalUiDisable))==MH_OK
            &&MH_EnableHook(disable)==MH_OK;
        if(menuRestrictionAbi)log("Native tutorial menu-restriction ownership captured; completed ACC guide recovery available");
    }
    enabled.store(true);
    if(externalCommands){actionPipeStopping.store(false);actionPipeThread=std::thread(actionPipeLoop);}
    log(externalCommands?"Native title reader and local action queue installed":"Native title reader installed; external actions disabled");
}
void stopNativeActions() noexcept{
    enabled.store(false);requestNativeIdroidClose(false);actionPipeStopping.store(true);
    std::shared_ptr<ActionConnection> connection;{std::lock_guard lock(connectionMutex);connection=activeConnection;}
    if(connection)connection->responseReady.notify_all();
    if(connection&&connection->pipe!=INVALID_HANDLE_VALUE)CancelIoEx(connection->pipe,nullptr);
    wakeActionPipe();
    if(actionPipeThread.joinable())actionPipeThread.join();
    std::lock_guard lock(fastQueueMutex);fastQueue.clear();
}
}
