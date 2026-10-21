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

-- The native end-of-mission controller can retain a Seq_Demo_* name after
-- its scene finishes. Credits, results and rewards own the stereo menu and
-- Confirm; player NORMAL_ACTION alone cannot classify that UI as a scene.
-- Loaded result resources do not claim gameplay or an active 3D/movie shot.
local endUi = false
if not title and type(TppMission) == 'table'
        and type(TppMission.GetMissionClearState) == 'function'
        and type(TppDefine) == 'table' and type(TppDefine.MISSION_CLEAR_STATE) == 'table'
        and queryBoolean(TppMission, 'IsDefiniteMissionClear') == true
        and queryBoolean(TppUiCommand, 'IsResultBlockLoaded') == true
        and queryBoolean(DemoDaemon, 'IsDemoPlaying') == false
        and queryBoolean(TppVideoPlayer, 'IsVideoStopped') == true then
    local ok, phase = pcall(TppMission.GetMissionClearState)
    local phases = TppDefine.MISSION_CLEAR_STATE
    endUi = ok and type(phase) == 'number' and
        (phase == phases.MISSION_GAME_END or phase == phases.SHOW_CREDIT_END
            or phase == phases.BLACK_TELEPHOHE_END or phase == phases.REWARD_END)
end
if endUi then return 'native-popup:mission-end' end
local helicopterSpace = false
if type(missionCode) == 'number' and type(TppMission) == 'table'
        and type(TppMission.IsHelicopterSpace) == 'function' then
    local ok, value = pcall(TppMission.IsHelicopterSpace, missionCode)
    helicopterSpace = ok and (value == true or (type(value) == 'number' and value ~= 0))
end

if sequenceName == 'Seq_Game_AvatarEdit' then return 'avatar-edit' end
if title and missionCode == 10010 and sequenceName == 'Seq_Game_TitleMenu' then
    return 'title-hospital-menu'
end
if title and helicopterSpace then return 'title-cabin' end
-- Sortie equipment selection replaces the retail camera with a front-end
-- preview. Keep that complete native canvas on a spatial panel; applying the
-- seated first-person camera stretches its backdrop and separates the UI.
local sortieCanvas = sequenceName == 'Seq_Game_MissionPreparationTop'
    or sequenceName == 'Seq_Game_MissionPreparation_SelectItem'
    or sequenceName == 'Seq_Game_MissionPreparation_SelectSlot'
    or sequenceName == 'Seq_Game_MissionPreparation_SelectDetail'
if not title and helicopterSpace and sortieCanvas
        and queryBoolean(DemoDaemon, 'IsDemoPlaying') == false then
    return 'native-popup:sortie'
end

-- Rewards have their own BonusPopup controller, separate from both the
-- iDroid and IsShowPopup. A live foreground popup owns Confirm even when
-- the mission keeps its gameplay sequence or a background demo is active.
-- Only affirmative native UI queries can claim this owner; player status
-- and sequence names alone cannot create a modal menu.
local popup = not title and (queryBoolean(TppUiCommand, 'IsShowBonusPopup') == true
        or queryBoolean(TppUiCommand, 'IsShowPopup') == true)
local function presentation(owner)
    -- Keep the underlying cabin/cinematic owner across the modal boundary.
    -- Closing a reward must not reset a passenger's accepted walking offset.
    if popup then return 'native-popup:' .. owner end
    return owner
end

-- The prologue keeps this sequence from StartNameEntry through its native
-- NameEntryClose/fade callback. IsShowPopup is false while the name/birthday
-- form itself is open. It still owns a spatial menu and the authored bed
-- body; it must not become locomotion or ordinary wrist HUD.
if missionCode == 10010 and sequenceName == 'Seq_Game_FewDaysLater2' then
    return presentation('profile-menu')
end

-- These hospital lessons read PlayerVars.rightStickX/Y while an authored
-- scene can still be playing. They need native look input, not locomotion or
-- combat input. Match both the mission and exact interactive sequence; a
-- generic Seq_Game_* name does not make a cinematic interactive.
local lookLesson = missionCode == 10010 and
    (sequenceName == 'Seq_Game_FewDaysLater0' or sequenceName == 'Seq_Game_FewDaysLater1')
if lookLesson then
    local lesson = sequenceName == 'Seq_Game_FewDaysLater0' and '0' or '1'
    if title then return presentation('scripted-opening-look-' .. lesson) end
    return presentation('scripted-look-' .. lesson)
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
        if title then return presentation('scripted-look-title') end
        return presentation('scripted-look')
    end
    -- Hospital QuietAppear is isInGame=true and retains NORMAL_ACTION /
    -- PARTS_ACTIVE while its named demo still owns the bed camera and actors.
    -- Those flags admit background gameplay only outside an authored demo
    -- sequence. They cannot turn an active Seq_Demo_* shot into gameplay.
    if not title and not namedDemo and nonPlayableDemo == false then
        local normalAction = checkStatus('NORMAL_ACTION')
        local partsActive = checkStatus('PARTS_ACTIVE')
        if normalAction == true and partsActive == true then
            return presentation('closed')
        end
    end
    -- A cleared-save hospital opening plays a real native scene while retail
    -- retains TitleMode. It is distinct from startup notices and Press Start.
    -- A recorded movie never claims this 3D opening owner.
    if title and missionCode == 10010 and sequenceName == 'Seq_Demo_StartHasTitleMission'
            and queryBoolean(TppVideoPlayer, 'IsVideoStopped') == true then
        return presentation('scripted-opening-demo')
    end
    if title then return presentation('scripted-demo-title') end
    return presentation('scripted-demo')
end

-- A positive non-playable result means an authored/paused cinematic even if
-- the active-demo API has a transient false result.
if nonPlayableDemo == true then
    if title then return presentation('scripted-demo-title') end
    return presentation('scripted-demo')
end

if namedDemo then
    if title then return presentation('scripted-demo-title') end
    -- Never infer gameplay from a sequence name alone. Missing/error native
    -- queries, incomplete mission prep, or unavailable player-control state
    -- retain the conservative authored-demo presentation.
    local missionPrepared = queryBoolean(TppSequence, 'IsMissionPrepareFinished')
    local normalAction = checkStatus('NORMAL_ACTION')
    local partsActive = checkStatus('PARTS_ACTIVE')
    if activeDemo == false and nonPlayableDemo == false and missionPrepared == true
            and normalAction == true and partsActive == true and type(missionCode) == 'number' then
        return presentation('stale-demo-candidate:' .. tostring(missionCode) .. ':' .. sequenceName)
    end
    if title then return presentation('scripted-demo-title') end
    return presentation('scripted-demo')
end

-- An unavailable demo API is not evidence that an ordinary game sequence is
-- playable: demos can run inside Seq_Game_* names too. Stay conservative.
if activeDemo == nil or nonPlayableDemo == nil then
    if title then return presentation('scripted-demo-title') end
    return presentation('scripted-demo')
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
        return presentation('cabin-menu')
    end
    return presentation('cabin')
end
-- The mission keeps Seq_Game_MainGame on its failure screen. Check the
-- retail game-over state only after authored demos have retained ownership.
if queryBoolean(TppMission, 'IsGameOver') == true then return presentation('game-over') end
-- The streamed field helicopter is not the static ACC space. Resolve the
-- actual local passenger owner, rather than a stale vehicle variable alone.
-- Authored cinematics and loading have already kept their own owners above.
if checkStatus('PARTS_ACTIVE') == true and type(Player) == 'table'
        and type(Player.GetGameObjectIdIsRiddenToLocal) == 'function'
        and type(Tpp) == 'table' and type(Tpp.IsHelicopter) == 'function' then
    local ok, ridden = pcall(Player.GetGameObjectIdIsRiddenToLocal)
    if ok and type(ridden) == 'number' and ridden ~= 65535
            and type(vars) == 'table' and vars.playerVehicleGameObjectId == ridden then
        local kindOk, helicopter = pcall(Tpp.IsHelicopter, ridden)
        if kindOk and helicopter == true then return presentation('cabin-flight') end
    end
end
return presentation('closed')
