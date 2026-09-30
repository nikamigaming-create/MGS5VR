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
   end},
   DemoDaemon=daemon,
   TppDemo=tppDemo,
   s10010_sequence=options.missionSequence or false,
   gvars={ini_isTitleMode=options.title or false},
   vars={missionCode=options.mission or 10010},
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
   daemon={IsDemoPlaying=function() return true end}})=='scripted-look',
   'hospital look lessons remain interactive while an authored demo plays')
  check(evaluate({sequence=sequence,mission=10010,title=true})=='scripted-look-title',
   'hospital look lessons retain title presentation when its native flag is set')
  check(evaluate({sequence=sequence,mission=10020,
   daemon={IsDemoPlaying=function() return true end}})=='scripted-demo',
   'a sequence name from another mission cannot enable look-lesson input')
 end
 check(evaluate({sequence='Seq_Demo_FewDaysLater1_NG1',mission=10010,
  daemon={IsDemoPlaying=function() return true end}})=='scripted-demo',
  'the adjacent noninteractive hospital shot still suppresses gameplay input')
 check(evaluate({sequence='Seq_Game_Idle',mission=10020,
  missionSequence={IsDemoPlaying=function() error('requires an explicit demo-name list') end}})=='closed',
  'the prologue list-query is not called as a global no-argument demo flag')
 check(evaluate({sequence='Seq_Demo_StartHasTitleMission',title=1})=='scripted-demo-title',
  'named title demo remains a native title presentation')
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
  activeDemo=true,notPlayable=false,missionPrepared=true,normalAction=true,partsActive=true})=='closed',
  'an active demo explicitly marked playable admits gameplay only with affirmative control state')
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
 return 'native presentation isolated checks passed: '..checks
end
