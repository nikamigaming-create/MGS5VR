#include "mgs5vr/native_input_delivery.hpp"
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
using namespace mgs5vr;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
NativeInputDelivery sample(uint64_t time,uint32_t index=0){
    NativeInputDelivery value;
    value.timeMs=time;value.index=index;value.packetValid=true;value.packetNumber=static_cast<uint32_t>(time);
    value.buttons=static_cast<uint16_t>(time%65536);value.axes={static_cast<int16_t>(time%30001),
        static_cast<int16_t>(-static_cast<int64_t>(time%30001)),32767,-32768};
    value.triggers={static_cast<uint8_t>(time%256),static_cast<uint8_t>(255-time%256)};
    value.caller={time,true};value.xrMailboxFresh=index==0;
    value.source=index==0?NativeInputDeliverySource::xrMailbox:NativeInputDeliverySource::nativeXInput;
    value.nativeSlotValid=index!=0;value.nativeSlot=index;
    return value;
}
bool intact(const NativeInputDelivery& value){
    if(!value.deliveryCounter)return true;
    const auto expected=sample(value.timeMs,value.index);
    return value.packetValid&&value.resultCode==0&&value.packetNumber==expected.packetNumber
        &&value.buttons==expected.buttons&&value.axes==expected.axes&&value.triggers==expected.triggers
        &&value.caller.inGame&&value.caller.rva==expected.caller.rva&&value.source==expected.source
        &&value.xrMailboxFresh==expected.xrMailboxFresh&&value.nativeSlotValid==expected.nativeSlotValid
        &&value.nativeSlot==expected.nativeSlot;
}
void callerBoundaries(){
    constexpr uintptr_t base=0x100000;
    require(nativeInputCaller(base,base,32).inGame&&nativeInputCaller(base,base,32).rva==0,
        "the first game image byte is a valid caller RVA zero");
    require(nativeInputCaller(base+31,base,32).inGame&&nativeInputCaller(base+31,base,32).rva==31,
        "the last game image byte is inside the image");
    require(!nativeInputCaller(base+32,base,32).inGame&&!nativeInputCaller(base-1,base,32).inGame,
        "outside addresses never masquerade as game RVAs");
    require(!nativeInputCaller(base,0,32).inGame&&!nativeInputCaller(base,base,0).inGame,
        "an uninitialized image descriptor cannot admit a caller");
    constexpr auto high=std::numeric_limits<uintptr_t>::max();
    require(nativeInputCaller(high,high-15,32).rva==15&&!nativeInputCaller(1,high-15,32).inGame,
        "image containment uses subtraction without wrapping an end address");
}
void retainedPackets(){
    NativeInputDeliveryAudit audit;
    const auto empty=audit.snapshot();
    require(empty.available&&!empty.last.deliveryCounter&&!empty.index0.deliveryCounter&&!empty.attemptedDeliveries,
        "before the first return witness there is no fabricated consumption");
    auto xr=sample(1000);xr.buttons=0xa5a5;xr.axes={-32768,32767,123,-456};xr.triggers={0,255};
    xr.packetNumber=0xffffffff;xr.caller={0x1041219,true};
    require(audit.publish(xr),"single-writer XR packet is published");
    auto snap=audit.snapshot();
    require(snap.available&&snap.last.deliveryCounter==1&&snap.index0.deliveryCounter==1
        &&snap.last.timeMs==1000&&snap.last.index==0&&snap.last.packetNumber==0xffffffff
        &&snap.last.buttons==0xa5a5&&snap.last.axes==xr.axes&&snap.last.triggers==xr.triggers
        &&snap.last.xrMailboxFresh&&snap.last.source==NativeInputDeliverySource::xrMailbox,
        "the delivered packet retains its exact signed axes, triggers, buttons, packet number and source");
    auto native=sample(1001,3);native.nativeSlot=1;
    require(audit.publish(native),"native passthrough return is published");
    snap=audit.snapshot();
    require(snap.last.index==3&&snap.last.deliveryCounter==2&&snap.last.nativeSlot==1
        &&snap.index0.deliveryCounter==1&&snap.index0.axes==xr.axes&&snap.attemptedDeliveries==2,
        "polling other slots retains the latest index-zero witness and actual native slot independently");
    auto stale=sample(1002);stale.xrMailboxFresh=false;stale.buttons=0;stale.axes={};stale.triggers={};
    audit.publish(stale);snap=audit.snapshot();
    require(snap.index0.deliveryCounter==3&&snap.index0.packetValid&&!snap.index0.xrMailboxFresh
        &&snap.index0.axes==std::array<int16_t,4>{}&&snap.index0.source==NativeInputDeliverySource::xrMailbox,
        "neutral stale-mailbox success remains distinct from a fresh action packet");
    auto failed=sample(1003);failed.resultCode=1167;failed.nativeSlotValid=true;failed.nativeSlot=2;
    audit.publish(failed);snap=audit.snapshot();
    require(snap.index0.deliveryCounter==4&&snap.index0.resultCode==1167&&!snap.index0.packetValid
        &&snap.index0.packetNumber==0&&snap.index0.buttons==0&&snap.index0.axes==std::array<int16_t,4>{}
        &&snap.index0.triggers==std::array<uint8_t,2>{}&&!snap.index0.nativeSlotValid,
        "an error cannot reuse an earlier successful packet or expose caller-supplied stale bytes");
}
void wireContract(){
    NativeInputDeliveryAudit audit;
    std::ostringstream empty;empty<<std::hex;
    writeNativeInputDeliveryJson(empty,audit.snapshot(),123);
    require(empty.str().find("\"snapshot_available\":true")!=std::string::npos
        &&empty.str().find("\"last\":null,\"index0\":null")!=std::string::npos,
        "available recorder with no calls has null records");
    empty<<16;require(empty.str().ends_with("}10"),"JSON restores caller numeric formatting");
    auto value=sample(1000);value.axes={0,32767,0,0};value.triggers={0,255};value.buttons=4096;
    value.packetNumber=16;value.caller={0x123,true};audit.publish(value);
    std::ostringstream out;out<<std::hex;writeNativeInputDeliveryJson(out,audit.snapshot(),1007);
    require(out.str().find("\"sample_ms\":1000,\"age_ms\":7,\"index\":0")!=std::string::npos
        &&out.str().find("\"caller_in_game\":true,\"caller_rva\":291")!=std::string::npos
        &&out.str().find("\"packet_number\":16,\"buttons\":4096,\"axes\":[0,32767,0,0],\"triggers\":[0,255]")!=std::string::npos,
        "wire fields use decimal numbers and preserve exact packet values regardless of stream flags");
    std::ostringstream future;writeNativeInputDeliveryJson(future,audit.snapshot(),999);
    require(future.str().find("\"age_ms\":null")!=std::string::npos,"future stamps cannot wrap into false freshness");
    value.resultCode=160;value.packetValid=false;value.source=NativeInputDeliverySource::unavailable;
    value.caller={};audit.publish(value);
    std::ostringstream failure;writeNativeInputDeliveryJson(failure,audit.snapshot(),1001);
    require(failure.str().find("\"caller_in_game\":false,\"caller_rva\":null")!=std::string::npos
        &&failure.str().find("\"packet_valid\":false,\"native_slot\":null,\"packet_number\":null,\"buttons\":null,\"axes\":null,\"triggers\":null")!=std::string::npos,
        "unavailable caller and failed return payload remain explicitly unknown");
    auto busy=audit.snapshot();busy.available=false;
    std::ostringstream unavailable;writeNativeInputDeliveryJson(unavailable,busy,1001);
    require(unavailable.str().find("\"snapshot_available\":false,\"record_copy_coherent\":false")!=std::string::npos
        &&unavailable.str().find("\"last\":null,\"index0\":null")!=std::string::npos,
        "a contended read never exposes an incoherent or previous copy as fresh");
}
void concurrentCopies(){
    NativeInputDeliveryAudit audit;
    constexpr unsigned writers=4,iterations=10000;
    std::atomic_bool begin{},done{},corrupt{};
    std::atomic_uint observations{};
    std::vector<std::thread> threads;
    for(unsigned worker=0;worker<writers;++worker)threads.emplace_back([&,worker]{
        while(!begin.load(std::memory_order_acquire))std::this_thread::yield();
        for(unsigned n=1;n<=iterations;++n){
            const uint64_t time=uint64_t(worker)*iterations+n;
            audit.publish(sample(time,static_cast<uint32_t>(time%4)));
        }
    });
    std::thread reader([&]{
        uint64_t previous{},previous0{};
        while(!begin.load(std::memory_order_acquire))std::this_thread::yield();
        do {
            const auto snap=audit.snapshot();
            if(snap.available){
                if(!intact(snap.last)||!intact(snap.index0)||snap.last.deliveryCounter<previous
                    ||snap.index0.deliveryCounter<previous0||snap.last.deliveryCounter>snap.attemptedDeliveries
                    ||(snap.index0.deliveryCounter&&snap.index0.index!=0))corrupt=true;
                previous=snap.last.deliveryCounter;previous0=snap.index0.deliveryCounter;++observations;
            }
        }while(!done.load(std::memory_order_acquire));
    });
    begin.store(true,std::memory_order_release);
    for(auto& thread:threads)thread.join();
    done.store(true,std::memory_order_release);reader.join();
    const auto snap=audit.snapshot();
    require(snap.available&&snap.attemptedDeliveries==uint64_t(writers)*iterations
        &&snap.publicationDropped<=snap.attemptedDeliveries&&snap.last.deliveryCounter>0,
        "every attempt is counted while contended publications are allowed to drop without waiting");
    require(!corrupt.load()&&observations.load()>0&&intact(snap.last)&&intact(snap.index0),
        "concurrent return writers and observer retain whole packets with monotonic per-slot witnesses");
}
}
int main(){try{
    callerBoundaries();retainedPackets();wireContract();concurrentCopies();
    std::cout<<"native XInput delivery contracts passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
