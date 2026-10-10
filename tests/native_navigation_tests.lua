return function(source)
    local checks=0
    local function check(v,m) assert(v,m); checks=checks+1 end
    local function fixture(options)
        local definitions, ids, positions = {camp={}}, {}, {}
        for i=1,(options.total or 2000) do
            local name='soldier_'..i
            definitions.camp[#definitions.camp+1]=name
            ids[name]=i<=(options.loaded or 3) and i or 65535
            positions[i]={i,2,3}
        end
        definitions.camp[#definitions.camp+1]='soldier_1' -- duplicate definition
        if options.zero then ids.soldier_1=0 end
        if options.bad then positions[1]={0/0,2,3} end
        if options.escaped then
            local name='soldier_1\n"\\'
            definitions.camp[1]=name; ids[name]=1
        end
        local calls=0
        local env={
            vars={missionCode=30010,locationCode=10},mvars={ene_soldierDefine=definitions},
            TppEnemy={GetPhase=function() return 'normal' end,LIFE_STATUS={NORMAL='normal'}},
            GameObject={NULL_ID=65535,GetGameObjectId=function(_,name) return ids[name] end,
                SendCommand=function(id,command)
                    check(id>0 and id<65535,'unloaded IDs must not receive commands')
                    calls=calls+1
                    return command.id=='GetPosition' and positions[id] or 'normal'
                end},
            TppMath={Vector3toTable=function(v) return v end},
        }
        setmetatable(env,{__index=_G})
        local fn=assert(loadstring(source));setfenv(fn,env)
        return fn(),calls
    end
    local text,calls=fixture({})
    check(#text<32768,'large open-world definitions fit the native response')
    check(text:find('"definitions_total":2000',1,true),'definitions counted once')
    check(text:find('"definitions_inspected":64',1,true),'bounded first page')
    check(text:find('"next_cursor":64',1,true),'remaining definitions have an explicit cursor')
    check(text:find('"truncated":false',1,true),'complete loaded perception')
    check(calls>=2 and calls<=6,'only actual loaded enemies queried')
    local zero,zeroCalls=fixture({zero=true})
    check(zeroCalls==calls-2,'zero is not a valid actor')
    local bad=fixture({bad=true})
    check(bad:find('"unavailable_loaded":1',1,true) and bad:find('"position":null',1,true),
        'nonfinite loaded position is explicit, never a safe enemy location')
    local many=fixture({total=1000,loaded=1000})
    check(#many<32768 and many:find('"truncated":false',1,true) and many:find('"next_cursor":64',1,true),
        'many loaded enemies have a valid page, not a truncated response')
    local limit=fixture({total=9000,loaded=0})
    check(limit:find('"definitions_total":8192',1,true) and limit:find('"truncated":true',1,true),
        'definition enumeration is bounded and incomplete explicitly')
    local escaped=fixture({escaped=true})
    check(escaped:find('soldier_1\\u000a\\"\\\\',1,true),'native names use JSON escaping')
    return '{"checks":'..checks..',"status":"passed","large_snapshot":'..many..'}'
end
