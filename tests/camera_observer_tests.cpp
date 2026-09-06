#include "mgs5vr/core.hpp"
#include "mgs5vr/camera_consumer.hpp"
#include "mgs5vr/player_visibility.hpp"
#include <windows.h>
#include <MinHook.h>
#include <array>
#include <cstring>
#include <iostream>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

extern "C" {
extern void* MgsCameraTrampoline;
void MgsCameraIntercept();
}
namespace {
template<class T> T visibilityField(void* object,size_t offset){
    T value{};std::memcpy(&value,static_cast<unsigned char*>(object)+offset,sizeof(value));return value;
}
void fixtureVisible(void* object,uint32_t group,bool show){
    const auto count=visibilityField<uint16_t>(object,0x1e8);
    const auto* names=visibilityField<const uint32_t*>(object,0x180);
    auto* flags=visibilityField<uint8_t*>(object,0x170);
    for(uint16_t i=0;i<count;++i)if(names[i]==group){
        if(show)flags[i]|=4;else flags[i]&=static_cast<uint8_t>(~4u);
        break;
    }
}
void fixtureHideVisible(void* object,uint32_t group){fixtureVisible(object,group,false);}
void fixtureShowVisible(void* object,uint32_t group){fixtureVisible(object,group,true);}
int visibilityChecks(){
    // Signature-qualified synthetic helpers change only normal draw membership.
    // The assertions cover shadow preservation and guarded ownership/restoration.
    auto* region=static_cast<unsigned char*>(VirtualAlloc(nullptr,0x1d0000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if(!region)return 1;
    const uintptr_t base=reinterpret_cast<uintptr_t>(region);
    constexpr unsigned char hidePrefix[]={0x40,0x53,0x48,0x83,0xec,0x20,0x44,0x0f,0xb7,0x89,0xe8,0x01,0,0,0x33,0xc0,0x48,0x8b,0xd9,0x45,0x85,0xc9};
    constexpr unsigned char showPrefix[]={0x48,0x83,0xec,0x28,0x44,0x0f,0xb7,0x91,0xe8,0x01,0,0,0x33,0xc0,0x4c,0x8b,0xd9,0x44,0x8b,0xc0,0x45,0x85,0xd2};
    const auto helper=[&](size_t offset,const auto& prefix,uintptr_t destination,bool hide){
        auto* out=region+offset;std::memcpy(out,prefix,sizeof(prefix));out+=sizeof(prefix);
        const unsigned char unwind[]={0x48,0x83,0xc4,static_cast<unsigned char>(hide?0x20:0x28)};
        std::memcpy(out,unwind,sizeof(unwind));out+=sizeof(unwind);if(hide)*out++=0x5b;
        *out++=0x48;*out++=0xb8;std::memcpy(out,&destination,8);out+=8;*out++=0xff;*out=0xe0;
    };
    helper(0x1ccc30,hidePrefix,reinterpret_cast<uintptr_t>(&fixtureHideVisible),true);
    helper(0x1cee30,showPrefix,reinterpret_cast<uintptr_t>(&fixtureShowVisible),false);
    DWORD prior{};if(!VirtualProtect(region,0x1d0000,PAGE_EXECUTE_READ,&prior)){VirtualFree(region,0,MEM_RELEASE);return 1;}
    FlushInstructionCache(GetCurrentProcess(),region,0x1d0000);
    std::array<unsigned char,0x388> owner{};
    std::array<unsigned char,0x618> character{};
    std::array<unsigned char,0x18> component{};
    std::array<unsigned char,0x70> bodyParts{};
    std::array<unsigned char,0x50> parts{};
    std::array<unsigned char,0x18> list{};
    std::array<unsigned char,0x48> renderer{};
    std::array<unsigned char,0x200> model{},body{},replacement{};
    std::array<uint32_t,3> groups{0xf948d635,0xa9e88501,0xdc3a5d6d};
    std::array<uint32_t,3> bodyGroups{0xf948d635,0x1a166b34,0x4e74fd8c};
    std::array<uint8_t,3> flags{15,15,7},bodyFlags{15,15,15},replacementFlags{15,15,7};
    const auto nativeFlags=flags,nativeBodyFlags=bodyFlags;
    std::array<uintptr_t,1> records{},entry{};
    const auto address=[](auto& a){return reinterpret_cast<uintptr_t>(a.data());};
    const auto put=[](auto& a,size_t offset,auto value){std::memcpy(a.data()+offset,&value,sizeof(value));};
    const auto mask=[](const auto& a){uint32_t value{};std::memcpy(&value,a.data()+0x1a4,4);return value;};
    put(owner,0,base+0x23b8218);put(owner,0x370,address(character));
    put(character,0,base+0x2295210);put(character,0x610,address(parts));put(character,0x10,address(component));
    put(component,0x10,address(bodyParts));put(bodyParts,0x68,address(body));
    put(parts,0,base+0x22e56c0);put(parts,0x38,address(character));put(parts,0x48,address(list));
    put(list,0,base+0x2215c78);put(list,8,address(records));put(list,0x10,uint32_t{1});
    records[0]=address(entry);entry[0]=address(renderer);
    put(renderer,0,base+0x20f9460);put(renderer,0x40,address(model));
    put(model,0,base+0x20f4d90);put(model,0x180,address(groups));put(model,0x170,address(flags));put(model,0x1e8,uint16_t{3});put(model,0x1a4,uint32_t{2});
    put(body,0,base+0x20f4d90);put(body,0x180,address(bodyGroups));put(body,0x170,address(bodyFlags));put(body,0x1e8,uint16_t{3});
    const auto original=model,originalBody=body;int failures=0;
    mgs5vr::initializePlayerVisibility(base);
    const auto update=[&](bool enabled){mgs5vr::updatePlayerVisibility(address(owner),enabled);};
    update(true);
    if(model!=original||body!=originalBody||flags!=std::array<uint8_t,3>{11,11,3}||bodyFlags!=std::array<uint8_t,3>{15,11,15})++failures;
    update(false);if(flags!=nativeFlags||bodyFlags!=nativeBodyFlags||model!=original)++failures;
    put(parts,0x38,uintptr_t{});update(true);if(flags!=nativeFlags)++failures;update(false);
    put(parts,0x38,address(character));groups[2]=0x4e74fd8c;
    update(true);if(flags!=nativeFlags)++failures;update(false);groups[2]=0xdc3a5d6d;
    put(list,0x10,uint32_t{33});update(true);if(flags!=nativeFlags)++failures;update(false);
    put(list,0x10,uint32_t{1});update(true);flags[1]=0;put(model,0x1a4,uint32_t{4});
    update(false);if(flags!=std::array<uint8_t,3>{15,0,7}||mask(model)!=4)++failures;
    model=original;flags=nativeFlags;
    update(true);replacement=original;put(replacement,0x170,address(replacementFlags));put(renderer,0x40,address(replacement));
    update(true);update(false);
    if(flags!=std::array<uint8_t,3>{11,11,3}||replacementFlags!=nativeFlags||mask(model)!=2)++failures;
    put(renderer,0x40,address(model));flags=nativeFlags;
    if(VirtualProtect(region,0x1d0000,PAGE_READWRITE,&prior)){
        region[0x1ccc30]=0x90;FlushInstructionCache(GetCurrentProcess(),region,0x1d0000);
        mgs5vr::initializePlayerVisibility(base);update(true);
        if(flags!=nativeFlags||bodyFlags!=nativeBodyFlags)++failures;
    }else ++failures;
    update(false);mgs5vr::initializePlayerVisibility(0);VirtualFree(region,0,MEM_RELEASE);
    std::cout<<"Player-only normal visibility, native shadow preservation, signature refusal and ownership restoration; "<<failures<<" failures.\n";
    return failures;
}
int consumerChecks(){
    // Synthetic camera storage, including both branches of each native getter.
    // The test checks pointer identity and that observing never mutates storage.
    constexpr std::array<uintptr_t,3> offsets{0x44ddd0,0x44dc70,0x44dca0};
    constexpr unsigned char raw[]={0x48,0x8d,0x81,0xf0,0,0,0,0xc3};
    constexpr unsigned char alt1[]={0x80,0xb9,0xb1,0,0,0,0,0x48,0x8d,0x81,0x10,1,0,0,0x75,7,0x48,0x8d,0x81,0xf0,0,0,0,0xf3,0xc3};
    constexpr unsigned char alt2[]={0x80,0xb9,0xb2,0,0,0,0,0x48,0x8d,0x81,0x30,1,0,0,0x75,7,0x48,0x8d,0x81,0xf0,0,0,0,0xf3,0xc3};
    auto* region=static_cast<unsigned char*>(VirtualAlloc(nullptr,0x450000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if(!region)return 1;
    std::memcpy(region+offsets[0],raw,sizeof(raw));std::memcpy(region+offsets[1],alt1,sizeof(alt1));std::memcpy(region+offsets[2],alt2,sizeof(alt2));
    DWORD prior{};
    if(!VirtualProtect(region,0x450000,PAGE_EXECUTE_READ,&prior)){VirtualFree(region,0,MEM_RELEASE);return 1;}
    FlushInstructionCache(GetCurrentProcess(),region,0x450000);
    const auto directory=std::filesystem::temp_directory_path()/("mgs5vr-consumer-fixture-"+std::to_string(GetCurrentProcessId()));
    int failures=0;
    try{
        bool refused=false;
        try{mgs5vr::installCameraConsumerObserver(reinterpret_cast<uintptr_t>(region)+1,directory);}
        catch(const std::runtime_error&){refused=true;}
        if(!refused)++failures;
        mgs5vr::installCameraConsumerObserver(reinterpret_cast<uintptr_t>(region),directory);
        using Getter=const float*(*)(void*);
        alignas(16) std::array<unsigned char,0x180> object{};
        for(unsigned flags=0;flags<4;++flags){
            object.fill(0x5a);object[0xb1]=static_cast<unsigned char>(flags&1);object[0xb2]=static_cast<unsigned char>(flags&2);
            const auto before=object;
            for(unsigned repeat=0;repeat<10;++repeat)for(size_t n=0;n<3;++n){
                const auto selected=n==1&&object[0xb1]?0x110:n==2&&object[0xb2]?0x130:0xf0;
                const auto result=reinterpret_cast<Getter>(region+offsets[n])(object.data());
                if(reinterpret_cast<const unsigned char*>(result)!=object.data()+selected||object!=before)++failures;
            }
        }
        mgs5vr::reportCameraConsumers();
        mgs5vr::stopCameraConsumerObserver();
        bool found=false;
        for(const auto& entry:std::filesystem::directory_iterator(directory)){
            std::ifstream file(entry.path());const std::string content{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
            if(content.find("\"method_rva\":\"0x44ddd0\"")!=std::string::npos
                &&content.find("\"method_rva\":\"0x44dc70\"")!=std::string::npos
                &&content.find("\"method_rva\":\"0x44dca0\"")!=std::string::npos)found=true;
        }
        if(!found)++failures;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';++failures;}
    mgs5vr::stopCameraConsumerObserver();
    for(const auto offset:offsets){MH_DisableHook(region+offset);MH_RemoveHook(region+offset);}
    VirtualFree(region,0,MEM_RELEASE);
    if(std::filesystem::exists(directory)){
        for(const auto& entry:std::filesystem::directory_iterator(directory))std::filesystem::remove(entry.path());
        std::filesystem::remove(directory);
    }
    std::cout<<"120 synthetic camera getter calls and mismatched signature refusal; "<<failures<<" failures.\n";
    return failures;
}
}
int main(){
    // A synthetic setter with the independently verified instruction layout.
    // This validates our detour/shim; it does not identify a retail camera.
    constexpr unsigned char leaf[]={0x0f,0x28,0x02,0x0f,0x29,0x81,0xf0,0,0,0,0x0f,0x28,0x4a,0x10,
        0x0f,0x29,0x89,0,1,0,0,0xc3};
    void* code=VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!code)return 1;
    std::memcpy(code,leaf,sizeof(leaf));DWORD old{};
    if(!VirtualProtect(code,4096,PAGE_EXECUTE_READ,&old)){VirtualFree(code,0,MEM_RELEASE);return 1;}
    FlushInstructionCache(GetCurrentProcess(),code,sizeof(leaf));
    if(MH_Initialize()!=MH_OK||MH_CreateHook(code,reinterpret_cast<void*>(&MgsCameraIntercept),&MgsCameraTrampoline)!=MH_OK
        ||MH_EnableHook(code)!=MH_OK){MH_Uninitialize();VirtualFree(code,0,MEM_RELEASE);return 1;}
    alignas(16) std::array<float,8> input{0,0,0,1,1,2,3,0};
    alignas(16) std::array<unsigned char,0x120> output{};
    using Setter=void(*)(void*,const float*);
    int failures=0;
    for(int n=0;n<100;++n){
        output.fill(0x5a);input[4]=static_cast<float>(n);
        const auto originalInput=input;
        reinterpret_cast<Setter>(code)(output.data(),input.data());
        if(std::memcmp(output.data()+0xf0,input.data(),32)!=0||input!=originalInput)++failures;
        for(size_t i=0;i<output.size();++i)if((i<0xf0||i>=0x110)&&output[i]!=0x5a)++failures;
    }
    MH_DisableHook(code);MH_RemoveHook(code);VirtualFree(code,0,MEM_RELEASE);
    std::cout<<"100 synthetic setter calls through camera observer; "<<failures<<" failures. Not in-game camera proof.\n";
    failures+=consumerChecks();failures+=visibilityChecks();MH_Uninitialize();
    return failures?1:0;
}
