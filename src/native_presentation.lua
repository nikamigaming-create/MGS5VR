-- Return a compact, read-only native presentation state for the VR runtime.
-- Sequence names are useful as a fallback, but TPP also runs story demos
-- inside ordinary Seq_Game_* states such as AfterHeliDemo.
local function returnsTrue(owner, name)
    if type(owner) ~= 'table' then return false end
    local fn = owner[name]
    if type(fn) ~= 'function' then return false end
    local ok, value = pcall(fn)
    return ok and (value == true or (type(value) == 'number' and value ~= 0))
end

local sequenceName
if type(TppSequence) == 'table' and type(TppSequence.GetCurrentSequenceName) == 'function' then
    local ok, value = pcall(TppSequence.GetCurrentSequenceName)
    if ok and type(value) == 'string' then sequenceName = value end
end

local titleValue = type(gvars) == 'table' and gvars.ini_isTitleMode or false
local title = titleValue == true or (type(titleValue) == 'number' and titleValue ~= 0)
local missionCode = type(vars) == 'table' and vars.missionCode or nil
local helicopterSpace = false
if type(missionCode) == 'number' and type(TppMission) == 'table'
        and type(TppMission.IsHelicopterSpace) == 'function' then
    local ok, value = pcall(TppMission.IsHelicopterSpace, missionCode)
    helicopterSpace = ok and (value == true or (type(value) == 'number' and value ~= 0))
end

if sequenceName == 'Seq_Game_AvatarEdit' then return 'avatar-edit' end
if title and helicopterSpace then return 'title-cabin' end

local namedDemo = type(sequenceName) == 'string' and string.sub(sequenceName, 1, 9) == 'Seq_Demo_'
local activeDemo = returnsTrue(DemoDaemon, 'IsDemoPlaying')
    or returnsTrue(s10010_sequence, 'IsDemoPlaying')
if namedDemo or activeDemo then
    if title then return 'scripted-demo-title' end
    return 'scripted-demo'
end

if title then return 'title' end
if helicopterSpace then return 'cabin' end
return 'closed'
