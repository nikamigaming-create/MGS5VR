return function(godSource, interruptionSource, openSource)
    local checks = 0
    local function check(value, reason) assert(value, reason); checks = checks + 1 end
    local function run(source, env)
        local chunk = assert(loadstring(source))
        setfenv(chunk, env)
        return chunk()
    end
    local owners = {['TppDemo.lua'] = true}
    local env = {type = type, requested = true, vars = {missionCode = 10195},
        gvars = {ini_isTitleMode = false}, TppMission = {IsFOBMission = function() return false end},
        TppGameStatus = {
            Set = function(owner, status) check(status == 'S_DISABLE_PLAYER_DAMAGE', 'Only player damage is protected'); owners[owner] = true end,
            Reset = function(owner, status) check(status == 'S_DISABLE_PLAYER_DAMAGE', 'Only owned player-damage status is released'); owners[owner] = nil end}}
    check(run(godSource, env) == 'enabled' and owners.MGS5VR_GodMode, 'Single-player toggle applies native protection')
    env.requested = false
    check(run(godSource, env) == 'disabled' and not owners.MGS5VR_GodMode, 'Toggle Off releases its own protection')
    check(owners['TppDemo.lua'], 'Native cinematic protection remains')
    env.requested = true; env.TppMission.IsFOBMission = function() return true end
    check(run(godSource, env) == 'disabled' and not owners.MGS5VR_GodMode, 'FOB keeps retail damage policy')
    env.TppMission.IsFOBMission = function() return false end; env.gvars.ini_isTitleMode = true
    check(run(godSource, env) == 'disabled', 'Title cannot acquire protection')
    env.gvars.ini_isTitleMode = false; env.vars.missionCode = 10010
    check(run(godSource, env) == 'disabled', 'Hospital authored damage remains retail')
    env.vars.locationCode = 30; env.mvars = {doesPlayerVolginRide = true}
    local sequence = 'Seq_Game_VolginRide'
    env.TppSequence = {GetCurrentSequenceName = function() return sequence end}
    check(run(godSource, env) == 'enabled' and owners.MGS5VR_GodMode, 'Requested God mode protects the native horse escape')
    sequence = 'Seq_Game_GunTutorial'
    check(run(godSource, env) == 'disabled' and not owners.MGS5VR_GodMode, 'Returning to a hospital lesson releases only the mod owner')
    local open, stops, failure = true, 0, false
    local interruption = {type = type,
        TppMission = {IsGameOver = function() return failure end},
        TppUiCommand = {IsMbDvcTerminalOpened = function() return open end,
            StopMbDvcTerminal = function() stops = stops + 1; open = false end}}
    check(run(interruptionSource, interruption) == 'unchanged' and open and stops == 0, 'Ordinary iDroid is preserved')
    failure = true
    check(run(interruptionSource, interruption) == 'closed' and not open and stops == 1, 'Native mission failure stows the owned UI terminal')
    check(run(interruptionSource, interruption) == 'closed' and stops == 1, 'Repeated failure pump does not reopen or repeat Stop')
    interruption.TppMission.IsGameOver = nil
    check(run(interruptionSource, interruption) == 'unchanged' and stops == 1, 'Missing state cannot invent a mission failure')
    local ridden, vehicle, isHelicopter = 7168, 7168, true
    interruption.fieldHandset = true
    interruption.vars = {playerVehicleGameObjectId = vehicle}
    interruption.Player = {GetGameObjectIdIsRiddenToLocal = function() return ridden end}
    interruption.Tpp = {IsHelicopter = function(id) return id == vehicle and isHelicopter end}
    open = true
    check(run(interruptionSource, interruption) == 'boarding-closed' and not open and stops == 2,
        'Boarding the exact native helicopter stows a field-owned handset')
    open = true; interruption.fieldHandset = false
    check(run(interruptionSource, interruption) == 'unchanged' and open and stops == 2,
        'A fresh handset opened aboard remains available')
    interruption.fieldHandset = true; isHelicopter = false
    check(run(interruptionSource, interruption) == 'unchanged' and open,
        'Vehicle and horse rides do not acquire the helicopter stow policy')
    isHelicopter = true; ridden = 65535
    check(run(interruptionSource, interruption) == 'unchanged' and open,
        'A stale vehicle variable without a local passenger cannot stow the handset')
    ridden = vehicle + 1
    check(run(interruptionSource, interruption) == 'unchanged' and open,
        'Mismatched local passenger and vehicle identities cannot stow the handset')
    ridden = vehicle
    interruption.TppUiCommand.StopMbDvcTerminal = function() stops = stops + 1 end
    check(run(interruptionSource, interruption) == 'pending' and open,
        'Unacknowledged native Stop retains ownership for the next pump')
    local function opening(changes)
        local state = {posture = 'horse', sequence = 'Seq_Game_IDroidTutorial', mission = 10020,
            open = false, popup = false, saving = false, title = false, demo = false,
            prepared = true, helicopter = false, starts = 0}
        for key, value in pairs(changes or {}) do state[key] = value end
        local openingEnv = {type = type, vars = {missionCode = state.mission},
            gvars = {ini_isTitleMode = state.title},
            PlayerStatus = {NORMAL_ACTION = 'normal', ON_VEHICLE = 'vehicle', ON_HORSE = 'horse'},
            PlayerInfo = {AndCheckStatus = function(status) return status[1] == state.posture end},
            TppSave = {IsSaving = function() return state.saving end},
            DemoDaemon = {IsDemoPlaying = function() return state.demo end},
            TppMission = {IsHelicopterSpace = function(code)
                check(code == state.mission, 'Availability uses the actual mission'); return state.helicopter end},
            TppSequence = {IsMissionPrepareFinished = function() return state.prepared end,
                GetCurrentSequenceName = function() return state.sequence end},
            TppUiCommand = {IsMbDvcTerminalOpened = function() return state.open end,
                IsShowPopup = function() return state.popup end,
                StartMbDvcTerminal = function() state.starts = state.starts + 1; state.open = true end}}
        if state.unregisteredHorse then openingEnv.PlayerStatus.ON_HORSE = nil end
        if state.riddenHelicopter then
            openingEnv.vars.playerVehicleGameObjectId = 7168
            openingEnv.Player = {GetGameObjectIdIsRiddenToLocal = function()
                return state.mismatchedPassenger and 7169 or 7168 end}
            openingEnv.Tpp = {IsHelicopter = function(id) return id == 7168 end}
        end
        if state.missingStart then openingEnv.TppUiCommand.StartMbDvcTerminal = nil end
        local expectedSequence = state.sequence
        local result = run(openSource, openingEnv)
        check(openingEnv.vars.missionCode == state.mission and state.sequence == expectedSequence,
            'Opening cannot advance mission or tutorial state')
        return result, state
    end
    for _, posture in ipairs{'normal', 'vehicle', 'horse'} do
        local result, state = opening{posture = posture}
        check(result == 'opened' and state.open and state.starts == 1,
            'Normal, vehicle and native horse states admit the owned handset request')
    end
    local flightResult, flightState = opening{posture = 'other', riddenHelicopter = true}
    check(flightResult == 'opened-flight' and flightState.open and flightState.starts == 1,
        'A fresh request aboard the exact field helicopter opens a new handset lease')
    for _, changes in ipairs{{posture = 'other'}, {popup = true}, {saving = true}, {title = true},
            {demo = true}, {prepared = false}, {helicopter = true}, {mission = 10010},
            {sequence = 'Seq_Demo_ArrivalInAfghanistan'}, {unregisteredHorse = true},
            {missingStart = true}, {open = true},
            {posture = 'other', riddenHelicopter = true, mismatchedPassenger = true},
            {posture = 'other', riddenHelicopter = true, demo = true},
            {posture = 'other', riddenHelicopter = true, helicopter = true}} do
        local result, state = opening(changes)
        check(result == 'owner changed' and state.starts == 0,
            'Unavailable or changed native owner never starts the handset')
    end
    return 'Native mod policy contracts passed: ' .. checks
end
