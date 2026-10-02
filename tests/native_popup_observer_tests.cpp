#include "mgs5vr/native_popup_state.hpp"
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>

using namespace mgs5vr;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Memory {
    static constexpr uintptr_t base=0x140000000,system=0x100000,child=0x200000,second=0x300000,choiceOwner=0x500000;
    std::map<uintptr_t,unsigned char> bytes;
    std::function<void(uintptr_t,size_t)> beforeRead;
    unsigned reads{};
    template<class T>void write(uintptr_t address,const T& value){
        const auto* data=reinterpret_cast<const unsigned char*>(&value);
        for(size_t i=0;i<sizeof(value);++i)bytes[address+i]=data[i];
    }
    bool read(uintptr_t address,void* output,size_t size){
        ++reads;if(beforeRead)beforeRead(address,size);
        auto* data=static_cast<unsigned char*>(output);
        for(size_t i=0;i<size;++i){
            const auto at=bytes.find(address+i);if(at==bytes.end())return false;
            data[i]=at->second;
        }
        return true;
    }
    Memory(){
        write(base+0x2bf1940,system);write(system,base+0x22447e8);
        write(system+0xe80,std::array<uintptr_t,2>{child,0});
        std::array<unsigned char,0x28> parameters{};
        const uint32_t id=3100;const int32_t result=2;const uint64_t name=0xabcdef123456;
        std::memcpy(parameters.data(),&id,sizeof(id));
        std::memcpy(parameters.data()+8,&result,sizeof(result));
        std::memcpy(parameters.data()+0x20,&name,sizeof(name));
        write(system+0x1510,parameters);makeChild(child,true,true);
    }
    void makeChild(uintptr_t address,bool open,bool ready){
        std::array<unsigned char,0x23> values{};
        const uintptr_t type=base+0x2270000,layout=0x400000;
        std::memcpy(values.data(),&type,sizeof(type));
        std::memcpy(values.data()+0x10,&layout,sizeof(layout));
        values[0x20]=static_cast<unsigned char>(open);values[0x22]=static_cast<unsigned char>(ready);
        write(address,values);
    }
    void makeChoice(uintptr_t address=choiceOwner,int32_t index=0,uint32_t count=2,bool negative=true){
        write(address,base+0x224ba40);
        std::array<unsigned char,0x30> state{};state[0]=1;
        const uint32_t phase=2,id=3100;
        std::memcpy(state.data()+4,&phase,sizeof(phase));
        std::memcpy(state.data()+8,&id,sizeof(id));
        std::memcpy(state.data()+0xc,&index,sizeof(index));
        std::memcpy(state.data()+0x10,&count,sizeof(count));
        std::memcpy(state.data()+0x28,&system,sizeof(system));
        write(address+0xc0,state);write(address+0x390,uint64_t{0xabcdef123456ULL});
        write(address+0x3b8,static_cast<unsigned char>(negative));
        write(address+0x3e0,static_cast<unsigned char>(negative));
    }
    NativePopupSnapshot snapshot(bool verified=true,const NativePopupChoiceLeases& leases={},uint64_t now=1000){
        return readNativePopupSnapshot(base,verified,[&](uintptr_t address,void* output,size_t size){
            return read(address,output,size);
        },leases,now);
    }
};
static bool unknown(const NativePopupSnapshot& value){
    return !value.active&&!value.numericId&&!value.stringId&&!value.lastResult;
}
int main(){try{
    {
        std::ostringstream out;out<<std::hex;
        writeNativePopupSnapshotJson(out,{},100);
        require(out.str()=="{\"reader_verified\":false,\"owner_verified\":false,\"sample_ms\":100,\"coherent_frame\":false,"
                           "\"active\":null,\"numeric_id\":null,\"string_id\":null,\"last_result\":null,"
                           "\"choice_owner_verified\":false,\"choice_sample_ms\":null,\"current_choice_index\":null,\"current_choice_negative\":null}",
                "wire contract preserves unsupported/unknown readiness and IDs as null");
        out<<16;
        require(out.str().ends_with("}10"),"JSON formatter does not change caller numeric formatting");
        Memory memory;memory.write(Memory::system+0x1510,uint32_t{});
        memory.write(Memory::system+0x1530,uint64_t{0xffffffffffffffffULL});
        memory.write(Memory::system+0x1518,int32_t{-1});
        std::ostringstream active;writeNativePopupSnapshotJson(active,memory.snapshot(),101);
        require(active.str()=="{\"reader_verified\":true,\"owner_verified\":true,\"sample_ms\":101,\"coherent_frame\":false,"
                              "\"active\":true,\"numeric_id\":0,\"string_id\":\"0xffffffffffffffff\",\"last_result\":-1,"
                              "\"choice_owner_verified\":false,\"choice_sample_ms\":null,\"current_choice_index\":null,\"current_choice_negative\":null}",
                "wire contract preserves zero ID, full StringId precision and signed completed response");
        memory.makeChild(Memory::child,false,true);
        std::ostringstream closed;writeNativePopupSnapshotJson(closed,memory.snapshot(),102);
        require(closed.str()=="{\"reader_verified\":true,\"owner_verified\":true,\"sample_ms\":102,\"coherent_frame\":false,"
                              "\"active\":false,\"numeric_id\":null,\"string_id\":null,\"last_result\":-1,"
                              "\"choice_owner_verified\":false,\"choice_sample_ms\":null,\"current_choice_index\":null,\"current_choice_negative\":null}",
                "wire contract distinguishes closed popup from unknown and completed response from current identity");
    }
    {
        Memory memory;const auto before=memory.bytes;const auto popup=memory.snapshot();
        require(popup.readerVerified&&popup.ownerVerified&&popup.active==true,"typed active owner is established");
        require(popup.numericId==3100U&&popup.stringId==0xabcdef123456ULL,"numeric and StringId retain distinct current identity");
        require(popup.lastResult==2,"last completed result is separate from current identity");
        require(memory.bytes==before,"observer changes no native bytes");
    }
    {
        Memory memory;require(unknown(memory.snapshot(false))&&memory.reads==0,"unverified ABI performs no reads");
        memory.write(Memory::system,uintptr_t{Memory::base+0x22447e0});
        require(unknown(memory.snapshot()),"wrong UiSystem type stays unknown");
    }
    {
        Memory memory;memory.makeChild(Memory::child,false,true);
        const auto popup=memory.snapshot();
        require(popup.active==false&&!popup.numericId&&!popup.stringId&&popup.lastResult==2,
                "closed owner cannot expose a stale ID as current; last result remains identified");
        memory.write(Memory::system+0x1518,int32_t{-1});
        require(memory.snapshot().lastResult==-1,"signed native response sentinel is preserved");
    }
    {
        Memory memory;memory.makeChild(Memory::child,true,false);
        require(unknown(memory.snapshot()),"open but uninitialized readiness is unknown without invoking native cache writes");
        memory.makeChild(Memory::child,false,false);
        require(memory.snapshot().active==false,"closed owner is inactive even before layout readiness");
        memory.makeChild(Memory::child,true,true);memory.write(Memory::child+0x10,uintptr_t{});
        require(memory.snapshot().active==false,"missing native layout cannot establish active popup");
    }
    {
        Memory memory;memory.makeChild(Memory::child,true,false);memory.makeChild(Memory::second,true,true);
        memory.write(Memory::system+0xe80,std::array<uintptr_t,2>{Memory::child,Memory::second});
        require(memory.snapshot().active==true,"one established active terminal is enough for native OR ownership");
        memory.makeChild(Memory::second,false,true);
        require(unknown(memory.snapshot()),"inactive second terminal cannot make unknown first terminal inactive");
        memory.write(Memory::system+0xe80,std::array<uintptr_t,2>{0,0});
        require(memory.snapshot().active==false,"both absent popup terminal slots establish no active popup");
    }
    for(const uintptr_t missing:{Memory::system+0xe8f,Memory::system+0x1537,Memory::child+0x22}){
        Memory memory;memory.bytes.erase(missing);
        require(unknown(memory.snapshot()),"partial read never supplies a zero-filled false state or ID");
    }
    for(const uintptr_t changed:{Memory::base+0x2bf1940,Memory::system+0xe80,Memory::system+0x1510,Memory::child}){
        Memory memory;unsigned visits=0;
        memory.beforeRead=[&](uintptr_t address,size_t){
            if(address==changed&&++visits==2)memory.bytes[changed]^=1;
        };
        const auto popup=memory.snapshot();
        require(unknown(popup)&&!popup.ownerVerified,"replaced parent/child, changed dialog or changing child snapshot stays unknown");
    }
    {
        Memory memory;memory.write(Memory::child+0x20,static_cast<unsigned char>(2));
        require(unknown(memory.snapshot()),"invalid native boolean cannot authorize a popup");
        memory.write(Memory::child+0x20,static_cast<unsigned char>(1));memory.write(Memory::child,uintptr_t{});
        require(unknown(memory.snapshot()),"uninitialized terminal object cannot provide an active owner");
    }
    const NativePopupChoiceLeases leased{{{Memory::choiceOwner,900},{},{},{}}};
    const auto choiceUnknown=[](const auto& popup){return !popup.choiceOwnerVerified&&!popup.currentChoiceIndex
        &&!popup.currentChoiceNegative&&!popup.choiceSampleMs;};
    {
        Memory memory;memory.makeChoice();const auto before=memory.bytes;
        const auto popup=memory.snapshot(true,leased);
        require(popup.choiceOwnerVerified&&popup.currentChoiceIndex==0U&&popup.currentChoiceNegative==true
            &&popup.choiceSampleMs==900U,"fresh typed native owner reports actual selected negative button");
        require(popup.lastResult==2&&memory.bytes==before,"current choice never overwrites last result or native memory");
        memory.makeChoice(Memory::choiceOwner,1,2,false);
        memory.write(Memory::system+0x1518,int32_t{0});
        const auto positive=memory.snapshot(true,leased);
        require(positive.currentChoiceIndex==1U&&positive.currentChoiceNegative==false&&positive.lastResult==0,
            "live positive choice remains distinct from completed response zero");
        std::ostringstream out;writeNativePopupSnapshotJson(out,positive,1000);
        require(out.str().find("\"choice_owner_verified\":true,\"choice_sample_ms\":900,\"current_choice_index\":1,\"current_choice_negative\":false")!=std::string::npos,
            "wire output preserves a false negative flag rather than treating it as unknown");
        memory.makeChoice(Memory::choiceOwner,1,1,false);
        require(memory.snapshot(true,leased).currentChoiceIndex==1U,"one visible native button uses physical slot one");
    }
    {
        Memory memory;memory.makeChoice();
        require(choiceUnknown(memory.snapshot()),"unpublished memory cannot establish callback ownership");
        require(choiceUnknown(memory.snapshot(true,leased,1051)),"expired native callback lease cannot establish current choice");
        require(choiceUnknown(memory.snapshot(true,leased,899)),"future callback timestamp cannot establish current choice");
        memory.makeChild(Memory::child,false,true);
        require(choiceUnknown(memory.snapshot(true,leased)),"closed popup cannot borrow a current choice from a live component");
    }
    for(const auto [index,count]:{std::pair{-1,2U},std::pair{2,2U},std::pair{0,0U},std::pair{0,3U},std::pair{0,1U}}){
        Memory memory;memory.makeChoice(Memory::choiceOwner,index,count);
        require(choiceUnknown(memory.snapshot(true,leased)),"invalid selected index or native button count cannot authorize confirmation");
    }
    for(const uintptr_t changed:{Memory::choiceOwner,Memory::choiceOwner+0xc0,Memory::choiceOwner+0xc4,
        Memory::choiceOwner+0xc8,Memory::choiceOwner+0xe8,Memory::choiceOwner+0x390,Memory::choiceOwner+0x3b8}){
        Memory memory;memory.makeChoice();
        if(changed==Memory::choiceOwner+0x3b8)memory.bytes[changed]=2;else memory.bytes[changed]^=1;
        require(choiceUnknown(memory.snapshot(true,leased)),"wrong type, readiness, activity, dialog, backlink or flag stays unknown");
    }
    for(const uintptr_t missing:{Memory::choiceOwner+0xef,Memory::choiceOwner+0x397,Memory::choiceOwner+0x3b8}){
        Memory memory;memory.makeChoice();memory.bytes.erase(missing);
        require(choiceUnknown(memory.snapshot(true,leased)),"incomplete native choice reads remain unknown");
    }
    for(const uintptr_t changed:{Memory::choiceOwner,Memory::choiceOwner+0xc0,Memory::choiceOwner+0x390,Memory::choiceOwner+0x3b8}){
        Memory memory;memory.makeChoice();unsigned visits{};
        memory.beforeRead=[&](uintptr_t address,size_t){if(address==changed&&++visits==2)memory.bytes[changed]^=1;};
        require(choiceUnknown(memory.snapshot(true,leased)),"changing owner, focus, identity or negative flag is rejected across repeated reads");
    }
    {
        Memory memory;memory.makeChoice();memory.makeChoice(Memory::choiceOwner+0x1000);
        const NativePopupChoiceLeases ambiguous{{{Memory::choiceOwner,900},{Memory::choiceOwner+0x1000,900},{},{}}};
        require(choiceUnknown(memory.snapshot(true,ambiguous)),"two matching active native components do not establish a unique choice owner");
    }
    {
        Memory memory;memory.makeChoice();unsigned visits{};
        memory.beforeRead=[&](uintptr_t address,size_t){
            if(address==Memory::choiceOwner+0xc0&&++visits==2)memory.write(Memory::choiceOwner+0xcc,int32_t{1});
        };
        require(choiceUnknown(memory.snapshot(true,leased)),"a selected-index change between reads cannot borrow its preceding negative flag");
    }
    for(const uintptr_t changed:{Memory::base+0x2bf1940,Memory::system+0xe80,Memory::system+0x1510,Memory::child}){
        Memory memory;memory.makeChoice();unsigned visits{};
        memory.beforeRead=[&](uintptr_t address,size_t){if(address==changed&&++visits==3)memory.bytes[changed]^=1;};
        const auto popup=memory.snapshot(true,leased);
        require(unknown(popup)&&choiceUnknown(popup),"dialog or terminal replacement during choice sampling invalidates the entire transaction");
    }
    std::cout<<"Native popup observation ownership, identity and failure contracts passed\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
