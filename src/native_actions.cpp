#include "mgs5vr/native_actions.hpp"
#include "mgs5vr/native_controls.hpp"
#include "mgs5vr/log.hpp"
#include "mgs5vr/animal_interaction.hpp"
#include "mgs5vr/small_animal.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/controller_rig.hpp"
#include "mgs5vr/ui_renderer.hpp"
#include "mgs5vr/opening_selector.hpp"
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
#include <sstream>
#include <thread>
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
std::mutex requestMutex;
std::filesystem::path requestPath,resultPath,tracePath;
bool externalCommands{};
uint64_t checkedAt{};
thread_local unsigned depth{};
constexpr wchar_t actionPipeName[]=L"\\\\.\\pipe\\MGS5VR.NativeActions";
constexpr uint32_t maxPipeScript=1024*1024;
constexpr size_t maxQueuedActions=256;
constexpr size_t maxActionsPerTransaction=8;
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
    setTop(state,top);return ready;
}
bool publishFastInput(const std::string& request,std::string& result){
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
        if(takeNativeIdroidClose()){
            // Stop is the terminal's own cleanup path. CloseMbDvcTerminal only
            // sets a deferred close flag, which the forced FOB tutorial ignores.
            // Do not mark any tutorial complete or write progression variables.
            constexpr char closeScript[]=
                "if type(TppUiCommand)=='table' and type(TppUiCommand.IsMbDvcTerminalOpened)=='function' "
                "and type(TppUiCommand.StopMbDvcTerminal)=='function' and TppUiCommand.IsMbDvcTerminalOpened() "
                "then TppUiCommand.StopMbDvcTerminal(); return 'requested' end return 'already closed'";
            int closeStatus=load(state,closeScript,sizeof(closeScript)-1,"@mgs5vr-idroid-back-recovery");
            if(!closeStatus)closeStatus=original(state,0,1,0);
            size_t length{};const auto* result=getString(state,-1,&length);
            log("Native iDroid held-Back recovery status="+std::to_string(closeStatus)+": "
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
        const bool titleActive=titleCabinActive||scriptedDemoTitleActive||(!presentationStatus&&presentationValue&&presentationLength==5
            &&std::memcmp(presentationValue,"title",5)==0);
        const bool cabinActive=!presentationStatus&&presentationValue&&presentationLength==5
            &&std::memcmp(presentationValue,"cabin",5)==0;
        const bool scriptedDemoActive=scriptedDemoTitleActive||(!presentationStatus&&presentationValue&&presentationLength==13
            &&std::memcmp(presentationValue,"scripted-demo",13)==0);
        publishNativeAvatarEdit(avatarEditorActive);
        publishNativeScriptedDemo(scriptedDemoActive);
        publishNativeTitleMode(titleActive);
        publishNativeTitleCabinMode(titleCabinActive);
        publishNativeCabinPlay(cabinActive);
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
