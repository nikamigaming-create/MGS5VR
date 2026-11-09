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
struct ForearmCuffShading {std::vector<Vec3> normals;size_t recessedLoops{};};

// Close only boundaries introduced by the crop. Optional fitted positions and
// copy pairs reuse dead vertex slots for a flat rim and independently shaded cap.
// Reuse removed triangles exclusive to this LOD:
// Fox's LOD ranges overlap, so shared slots cannot hold a LOD-specific cap.
inline size_t closeForearmCuffs(std::span<const Vec3> positions,
    std::span<const uint16_t> original,std::span<uint16_t> cropped,
    std::span<const ForearmIndexRange> lods,bool right,float cut=.452f,
    std::vector<Vec3>* fitted=nullptr,std::vector<std::array<uint16_t,2>>* copies=nullptr,float elbow=.472f,
    ForearmCuffShading* shading=nullptr,std::span<const Vec3> nativePositions={}){
    if(original.size()!=cropped.size()||positions.size()>65536)return 0;
    if(!std::isfinite(cut)||!std::isfinite(elbow)||cut<=0||elbow<cut||elbow>1.f)return 0;
    if((fitted==nullptr)!=(copies==nullptr))return 0;
    if(shading){shading->normals.assign(positions.size(),{right?1.f:-1.f,0,0});shading->recessedLoops=0;}
    // The second arm consumes accumulated output, but all native topology
    // and volume queries must use the untouched source stream.
    const auto native=nativePositions.empty()?positions:nativePositions;
    if(native.size()!=positions.size())return 0;
    std::map<std::array<int,3>,uint16_t> weld;
    std::vector<uint16_t> canonical;
    for(size_t i=0;i<positions.size();++i){
        const auto p=native[i];
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
        // A native partition can contain BOTH arms. Reserve every retained
        // triangle, including the other arm and caps already emitted for it.
        if(cropped[at]!=cropped[at+1]&&cropped[at]!=cropped[at+2]&&cropped[at+1]!=cropped[at+2])
            for(size_t i=0;i<3;++i){if(cropped[at+i]>=positions.size())return 0;used[canonical[cropped[at+i]]]=true;}
    }
    std::vector<uint16_t> spare;
    if(fitted){
        fitted->assign(positions.begin(),positions.end());copies->clear();
        // A welded representative may not itself be indexed by a retained
        // triangle. Reserve its entire seam group, not just indexed copies.
        for(size_t i=0;i<used.size();++i)if(!used[canonical[i]])spare.push_back(static_cast<uint16_t>(i));
    }
    // Extra lining vertices must never deprive a later native LOD of its
    // existing rim fit. Retain the flat result as an exact outer-shape
    // transaction guard, including partitions shared by the two arms.
    std::vector<uint16_t> flatIndices;
    std::vector<Vec3> flatPositions;
    std::vector<std::array<uint16_t,2>> flatCopies;
    size_t flatCount{};
    if(shading&&fitted){
        flatIndices.assign(cropped.begin(),cropped.end());
        flatCount=closeForearmCuffs(positions,original,flatIndices,lods,right,cut,&flatPositions,&flatCopies,elbow,nullptr,native);
    }
    size_t added{};
    for(size_t lod=0;lod<lods.size();++lod){
        struct Edge {unsigned all{},kept{};uint16_t a{},b{};};
        std::map<std::array<uint16_t,2>,Edge> edges;
        std::vector<size_t> slots;
        const auto range=lods[lod];
        for(size_t at=range.first;at<range.end;at+=3){
            const std::array<uint16_t,3> face{original[at],original[at+1],original[at+2]};
            const bool keep=std::all_of(face.begin(),face.end(),[&](auto i){return (right?-native[i].x:native[i].x)>=cut;});
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
        std::vector<std::vector<uint16_t>> rims;
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
            // The native rim can step axially with identical Y/Z. Once
            // flattened those adjacent vertices form a zero-length edge,
            // which prevents ear clipping even though the ring is closed.
            // Keep the full rim below for fitting, but triangulate its unique
            // projected corners. Native skin/UV seams remain separate.
            const auto coincident=[&](uint16_t a,uint16_t b){
                const auto p=native[a],q=native[b];
                return std::abs(p.y-q.y)<.000001f&&std::abs(p.z-q.z)<.000001f;
            };
            for(size_t i=0;polygon.size()>2&&i<polygon.size();){
                const auto nextCorner=(i+1)%polygon.size();
                if(coincident(polygon[i],polygon[nextCorner]))polygon.erase(polygon.begin()+nextCorner);
                else ++i;
            }
            if(polygon.size()<3){manifold=false;break;}
            rims.push_back(polygon);
            const auto area=[&](uint16_t a,uint16_t b,uint16_t c){
                const auto p=native[a],q=native[b],r=native[c];
                return (q.y-p.y)*(r.z-p.z)-(q.z-p.z)*(r.y-p.y);
            };
            float signedArea{};
            for(size_t i=0;i<polygon.size();++i){const auto p=native[polygon[i]],q=native[polygon[(i+1)%polygon.size()]];signedArea+=p.y*q.z-p.z*q.y;}
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
            // Keep the exact outer rim. Only new, independently shaded cap
            // vertices form the inset lining: 12 mm toward the wrist, with a
            // smaller inner ring. A non-star-shaped/ambiguous cuff retains its
            // existing flat closure; never bridge an unrelated opening.
            bool recess=shading!=nullptr;
            size_t corners{};
            std::vector<Vec3> centers;
            for(const auto& rim:rims){
                Vec3 center{};float signedArea{};
                for(size_t i=0;i<rim.size();++i){
                    const auto p=native[rim[i]],q=native[rim[(i+1)%rim.size()]];
                    center=center+p;signedArea+=p.y*q.z-p.z*q.y;
                }
                center=center*(1.f/static_cast<float>(rim.size()));corners+=rim.size();
                const float winding=signedArea>0?1.f:-1.f;
                // The mean of a concave cuff can lie outside its visibility
                // kernel. Intersect the edge halfplanes instead of assuming a
                // convex sleeve; the resulting center sees every rim edge.
                std::vector<Vec3> kernel{{0,-10,-10},{0,10,-10},{0,10,10},{0,-10,10}};
                for(size_t i=0;i<rim.size();++i){
                    const auto p=native[rim[i]],q=native[rim[(i+1)%rim.size()]];
                    const auto side=[&](Vec3 v){return ((q.y-p.y)*(v.z-p.z)-(q.z-p.z)*(v.y-p.y))*winding;};
                    std::vector<Vec3> clipped;
                    for(size_t j=0;j<kernel.size();++j){
                        const auto a=kernel[j],b=kernel[(j+1)%kernel.size()];const auto sa=side(a),sb=side(b);
                        if(sa>=0)clipped.push_back(a);
                        if((sa>=0)!=(sb>=0))clipped.push_back(a+(b-a)*(sa/(sa-sb)));
                    }
                    kernel=std::move(clipped);
                }
                if(kernel.size()<3)recess=false;
                else {center={};for(auto p:kernel)center=center+p;center=center*(1.f/static_cast<float>(kernel.size()));}
                centers.push_back(center);
                for(size_t i=0;i<rim.size();++i){
                    const auto p=native[rim[i]],q=native[rim[(i+1)%rim.size()]];
                    if(((q.y-p.y)*(center.z-p.z)-(q.z-p.z)*(center.y-p.y))*winding<=1e-9f)recess=false;
                }
            }
            recess=recess&&corners==duplicates.size()&&corners*3<=spare.size()&&caps.size()+corners*2<=slots.size();
            // Admit an inset only inside this exact native sleeve's axial
            // cross-section. Outfit hashes alone do not prove that a folded
            // or separate material partition contains a generic inset.
            const auto insideNativeSlice=[&](Vec3 point){
                std::vector<float> crossings;
                for(size_t at=range.first;at<range.end;at+=3){
                    std::array<Vec3,2> section{};size_t count{};
                    for(size_t edge=0;edge<3;++edge){
                        const auto a=native[original[at+edge]],b=native[original[at+(edge+1)%3]];
                        if((a.x>point.x)!=(b.x>point.x)){
                            if(count>=section.size())return false;
                            section[count++]=a+(b-a)*((point.x-a.x)/(b.x-a.x));
                        }
                    }
                    if(count!=2)continue;
                    const auto a=section[0],b=section[1];
                    if((a.z>point.z)!=(b.z>point.z)){
                        const auto y=a.y+(b.y-a.y)*((point.z-a.z)/(b.z-a.z));
                        if(y>point.y)crossings.push_back(y);
                    }
                }
                std::sort(crossings.begin(),crossings.end());size_t count{};float last{};
                for(float y:crossings)if(!count||std::abs(y-last)>1e-6f){++count;last=y;}
                return (count&1u)!=0;
            };
            std::map<uint16_t,Vec3> inset;
            if(recess){
                for(size_t loop=0;loop<rims.size();++loop)for(auto vertex:rims[loop]){
                    const auto p=native[vertex],center=centers[loop];
                    const Vec3 inner{right?-elbow-.012f:elbow+.012f,
                        center.y+(p.y-center.y)*.7f,center.z+(p.z-center.z)*.7f};
                    inset.emplace(vertex,inner);
                    if(!insideNativeSlice(inner))recess=false;
                }
                for(const auto& face:caps){
                    const auto a=inset.at(face[0]),b=inset.at(face[1]),c=inset.at(face[2]);
                    if(!insideNativeSlice((a+b+c)*(1.f/3.f))||!insideNativeSlice((a+b)*.5f)
                        ||!insideNativeSlice((b+c)*.5f)||!insideNativeSlice((c+a)*.5f))recess=false;
                }
            }
            // A cut through whole triangles has a saw-tooth axial rim. Fit
            // only that rim to the authored elbow plane; hands and distal
            // forearms keep their native positions. All coincident seam
            // copies move together so UV/normal seams cannot open a crack.
            for(const auto& loop:loops)for(auto vertex:loop){
                for(size_t i=0;i<canonical.size();++i)if(canonical[i]==vertex)
                    (*fitted)[i].x=right?-elbow:elbow;
            }
            for(auto& [vertex,duplicate]:duplicates){
                duplicate=spare.back();spare.pop_back();
                (*fitted)[duplicate]=(*fitted)[vertex];
                copies->push_back({duplicate,vertex});
            }
            for(auto& cap:caps)for(auto& vertex:cap)vertex=duplicates.at(vertex);
            if(recess){
                for(size_t loop=0;loop<rims.size();++loop){
                    const auto& rim=rims[loop];
                    std::vector<uint16_t> outer,inner;
                    for(auto vertex:rim){
                        const auto p=inset.at(vertex);
                        (*fitted)[duplicates.at(vertex)]=p;
                        const auto a=spare.back();spare.pop_back();const auto b=spare.back();spare.pop_back();
                        outer.push_back(a);inner.push_back(b);
                        (*fitted)[a]=(*fitted)[vertex];(*fitted)[b]=p;
                        copies->push_back({a,vertex});copies->push_back({b,vertex});
                        shading->normals[a]=shading->normals[b]={};
                    }
                    for(size_t i=0;i<rim.size();++i){
                        const auto nextCorner=(i+1)%rim.size();
                        for(const auto face:std::array<std::array<uint16_t,3>,2>{{
                                {outer[i],outer[nextCorner],inner[i]},{outer[nextCorner],inner[nextCorner],inner[i]}}}){
                            caps.push_back(face);
                            const auto u=(*fitted)[face[1]]-(*fitted)[face[0]],v=(*fitted)[face[2]]-(*fitted)[face[0]];
                            const Vec3 n{u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};
                            for(auto vertex:face)shading->normals[vertex]=shading->normals[vertex]+n;
                        }
                    }
                    for(auto v:outer)shading->normals[v]=shading->normals[v]*(1.f/std::sqrt(dot(shading->normals[v],shading->normals[v])));
                    for(auto v:inner)shading->normals[v]=shading->normals[v]*(1.f/std::sqrt(dot(shading->normals[v],shading->normals[v])));
                    ++shading->recessedLoops;
                }
            }
        }
        for(size_t i=0;i<caps.size();++i)std::copy(caps[i].begin(),caps[i].end(),cropped.begin()+slots[i]);
        added+=caps.size();
    }
    if(shading&&fitted){
        bool preserved=flatPositions.size()==fitted->size();
        for(size_t i=0;preserved&&i<positions.size();++i)if(used[canonical[i]]){
            const auto a=(*fitted)[i],b=flatPositions[i];preserved=a.x==b.x&&a.y==b.y&&a.z==b.z;
        }
        if(!preserved){
            std::copy(flatIndices.begin(),flatIndices.end(),cropped.begin());
            *fitted=std::move(flatPositions);*copies=std::move(flatCopies);
            shading->normals.assign(positions.size(),{right?1.f:-1.f,0,0});shading->recessedLoops=0;
            return flatCount;
        }
    }
    return added;
}
}
