-- Read-only scenario identity. Raw save hashes alone cannot identify a scene.
local function quoted(value)
    local text = tostring(value)
    return '"' .. text:gsub('\\', '\\\\'):gsub('"', '\\"'):gsub('\n', '\\n'):gsub('\r', '\\r') .. '"'
end
local rows = {}
local function field(name, fn)
    local ok, value = pcall(fn)
    if not ok then value = 'unavailable:' .. tostring(value) end
    local encoded = (type(value) == 'number' or type(value) == 'boolean') and tostring(value)
        or value == nil and 'null' or quoted(value)
    rows[#rows + 1] = quoted(name) .. ':' .. encoded
end
field('mission', function() return vars.missionCode end)
field('location', function() return vars.locationCode end)
field('sequence', function() return TppSequence.GetCurrentSequenceName() end)
-- ACC weapon, helicopter and vehicle selectors share WeaponCustomize sequence
-- names. Preserve the authored target separately; nil is unknown, never a
-- default helicopter target. This read does not change native menu ownership.
field('customization_target', function() return mvars.startCustomizeTarget end)
field('helicopter_space', function() return TppMission.IsHelicopterSpace(vars.missionCode) end)
field('customization_kind', function()
    if TppSequence.GetCurrentSequenceName() ~= 'Seq_Game_WeaponCustomize' then return nil end
    local target = mvars.startCustomizeTarget
    if target == nil then return nil end
    assert(type(target) == 'number' and type(Fox) == 'table' and type(Fox.StrCode32) == 'function',
        'native customization target/hash API unavailable')
    for kind, name in pairs({weapon='Customize_Target_Weapon', helicopter='Customize_Target_Helicopter',
                            vehicle='Customize_Target_Vehicle'}) do
        if target == Fox.StrCode32(name) then return kind end
    end
    return 'unrecognized'
end)
-- Read our own player-pad registration separately from the retail selector's
-- mask. A terminal-close recovery is not a selector Cancel/End transition.
field('vr_idroid_player_pad_block', function()
    return TppGameStatus.IsSet('MGS5VR_iDroid', 'S_DISABLE_PLAYER_PAD')
end)
field('title', function() return gvars.ini_isTitleMode end)
field('story', function() return gvars.str_storySequence end)
field('player_x', function() return vars.playerPosX end)
field('player_y', function() return vars.playerPosY end)
field('player_z', function() return vars.playerPosZ end)
field('player_yaw', function() return vars.playerRotY end)
field('player_camera_yaw', function() return vars.playerCameraRotation[1] end)
field('demo', function() return DemoDaemon.IsDemoPlaying() end)
field('demo_nonplayable', function() return TppDemo.IsNotPlayable() end)
field('checkpoint', function() return gvars.mis_checkPoint end)
field('mission6_event_sequence', function() if vars.missionCode == 10040 then return svars.eventSequenceNum end end)
field('mission6_bridge_demo', function()
    if vars.missionCode == 10040 then return DemoDaemon.IsPlayingDemoId('p31_020020_000') end
end)
field('saving', function() return TppSave.IsSaving() end)
field('player_life', function() return vars.playerLife end)
field('not_alert', function() return Tpp.IsNotAlert() end)
field('game_over', function() return svars.mis_gameOverType end)
field('popup', function() return TppUiCommand.IsShowPopup() end)
-- A replayed Mission 1 tutorial enables a different Resume/Skip page. This
-- state must never be mistaken for the ordinary field Pause menu in QA.
field('tutorial_pause', function() return TppGameStatus.IsSet('s10020', 'S_ENABLE_TUTORIAL_PAUSE') end)
-- Retail TppSequence sets this only after mission preparation/save completion
-- and PermitEndLoadingTips, then clears it on PushEndLoadingTips. It is not
-- interchangeable with the loading terminal's visible/open flag.
field('loading_wait_confirm', function() return mvars.seq_nowWaitingPushEndLoadingTips == true end)
field('mission_prepared', function() return TppSequence.IsMissionPrepareFinished() end)
-- Query only status identifiers used by the owned retail TPP scripts. Keep
-- these fields even when a native constant/API is absent: field() serializes
-- the error as "unavailable:..." instead of silently dropping it or saying
-- false. NORMAL_ACTION and PARTS_ACTIVE are broad engine statuses, not aim,
-- CQC, cover, or mounted-state proofs.
for _, name in ipairs({'CARRY', 'CRAWL', 'NORMAL_ACTION', 'PARTS_ACTIVE', 'SQUAT', 'STAND'}) do
    field('status_' .. name, function()
        assert(type(PlayerStatus) == 'table', 'PlayerStatus API unavailable')
        assert(type(PlayerInfo) == 'table' and type(PlayerInfo.AndCheckStatus) == 'function',
            'PlayerInfo.AndCheckStatus API unavailable')
        local value = PlayerStatus[name]
        assert(type(value) == 'number', name .. ' status constant unavailable')
        local active = PlayerInfo.AndCheckStatus{value}
        assert(type(active) == 'boolean', name .. ' status result unavailable')
        return active
    end)
end
-- Preserve this separately seeded native binocular status. The retail Lua
-- source has no equivalent validated readback for weapon-aim/subject state.
field('status_BINOCLE', function()
    assert(type(PlayerStatus) == 'table', 'PlayerStatus API unavailable')
    assert(type(PlayerInfo) == 'table' and type(PlayerInfo.OrCheckStatus) == 'function',
        'PlayerInfo.OrCheckStatus API unavailable')
    local value = PlayerStatus.BINOCLE
    assert(type(value) == 'number', 'BINOCLE status constant unavailable')
    local active = PlayerInfo.OrCheckStatus{value}
    assert(type(active) == 'boolean', 'BINOCLE status result unavailable')
    return active
end)
field('buddy_type', function() return vars.buddyType end)
field('sortie_buddy_type', function() return vars.sortieBuddyType end)
for _, name in ipairs({'HORSE', 'DOG', 'QUIET', 'WALKER_GEAR'}) do
    field('buddy_' .. name, function() return TppBuddyService.DidObtainBuddyType(BuddyType[name]) end)
end
-- Inventory IDs are finite named loadout positions, not counts of unlocked
-- variants. A missing table/constant/entry is reported as unavailable; a
-- recognized empty slot remains the game's numeric EQP_None ID.
for _, name in ipairs({'PRIMARY_HIP', 'PRIMARY_BACK', 'SECONDARY'}) do
    field('weapon_' .. name, function()
        assert(type(TppDefine) == 'table' and type(TppDefine.WEAPONSLOT) == 'table',
            'TppDefine.WEAPONSLOT API unavailable')
        local index = TppDefine.WEAPONSLOT[name]
        assert(type(index) == 'number', name .. ' weapon slot constant unavailable')
        assert(vars.weapons ~= nil, 'vars.weapons unavailable')
        local value = vars.weapons[index]
        assert(type(value) == 'number', 'vars.weapons[' .. tostring(index) .. '] unavailable')
        return value
    end)
end
for index = 0, 7 do
    field('support_weapon_' .. index, function()
        assert(vars.supportWeapons ~= nil, 'vars.supportWeapons unavailable')
        local value = vars.supportWeapons[index]
        assert(type(value) == 'number', 'vars.supportWeapons[' .. index .. '] unavailable')
        return value
    end)
    field('item_' .. index, function()
        assert(vars.items ~= nil, 'vars.items unavailable')
        local value = vars.items[index]
        assert(type(value) == 'number', 'vars.items[' .. index .. '] unavailable')
        return value
    end)
end
-- Retail Lua reads these arrays by the same three WEAPONSLOT IDs when saving
-- and restoring ammo. They provide per-slot counters, not a selected-weapon
-- pointer or proof that a fire/reload action completed.
for _, name in ipairs({'PRIMARY_HIP', 'PRIMARY_BACK', 'SECONDARY'}) do
    field('ammo_in_weapon_' .. name, function()
        assert(type(TppDefine) == 'table' and type(TppDefine.WEAPONSLOT) == 'table',
            'TppDefine.WEAPONSLOT API unavailable')
        local index = TppDefine.WEAPONSLOT[name]
        assert(type(index) == 'number', name .. ' weapon slot constant unavailable')
        assert(vars.ammoInWeapons ~= nil, 'vars.ammoInWeapons unavailable')
        return vars.ammoInWeapons[index]
    end)
    field('ammo_sub_in_weapon_' .. name, function()
        assert(type(TppDefine) == 'table' and type(TppDefine.WEAPONSLOT) == 'table',
            'TppDefine.WEAPONSLOT API unavailable')
        local index = TppDefine.WEAPONSLOT[name]
        assert(type(index) == 'number', name .. ' weapon slot constant unavailable')
        assert(vars.ammoSubInWeapons ~= nil, 'vars.ammoSubInWeapons unavailable')
        return vars.ammoSubInWeapons[index]
    end)
end
-- currentItemIndex is initialized by retail TppPlayer code, but the inspected
-- Lua does not prove it remains the live selection cursor. Name it as a
-- candidate and keep it distinct from item_0..7 inventory IDs.
field('item_cursor_candidate', function()
    assert(type(vars.currentItemIndex) == 'number', 'vars.currentItemIndex unavailable')
    return vars.currentItemIndex
end)
-- No source-verified read-only selected-weapon-slot getter was found. Expose
-- that gap explicitly instead of copying an inventory index into this field.
field('selected_weapon_slot', function()
    error('no source-verified current weapon slot getter')
end)
-- This native flag is set/reset by retail TppPlayer during mission-end camera
-- handling. Report its raw value and named constant without guessing whether
-- the engine combines action flags as a bitmask.
field('player_disable_action_flag', function()
    assert(type(vars.playerDisableActionFlag) == 'number', 'vars.playerDisableActionFlag unavailable')
    return vars.playerDisableActionFlag
end)
field('disable_action_subjective_camera_value', function()
    assert(type(PlayerDisableAction) == 'table', 'PlayerDisableAction API unavailable')
    local value = PlayerDisableAction.SUBJECTIVE_CAMERA
    assert(type(value) == 'number', 'SUBJECTIVE_CAMERA constant unavailable')
    return value
end)
-- TppPlayer/TppMission use playerVehicleGameObjectId with these native type
-- predicates. Each query stays separate so unknown APIs/IDs are unavailable,
-- never collapsed into a false mode.
field('player_vehicle_id', function()
    assert(type(vars.playerVehicleGameObjectId) == 'number', 'vars.playerVehicleGameObjectId unavailable')
    return vars.playerVehicleGameObjectId
end)
for _, mode in ipairs({'Horse', 'Vehicle', 'Helicopter'}) do
    field('player_vehicle_is_' .. string.lower(mode), function()
        assert(type(Tpp) == 'table' and type(Tpp['Is' .. mode]) == 'function',
            'Tpp.Is' .. mode .. ' API unavailable')
        assert(type(vars.playerVehicleGameObjectId) == 'number', 'vars.playerVehicleGameObjectId unavailable')
        return Tpp['Is' .. mode](vars.playerVehicleGameObjectId)
    end)
end
field('player_in_support_helicopter', function()
    assert(type(mvars) == 'table' and type(mvars.hel_heliPassengerTable) == 'table',
        'support helicopter passenger table unavailable')
    assert(type(PlayerInfo) == 'table' and type(PlayerInfo.GetLocalPlayerIndex) == 'function',
        'PlayerInfo.GetLocalPlayerIndex API unavailable')
    assert(type(GameObject) == 'table' and type(GameObject.GetGameObjectIdByIndex) == 'function',
        'GameObject.GetGameObjectIdByIndex API unavailable')
    assert(type(GameObject.NULL_ID) == 'number', 'GameObject.NULL_ID constant unavailable')
    assert(type(TppHelicopter) == 'table' and type(TppHelicopter.IsInHelicopter) == 'function',
        'TppHelicopter.IsInHelicopter API unavailable')
    local playerId = GameObject.GetGameObjectIdByIndex('TppPlayer2', PlayerInfo.GetLocalPlayerIndex())
    assert(type(playerId) == 'number' and playerId ~= GameObject.NULL_ID, 'local player object ID unavailable')
    return TppHelicopter.IsInHelicopter(playerId) == true
end)
-- Indexed IDs include uninstantiated slots. Do not report them as actor counts.
field('active_buddy', function() return TppBuddy2BlockController.GetActiveBuddyType() end)
field('buddy_loading', function() return TppBuddy2BlockController.IsLoading() end)
return '{' .. table.concat(rows, ',') .. '}'
