-- Game-owned D-Dog lifecycle for the title helicopter only. No save, unlock,
-- synthetic mesh, or field actor placement. `requested` is supplied by C++.
if type(mvars)~='table' or type(title_sequence)~='table' then return 'unavailable' end
-- Retail title_sequence.lua: Continue selects 0, RestartHeli selects 1;
-- both enter Seq_Demo_CheckDlc and the normal native loading lifecycle.
-- Only the unlocked helicopter title gets the ACC landing policy. Hospital
-- onboarding, New Game (2), MGO (3), saves and native progression stay owned
-- by the game. Menu and cassette Continue dispatch through this same setter.
local handoff=mvars.mgs5vr_titleAccHandoff
if requested and gvars.ini_isTitleMode and TppMission.IsHelicopterSpace(vars.missionCode) then
 if not handoff and type(title_sequence.SetOnSelectFlag)=='function' then
  handoff={original=title_sequence.SetOnSelectFlag}
  handoff.wrapper=function(flag)
   if flag==0 and gvars.ini_isTitleMode and TppMission.IsHelicopterSpace(vars.missionCode) then flag=1 end
   return handoff.original(flag)
  end
  mvars.mgs5vr_titleAccHandoff=handoff
  title_sequence.SetOnSelectFlag=handoff.wrapper
 end
elseif handoff then
 if title_sequence.SetOnSelectFlag==handoff.wrapper then title_sequence.SetOnSelectFlag=handoff.original end
 mvars.mgs5vr_titleAccHandoff=nil
end
local scope='heli_common_sequence.lua'
local saved=mvars.mgs5vr_cabinBuddyRestore
local function positioned(object)
 if not object or object==65535 or type(GameObject.GetPosition)~='function' then return false end
 local ok,p=pcall(GameObject.GetPosition,object)
 if not ok or not p then return false end
 local valid,x,y,z=pcall(function() return p:GetX(),p:GetY(),p:GetZ() end)
 local function finite(v)return type(v)=='number' and v==v and math.abs(v)<math.huge end
 return valid and finite(x) and finite(y) and finite(z)
end
local function dogPositioned()
 -- TPP's generic GetPosition returns nil even for a visibly skinned buddy.
 -- C++ queries the game's same fresh native bones used by physical petting.
 return nativeDogPositioned==true
end
local ratNames={'mgs5vr_cabin_rat_00','mgs5vr_cabin_rat_01'}
local ratStateKeys={'rat_bird_ratList','rat_bird_ratRouteList','rat_bird_enableRat','rat_bird_baseStrCodeList'}
local function ratsPositioned()
 if type(GameObject)~='table' then return false end
 for _,name in ipairs(ratNames) do
  if not positioned(GameObject.GetGameObjectId(name)) then return false end
 end
 return true
end
local function cabinRats(enabled)
 -- These names exist only in our locally authored native title dataset.
 -- Never touch arbitrary mission rats or manufacture registry IDs.
 if type(GameObject)~='table' then return false end
 local names=ratNames
 for _,name in ipairs(names) do
  local object=GameObject.GetGameObjectId(name)
  if not object or object==65535 then return false end
 end
 GameObject.SendCommand({type='TppRat',index=0},{id='SetIgnoreDisableNpc',enabled=enabled})
 if enabled then
  if type(TppRatBird)~='table' then return false end
  local block='mgs5vr_cabin'
  local route={[block]={
   {name='rt_mgs5vr_cabin_rat_00',pos={-0.35,1197.305,-0.50}},
   {name='rt_mgs5vr_cabin_rat_01',pos={0.35,1197.305,-0.45}}
  }}
  -- Use the game's own animal-block lifecycle and native SetEnabled/Warp
  -- commands. The title has no streamed large-block name, so activate this
  -- locally registered cabin block explicitly.
  TppRatBird.RegisterBaseList({block})
  TppRatBird.RegisterRat(names,route)
  TppRatBird.EnableRat()
  -- The shared helper also activates birds. This dataset owns only rats.
  local birds=mvars.rat_bird_enableBird
  mvars.rat_bird_enableBird=false
  local ok,err=pcall(TppRatBird._Activate,block)
  mvars.rat_bird_enableBird=birds
  if not ok then error(err) end
 else
  for _,name in ipairs(names) do
   GameObject.SendCommand({type='TppRat',index=0},{id='SetEnabled',name=name,ratIndex=0,enabled=false})
  end
 end
 return true
end
local function restore(s)
 local sameScene=vars.missionCode==s.mission
 if sameScene then
  if s.ratsRegistered then cabinRats(false) end
  for _,key in ipairs(ratStateKeys) do mvars[key]=s.ratBirdState[key] end
 end
 if s.dog and not s.dogSelectionRestored then
  TppBuddy2BlockController.SetVarsBuddyType(s.buddyType)
  s.dogSelectionRestored=true
 end
 if sameScene and s.dog and s.npcDisabled then TppGameStatus.Set(scope,'S_DISABLE_NPC') end
 if s.originalClear and title_sequence.ClearTitleMode==s.clearWrapper then title_sequence.ClearTitleMode=s.originalClear end
 mvars.mgs5vr_cabinBuddyRestore=nil
 mvars.mgs5vr_cabinNpcDisabled=nil
 return sameScene
end
local helicopter=TppMission.IsHelicopterSpace(vars.missionCode)
local inCabin=gvars.ini_isTitleMode and helicopter
if saved then
 if vars.missionCode~=saved.mission then restore(saved);return 'cabin actors retired after saved scene loaded' end
 -- Closing Continue's widget is not cancellation of an accepted asynchronous
 -- buddy load. Finish that transaction in the same helicopter, even after
 -- ClearTitleMode. Do not initiate a new title override in ordinary ACC play.
 if helicopter and (not gvars.ini_isTitleMode or not requested) and not saved.phase then
  return gvars.ini_isTitleMode and 'cabin actors retained in helicopter' or 'cabin actors retained while selected game loads'
 end
end
if (not requested or not inCabin) and not (saved and saved.phase and helicopter) then
 return 'idle'
end
if not saved then
 local ratBirdState={}
 for _,key in ipairs(ratStateKeys) do
  ratBirdState[key]=mvars[key]
 end
 saved={mission=vars.missionCode,ratBirdState=ratBirdState}
 mvars.mgs5vr_cabinBuddyRestore=saved
end
if not saved.originalClear then
 saved.npcDisabled=mvars.mgs5vr_cabinNpcDisabled
 if saved.npcDisabled==nil then saved.npcDisabled=TppGameStatus.IsSet(scope,'S_DISABLE_NPC') end
 saved.originalClear=title_sequence.ClearTitleMode
 assert(type(saved.originalClear)=='function','native title exit unavailable')
 -- Restore the saved buddy choice before title exit without unloading the
 -- visible cabin dog. Keep both native actors until the destination scene is
 -- loaded; the lifecycle retires this cabin data set at the scene boundary.
 saved.clearWrapper=function(...)
  local original=saved.originalClear
  if saved.dog and not saved.dogSelectionRestored then
   TppBuddy2BlockController.SetVarsBuddyType(saved.buddyType)
   saved.dogSelectionRestored=true
  end
  return original(...)
 end
 title_sequence.ClearTitleMode=saved.clearWrapper
end
if not saved.ratsRegistered then saved.ratsRegistered=cabinRats(true) end
-- Keep registration separate from position publication: waiting for a model
-- must not Warp the animals again on every lifecycle tick.
local ratStatus=not saved.ratsRegistered and 'native rats unavailable; '
 or (ratsPositioned() and 'native rats positioned; ' or 'native rats registered, position unverified; ')
if not TppBuddyService.DidObtainBuddyType(BuddyType.DOG)
 or not TppBuddyService.CanSortieBuddyType(BuddyType.DOG) then return ratStatus..'dog unavailable' end
local position,rotation=Tpp.GetLocatorByTransform('HelispaceLocatorIdentifier','BuddyDDogLocator')
if not position or not rotation then return ratStatus..'dog locator unavailable' end
if not saved.dog then
 saved.buddyType=vars.buddyType
 saved.dog=true
end
if not saved.phase then
 if TppBuddy2BlockController.IsLoading() then return 'waiting for native buddy block' end
 TppBuddy2BlockController.SetVarsBuddyType(BuddyType.DOG)
 TppBuddy2BlockController.Load()
 saved.phase='loading'
 return 'dog block requested'
end
if saved.phase=='loading' then
 if TppBuddy2BlockController.IsLoading()
  or TppBuddy2BlockController.GetActiveBuddyType()~=BuddyType.DOG then return 'loading dog block' end
 local yaw=Tpp.GetRotationY(rotation)
 assert(type(yaw)=='number' and yaw==yaw,'native cabin rotation unavailable')
 TppBuddyService.HeliSpaceSetting(position,rotation,BuddyType.DOG)
 if not TppBuddy2BlockController.CallBuddy(BuddyType.DOG,position,yaw,true) then return 'native buddy call declined' end
 -- Title's blanket NPC stop prevents both actor updates and bone queries.
 -- Remove only the verified helicopter script's stop; keep all other gates.
 TppGameStatus.Reset(scope,'S_DISABLE_NPC')
 saved.phase='called'
end
-- Returning from a loaded mission can reapply the helicopter's NPC stop
-- after CallBuddy completes. Keep the already loaded title dog updating;
-- do not reload or reposition him, or touch another scene's stop flags.
if (saved.phase=='called' or saved.phase=='active') and TppGameStatus.IsSet(scope,'S_DISABLE_NPC') then
 TppGameStatus.Reset(scope,'S_DISABLE_NPC')
end
if dogPositioned() then
 saved.phase='active'
 return ratStatus..'native dog positioned at authored cabin locator'
end
return ratStatus..'waiting for native dog position'
