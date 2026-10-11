-- Called only while MGS5VR owns a UI-only handheld terminal. A native failure
-- screen must replace that terminal instead of retaining its stale skin/UI.
if type(TppMission) ~= 'table' or type(TppMission.IsGameOver) ~= 'function'
        or TppMission.IsGameOver() ~= true then return 'unchanged' end
if type(TppUiCommand) ~= 'table'
        or type(TppUiCommand.IsMbDvcTerminalOpened) ~= 'function'
        or type(TppUiCommand.StopMbDvcTerminal) ~= 'function' then return 'unavailable' end
if TppUiCommand.IsMbDvcTerminalOpened() then TppUiCommand.StopMbDvcTerminal() end
return TppUiCommand.IsMbDvcTerminalOpened() and 'pending' or 'closed'
