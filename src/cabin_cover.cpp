#include "mgs5vr/cabin_cover.hpp"
#include "mgs5vr/core.hpp"
#include "mgs5vr/log.hpp"
#include <wrl/client.h>
#include <bcrypt.h>
#include <array>
#include <atomic>
#include <bit>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace mgs5vr {
namespace {
using Microsoft::WRL::ComPtr;
constexpr unsigned steps=32;
struct Stream {ComPtr<ID3D11Buffer> original;std::array<ComPtr<ID3D11Buffer>,steps+1> variants;std::vector<unsigned char> bytes;D3D11_BUFFER_DESC description{};};
std::array<Stream,2> streams;
std::mutex mutex;
std::atomic_bool ready{};
std::atomic_uint opening{};
thread_local bool building{};
std::string digest(const void* data,ULONG size){
    BCRYPT_ALG_HANDLE algorithm{};std::array<unsigned char,32> hash{};
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return {};
    const auto result=BCryptHash(algorithm,nullptr,0,const_cast<PUCHAR>(static_cast<const unsigned char*>(data)),size,hash.data(),32);
    BCryptCloseAlgorithmProvider(algorithm,0);if(result<0)return {};
    constexpr char hex[]="0123456789abcdef";std::string text;
    for(auto b:hash){text+=hex[b>>4];text+=hex[b&15];}return text;
}
float half(uint16_t h){
    const float value=(h&0x7c00)?std::ldexp(1.f+float(h&1023)/1024,int((h>>10)&31)-15):std::ldexp(float(h&1023),-24);
    return h&0x8000?-value:value;
}
uint16_t half(float f){
    const auto bits=std::bit_cast<uint32_t>(f);const unsigned sign=(bits>>16)&0x8000;
    const int exponent=int((bits>>23)&255)-127+15;
    if(exponent<=0)return static_cast<uint16_t>(sign);
    if(exponent>=31)return static_cast<uint16_t>(sign|0x7bff);
    const unsigned rounded=(bits&0x7fffff)+0x1000;
    return static_cast<uint16_t>(sign+(unsigned(exponent)<<10)+(rounded>>13));
}
Vec3 position(const std::vector<unsigned char>& b,size_t at){Vec3 p;std::memcpy(&p,b.data()+at,12);return p;}
Vec3 normal(const std::vector<unsigned char>& b,size_t at){std::array<uint16_t,3> h;std::memcpy(h.data(),b.data()+at,6);return {half(h[0]),half(h[1]),half(h[2])};}
void writeNormal(std::vector<unsigned char>& b,size_t at,Vec3 n){const std::array<uint16_t,3> h{half(n.x),half(n.y),half(n.z)};std::memcpy(b.data()+at,h.data(),6);}
Vec3 rotateCover(Vec3 p,float angle){
    constexpr Vec3 axis{.2588190451f,0,.9659258263f};
    const float c=std::cos(angle),s=std::sin(angle);
    const Vec3 cross{axis.y*p.z-axis.z*p.y,axis.z*p.x-axis.x*p.z,axis.x*p.y-axis.y*p.x};
    return p*c+cross*s+axis*(dot(axis,p)*(1-c));
}
}
void setCabinCoverOpening(float value) noexcept {opening.store(static_cast<unsigned>(std::lround(std::clamp(value,0.f,1.f)*steps)),std::memory_order_relaxed);}
ID3D11Buffer* cabinCoverBuffer(ID3D11Buffer* original) noexcept {
    if(!ready.load(std::memory_order_acquire))return original;
    const auto step=opening.load(std::memory_order_relaxed);
    for(auto& stream:streams)if(original==stream.original.Get())return stream.variants[step].Get();
    return original;
}
void observeCabinCoverBuffer(ID3D11Device* device,const D3D11_BUFFER_DESC* desc,const D3D11_SUBRESOURCE_DATA* data,ID3D11Buffer* buffer) noexcept {
    if(building||!buffer||!desc||!data||!data->pSysMem||!(desc->BindFlags&D3D11_BIND_VERTEX_BUFFER))return;
    const int slot=desc->ByteWidth==410704?0:desc->ByteWidth==691280?1:-1;if(slot<0)return;
    try{
        constexpr std::array<const char*,2> hashes{
            "84d7c87578b2e9c589696f8c5ccfff359e445c671d1414b1af2536a5529d8332",
            "8aa2d39d6f58c1a93612a45e2f377b97574b98634e0a002dc0c4311a2e4e89cd"};
        if(digest(data->pSysMem,desc->ByteWidth)!=hashes[slot])return;
        std::lock_guard lock(mutex);if(ready.load()||streams[slot].original)return;
        auto& stream=streams[slot];stream.original=buffer;stream.description=*desc;
        const auto bytes=static_cast<const unsigned char*>(data->pSysMem);stream.bytes.assign(bytes,bytes+desc->ByteWidth);
        log("Native cabin cover recognized owned stream "+std::to_string(slot));
        if(!streams[0].original||!streams[1].original)return;
        struct CoverVertex{size_t position,attributes;};
        std::vector<CoverVertex> vertices;
        // Lid triangle components in the exact hashed native mesh. Selecting
        // only upward-facing vertices left ten triangles partly stationary,
        // visibly stretching the lid as it opened. These 68 indices include
        // the side and edge vertices. The two independent four-vertex end
        // faces duplicate moving lid positions and must move with them too.
        constexpr std::array<size_t,76> lid{
            0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,
            145,146,147,148,149,150,151,152,153,154,155,156,190,191,192,193,194,195,196,197,
            242,243,244,245,
            304,305,306,307,308,309,310,311,354,355,356,357,358,359,360,361};
        for(const auto i:lid)vertices.push_back({297712+i*12,496176+i*20});
        // The window is a separate material mesh; its four front vertices
        // belong to the cover, while its other four belong to the side control.
        for(size_t i=0;i<8;++i)if(position(streams[0].bytes,303456+i*12).y> -2.188f
            &&normal(streams[1].bytes,505744+i*20).y>.6f)vertices.push_back({303456+i*12,505744+i*20});
        if(vertices.size()!=80){log("Native cabin cover rejected: owned cover topology differs");return;}
        // Exact owned interior streams and measured rigid placement of pcp0.
        // Only the existing cover and window move; all other bytes stay native.
        constexpr Vec3 hinge{.61138933f,-2.18173569f,-1.12080353f};
        struct Guard{Guard(){building=true;}~Guard(){building=false;}} guard;
        for(auto& s:streams)s.variants[0]=s.original;
        for(unsigned step=1;step<=steps;++step){
            auto positions=streams[0].bytes,attributes=streams[1].bytes;
            const float angle=-1.308996939f*float(step)/steps;
            for(const auto vertex:vertices){
                auto p=rotateCover(position(positions,vertex.position)-hinge,angle)+hinge;
                std::memcpy(positions.data()+vertex.position,&p,12);
                for(size_t offset:{0u,8u})writeNormal(attributes,vertex.attributes+offset,rotateCover(normal(attributes,vertex.attributes+offset),angle));
            }
            for(size_t i=0;i<2;++i){D3D11_SUBRESOURCE_DATA initial{};initial.pSysMem=i?attributes.data():positions.data();
                if(FAILED(device->CreateBuffer(&streams[i].description,&initial,&streams[i].variants[step]))){log("Native cabin cover GPU allocation failed");return;}}
        }
        ready.store(true,std::memory_order_release);log("Native cabin cover hinge ready: exact owned geometry, 80 vertices, 33 poses");
    }catch(...){log("Native cabin cover unavailable; original geometry retained");}
}
}
