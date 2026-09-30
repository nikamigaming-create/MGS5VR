-- Read-only loaded soldiers and command-post phases. Unloaded enemies remain
-- explicitly unavailable, rather than fabricated at their spawn locations.
local rows = {}
local function quote(s) return '"' .. tostring(s):gsub('\\','\\\\'):gsub('"','\\"') .. '"' end
local definitions = mvars.ene_soldierDefine
assert(type(definitions) == 'table', 'Native soldier definitions unavailable')
local seen = {}
for camp, soldiers in pairs(definitions) do
    local phaseOk, phase = pcall(TppEnemy.GetPhase, camp)
    for _, name in ipairs(soldiers) do
        if type(name) == 'string' and not seen[name] then
            seen[name] = true
            local id = GameObject.GetGameObjectId('TppSoldier2', name)
            local ok, position = pcall(GameObject.SendCommand, id, {id='GetPosition'})
            local values = ok and position and TppMath.Vector3toTable(position) or nil
            local lifeOk, life = pcall(GameObject.SendCommand, id, {id='GetLifeStatus'})
            rows[#rows+1] = '{"name":'..quote(name)..',"camp":'..quote(camp)..',"id":'..tostring(id)
                ..',"phase":'..(phaseOk and quote(phase) or 'null')
                ..',"life":'..(lifeOk and quote(life) or 'null')
                ..',"position":'..(values and ('['..table.concat(values, ',')..']') or 'null')..'}'
        end
    end
end
return '{"mission":'..tostring(vars.missionCode)..',"location":'..tostring(vars.locationCode)
    ..',"normal_life":'..quote(TppEnemy.LIFE_STATUS.NORMAL)..',"soldiers":['..table.concat(rows,',')..']}'
