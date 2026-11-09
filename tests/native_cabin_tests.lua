return function(source)
 local checks=0
 local function check(value,message) assert(value,message);checks=checks+1 end
 local function fixture()
  local e={mvars={},vars={missionCode=40050,buddyType=1},gvars={ini_isTitleMode=true},BuddyType={DOG=2}}
  local calls={load=0,call=0,clear=0,npc=true,loading=false,active=1,obtained=true,rats={},bird={},ratDataset=true,ratPosition=true,dogPosition=true,ratActivations=0}
  -- Native locator rotations do not expose GetY/GetW. A mock with those
  -- methods hid the first integration failure; require the game helper.
  local p={};local q={}
  e.TppMission={IsHelicopterSpace=function(m)return m==40050 end}
  e.Tpp={GetLocatorByTransform=function(a,b)check(a=='HelispaceLocatorIdentifier' and b=='BuddyDDogLocator','authored locator only');return p,q end,
   GetRotationY=function(rot)check(rot==q,'native rotation helper');return 180 end}
  e.TppBuddyService={DidObtainBuddyType=function()return calls.obtained end,CanSortieBuddyType=function()return true end,
   HeliSpaceSetting=function(pos,rot,kind)check(pos==p and rot==q and kind==2,'native locator passed intact') end}
  e.TppBuddy2BlockController={
   IsLoading=function()return calls.loading end,
   GetActiveBuddyType=function()return calls.active end,
   SetVarsBuddyType=function(kind)e.vars.buddyType=kind end,
   Load=function()calls.load=calls.load+1 end,
   CallBuddy=function(kind,pos,yaw)check(kind==2 and pos==p and math.abs(yaw-180)<.001,'native dog call parameters');calls.call=calls.call+1;return true end}
  e.TppGameStatus={IsSet=function()return calls.npc end,Set=function()calls.npc=true end,Reset=function()calls.npc=false end}
  e.GameObject={GetGameObjectId=function(name)check(name=='mgs5vr_cabin_rat_00' or name=='mgs5vr_cabin_rat_01','resolve native named rat locators');return calls.ratDataset and (name=='mgs5vr_cabin_rat_00' and 123 or 124) or 65535 end,
   GetGameObjectIdByIndex=function(kind,index)check(kind=='TppBuddyDog2' and index==0,'native dog slot only');return 456 end,
   GetPosition=function(id)
    if (id==456 and not calls.dogPosition) or (id~=456 and not calls.ratPosition) then return nil end
    return {GetX=function()return calls.positionX or 0 end,GetY=function()return 1197.3 end,GetZ=function()return 0 end}
   end,
   SendCommand=function(id,cmd)
    check(id.type=='TppRat' and id.index==0,'native rat group command')
    if cmd.id=='SetIgnoreDisableNpc' then return end
    check(cmd.name=='mgs5vr_cabin_rat_00' or cmd.name=='mgs5vr_cabin_rat_01','cabin locators only')
    if cmd.id=='Warp' then check(cmd.route=='rt_'..cmd.name and cmd.nodeIndex==0,'bounded native cabin route only')
    else check(cmd.id=='SetEnabled' and cmd.ratIndex==0,'native enable command only');calls.rats[cmd.name]=cmd.enabled end
   end}
  e.TppRatBird={RegisterBaseList=function(block)check(block[1]=='mgs5vr_cabin','register native cabin route scope')end,
   RegisterRat=function(names,routes)check(#names==2 and routes.mgs5vr_cabin[2].name=='rt_mgs5vr_cabin_rat_01','register both native bounded routes')end,
   EnableRat=function()calls.bird.enabled=true end,
   _Activate=function(block)check(block=='mgs5vr_cabin' and calls.bird.enabled,'activate through native rat lifecycle');check(e.mvars.rat_bird_enableBird==false,'do not activate unrelated birds');calls.ratActivations=calls.ratActivations+1;calls.rats.mgs5vr_cabin_rat_00=true;calls.rats.mgs5vr_cabin_rat_01=true end,
   _Deactivate=function()calls.rats.mgs5vr_cabin_rat_00=false;calls.rats.mgs5vr_cabin_rat_01=false end}
  local original=function(value)calls.clear=calls.clear+1;return value end
  e.title_sequence={ClearTitleMode=original,SetOnSelectFlag=function(flag)calls.selection=flag;return flag end}
  setmetatable(e,{__index=_G})
  local function run(requested)
   local fn=assert(loadstring('local requested='..tostring(requested)..'\nlocal nativeDogPositioned='..tostring(calls.dogPosition)..'\n'..source))
   setfenv(fn,e);return fn()
  end
  return e,calls,run,original
 end
 local e,c,r,original=fixture()
 check(r(false)=='idle' and c.load==0,'disabled must not mutate')
 c.obtained=false;check(r(true)=='native rats positioned; dog unavailable' and c.load==0,'rats spawn even without unlocking a dog')
 check(c.rats.mgs5vr_cabin_rat_00 and c.rats.mgs5vr_cabin_rat_01,'both rats spawn independently of the dog')
 c.obtained=true;check(r(true)=='dog block requested' and c.load==1,'request dog once')
 check(e.mvars.mgs5vr_cabinBuddyRestore.buddyType==1 and e.vars.buddyType==2,'capture original buddy before change')
 c.loading=true;check(r(true)=='loading dog block' and c.load==1 and c.call==0,'wait without repeated load')
 c.loading=false;c.active=2
 check(r(true)=='native rats positioned; native dog positioned at authored cabin locator' and not c.npc and c.call==1,'native activation')
 check(c.rats.mgs5vr_cabin_rat_00 and c.rats.mgs5vr_cabin_rat_01,'keep both native cabin rats active')
 check(r(true)=='native rats positioned; native dog positioned at authored cabin locator' and c.call==1,'no repeated actor placement')
 c.npc=true
 check(r(true)=='native rats positioned; native dog positioned at authored cabin locator' and not c.npc and c.call==1 and c.load==1,
  'a late native title stop must not freeze the loaded dog or reload him')
 check(e.title_sequence.ClearTitleMode('continued')=='continued','preserve native return')
 check(c.clear==1 and c.load==1 and e.vars.buddyType==1,'restore saved buddy choice without unloading cabin dog')
 check(c.rats.mgs5vr_cabin_rat_00 and c.rats.mgs5vr_cabin_rat_01,'keep rats visible during title-to-game loading')
 e.vars.missionCode=30010
 check(r(false)=='cabin actors retired after saved scene loaded' and c.load==1,'retire cabin only at destination scene boundary')
 check(e.title_sequence.ClearTitleMode==original and e.mvars.mgs5vr_cabinBuddyRestore==nil,'restore hook and discard snapshot after load')
 e,c,r=fixture();r(true);check(r(false)=='loading dog block' and e.vars.buddyType==2,'continue pending load through a closed title widget')
 e,c,r=fixture();r(true);e.vars.missionCode=30010
 check(r(false)=='cabin actors retired after saved scene loaded' and c.load==1 and e.vars.buddyType==1,
  'restore the saved buddy choice only after the destination scene arrives')
 e,c,r=fixture()
 local setter=e.title_sequence.SetOnSelectFlag
 r(true)
 check(e.title_sequence.SetOnSelectFlag(0)==1 and c.selection==1,'helicopter-title Continue uses native ACC branch')
 check(e.title_sequence.SetOnSelectFlag(2)==2 and c.selection==2,'New Game is not redirected')
 check(e.title_sequence.SetOnSelectFlag(3)==3 and c.selection==3,'MGO is not redirected')
 e.gvars.ini_isTitleMode=false
 check(e.title_sequence.SetOnSelectFlag(0)==0,'late non-title selection stays native')
 r(false)
 check(e.title_sequence.SetOnSelectFlag==setter,'retire the title handoff wrapper')
 e,c,r=fixture();e.vars.missionCode=10010;r(true)
 check(e.title_sequence.SetOnSelectFlag(0)==0,'hospital onboarding retains native Continue')
 -- Continue can close title before the asynchronous buddy block publishes.
 e,c,r=fixture();r(true);e.title_sequence.ClearTitleMode();e.gvars.ini_isTitleMode=false;c.active=2;c.dogPosition=false
 check(r(false)=='native rats positioned; waiting for native dog position' and c.call==1 and e.vars.buddyType==1,'finish pending load after Continue without changing restored buddy choice')
 c.npc=true;c.dogPosition=true
 check(r(false)=='native rats positioned; native dog positioned at authored cabin locator' and not c.npc and c.call==1,'maintain dog updates in the accepted ACC scene')
 local replacement=function()end;e.title_sequence.ClearTitleMode=replacement;e.vars.missionCode=30010;r(false)
 check(e.title_sequence.ClearTitleMode==replacement,'cleanup must not clobber a newer native hook')
 -- Registry IDs are insufficient; missing dataset/positions stay explicit.
 e,c,r=fixture();c.ratDataset=false;c.obtained=false
 check(r(true)=='native rats unavailable; dog unavailable' and c.ratActivations==0,'missing native rat dataset must not count as animals')
 e,c,r=fixture();c.ratPosition=false;c.obtained=false;e.mvars.rat_bird_enableBird=true
 check(r(true)=='native rats registered, position unverified; dog unavailable','registry success must not claim positioned animals')
 r(true);check(c.ratActivations==1 and e.mvars.rat_bird_enableBird==true,'wait without repeated Warp or changed bird state')
 c.ratPosition=true;check(r(true)=='native rats positioned; dog unavailable' and c.ratActivations==1,'recognize delayed native rat publication')
 e,c,r=fixture();c.positionX=0/0;c.dogPosition=false;c.active=2;r(true)
 check(r(true)=='native rats registered, position unverified; waiting for native dog position','nonfinite native positions are not actors')
 e,c,r=fixture();e.gvars.ini_isTitleMode=false
 check(r(true)=='idle' and c.load==0 and c.ratActivations==0,'ordinary ACC must not start a title buddy override')
 return 'native cabin isolated lifecycle checks passed: '..checks
end
