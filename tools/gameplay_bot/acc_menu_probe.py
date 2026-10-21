"""Observe the native ACC destination initialization without save edits."""
from .core import BotFault, atomic_json


def inspect(live, seconds, policy):
    live.release()
    state=live.observe(native=True)
    n=state['native']
    if (state['scene']!='cabin' or n['mission']!=40050 or n['story']!=33
            or state['menu'] or n['popup'] or n['saving']):
        raise BotFault('ACC destination probe lost the reviewed unlocked native scene')
    result={'status':'running','before':state,'save_modified':False}
    try:
        script='''assert(vars.missionCode==40050 and not gvars.ini_isTitleMode)
assert(gvars.str_storySequence==33 and not TppUiCommand.IsShowPopup())
assert(not TppUiCommand.IsMbDvcTerminalOpened())
local story,gmp=gvars.str_storySequence,TppMotherBaseManagement.GetGmp()
local before=TppUiCommand.IsMbTopMenuItemVisible('MSN_LOCATION')
assert(type(TppTerminal.ReleaseFreePlay)=='function')
assert(story>=TppDefine.STORY_SEQUENCE.CLEARD_TO_MATHER_BASE)
TppTerminal.ReleaseFreePlay()
assert(story==gvars.str_storySequence and gmp==TppMotherBaseManagement.GetGmp())
local after=TppUiCommand.IsMbTopMenuItemVisible('MSN_LOCATION')
return '{"before_visible":'..tostring(before)..',"after_visible":'..tostring(after)..
 ',"story":'..tostring(story)..',"gmp":'..tostring(gmp)..'}' '''
        result['native'],result['transport']=live.native.read(script)
        result['after']=live.observe(native=True)
        result['status']='observed_native_destination_initialization'
        return result
    finally:
        live.release()
        atomic_json(live.events.output/'acc-menu-probe.json',result)
