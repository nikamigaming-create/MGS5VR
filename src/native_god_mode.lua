-- requested is supplied from the validated personal launcher settings.
-- Own one native status entry; never reset retail cinematic/tutorial owners.
local owner = 'MGS5VR_GodMode'
local status = 'S_DISABLE_PLAYER_DAMAGE'
if type(TppGameStatus) ~= 'table' or type(TppGameStatus.Set) ~= 'function'
        or type(TppGameStatus.Reset) ~= 'function' then return 'unavailable' end
local eligible = type(vars) == 'table' and type(vars.missionCode) == 'number'
        and vars.missionCode ~= 10010 and type(gvars) == 'table'
        and not gvars.ini_isTitleMode and type(TppMission) == 'table'
        and type(TppMission.IsFOBMission) == 'function'
        and TppMission.IsFOBMission(vars.missionCode) == false
local active = requested == true and eligible
if active then TppGameStatus.Set(owner, status)
else TppGameStatus.Reset(owner, status) end
return active and 'enabled' or 'disabled'
