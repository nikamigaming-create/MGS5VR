-- Called only while MGS5VR owns a UI-only handheld terminal. A native failure
-- screen or helicopter boarding must release its field camera/input owner.
-- A handset deliberately reopened on board belongs to that new presentation.
local failure = type(TppMission) == 'table' and type(TppMission.IsGameOver) == 'function'
        and TppMission.IsGameOver() == true
local boarding = false
local returnedToField = false
if type(Player) == 'table'
        and type(Player.GetGameObjectIdIsRiddenToLocal) == 'function'
        and type(Tpp) == 'table' and type(Tpp.IsHelicopter) == 'function'
        and type(vars) == 'table' then
    local ridden = Player.GetGameObjectIdIsRiddenToLocal()
    local aboard = type(ridden) == 'number' and ridden ~= 65535
        and vars.playerVehicleGameObjectId == ridden and Tpp.IsHelicopter(ridden) == true
    boarding = fieldHandset == true and aboard
    -- Keep a deliberately reopened cabin terminal through the exit animation,
    -- then return its ownership policy to the field. Otherwise the next pickup
    -- would inherit an indefinite exemption from boarding stow.
    returnedToField = fieldHandset == false and ridden == 65535
        and vars.playerVehicleGameObjectId == 65535
        and type(PlayerInfo) == 'table' and type(PlayerInfo.AndCheckStatus) == 'function'
        and type(PlayerStatus) == 'table' and PlayerStatus.NORMAL_ACTION ~= nil
        and PlayerInfo.AndCheckStatus{PlayerStatus.NORMAL_ACTION} == true
end
if not failure and returnedToField then return 'returned-to-field' end
if not failure and not boarding then return 'unchanged' end
if type(TppUiCommand) ~= 'table'
        or type(TppUiCommand.IsMbDvcTerminalOpened) ~= 'function'
        or type(TppUiCommand.StopMbDvcTerminal) ~= 'function' then return 'unavailable' end
if TppUiCommand.IsMbDvcTerminalOpened() then TppUiCommand.StopMbDvcTerminal() end
if TppUiCommand.IsMbDvcTerminalOpened() then return 'pending' end
return boarding and not failure and 'boarding-closed' or 'closed'
