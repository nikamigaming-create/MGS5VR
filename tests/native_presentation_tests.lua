return function(source)
 local checks=0
 local function check(value,message) assert(value,message);checks=checks+1 end
 local function evaluate(options)
  local state={
   TppSequence={GetCurrentSequenceName=function()
    if options.sequenceError then error('sequence query failed') end
    return options.sequence
   end},
   TppMission={IsHelicopterSpace=function(code)
    check(code==(options.mission or 10010),'helicopter query uses the current native mission')
    if options.helicopterError then error('helicopter query failed') end
    return options.helicopter
   end},
   DemoDaemon=options.daemon or false,
   s10010_sequence=options.missionSequence or false,
   gvars={ini_isTitleMode=options.title or false},
   vars={missionCode=options.mission or 10010}
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
  missionSequence={IsDemoPlaying=function() return true end}})=='scripted-demo',
  'mission sequence demo flag catches the helicopter handoff')
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
  missionSequence={IsDemoPlaying=function() return false end}})=='closed',
  'native query errors fail safely without crashing the presentation check')
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
 return 'native presentation isolated checks passed: '..checks
end
