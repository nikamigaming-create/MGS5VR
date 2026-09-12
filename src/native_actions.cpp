#include "mgs5vr/native_actions.hpp"
#include "mgs5vr/log.hpp"
#include "mgs5vr/animal_interaction.hpp"
#include "mgs5vr/small_animal.hpp"
#include <windows.h>
#include <MinHook.h>
#include <intrin.h>
#include <array>
#include <atomic>
#include <cstring>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <sstream>
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
uint64_t checkedAt{};
thread_local unsigned depth{};
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
void pump(void* state){
    std::unique_lock lock(requestMutex,std::try_to_lock);
    const auto now=GetTickCount64();
    if(!lock.owns_lock()||now-checkedAt<250)return;
    checkedAt=now;
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
        constexpr char check[]="return type(TppMain)=='table' and type(GameObject)=='table' and 'ready' or 'wait'";
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
    const std::string script="if type(TppMain)~='table' or type(GameObject)~='table' then return '__MGS5VR_WAIT__' end\n"+request;
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
void installNativeActions(uintptr_t base,const std::filesystem::path& folder){
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
    enabled.store(true);log("Local native action queue installed on the game's Lua transaction thread");
}
void stopNativeActions() noexcept{enabled.store(false);}
}
