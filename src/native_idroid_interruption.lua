-- Called only while MGS5VR owns a UI-only handheld terminal. A native failure
-- screen or helicopter boarding must release its field camera/input owner.
-- A handset deliberately reopened on board belongs to that new presentation.
local failure = type(TppMission) == 'table' and type(TppMission.IsGameOver) == 'function'
        and TppMission.IsGameOver() == true
local boarding = false
if fieldHandset == true and type(Player) == 'table'
        and type(Player.GetGameObjectIdIsRiddenToLocal) == 'function'
        and type(Tpp) == 'table' and type(Tpp.IsHelicopter) == 'function'
        and type(vars) == 'table' then
    local ridden = Player.GetGameObjectIdIsRiddenToLocal()
    boarding = type(ridden) == 'number' and ridden ~= 65535
        and vars.playerVehicleGameObjectId == ridden and Tpp.IsHelicopter(ridden) == true
end
if not failure and not boarding then return 'unchanged' end
if type(TppUiCommand) ~= 'table'
        or type(TppUiCommand.IsMbDvcTerminalOpened) ~= 'function'
        or type(TppUiCommand.StopMbDvcTerminal) ~= 'function' then return 'unavailable' end
if TppUiCommand.IsMbDvcTerminalOpened() then TppUiCommand.StopMbDvcTerminal() end
if TppUiCommand.IsMbDvcTerminalOpened() then return 'pending' end
return boarding and not failure and 'boarding-closed' or 'closed'
