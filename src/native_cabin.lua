-- Game-owned D-Dog lifecycle for the title helicopter only. No save, unlock,
-- synthetic mesh, or field actor placement. `requested` is supplied by C++.
if type(mvars)~='table' or type(title_sequence)~='table' then return 'unavailable' end
local scope='heli_common_sequence.lua'
local saved=mvars.mgs5vr_cabinBuddyRestore
local function cabinRats(enabled)
 -- These names exist only in our locally authored native title dataset.
 -- Never touch arbitrary mission rats or manufacture registry IDs.
 if type(GameObject)~='table' then return false end
 local names={'mgs5vr_cabin_rat_00','mgs5vr_cabin_rat_01'}
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
  TppRatBird._Activate(block)
 else
  if type(TppRatBird)=='table' then TppRatBird._Deactivate() end
  for _,name in ipairs(names) do
   GameObject.SendCommand({type='TppRat',index=0},{id='SetEnabled',name=name,ratIndex=0,enabled=false})
  end
 end
 return true
end
local function restore(s)
 local sameScene=vars.missionCode==s.mission
 if sameScene then
  cabinRats(false)
  for key,value in pairs(s.ratBirdState) do mvars[key]=value end
 end
 if s.dog and not s.dogSelectionRestored then
  TppBuddy2BlockController.SetVarsBuddyType(s.buddyType)
  s.dogSelectionRestored=true
 end
 if sameScene and s.dog and s.npcDisabled then TppGameStatus.Set(scope,'S_DISABLE_NPC') end
 if s.originalClear then title_sequence.ClearTitleMode=s.originalClear end
 mvars.mgs5vr_cabinBuddyRestore=nil
 mvars.mgs5vr_cabinNpcDisabled=nil
 return sameScene
end
local helicopter=TppMission.IsHelicopterSpace(vars.missionCode)
local inCabin=gvars.ini_isTitleMode and helicopter
if saved then
 if vars.missionCode~=saved.mission then restore(saved);return 'cabin actors retired after saved scene loaded' end
 if helicopter and (not gvars.ini_isTitleMode or not requested) then
  return gvars.ini_isTitleMode and 'cabin actors retained in helicopter' or 'cabin actors retained while selected game loads'
 end
end
if not requested or not inCabin then
 return 'idle'
end
if not saved then
 local ratBirdState={}
 for _,key in ipairs({'rat_bird_ratList','rat_bird_ratRouteList','rat_bird_enableRat','rat_bird_baseStrCodeList'}) do
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
 title_sequence.ClearTitleMode=function(...)
  local original=saved.originalClear
  if saved.dog and not saved.dogSelectionRestored then
   TppBuddy2BlockController.SetVarsBuddyType(saved.buddyType)
   saved.dogSelectionRestored=true
  end
  return original(...)
 end
end
if not saved.rats then saved.rats=cabinRats(true) end
if not TppBuddyService.DidObtainBuddyType(BuddyType.DOG)
 or not TppBuddyService.CanSortieBuddyType(BuddyType.DOG) then return 'native rats active; dog unavailable' end
local position,rotation=Tpp.GetLocatorByTransform('HelispaceLocatorIdentifier','BuddyDDogLocator')
if not position or not rotation then return 'native rats active; dog locator unavailable' end
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
 saved.phase='active'
 return 'native dog active at authored cabin locator'
end
-- Returning from a loaded mission can reapply the helicopter's NPC stop
-- after CallBuddy completes. Keep the already loaded title dog updating;
-- do not reload or reposition him, or touch another scene's stop flags.
if saved.phase=='active' and TppGameStatus.IsSet(scope,'S_DISABLE_NPC') then
 TppGameStatus.Reset(scope,'S_DISABLE_NPC')
end
return 'active'
