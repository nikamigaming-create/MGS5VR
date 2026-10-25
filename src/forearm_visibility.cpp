#include "mgs5vr/forearm_visibility.hpp"
#include "mgs5vr/arm_ik.hpp"
#include "mgs5vr/forearm_crop.hpp"
#include "mgs5vr/log.hpp"
#include <d3d11_1.h>
#include <bcrypt.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <vector>
#include <sstream>

namespace mgs5vr {
namespace {
using Microsoft::WRL::ComPtr;
std::atomic_uintptr_t playerOwner{},playerModel{};
struct DrawState {
    ID3D11DeviceContext* context{};
    ComPtr<ID3D11Buffer> indices,object;
    std::array<ComPtr<ID3D11Buffer>,32> vertices;
    std::array<UINT,32> strides{},offsets{};
    DXGI_FORMAT format{};UINT offset{};bool color{};
};
thread_local std::array<DrawState,4> drawStates;
DrawState* drawState(ID3D11DeviceContext* context){
    for(auto& state:drawStates)if(state.context==context)return &state;
    for(auto& state:drawStates)if(!state.context){state.context=context;return &state;}
    return nullptr;
}
thread_local bool building{};
std::mutex geometryMutex;
struct Geometry {
    ComPtr<ID3D11Buffer> positions,attributes,indices,cropped;
    std::vector<unsigned char> positionBytes,attributeBytes,indexBytes;
    D3D11_BUFFER_DESC positionDescription{},attributeDescription{},indexDescription{};
};
Geometry geometry;
constexpr GUID croppedKey{0xd09211a8,0x93a2,0x4bd7,{0xb8,0x16,0x74,0xc8,0x19,0x43,0x0b,0x72}};
constexpr GUID vertexKey{0xd09211a9,0x93a2,0x4bd7,{0xb8,0x16,0x74,0xc8,0x19,0x43,0x0b,0x72}};
struct Constants {
    ComPtr<ID3D11Resource> resource;
    ID3D11DeviceContext* context{};
    const void* mapped{};
    std::vector<unsigned char> bytes;
    bool complete{};
};
thread_local std::array<Constants,16> constants;
template<class T> bool read(uintptr_t address,T& value){
    SIZE_T count{};return address&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),&value,sizeof(value),&count)&&count==sizeof(value);
}
template<class T> T get(uintptr_t address){T value{};read(address,value);return value;}
std::string digest(const void* bytes,ULONG count){
    BCRYPT_ALG_HANDLE algorithm{};std::array<unsigned char,32> hash{};
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return {};
    const auto result=BCryptHash(algorithm,nullptr,0,const_cast<PUCHAR>(static_cast<const unsigned char*>(bytes)),count,hash.data(),32);
    BCryptCloseAlgorithmProvider(algorithm,0);if(result<0)return {};
    constexpr char hex[]="0123456789abcdef";std::string text;
    for(auto b:hash){text+=hex[b>>4];text+=hex[b&15];}return text;
}
Constants* known(ID3D11DeviceContext* context,ID3D11Resource* resource){
    for(auto& entry:constants)if(entry.resource.Get()==resource&&entry.context==context)return &entry;
    return nullptr;
}
}
void resetForearmBindings(ID3D11DeviceContext* c) noexcept {
    for(auto& state:drawStates)if(state.context==c){state={};break;}
}
void setForearmPlayer(uintptr_t owner,uintptr_t model) noexcept {
    playerOwner.store(owner);playerModel.store(model);
}
void bindForearmIndex(ID3D11DeviceContext* c,ID3D11Buffer* buffer,DXGI_FORMAT format,UINT offset) noexcept {
    if(auto* state=drawState(c)){state->indices=buffer;state->format=format;state->offset=offset;}
}
void bindForearmVertices(ID3D11DeviceContext* c,UINT start,UINT count,ID3D11Buffer* const* buffers,const UINT* strides,const UINT* offsets) noexcept {
    if(start>32||count>32-start||!buffers||!strides||!offsets)return;
    if(auto* state=drawState(c))for(UINT i=0;i<count;++i){
        state->vertices[start+i]=buffers[i];state->strides[start+i]=strides[i];state->offsets[start+i]=offsets[i];
    }
}
void bindForearmConstants(ID3D11DeviceContext* c,UINT start,UINT count,ID3D11Buffer* const* buffers) noexcept {
    if(start<=5&&count>5-start&&buffers)if(auto* state=drawState(c))state->object=buffers[5-start];
}
void bindForearmTargets(ID3D11DeviceContext* c,UINT count,ID3D11RenderTargetView* const* targets) noexcept {
    if(count!=D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL)if(auto* state=drawState(c))state->color=count&&targets&&targets[0];
}
void observeForearmBuffer(ID3D11Device* device,const D3D11_BUFFER_DESC* desc,const D3D11_SUBRESOURCE_DATA* data,ID3D11Buffer* buffer) noexcept {
    if(building||!device||!desc||!data||!data->pSysMem||!buffer)return;
    const bool positions=desc->ByteWidth==80640&&(desc->BindFlags&D3D11_BIND_VERTEX_BUFFER);
    const bool attributes=desc->ByteWidth==214368&&(desc->BindFlags&D3D11_BIND_VERTEX_BUFFER);
    const bool indices=desc->ByteWidth==85824&&(desc->BindFlags&D3D11_BIND_INDEX_BUFFER);
    if(!positions&&!attributes&&!indices)return;
    try{
        const auto hash=digest(data->pSysMem,desc->ByteWidth);
        if(hash!=(positions?"939ed828f424ab2271768682c4c9bf1640f1656cac6a2ae2012e7b6ab17975bb":
            attributes?"951de936fba0c8b06d6f29996b138f33db442714776c4bc33fe5366a2adfcbd6":
            "7616a3f90d1f1eb01a5746707edd0bb56adf506b476f89376c88a652ac5adb49"))return;
        std::lock_guard lock(geometryMutex);
        auto& g=geometry;
        // A reload creates new native resources; attach the variant to that
        // exact index buffer so resource reuse cannot inherit concealment.
        const auto* bytes=static_cast<const unsigned char*>(data->pSysMem);
        if(positions){g.positions=buffer;g.positionBytes.assign(bytes,bytes+desc->ByteWidth);g.positionDescription=*desc;}
        else if(attributes){g.attributes=buffer;g.attributeBytes.assign(bytes,bytes+desc->ByteWidth);g.attributeDescription=*desc;}
        else {g.indices=buffer;g.indexBytes.assign(bytes,bytes+desc->ByteWidth);g.indexDescription=*desc;}
        g.cropped.Reset();
        if(!g.positions||!g.attributes||!g.indices)return;
        for(auto* resource:{g.positions.Get(),g.attributes.Get(),g.indices.Get()}){
            ComPtr<ID3D11Device> owner;resource->GetDevice(&owner);if(owner.Get()!=device)return;
        }
        auto cropped=g.indexBytes;
        auto fittedPositions=g.positionBytes,capAttributes=g.attributeBytes;
        struct Mesh {size_t first,end,positions,count;bool right;};
        // Owned sna5 mesh partitions, including every native LOD. The same
        // right-hand partition contains shoulder cloth, forearm and fingers.
        constexpr std::array<Mesh,6> meshes{{{261,2922,912,372,false},{2922,3138,5376,50,false},
            {3138,3195,5984,14,false},{3195,3219,6160,7,true},{3219,10743,6256,1057,true},{10743,11112,18944,92,true}}};
        size_t removed{};
        for(const auto& mesh:meshes)for(size_t at=mesh.first;at<mesh.end;at+=3){
            std::array<uint16_t,3> face{};std::memcpy(face.data(),cropped.data()+at*2,6);
            bool shoulder=false;
            for(auto index:face){
                if(index>=mesh.count)return;
                float x{};std::memcpy(&x,g.positionBytes.data()+mesh.positions+index*12,4);
                // The native elbow is +/-0.472 m in the authored T pose.
                // Keep the elbow seam and all distal vertices. Classification
                // precedes skinning, so bending cannot expose the shoulder.
                shoulder|=(mesh.right?-x:x)<.452f;
            }
            if(shoulder){face[1]=face[2]=face[0];std::memcpy(cropped.data()+at*2,face.data(),6);++removed;}
        }
        // Native LOD ranges share triangles. Caps use only removed slots that
        // belong to one draw range. Dead vertex slots give each cap its own
        // normal/UV while retaining the original ring's skin weights.
        constexpr std::array<ForearmIndexRange,4> leftLods{{{261,1788},{1413,2367},{2142,2703},{2661,2922}}};
        constexpr std::array<ForearmIndexRange,4> rightLods{{{3219,7719},{7089,9384},{9015,10254},{10143,10743}}};
        std::vector<uint16_t> nativeIndices(g.indexBytes.size()/2),closedIndices(cropped.size()/2);
        std::memcpy(nativeIndices.data(),g.indexBytes.data(),g.indexBytes.size());
        std::memcpy(closedIndices.data(),cropped.data(),cropped.size());
        size_t caps{};
        for(const size_t part:{size_t{0},size_t{4}}){
            const auto& mesh=meshes[part];std::vector<Vec3> points(mesh.count);
            static_assert(sizeof(Vec3)==12);
            std::memcpy(points.data(),g.positionBytes.data()+mesh.positions,mesh.count*12);
            std::vector<Vec3> fitted;std::vector<std::array<uint16_t,2>> copies;
            caps+=closeForearmCuffs(points,nativeIndices,closedIndices,part?rightLods:leftLods,mesh.right,.452f,&fitted,&copies);
            if(fitted.size()!=points.size())return;
            std::memcpy(fittedPositions.data()+mesh.positions,fitted.data(),mesh.count*12);
            const size_t attributeOffset=part?16640:2432;
            // Native normal and tangent are half4; weights, indices and
            // color occupy bytes 16..27, UV0 half2 occupies bytes 28..31.
            const std::array<uint16_t,8> frame{static_cast<uint16_t>(mesh.right?0x3c00:0xbc00),0,0,0x3c00,0,0x3c00,0,0x3c00};
            for(const auto copy:copies){
                auto* destination=capAttributes.data()+attributeOffset+copy[0]*32;
                std::memcpy(destination,g.attributeBytes.data()+attributeOffset+copy[1]*32,32);
                std::memcpy(destination,frame.data(),16);
                // One native fabric sample across the cap avoids drawing a
                // stretched strip of the shoulder atlas over the cut end.
                std::memcpy(destination+28,g.attributeBytes.data()+attributeOffset+copies.front()[1]*32+28,4);
            }
        }
        std::memcpy(cropped.data(),closedIndices.data(),cropped.size());
        struct Guard {Guard(){building=true;}~Guard(){building=false;}} guard;
        D3D11_SUBRESOURCE_DATA initial{};ComPtr<ID3D11Buffer> positionVariant,attributeVariant;
        initial.pSysMem=fittedPositions.data();
        if(FAILED(device->CreateBuffer(&g.positionDescription,&initial,&positionVariant)))return;
        initial.pSysMem=capAttributes.data();
        if(FAILED(device->CreateBuffer(&g.attributeDescription,&initial,&attributeVariant)))return;
        initial.pSysMem=cropped.data();
        if(FAILED(device->CreateBuffer(&g.indexDescription,&initial,&g.cropped)))return;
        if(FAILED(g.positions->SetPrivateDataInterface(vertexKey,positionVariant.Get()))
            ||FAILED(g.attributes->SetPrivateDataInterface(vertexKey,attributeVariant.Get())))return;
        g.indices->SetPrivateDataInterface(croppedKey,g.cropped.Get());
        log("Native forearm-only indices ready; shoulder triangles removed="+std::to_string(removed)+" cuff triangles="+std::to_string(caps));
    }catch(...){}
}
void mapForearmConstants(ID3D11DeviceContext* c,ID3D11Resource* r,UINT sub,D3D11_MAP type,const D3D11_MAPPED_SUBRESOURCE* mapped) noexcept {
    if(sub||!r)return;
    if(auto* entry=known(c,r)){
        entry->complete=false;entry->mapped=nullptr;
        if(mapped&&(type==D3D11_MAP_WRITE_DISCARD||type==D3D11_MAP_WRITE_NO_OVERWRITE))entry->mapped=mapped->pData;
    }
}
void unmapForearmConstants(ID3D11DeviceContext* c,ID3D11Resource* r,UINT sub) noexcept {
    if(sub)return;
    if(auto* entry=known(c,r);entry&&entry->mapped){
        std::memcpy(entry->bytes.data(),entry->mapped,entry->bytes.size());entry->mapped=nullptr;entry->complete=true;
    }
}
void updateForearmConstants(ID3D11DeviceContext* c,ID3D11Resource* r,UINT sub,const D3D11_BOX* box,const void* data) noexcept {
    if(sub)return;
    if(auto* entry=known(c,r)){entry->complete=false;if(!box&&data){std::memcpy(entry->bytes.data(),data,entry->bytes.size());entry->complete=true;}}
}
ForearmDraw::ForearmDraw(ID3D11DeviceContext* c) noexcept {
    if(!c)return;
    const auto owner=playerOwner.load(),model=playerModel.load();
    if(!owner||!model)return;
    try{
        const auto* state=drawState(c);
        const bool deferred=c->GetType()==D3D11_DEVICE_CONTEXT_DEFERRED;
        ComPtr<ID3D11Buffer> indices,variant;
        if(deferred){if(!state)return;indices=state->indices;format_=state->format;offset_=state->offset;}
        else c->IAGetIndexBuffer(&indices,&format_,&offset_);
        if(!indices||format_!=DXGI_FORMAT_R16_UINT)return;
        UINT size=sizeof(ID3D11Buffer*);
        if(FAILED(indices->GetPrivateData(croppedKey,&size,variant.GetAddressOf())))return;
        static std::atomic_bool seen{};
        if(!seen.exchange(true))log("Native forearm-only owned index draw observed");
        // Depth-only shadow passes retain the complete native silhouette.
        if(deferred){if(!state->color)return;}
        else {ComPtr<ID3D11RenderTargetView> color;c->OMGetRenderTargets(1,&color,nullptr);if(!color)return;}
        const auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        const auto character=get<uintptr_t>(owner+0x370);
        if(get<uintptr_t>(owner)!=base+0x23b8218||get<uintptr_t>(character)!=base+0x2295210
            ||get<uintptr_t>(get<uintptr_t>(get<uintptr_t>(character+0x10)+0x10)+0x68)!=model
            ||get<uintptr_t>(model)!=base+0x20f4d90||get<uint64_t>(model+0x120)!=0x84a033c711e29aedull)return;
        std::array<float,16> playerWorld{};if(!read(model+0x40,playerWorld)||!nativeAffinePose(playerWorld))return;
        ComPtr<ID3D11Buffer> object;UINT first=0,count=0;
        ComPtr<ID3D11DeviceContext1> context1;
        if(deferred)object=state->object;
        else if(SUCCEEDED(c->QueryInterface(IID_PPV_ARGS(&context1))))context1->VSGetConstantBuffers1(5,1,&object,&first,&count);
        else c->VSGetConstantBuffers(5,1,&object);
        if(!object)return;
        auto* entry=known(c,object.Get());
        if(!entry){
            D3D11_BUFFER_DESC desc{};object->GetDesc(&desc);
            if(desc.ByteWidth<208||desc.ByteWidth>65536)return;
            for(auto& free:constants)if(!free.resource){free.resource=object;free.context=c;free.bytes.resize(desc.ByteWidth);break;}
            return;
        }
        const size_t at=static_cast<size_t>(first)*16+64;
        if(!entry->complete||at+64>entry->bytes.size())return;
        std::array<float,16> world{};std::memcpy(world.data(),entry->bytes.data()+at,64);
        if(!std::all_of(world.begin(),world.end(),[](float v){return std::isfinite(v);}))return;
        float error=0;
        for(size_t row=0;row<4;++row)for(size_t column=0;column<4;++column)
            error=std::max(error,std::abs(world[row*4+column]-playerWorld[column*4+row]));
        if(!std::isfinite(error)||error>.0001f){
            static std::atomic_bool reported{};
            if(!reported.exchange(true)){
                std::ostringstream line;line<<"Native forearm-only object join rejected error="<<error<<" first="<<first<<" world=";
                for(auto v:world)line<<v<<',';
                line<<" expected=";for(auto v:playerWorld)line<<v<<',';log(line.str());
            }
            return;
        }
        // Change all three streams together, only for the admitted player
        // color draw. A missing attribute stream must not select cap indices
        // whose reused vertices still contain the native shoulder data.
        std::array<ComPtr<ID3D11Buffer>,32> bound;
        std::array<UINT,32> strides{},offsets{};
        if(deferred){bound=state->vertices;strides=state->strides;offsets=state->offsets;}
        else {
            std::array<ID3D11Buffer*,32> raw{};
            c->IAGetVertexBuffers(0,32,raw.data(),strides.data(),offsets.data());
            for(size_t i=0;i<raw.size();++i)bound[i].Attach(raw[i]);
        }
        std::array<ComPtr<ID3D11Buffer>,32> replacements;
        std::array<bool,2> streams{};
        for(UINT slot=0;slot<bound.size();++slot)if(bound[slot]){
            ComPtr<ID3D11Buffer> replacement;size=sizeof(ID3D11Buffer*);
            if(FAILED(bound[slot]->GetPrivateData(vertexKey,&size,replacement.GetAddressOf())))continue;
            D3D11_BUFFER_DESC desc{};bound[slot]->GetDesc(&desc);
            const size_t stream=desc.ByteWidth==80640?0:1;
            if((stream==1&&desc.ByteWidth!=214368)
                ||(stream==0?strides[slot]!=12u:
                    strides[slot]!=28u&&strides[slot]!=32u&&strides[slot]!=36u))return;
            // The owned FMDL binds normal/tangent, skinning and UV groups
            // separately from the same attribute buffer. Replace every
            // alias, retaining each native offset and mesh-specific stride.
            streams[stream]=true;
            replacements[vertexCount_]=replacement;
            vertices_[vertexCount_++]={bound[slot],slot,strides[slot],offsets[slot]};
        }
        if(!streams[0]||!streams[1])return;
        original_=indices;context_=c;
        for(size_t i=0;i<vertexCount_;++i){const auto& v=vertices_[i];auto* replacement=replacements[i].Get();
            c->IASetVertexBuffers(v.slot,1,&replacement,&v.stride,&v.offset);
        }
        c->IASetIndexBuffer(variant.Get(),format_,offset_);
        static std::atomic_bool reported{};
        if(!reported.exchange(true))log("Native forearm-only draw selected for the owned player; native hands/materials retained");
    }catch(...){}
}
ForearmDraw::~ForearmDraw(){if(context_){
    for(size_t i=0;i<vertexCount_;++i){const auto& v=vertices_[i];auto* original=v.original.Get();context_->IASetVertexBuffers(v.slot,1,&original,&v.stride,&v.offset);}
    context_->IASetIndexBuffer(original_.Get(),format_,offset_);
}}
}
