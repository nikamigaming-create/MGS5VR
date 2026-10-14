return function(godSource, interruptionSource)
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
    return 'Native mod policy contracts passed: ' .. checks
end
