-- The fresh, owned VR handset request is admitted by native_actions.cpp.
-- Retail still owns terminal availability and all tutorial progression.
if type(TppUiCommand.StartMbDvcTerminal) == 'function'
        and not TppUiCommand.IsMbDvcTerminalOpened()
        and not TppUiCommand.IsShowPopup() and not TppSave.IsSaving()
        and not gvars.ini_isTitleMode and not DemoDaemon.IsDemoPlaying()
        and TppSequence.IsMissionPrepareFinished()
        and not TppMission.IsHelicopterSpace(vars.missionCode)
        and vars.missionCode ~= 10010
        and TppSequence.GetCurrentSequenceName():match('^Seq_Game_')
        and (PlayerInfo.AndCheckStatus{PlayerStatus.NORMAL_ACTION}
            or PlayerInfo.AndCheckStatus{PlayerStatus.ON_VEHICLE}
            or (PlayerStatus.ON_HORSE ~= nil
                and PlayerInfo.AndCheckStatus{PlayerStatus.ON_HORSE})) then
    TppUiCommand.StartMbDvcTerminal()
    return 'opened'
end
return 'owner changed'
