#pragma once
#include "core.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>

namespace mgs5vr {
// Read-only world-ray evidence. NAV support, capsule clearance, enemy vision,
// concealment and path execution remain independent caller/game contracts.
struct CoverProbeOwner {
    uint64_t activation{},playerOwner{},presentationEpoch{};
    bool operator==(const CoverProbeOwner&) const = default;
};
struct CoverProbeNode {uint64_t node{};Vec3 point{};};
struct CoverProbeRequest {
    uint64_t nonce{},leaseMs{},observedMs{},issuedMs{},expiresMs{};
    CoverProbeOwner owner{};
    Vec3 observedPlayer{};
    std::array<CoverProbeNode,8> nodes{};
    size_t count{};
};
struct CoverProbeFrame {
    CoverProbeOwner owner{};
    uint64_t sampleMs{},trackingSequence{};
    Vec3 nativeEye{};
    bool usable{};
};
enum class CoverProbeReason:uint8_t {
    none,pending,disabled,lease,owner,frame,staleFrame,staleRoot,eyeReach,nonce,noActors,request
};
inline const char* coverProbeReasonName(CoverProbeReason reason) noexcept {
    switch(reason){
    case CoverProbeReason::none:return "available";
    case CoverProbeReason::pending:return "awaiting_native_marker_job";
    case CoverProbeReason::disabled:return "native_probe_disabled";
    case CoverProbeReason::lease:return "lease_unavailable_or_expired";
    case CoverProbeReason::owner:return "native_owner_mismatch";
    case CoverProbeReason::frame:return "focused_gameplay_frame_unavailable";
    case CoverProbeReason::staleFrame:return "native_frame_or_result_stale";
    case CoverProbeReason::staleRoot:return "observed_player_root_stale";
    case CoverProbeReason::eyeReach:return "native_eye_offset_exceeds_3m";
    case CoverProbeReason::nonce:return "request_nonce_mismatch";
    case CoverProbeReason::noActors:return "nearby_active_person_markers_unavailable";
    case CoverProbeReason::request:return "invalid_bounded_request";
    }
    return "unavailable";
}
inline bool coverProbeFinite(Vec3 point) noexcept {
    return std::isfinite(point.x)&&std::isfinite(point.y)&&std::isfinite(point.z)
        &&std::abs(point.x)<=1.e6f&&std::abs(point.y)<=1.e6f&&std::abs(point.z)<=1.e6f;
}
inline float coverProbeDistanceSquared(Vec3 a,Vec3 b) noexcept {
    const float x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;
    return x*x+y*y+z*z;
}
inline bool validCoverProbeRequest(const CoverProbeRequest& request) noexcept {
    if(!request.nonce||request.leaseMs<100||request.leaseMs>5000||!request.observedMs
       ||!request.owner.activation||!request.owner.playerOwner||!request.owner.presentationEpoch
       ||!request.count||request.count>request.nodes.size()||!coverProbeFinite(request.observedPlayer))return false;
    for(size_t i=0;i<request.count;++i){
        if(!coverProbeFinite(request.nodes[i].point)
           ||coverProbeDistanceSquared(request.nodes[i].point,request.observedPlayer)>60.f*60.f)return false;
        for(size_t j=0;j<i;++j)if(request.nodes[i].node==request.nodes[j].node)return false;
    }
    return true;
}
// Wire body (<=1600 bytes, no whitespace or trailing separator):
// nonce:lease_ms:activation:player_owner:presentation_epoch:observed_ms:
// px,py,pz:node_id,x,y,z[;node_id,x,y,z...]
inline std::optional<CoverProbeRequest> parseCoverProbeRequest(std::string_view text) noexcept {
    if(text.empty()||text.size()>1600)return {};
    CoverProbeRequest result;
    auto take=[&](char delimiter)->std::optional<std::string_view>{
        const auto position=text.find(delimiter);
        if(position==std::string_view::npos||position==0)return {};
        const auto token=text.substr(0,position);text.remove_prefix(position+1);return token;
    };
    const auto integer=[](std::string_view token,uint64_t& value){
        if(token.empty())return false;
        const auto parsed=std::from_chars(token.data(),token.data()+token.size(),value);
        return parsed.ec==std::errc{}&&parsed.ptr==token.data()+token.size();
    };
    const auto scalar=[](std::string_view token,float& value){
        if(token.empty())return false;
        const auto parsed=std::from_chars(token.data(),token.data()+token.size(),value,std::chars_format::general);
        return parsed.ec==std::errc{}&&parsed.ptr==token.data()+token.size()&&std::isfinite(value)&&std::abs(value)<=1.e6f;
    };
    const auto point=[&](std::string_view token,Vec3& value){
        const auto first=token.find(',');
        if(first==std::string_view::npos)return false;
        const auto second=token.find(',',first+1);
        return second!=std::string_view::npos&&scalar(token.substr(0,first),value.x)
            &&scalar(token.substr(first+1,second-first-1),value.y)&&scalar(token.substr(second+1),value.z);
    };
    std::array<uint64_t*,6> fields{&result.nonce,&result.leaseMs,&result.owner.activation,
        &result.owner.playerOwner,&result.owner.presentationEpoch,&result.observedMs};
    for(auto* field:fields){const auto token=take(':');if(!token||!integer(*token,*field))return {};}
    if(!result.nonce||result.leaseMs<100||result.leaseMs>5000||!result.owner.activation
       ||!result.owner.playerOwner||!result.owner.presentationEpoch||!result.observedMs)return {};
    const auto root=take(':');if(!root||!point(*root,result.observedPlayer))return {};
    while(!text.empty()){
        if(result.count==result.nodes.size())return {};
        const auto separator=text.find(';');
        auto token=text.substr(0,separator);
        const auto comma=token.find(',');
        auto& node=result.nodes[result.count];
        if(comma==std::string_view::npos||!integer(token.substr(0,comma),node.node)
           ||!point(token.substr(comma+1),node.point)
           ||coverProbeDistanceSquared(node.point,result.observedPlayer)>60.f*60.f)return {};
        for(size_t i=0;i<result.count;++i)if(result.nodes[i].node==node.node)return {};
        ++result.count;
        if(separator==std::string_view::npos){text={};break;}
        text.remove_prefix(separator+1);if(text.empty())return {};
    }
    return validCoverProbeRequest(result)?std::optional<CoverProbeRequest>{result}:std::nullopt;
}
inline CoverProbeReason coverProbeFrameReason(const CoverProbeRequest& request,
    const CoverProbeFrame& frame,uint64_t now) noexcept {
    if(!validCoverProbeRequest(request))return CoverProbeReason::request;
    if(!request.expiresMs||now<request.issuedMs||now>=request.expiresMs)return CoverProbeReason::lease;
    if(frame.owner!=request.owner)return CoverProbeReason::owner;
    if(!frame.usable||!frame.trackingSequence||!coverProbeFinite(frame.nativeEye))return CoverProbeReason::frame;
    if(!frame.sampleMs||now<frame.sampleMs||now-frame.sampleMs>150)return CoverProbeReason::staleFrame;
    if(now<request.observedMs||now-request.observedMs>250)return CoverProbeReason::staleRoot;
    if(coverProbeDistanceSquared(frame.nativeEye,request.observedPlayer)>9.f)return CoverProbeReason::eyeReach;
    return CoverProbeReason::none;
}
inline bool coverProbeTerrainObstruction(bool queryValid,uint8_t queryReason,uint32_t rayResult,
    float length,float along,float lateralSquared) noexcept {
    return queryValid&&queryReason==9&&rayResult&&std::isfinite(length)&&std::isfinite(along)
        &&std::isfinite(lateralSquared)&&length>=1.f&&length<=300.f&&along>=.05f
        &&along<length-.35f&&lateralSquared>=0.f&&lateralSquared<=.04f;
}
struct CoverProbePerson {uint16_t id{0xffff},recordIndex{};uint8_t flags{};Vec3 marker{};float distance{};};
struct CoverProbeRay {
    uint16_t actor{0xffff};Vec3 marker{},endpoint{},rawSurface{};
    float distance{},surfaceDistance{},rawAlong{},rawLateralSquared{};
    bool queryValid{},terrainObstructed{};
    uint8_t queryReason{};
    uint32_t rayResult{},hitCount{};
    int32_t hitIndex{-1};
};
struct CoverProbeCandidate {
    uint64_t node{};Vec3 point{},eye{};
    std::array<CoverProbeRay,4> rays{};size_t count{};
    bool allMarkerRaysObstructed{};
};
struct CoverProbeSnapshot {
    CoverProbeRequest request{};
    CoverProbeFrame frame{};
    Vec3 eyeOffset{};
    std::array<CoverProbePerson,4> persons{};size_t personCount{};
    std::array<CoverProbeCandidate,8> candidates{};size_t count{};
    uint64_t sampleMs{};
    bool available{};
    CoverProbeReason reason{CoverProbeReason::pending};
};
}
