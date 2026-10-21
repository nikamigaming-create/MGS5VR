return function(source)
 local checks=0
 local function check(value,message) assert(value,message);checks=checks+1 end
 local function evaluate(options)
  local daemon=options.daemon
  if daemon==nil then
   if options.daemonUnavailable then daemon=false
   else daemon={IsDemoPlaying=function() return options.activeDemo==true end} end
  end
  local tppDemo=options.tppDemo
  if tppDemo==nil then
   if options.notPlayableUnavailable then tppDemo=false
   else tppDemo={IsNotPlayable=function() return options.notPlayable==true end} end
  end
  local state={
   TppSequence={GetCurrentSequenceName=function()
    if options.sequenceError then error('sequence query failed') end
    return options.sequence
   end,IsMissionPrepareFinished=function()
    if options.prepareError then error('mission preparation query failed') end
    return options.missionPrepared==true
   end},
   TppMission={IsGameOver=function() return options.gameOver==true end,IsHelicopterSpace=function(code)
    check(code==(options.mission or 10010),'helicopter query uses the current native mission')
    if options.helicopterError then error('helicopter query failed') end
    return options.helicopter
   end,GetMissionClearState=function() return options.missionClearState end,
    IsDefiniteMissionClear=function() return options.definiteClear==true end},
   TppDefine={MISSION_CLEAR_STATE={MISSION_GAME_END=3,SHOW_CREDIT_END=4,BLACK_TELEPHOHE_END=5,REWARD_END=6}},
   DemoDaemon=daemon,
   TppDemo=tppDemo,
   TppUiCommand=options.ui or false,
   TppVideoPlayer=options.videoStopped==nil and false or {
    IsVideoStopped=function() return options.videoStopped end},
   s10010_sequence=options.missionSequence or false,
   gvars={ini_isTitleMode=options.title or false},
   vars={missionCode=options.mission or 10010,playerVehicleGameObjectId=options.vehicle},
   Player={GetGameObjectIdIsRiddenToLocal=function()
    if options.riddenError then error('rider owner unavailable') end
    return options.ridden
   end},
   Tpp={IsHelicopter=function(id)
    if options.typeError then error('vehicle type unavailable') end
    return id==options.ridden and options.riddenHelicopter==true
   end},
   PlayerStatus={NORMAL_ACTION=1,PARTS_ACTIVE=2},
   PlayerInfo={AndCheckStatus=function(status)
    if options.statusError then error('player status query failed') end
    if status[1]==1 then return options.normalAction==true end
    if status[1]==2 then return options.partsActive==true end
    return false
   end}
  }
  setmetatable(state,{__index=_G})
  local fn=assert(loadstring(source))
  setfenv(fn,state)
  return fn()
 end

 check(evaluate({sequence='Seq_Game_GameOverBeforeSmokeRoom',
  daemon={IsDemoPlaying=function() return true end}})=='scripted-demo',
  'active DemoDaemon classifies an ordinary game sequence as cinematic')
 check(evaluate({sequence='Seq_Game_AfterHeliDemo',
  daemon={IsDemoPlaying=function() return true end}})=='scripted-demo',
  'native demo query catches the helicopter handoff')
 for _,sequence in ipairs({'Seq_Game_FewDaysLater0','Seq_Game_FewDaysLater1'}) do
  check(evaluate({sequence=sequence,mission=10010,
   daemon={IsDemoPlaying=function() return true end}})=='scripted-look-'..sequence:sub(-1),
   'hospital look lessons remain interactive while an authored demo plays')
  check(evaluate({sequence=sequence,mission=10010,title=true})=='scripted-opening-look-'..sequence:sub(-1),
   'first-launch hospital lessons keep their native title and tracked 3D presentation')
  check(evaluate({sequence=sequence,mission=10020,
   daemon={IsDemoPlaying=function() return true end}})=='scripted-demo',
   'a sequence name from another mission cannot enable look-lesson input')
 end
 check(evaluate({sequence='Seq_Demo_FewDaysLater1_NG1',mission=10010,
  daemon={IsDemoPlaying=function() return true end}})=='scripted-demo',
  'the adjacent noninteractive hospital shot still suppresses gameplay input')
 check(evaluate({sequence='Seq_Game_FewDaysLater2',mission=10010})=='profile-menu',
  'the native hospital name/birthday form retains menu and authored-body ownership without a popup flag')
 check(evaluate({sequence='Seq_Game_FewDaysLater2',mission=10020})=='closed',
  'the hospital profile form cannot claim another mission')
 local finished={sequence='Seq_Demo_Bridge',mission=10010,missionClearState=3,
  definiteClear=true,videoStopped=true,normalAction=true,partsActive=true,missionPrepared=true,
  ui={IsResultBlockLoaded=function() return true end}}
 check(evaluate(finished)=='native-popup:mission-end','native mission-end UI owns the ended bridge scene and Confirm')
 finished.activeDemo=true
 check(evaluate(finished)=='scripted-demo','loaded result resources never replace an active native 3D scene')
 finished.activeDemo=false;finished.videoStopped=false
 check(evaluate(finished)~='native-popup:mission-end','a playing recorded movie keeps its cinematic owner')
 check(evaluate({sequence='Seq_Game_MissionPreparation_SelectItem',mission=40020,
  helicopter=true})=='native-popup:sortie','native sortie preview owns its complete spatial canvas')
 check(evaluate({sequence='Seq_Game_MissionPreparation_SelectItem',mission=40020,
  helicopter=true,activeDemo=true})=='scripted-demo','a real authored shot keeps ownership during sortie preparation')
 check(evaluate({sequence='Seq_Game_Idle',mission=10020,
  missionSequence={IsDemoPlaying=function() error('requires an explicit demo-name list') end}})=='closed',
  'the prologue list-query is not called as a global no-argument demo flag')
 check(evaluate({sequence='Seq_Demo_StartHasTitleMission',title=1})=='scripted-demo-title',
  'named title demo remains a native title presentation')
 check(evaluate({sequence='Seq_Game_TitleMenu',title=true,mission=10010})=='title-hospital-menu',
  'the witnessed first-launch hospital title menu owns its spatial VR panel')
 check(evaluate({sequence='Seq_Demo_StartHasTitleMission',title=true,mission=10010,
  activeDemo=true,videoStopped=true})=='scripted-opening-demo',
  'a witnessed native hospital opening is 3D despite retail TitleMode')
 check(evaluate({sequence='Seq_Demo_StartHasTitleMission',title=true,mission=10010,
  activeDemo=true,videoStopped=false})=='scripted-demo-title',
  'a recorded movie cannot claim the native 3D opening owner')
 check(evaluate({sequence='Seq_Game_AvatarEdit',title=true,
  daemon={IsDemoPlaying=function() return true end}})=='avatar-edit',
  'avatar editor state takes precedence over a demo flag')
 check(evaluate({sequence='Seq_Game_Helicopter',title=true,mission=40050,helicopter=true,
  daemon={IsDemoPlaying=function() return true end}})=='title-cabin',
  'title helicopter cabin takes precedence over authored demo state')
 check(evaluate({sequence='Seq_Game_Ordinary',daemon={IsDemoPlaying=function() return 1 end}})=='scripted-demo',
  'numeric native true values are handled without treating zero as true')
 check(evaluate({sequence='Seq_Demo_Fallback'})=='scripted-demo',
  'demo sequence name remains a fallback when native APIs are unavailable')
 check(evaluate({sequence='Seq_Game_Idle',daemon={IsDemoPlaying=function() return 0 end},
  missionSequence={IsDemoPlaying=function() return false end}})=='closed',
  'false and numeric zero do not leak cutscene state into gameplay')
 check(evaluate({sequenceError=true,daemon={IsDemoPlaying=function() error('unavailable') end},
  missionSequence={IsDemoPlaying=function() return false end}})=='scripted-demo',
  'native query errors fail safely and conservatively retain authored presentation')
 check(evaluate({sequence='Seq_Game_Helicopter',mission=40050,helicopter=true})=='cabin',
  'non-title helicopter scene remains a cabin state')
 for _,mission in ipairs({40010,40020,40060}) do
  check(evaluate({sequence='Seq_Game_Helicopter',title=true,mission=mission,helicopter=true})=='title-cabin',
   'returning to the title recognizes every native helicopter location')
 end
 check(evaluate({sequence='Seq_Game_Idle',title=true,mission=40010,helicopterError=true})=='title',
  'an unavailable helicopter query retains the ordinary title')
 check(evaluate({sequence='Seq_Game_Idle',title=true})=='title',
  'ordinary title presentation remains recognized')
 check(evaluate({sequence='Seq_Demo_Fallback',mission=10020,
  activeDemo=false,notPlayable=false})=='scripted-demo',
  'a stale demo name alone cannot release gameplay without positive control state')
 check(evaluate({sequence='Seq_Demo_Fallback',mission=10020,
  activeDemo=false,notPlayable=false,missionPrepared=true,normalAction=true,partsActive=true})
   =='stale-demo-candidate:10020:Seq_Demo_Fallback',
  'explicit inactive-demo and positive prepared player statuses create only a candidate')
 check(evaluate({sequence='Seq_Demo_Fallback',mission=10020,title=true,
  activeDemo=false,notPlayable=false,missionPrepared=true,normalAction=true,partsActive=true})=='scripted-demo-title',
  'title scenes cannot enter stale-demo recovery')
 check(evaluate({sequence='Seq_Demo_Fallback',mission=10020,
  activeDemo=true,notPlayable=false,missionPrepared=true,normalAction=true,partsActive=true})=='scripted-demo',
  'an active named authored sequence retains cinematic ownership despite ordinary player-status flags')
 check(evaluate({sequence='Seq_Demo_QuietAppear',mission=10010,
  activeDemo=true,notPlayable=false,normalAction=true,partsActive=true,videoStopped=true})=='scripted-demo',
  'the witnessed post-avatar hospital scene remains immersive despite its native isInGame classification')
 check(evaluate({sequence='Seq_Game_BackgroundNpcDemo',mission=10040,
  activeDemo=true,notPlayable=false,missionPrepared=true,normalAction=true,partsActive=true})=='closed',
  'an explicitly playable background demo retains gameplay when native player control is active')
 check(evaluate({sequence='Seq_Game_BackgroundNpcDemo',mission=10040,
  activeDemo=true,notPlayable=false,missionPrepared=true,normalAction=false,partsActive=true})=='scripted-demo',
  'playable demo classification alone cannot admit gameplay without normal player control')
 check(evaluate({sequence='Seq_Game_BackgroundNpcDemo',mission=10040,
  activeDemo=true,notPlayable=false,missionPrepared=true,normalAction=true,partsActive=false})=='scripted-demo',
  'playable demo classification cannot admit gameplay without active player parts')
 check(evaluate({sequence='Seq_Game_BackgroundNpcDemo',mission=10040,
  activeDemo=true,notPlayable=false,missionPrepared=true,normalAction=true,partsActive=true,
  notPlayableUnavailable=true})=='scripted-demo',
  'missing active-demo playability classification remains conservative')
 check(evaluate({sequence='Seq_Game_BackgroundNpcDemo',mission=10040,title=true,
  activeDemo=true,notPlayable=false,missionPrepared=true,normalAction=true,partsActive=true})=='scripted-demo-title',
  'title presentation remains protected even if player-status fixtures look playable')
 check(evaluate({sequence='Seq_Game_AuthoredDemo',mission=10040,
  activeDemo=true,notPlayable=true,missionPrepared=true,normalAction=true,partsActive=true})=='scripted-demo',
  'an active demo positively classified as non-playable remains authored presentation')
 check(evaluate({sequence='Seq_Demo_Fallback',mission=10020,
  activeDemo=false,notPlayable=true,missionPrepared=true,normalAction=true,partsActive=true})=='scripted-demo',
  'a paused or otherwise non-playable retail demo blocks stale recovery')
 check(evaluate({sequence='Seq_Demo_Fallback',mission=10020,
  daemonUnavailable=true,notPlayable=false,missionPrepared=true,normalAction=true,partsActive=true})=='scripted-demo',
  'missing native active-demo query remains conservative')
 check(evaluate({sequence='Seq_Demo_Fallback',mission=10020,
  activeDemo=false,notPlayableUnavailable=true,missionPrepared=true,normalAction=true,partsActive=true})=='scripted-demo',
  'missing retail playable-state query remains conservative')
 check(evaluate({sequence='Seq_Demo_Fallback',mission=10020,
  activeDemo=false,notPlayable=false,missionPrepared=true,normalAction=true,partsActive=false})=='scripted-demo',
  'missing affirmative parts-active state cannot promote the candidate')
 check(evaluate({sequence='Seq_Game_Idle',mission=10020,daemonUnavailable=true})=='scripted-demo',
  'unavailable demo APIs remain conservative inside ordinary game sequence names')
 for _,sequence in ipairs({'Seq_Game_MissionPreparationTop','Seq_Game_MissionPreparation_SelectItem',
  'Seq_Game_MissionPreparation_SelectSlot','Seq_Game_MissionPreparation_SelectDetail','Seq_Game_WeaponCustomize'}) do
  check(evaluate({sequence=sequence,mission=40010,helicopter=true})=='cabin-menu',
   'retail sortie and customization screens own VR menu input')
  check(evaluate({sequence=sequence,mission=10040,helicopter=false})=='closed',
   'a matching sequence outside helicopter space cannot create a sortie menu')
 end
 check(evaluate({sequence='Seq_Game_MissionPreparationEnd',mission=40010,helicopter=true})=='cabin',
  'the sortie menu releases its ownership for deployment')
 check(evaluate({sequence='Seq_Game_MainGame',mission=10040,gameOver=true})=='game-over',
  'native game-over menu owns UI even while the mission retains its gameplay sequence')
 check(evaluate({sequence='Seq_Game_MainGame',mission=10040,gameOver=true,activeDemo=true})=='scripted-demo',
  'authored death movies retain their cinematic presentation before the game-over menu')
 for _,query in ipairs({'IsShowBonusPopup','IsShowPopup'}) do
  for _,mission in ipairs({30010,30050,40020}) do
   for _,value in ipairs({true,1}) do
    check(evaluate({sequence='Seq_Game_MainGame',mission=mission,helicopter=mission==40020,
     ui={[query]=function() return value end}})==(mission==40020 and 'native-popup:cabin' or 'native-popup:closed'),
     'a positively visible native popup owns Confirm in the field, Mother Base and ACC')
   end
  end
  check(evaluate({sequence='Seq_Game_MainGame',mission=30050,activeDemo=true,notPlayable=true,
   ui={[query]=function() return true end}})=='native-popup:scripted-demo',
   'the foreground modal owns Confirm over a retained cinematic flag')
  for _,value in ipairs({false,0,'unknown'}) do
   check(evaluate({sequence='Seq_Game_MainGame',mission=30050,
    ui={[query]=function() return value end}})=='closed',
    'a false, zero or malformed UI query cannot claim gameplay')
  end
  check(evaluate({sequence='Seq_Game_MainGame',mission=30050,activeDemo=true,
   ui={[query]=function() error('UI unavailable') end}})=='scripted-demo',
   'a failing popup query preserves authored demo ownership')
 end
 check(evaluate({sequence='Seq_Game_Helicopter',mission=40020,helicopter=true,
  ui={IsShowBonusPopup=function() return false end}})=='cabin',
  'closing the reward popup restores cabin ownership')
 local transit={sequence='Seq_Game_FreePlay',mission=30010,partsActive=true,
  vehicle=7168,ridden=7168,riddenHelicopter=true}
 check(evaluate(transit)=='cabin-flight','a current native helicopter passenger enables moving cabin controls')
 transit.ui={IsShowBonusPopup=function() return true end}
 check(evaluate(transit)=='native-popup:cabin-flight','a modal preserves the moving cabin owner across Confirm')
 transit.ui=nil
 transit.vehicle=7169
 check(evaluate(transit)=='closed','a stale vehicle variable cannot enable cabin flight')
 transit.vehicle=7168;transit.riddenHelicopter=false
 check(evaluate(transit)=='closed','a wheeled vehicle keeps vehicle controls')
 transit.riddenHelicopter=true;transit.riddenError=true
 check(evaluate(transit)=='closed','missing passenger ownership cannot enable cabin flight')
 transit.riddenError=false;transit.typeError=true
 check(evaluate(transit)=='closed','missing vehicle type cannot enable cabin flight')
 transit.typeError=false;transit.activeDemo=true
 check(evaluate(transit)=='scripted-demo','authored helicopter cinematics retain native ownership')
 transit.activeDemo=false;transit.gameOver=true
 check(evaluate(transit)=='game-over','game-over UI has priority over helicopter walking')
 return 'native presentation isolated checks passed: '..checks
end
