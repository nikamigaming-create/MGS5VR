#pragma once
#include "core.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <span>
#include <vector>

namespace mgs5vr {
struct ForearmIndexRange {size_t first,end;};

// Close only boundaries introduced by the crop. Optional fitted positions and
// copy pairs reuse dead vertex slots for a flat rim and independently shaded cap.
// Reuse removed triangles exclusive to this LOD:
// Fox's LOD ranges overlap, so shared slots cannot hold a LOD-specific cap.
inline size_t closeForearmCuffs(std::span<const Vec3> positions,
    std::span<const uint16_t> original,std::span<uint16_t> cropped,
    std::span<const ForearmIndexRange> lods,bool right,float cut=.452f,
    std::vector<Vec3>* fitted=nullptr,std::vector<std::array<uint16_t,2>>* copies=nullptr){
    if(original.size()!=cropped.size()||positions.size()>65536)return 0;
    if((fitted==nullptr)!=(copies==nullptr))return 0;
    std::map<std::array<int,3>,uint16_t> weld;
    std::vector<uint16_t> canonical;
    for(size_t i=0;i<positions.size();++i){
        const auto p=positions[i];
        if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)
            ||std::max({std::abs(p.x),std::abs(p.y),std::abs(p.z)})>10)return 0;
        const std::array<int,3> key{int(std::lround(p.x*100000)),int(std::lround(p.y*100000)),int(std::lround(p.z*100000))};
        canonical.push_back(weld.emplace(key,static_cast<uint16_t>(i)).first->second);
    }
    for(const auto range:lods){
        if(range.first>range.end||range.end>original.size()||(range.end-range.first)%3)return 0;
        for(size_t at=range.first;at<range.end;++at)if(original[at]>=positions.size())return 0;
    }
    std::vector<bool> used(positions.size());
    for(const auto range:lods)for(size_t at=range.first;at<range.end;at+=3){
        const bool keep=std::all_of(original.begin()+at,original.begin()+at+3,
            [&](auto i){return (right?-positions[i].x:positions[i].x)>=cut;});
        if(keep)for(size_t i=0;i<3;++i)used[canonical[original[at+i]]]=true;
    }
    std::vector<uint16_t> spare;
    if(fitted){
        fitted->assign(positions.begin(),positions.end());copies->clear();
        // A welded representative may not itself be indexed by a retained
        // triangle. Reserve its entire seam group, not just indexed copies.
        for(size_t i=0;i<used.size();++i)if(!used[canonical[i]])spare.push_back(static_cast<uint16_t>(i));
    }
    size_t added{};
    for(size_t lod=0;lod<lods.size();++lod){
        struct Edge {unsigned all{},kept{};uint16_t a{},b{};};
        std::map<std::array<uint16_t,2>,Edge> edges;
        std::vector<size_t> slots;
        const auto range=lods[lod];
        for(size_t at=range.first;at<range.end;at+=3){
            const std::array<uint16_t,3> face{original[at],original[at+1],original[at+2]};
            const bool keep=std::all_of(face.begin(),face.end(),[&](auto i){return (right?-positions[i].x:positions[i].x)>=cut;});
            if(!keep){
                bool exclusive=true;
                for(size_t other=0;other<lods.size();++other)if(other!=lod&&at>=lods[other].first&&at<lods[other].end)exclusive=false;
                if(exclusive&&cropped[at]==cropped[at+1]&&cropped[at]==cropped[at+2])slots.push_back(at);
            }
            for(size_t i=0;i<3;++i){
                const auto a=canonical[face[i]],b=canonical[face[(i+1)%3]];
                if(a==b)continue;
                auto& edge=edges[{std::min(a,b),std::max(a,b)}];++edge.all;
                if(keep){++edge.kept;edge.a=a;edge.b=b;}
            }
        }
        std::multimap<uint16_t,uint16_t> next;
        std::map<uint16_t,unsigned> incoming;
        bool manifold=true;
        for(const auto& [key,e]:edges)if(e.all==2&&e.kept==1){
            // Reverse the retained boundary winding to face out of the cuff.
            next.emplace(e.b,e.a);
            ++incoming[e.a];
        }
        for(const auto& [a,b]:next)if(incoming[a]!=next.count(a))manifold=false;
        if(!manifold)continue;
        std::vector<std::array<uint16_t,3>> caps;
        std::vector<std::vector<uint16_t>> loops;
        while(!next.empty()&&manifold){
            const auto start=next.begin()->first;auto at=start;
            std::vector<uint16_t> polygon;
            do{
                const auto repeat=std::find(polygon.begin(),polygon.end(),at);
                if(repeat!=polygon.end()){
                    loops.emplace_back(repeat,polygon.end());polygon.erase(repeat,polygon.end());
                }
                const auto it=next.find(at);
                if(it==next.end()){manifold=false;break;}
                polygon.push_back(at);at=it->second;next.erase(it);
            }while(at!=start);
            if(!polygon.empty())loops.push_back(std::move(polygon));
        }
        for(auto polygon:loops){
            if(!manifold||polygon.size()<3){manifold=false;break;}
            const auto area=[&](uint16_t a,uint16_t b,uint16_t c){
                const auto p=positions[a],q=positions[b],r=positions[c];
                return (q.y-p.y)*(r.z-p.z)-(q.z-p.z)*(r.y-p.y);
            };
            float signedArea{};
            for(size_t i=0;i<polygon.size();++i){const auto p=positions[polygon[i]],q=positions[polygon[(i+1)%polygon.size()]];signedArea+=p.y*q.z-p.z*q.y;}
            if(std::abs(signedArea)<1e-8f){manifold=false;break;}
            const float winding=signedArea>0?1.f:-1.f;
            while(polygon.size()>3){
                bool clipped=false;
                for(size_t i=0;i<polygon.size();++i){
                    const auto a=polygon[(i+polygon.size()-1)%polygon.size()],b=polygon[i],c=polygon[(i+1)%polygon.size()];
                    if(area(a,b,c)*winding<=1e-9f)continue;
                    bool contains=false;
                    for(auto v:polygon)if(v!=a&&v!=b&&v!=c&&area(a,b,v)*winding>=-1e-9f
                        &&area(b,c,v)*winding>=-1e-9f&&area(c,a,v)*winding>=-1e-9f){contains=true;break;}
                    if(contains)continue;
                    caps.push_back({a,b,c});polygon.erase(polygon.begin()+i);clipped=true;break;
                }
                if(!clipped){manifold=false;break;}
            }
            if(manifold)caps.push_back({polygon[0],polygon[1],polygon[2]});
        }
        if(!manifold||caps.size()>slots.size())continue;
        if(fitted){
            std::map<uint16_t,uint16_t> duplicates;
            for(const auto& cap:caps)for(auto vertex:cap)duplicates.emplace(vertex,uint16_t{});
            if(duplicates.size()>spare.size())continue;
            // A cut through whole triangles has a saw-tooth axial rim. Fit
            // only that rim to the authored elbow plane; hands and distal
            // forearms keep their native positions. All coincident seam
            // copies move together so UV/normal seams cannot open a crack.
            for(auto& [vertex,duplicate]:duplicates){
                duplicate=spare.back();spare.pop_back();
                for(size_t i=0;i<canonical.size();++i)if(canonical[i]==vertex)
                    (*fitted)[i].x=right?-.472f:.472f;
                (*fitted)[duplicate]=(*fitted)[vertex];
                copies->push_back({duplicate,vertex});
            }
            for(auto& cap:caps)for(auto& vertex:cap)vertex=duplicates.at(vertex);
        }
        for(size_t i=0;i<caps.size();++i)std::copy(caps[i].begin(),caps[i].end(),cropped.begin()+slots[i]);
        added+=caps.size();
    }
    return added;
}
}
