-- Return a compact, read-only native presentation state for the VR runtime.
-- Sequence names are useful as a fallback, but TPP also runs story demos
-- inside ordinary Seq_Game_* states such as AfterHeliDemo.
local function queryBoolean(owner, name)
    if type(owner) ~= 'table' then return nil end
    local fn = owner[name]
    if type(fn) ~= 'function' then return nil end
    local ok, value = pcall(fn)
    if not ok then return nil end
    if value == true or (type(value) == 'number' and value ~= 0) then return true end
    if value == false or value == 0 then return false end
    return nil
end

local function checkStatus(name)
    if type(PlayerStatus) ~= 'table' or type(PlayerStatus[name]) ~= 'number'
            or type(PlayerInfo) ~= 'table' or type(PlayerInfo.AndCheckStatus) ~= 'function' then
        return nil
    end
    local ok, value = pcall(PlayerInfo.AndCheckStatus, {PlayerStatus[name]})
    if not ok or type(value) ~= 'boolean' then return nil end
    return value
end

local sequenceName
if type(TppSequence) == 'table' and type(TppSequence.GetCurrentSequenceName) == 'function' then
    local ok, value = pcall(TppSequence.GetCurrentSequenceName)
    if ok and type(value) == 'string' then sequenceName = value end
end

local titleValue = type(gvars) == 'table' and gvars.ini_isTitleMode or false
local title = titleValue == true or (type(titleValue) == 'number' and titleValue ~= 0)
local missionCode = type(vars) == 'table' and vars.missionCode or nil
local helicopterSpace = false
if type(missionCode) == 'number' and type(TppMission) == 'table'
        and type(TppMission.IsHelicopterSpace) == 'function' then
    local ok, value = pcall(TppMission.IsHelicopterSpace, missionCode)
    helicopterSpace = ok and (value == true or (type(value) == 'number' and value ~= 0))
end

if sequenceName == 'Seq_Game_AvatarEdit' then return 'avatar-edit' end
if title and helicopterSpace then return 'title-cabin' end

-- These hospital lessons read PlayerVars.rightStickX/Y while an authored
-- scene can still be playing. They need native look input, not locomotion or
-- combat input. Match both the mission and exact interactive sequence; a
-- generic Seq_Game_* name does not make a cinematic interactive.
local lookLesson = missionCode == 10010 and
    (sequenceName == 'Seq_Game_FewDaysLater0' or sequenceName == 'Seq_Game_FewDaysLater1')
if lookLesson then
    if title then return 'scripted-look-title' end
    return 'scripted-look'
end

local namedDemo = type(sequenceName) == 'string' and string.sub(sequenceName, 1, 9) == 'Seq_Demo_'
local activeDemo = queryBoolean(DemoDaemon, 'IsDemoPlaying')
local nonPlayableDemo = queryBoolean(TppDemo, 'IsNotPlayable')
-- s10010_sequence.IsDemoPlaying requires a list of demo names. Calling it
-- without that list always returns false; it is not a global demo query.
-- IsNotPlayable is about the currently active demo's isInGame flag, not
-- whether a demo is running. Some background NPC demos run with isInGame=true
-- while the player remains controllable. Preserve gameplay only when that
-- explicit native classification agrees with affirmative player-control
-- state; any missing query/status keeps the authored presentation conservative.
if activeDemo == true then
    if lookLesson then
        if title then return 'scripted-look-title' end
        return 'scripted-look'
    end
    if not title and nonPlayableDemo == false then
        local normalAction = checkStatus('NORMAL_ACTION')
        local partsActive = checkStatus('PARTS_ACTIVE')
        if normalAction == true and partsActive == true then
            return 'closed'
        end
    end
    if title then return 'scripted-demo-title' end
    return 'scripted-demo'
end

-- A positive non-playable result means an authored/paused cinematic even if
-- the active-demo API has a transient false result.
if nonPlayableDemo == true then
    if title then return 'scripted-demo-title' end
    return 'scripted-demo'
end

if namedDemo then
    if title then return 'scripted-demo-title' end
    -- Never infer gameplay from a sequence name alone. Missing/error native
    -- queries, incomplete mission prep, or unavailable player-control state
    -- retain the conservative authored-demo presentation.
    local missionPrepared = queryBoolean(TppSequence, 'IsMissionPrepareFinished')
    local normalAction = checkStatus('NORMAL_ACTION')
    local partsActive = checkStatus('PARTS_ACTIVE')
    if activeDemo == false and nonPlayableDemo == false and missionPrepared == true
            and normalAction == true and partsActive == true and type(missionCode) == 'number' then
        return 'stale-demo-candidate:' .. tostring(missionCode) .. ':' .. sequenceName
    end
    if title then return 'scripted-demo-title' end
    return 'scripted-demo'
end

-- An unavailable demo API is not evidence that an ordinary game sequence is
-- playable: demos can run inside Seq_Game_* names too. Stay conservative.
if activeDemo == nil or nonPlayableDemo == nil then
    if title then return 'scripted-demo-title' end
    return 'scripted-demo'
end

if title then return 'title' end
-- These are the retail ACC menu-owning sequences. They use MissionPrep UI,
-- not the iDroid terminal's open bit, and must keep the existing VR menu
-- bindings instead of being mistaken for cabin locomotion.
if helicopterSpace then
    if sequenceName == 'Seq_Game_MissionPreparationTop'
            or sequenceName == 'Seq_Game_MissionPreparation_SelectItem'
            or sequenceName == 'Seq_Game_MissionPreparation_SelectSlot'
            or sequenceName == 'Seq_Game_MissionPreparation_SelectDetail'
            or sequenceName == 'Seq_Game_WeaponCustomize' then
        return 'cabin-menu'
    end
    return 'cabin'
end
-- The mission keeps Seq_Game_MainGame on its failure screen. Check the
-- retail game-over state only after authored demos have retained ownership.
if queryBoolean(TppMission, 'IsGameOver') == true then return 'game-over' end
return 'closed'
