-- Read-only loaded soldiers and command-post phases. Unloaded enemies remain
-- explicitly unavailable, rather than fabricated at their spawn locations.
local rows, bytes = {}, 0
-- The native action bridge admits at most 32768 result bytes. Keep this query
-- below that limit; never let a partial JSON response stand for perception.
local MAX_DEFINITIONS, MAX_ROW_BYTES = 8192, 24000
local function quote(s)
    return '"' .. tostring(s):gsub('\\','\\\\'):gsub('"','\\"')
        :gsub('[%z\1-\31]', function(c) return string.format('\\u%04x', string.byte(c)) end) .. '"'
end
local definitions = mvars.ene_soldierDefine
assert(type(definitions) == 'table', 'Native soldier definitions unavailable')
assert(TppEnemy.LIFE_STATUS.NORMAL~=nil,'Native normal life status unavailable')
local seen, entries = {}, {}
local definitionLimit = false
local function finite(v) return type(v)=='number' and v==v and v~=math.huge and v~=-math.huge end
for camp, soldiers in pairs(definitions) do
    for _, name in ipairs(soldiers) do
        if type(name) == 'string' and not seen[name] then
            if #entries >= MAX_DEFINITIONS then definitionLimit=true; break end
            seen[name] = true
            entries[#entries+1]={name=name,camp=camp}
        end
    end
    if definitionLimit then break end
end
table.sort(entries,function(a,b) return a.name<b.name end)
-- Stable definition order joins bounded pages without writing a cursor into
-- game state. The caller prefixes a local cursor into this Lua chunk.
local cursor=MGS5VR_NAV_QUERY_CURSOR or 0
assert(type(cursor)=='number' and cursor%1==0 and cursor>=0 and cursor<=#entries,'Invalid enemy page cursor')
local definitionIdentity=0
for _,entry in ipairs(entries) do
    local key=entry.name..'\0'..tostring(entry.camp)..'\0'
    for i=1,#key do definitionIdentity=(definitionIdentity*33+key:byte(i))%4294967296 end
end
local inspected, unloaded, unavailable, oversized = 0, 0, 0, false
local endCursor=cursor
local phases={}
for index=cursor+1,math.min(#entries,cursor+64) do
            local name,camp=entries[index].name,entries[index].camp
            if phases[camp]==nil then
                local ok,value=pcall(TppEnemy.GetPhase,camp)
                phases[camp]=ok and value~=nil and quote(value) or 'null'
            end
            local id = GameObject.GetGameObjectId('TppSoldier2', name)
            if type(id) ~= 'number' or id%1~=0 or id<=0 or id>=65535 or id==GameObject.NULL_ID then
                -- Most open-world definitions are not instantiated. Do not
                -- query NULL IDs or serialize thousands of unavailable rows.
                unloaded = unloaded + 1
            else
                local ok, position = pcall(GameObject.SendCommand, id, {id='GetPosition'})
                local vectorOk, values = false, nil
                if ok and position then vectorOk, values=pcall(TppMath.Vector3toTable,position) end
                local validPosition = vectorOk and type(values)=='table' and
                    finite(values[1]) and finite(values[2]) and finite(values[3])
                local lifeOk, life = pcall(GameObject.SendCommand, id, {id='GetLifeStatus'})
                if not validPosition or not lifeOk or life==nil then unavailable=unavailable+1 end
                local row = '{"name":'..quote(name)..',"camp":'..quote(camp)..',"id":'..tostring(id)
                    ..',"phase":'..phases[camp]
                    ..',"life":'..(lifeOk and life~=nil and quote(life) or 'null')
                    ..',"position":'..(validPosition and ('['..tostring(values[1])..','..
                        tostring(values[2])..','..tostring(values[3])..']') or 'null')..'}'
                if bytes+#row+1<=MAX_ROW_BYTES then
                    rows[#rows+1]=row; bytes=bytes+#row+1
                else
                    oversized=#rows==0
                    break
                end
            end
            inspected=inspected+1;endCursor=index
end
return '{"mission":'..tostring(vars.missionCode)..',"location":'..tostring(vars.locationCode)
    ..',"normal_life":'..quote(TppEnemy.LIFE_STATUS.NORMAL)..',"definitions_total":'..#entries
    ..',"definition_identity":'..definitionIdentity..',"definitions_inspected":'..inspected
    ..',"page_cursor":'..cursor..',"next_cursor":'..(endCursor<#entries and tostring(endCursor) or 'null')
    ..',"unloaded_count":'..unloaded..',"unavailable_loaded":'..unavailable
    ..',"truncated":'..tostring(definitionLimit or oversized)
    ..',"soldiers":['..table.concat(rows,',')..']}'
