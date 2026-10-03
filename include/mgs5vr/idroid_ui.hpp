#pragma once
#include <cstdint>

namespace mgs5vr {
enum class IdroidUiRole { unknown, device, personalHud };
// Exact original scene resources joined to their current typed draw owners in
// two native open/close cycles. Priorities and camera depth do not identify a
// scene: Map and the ordinary left-arm HUD reuse both.
constexpr IdroidUiRole idroidUiRole(uint64_t resource) noexcept {
    switch(resource){
    case 0x504021c76a2a4cbaull: // mb_map_mapmenu_main
    case 0x5040741ac64544cfull: // mb_map_layout_sub
    case 0x504074e62baad619ull: // mb_map_base
    case 0x504094fe110e6190ull: // mb_cmn_hero
    case 0x5041279d9bbad3e7ull: // mb_cmn_btm_line
    case 0x5041cbb01589c388ull: // mb_cmn_gmp
    case 0x5041fbae29f09edaull: // mb_map_icn_GOAL_base
    case 0x504234fc6f91c7d8ull: // mb_map_mapfilter_tablist
    case 0x504261ee7ecaa18bull: // mb_map_icn_GOAL
    case 0x5042b59328405fccull: // mb_cmn_mark_new
    case 0x5042c3876f5a59e2ull: // mb_map_search
    case 0x5042eb67eeb4de89ull: // mb_cmn_layout
    case 0x504308200fe46da1ull: // mb_layout_master
    case 0x50430ad0dfd261d5ull: // cube_area
    case 0x5043148fe514447dull: // mb_cmn_bg_nologo
    case 0x50432962f66ecdf4ull: // mb_map_icn_SNA
    case 0x50439c74ecdacf51ull: // map_mb_eff
    case 0x5043d809a6309ca0ull: // mb_map_icn_common
    case 0x5043e2ca55e8f7bbull: // STC_mb_map_hide
    case 0x5043f3b3f0a7aff9ull: // mb_map_layout
        return IdroidUiRole::device;
    case 0x50418b3d353471adull:
    case 0x50419149642d99b4ull:
    case 0x5043132d8cf49a67ull:
    case 0x5042c796d643f58eull: // sys_mb_tutorial
        return IdroidUiRole::personalHud;
    // mm_ordr_lyt also belongs to mission presentation. A surviving open-menu
    // tuple does not establish exclusive device ownership for that resource.
    default:return IdroidUiRole::unknown;
    }
}
struct IdroidUiIdentity {
    uintptr_t owner{},root{},packet{},buffer{},camera{};
    uint64_t resource{};
    bool operator==(const IdroidUiIdentity&) const = default;
};
struct IdroidUiSource {
    uintptr_t player{};
    uint64_t activation{},referenceEpoch{},presentationEpoch{},menuGeneration{},source{};
    bool focused{},handheld{},menuOpen{},idroid{};
};
constexpr bool validIdroidUiSource(const IdroidUiSource& source) noexcept {
    return source.player&&source.activation&&source.referenceEpoch&&source.presentationEpoch
        &&source.menuGeneration&&source.source&&source.focused&&source.handheld;
}
struct IdroidUiOpenLease { IdroidUiIdentity identity{};IdroidUiSource source{}; };
constexpr bool sameIdroidUiOwner(const IdroidUiSource& a,const IdroidUiSource& b) noexcept {
    return validIdroidUiSource(a)&&validIdroidUiSource(b)&&a.player==b.player
        &&a.activation==b.activation&&a.referenceEpoch==b.referenceEpoch
        &&a.presentationEpoch==b.presentationEpoch;
}
// The native draw itself establishes pixel lifetime. This lease establishes
// only which observed open transaction owned that exact surviving layout;
// it does not extend visibility, select a pose, or read the persistent closing
// byte. A different menu generation, owner, mode or focus epoch invalidates it.
constexpr bool outgoingIdroidUi(const IdroidUiOpenLease& open,const IdroidUiIdentity& current,
    const IdroidUiSource& source,uint32_t candidates,bool currentVerified) noexcept {
    return candidates==1&&currentVerified&&idroidUiRole(current.resource)==IdroidUiRole::device
        &&current.owner&&current.root&&current.packet&&current.buffer&&current.camera
        &&current==open.identity&&sameIdroidUiOwner(open.source,source)
        &&open.source.menuOpen&&open.source.idroid&&!source.menuOpen&&!source.idroid
        &&source.source>open.source.source&&open.source.menuGeneration!=UINT64_MAX
        &&source.menuGeneration==open.source.menuGeneration+1;
}
enum class IdroidUiClosingRoute { unchanged,display,suppress };
constexpr IdroidUiClosingRoute idroidUiClosingRoute(bool verifiedOutgoing,bool exactDisplay) noexcept {
    return !verifiedOutgoing?IdroidUiClosingRoute::unchanged
        :exactDisplay?IdroidUiClosingRoute::display:IdroidUiClosingRoute::suppress;
}
}
