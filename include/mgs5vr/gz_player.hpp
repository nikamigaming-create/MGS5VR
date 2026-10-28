#pragma once
#include "core.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <optional>

namespace mgs5vr {
struct GzMenuState { bool idroid{},pause{}; };
// GZ IsDuringBasicAction (0x67c1e0) reads component 0x23's native
// suspension bit. Other actions retain native camera/input handling.
template<class Read>
std::optional<bool> readGzBasicAction(uintptr_t base,uintptr_t entity,Read&& read){
    uintptr_t table{},componentInterface{},type{},ownerEntity{},again{};
    uint32_t count{};uint8_t flags{},flagsAgain{};
    const auto get=[&](uintptr_t address,auto& value){return address&&read(address,&value,sizeof(value));};
    if(!entity||!get(entity+0x60,count)||count<=0x23||count>512
        ||!get(entity+0x68,table)||!table||!get(table+0x23*8,componentInterface)||componentInterface<0x28)return {};
    const auto owner=componentInterface-0x28;
    if(!get(owner,type)||type!=base+0x16d0ef0||!get(owner+0x30,ownerEntity)||ownerEntity!=entity
        ||!get(owner+0x50,flags)||!get(owner+0x50,flagsAgain)||flagsAgain!=flags
        ||!get(entity+0x68,again)||again!=table||!get(table+0x23*8,again)||again!=componentInterface)return {};
    return (flags&0x40)==0;
}
// Native Start/StopMbDvcTerminal and Pause's open/close handlers own these
// fields. An unreadable or replaced owner is unknown, never a closed menu.
template<class Read>
std::optional<GzMenuState> readGzMenuState(uintptr_t base,Read&& read){
    uintptr_t system{},ui{},pause{},terminal{},type{};
    uint32_t kind{};uint8_t open{};
    const auto get=[&](uintptr_t address,auto& value){return address&&read(address,&value,sizeof(value));};
    if(!get(base+0x1e07a28,system)||!system||!get(system,type)||type!=base+0x17674e0
       ||!get(system+0x3e8,pause)||!pause||!get(pause,type)||type!=base+0x176c6c8
       ||!get(pause+0x30,kind)||kind>0x80)return {};
    if(!get(base+0x1e7e228,ui)||!ui||!get(ui,type)||type!=base+0x17c70e8
       ||!get(ui+0x7b0,terminal)||!terminal||!get(terminal,type)||type!=base+0x17c6698
       ||!get(terminal+0x28,open)||open>1)return {};
    uintptr_t again{};uint32_t kindAgain{};uint8_t openAgain{};
    if(!get(base+0x1e07a28,again)||again!=system||!get(system+0x3e8,again)||again!=pause
       ||!get(base+0x1e7e228,again)||again!=ui||!get(ui+0x7b0,again)||again!=terminal
       ||!get(pause+0x30,kindAgain)||kindAgain!=kind||!get(terminal+0x28,openAgain)||openAgain!=open)return {};
    return GzMenuState{open!=0,kind!=0};
}
// Ground Zeroes 1.0.0.5 only. The native Character head query resolves the
// named body bone through component 0, then reads its published world palette.
// These are independent GZ contracts, not TPP camera-owner offsets.
struct GzPlayerHead {
    uintptr_t player{},entity{},character{},model{},camera{};
    std::array<float,16> root{},head{};
};
template<class Read>
std::optional<GzPlayerHead> readGzPlayerHead(uintptr_t base,Pose source,Read&& read){
    const auto pointer=[&](uintptr_t address){uintptr_t value{};return read(address,&value,sizeof(value))?value:0;};
    const auto integer=[&](uintptr_t address){int32_t value{-1};return read(address,&value,sizeof(value))?value:-1;};
    const auto registry=pointer(base+0x1e3b600);
    const auto entry=registry?pointer(registry+0x128):0;
    const auto player=entry?pointer(entry+0x150):0;
    GzPlayerHead result;
    result.player=player;
    result.entity=player?pointer(player+0x38):0;
    if(!result.entity||integer(result.entity+0x60)<0x49)return {};
    const auto table=pointer(result.entity+0x68);
    const auto selectorInterface=table?pointer(table+0x43*8):0;
    if(selectorInterface<0x28||pointer(selectorInterface-0x28)!=base+0x16b2088)return {};
    const auto mode=integer(selectorInterface-0x28+0x1fc);
    if(mode<0||mode>3)return {};
    const auto cameraInterface=pointer(table+(0x45+mode)*8);
    if(cameraInterface<0x28||pointer(cameraInterface+8)!=result.entity)return {};
    result.camera=pointer(cameraInterface-0x28+0xc0);
    if(!result.camera||pointer(result.camera)!=base+0x16228e8)return {};
    std::array<float,8> native{};
    if(!read(result.camera+0xc0,native.data(),sizeof(native)))return {};
    const std::array<float,7> sourceValues{source.orientation.x,source.orientation.y,source.orientation.z,
        source.orientation.w,source.position.x,source.position.y,source.position.z};
    if(std::memcmp(native.data(),sourceValues.data(),sizeof(sourceValues)))return {};
    const auto characterInterface=pointer(table);
    if(characterInterface<0x28)return {};
    result.character=characterInterface-0x28;
    if(pointer(result.character)!=base+0x15f7460||integer(result.character+0x230)<=0)return {};
    const auto models=pointer(result.character+0x238);
    result.model=models?pointer(models):0;
    if(!result.model||pointer(result.model)!=base+0x18437c0)return {};
    const auto count=integer(result.model+0xf8);
    const auto names=pointer(result.model+0xe8),parents=pointer(result.model+0xf0),palette=pointer(result.model+0xe0);
    if(count<53||count>512||!names||!parents||!palette
       ||static_cast<uint32_t>(integer(names+4*4))!=0xc3a88a1bu)return {};
    for(int32_t bone=0;bone<=4;++bone)if(integer(parents+bone*0x20)!=bone-1)return {};
    std::array<float,16> worldHead{},rootAgain{},headAgain{};
    if(!read(result.model+0xa0,result.root.data(),sizeof(result.root))
       ||!read(palette+4*64,worldHead.data(),sizeof(worldHead)))return {};
    // Published bone matrices are already world-space. Convert to the local
    // affine expected by the shared head camera using the actual body root.
    for(size_t axis=0;axis<3;++axis)for(size_t row=0;row<3;++row)
        for(size_t component=0;component<3;++component)
            result.head[axis*4+row]+=worldHead[axis*4+component]*result.root[row*4+component];
    for(size_t row=0;row<3;++row)for(size_t component=0;component<3;++component)
        result.head[12+row]+=(worldHead[12+component]-result.root[12+component])*result.root[row*4+component];
    result.head[15]=1;
    std::array<float,8> cameraAgain{};
    if(!read(result.model+0xa0,rootAgain.data(),sizeof(rootAgain))
       ||!read(palette+4*64,headAgain.data(),sizeof(headAgain))
       ||!read(result.camera+0xc0,cameraAgain.data(),sizeof(cameraAgain))
       ||std::memcmp(rootAgain.data(),result.root.data(),sizeof(rootAgain))
       ||std::memcmp(headAgain.data(),worldHead.data(),sizeof(headAgain))
       ||std::memcmp(cameraAgain.data(),native.data(),sizeof(native)))return {};
    return result;
}
}
