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
field('title', function() return gvars.ini_isTitleMode end)
field('story', function() return gvars.str_storySequence end)
field('player_x', function() return vars.playerPosX end)
field('player_y', function() return vars.playerPosY end)
field('player_z', function() return vars.playerPosZ end)
field('player_yaw', function() return vars.playerRotY end)
field('demo', function() return DemoDaemon.IsDemoPlaying() end)
field('saving', function() return TppSave.IsSaving() end)
field('player_life', function() return vars.playerLife end)
field('game_over', function() return svars.mis_gameOverType end)
field('popup', function() return TppUiCommand.IsShowPopup() end)
field('buddy_type', function() return vars.buddyType end)
field('sortie_buddy_type', function() return vars.sortieBuddyType end)
for _, name in ipairs({'HORSE', 'DOG', 'QUIET', 'WALKER_GEAR'}) do
    field('buddy_' .. name, function() return TppBuddyService.DidObtainBuddyType(BuddyType[name]) end)
end
for _, name in ipairs({'PRIMARY_HIP', 'PRIMARY_BACK', 'SECONDARY'}) do
    field('weapon_' .. name, function() return vars.weapons[TppDefine.WEAPONSLOT[name]] end)
end
-- Indexed IDs include uninstantiated slots. Do not report them as actor counts.
field('active_buddy', function() return TppBuddy2BlockController.GetActiveBuddyType() end)
field('buddy_loading', function() return TppBuddy2BlockController.IsLoading() end)
return '{' .. table.concat(rows, ',') .. '}'
