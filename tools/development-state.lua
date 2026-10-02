-- Read-only retail development facts for isolated menu/upgrade fixtures.
-- This identifies eligibility and completion, never the selected UI page.
-- API/argument contract verified in the supported owned retail executable:
-- IsEquipDevelopedFromDevelopID reads the numeric equipDevelopID table field.
local function quoted(value)
    return '"' .. tostring(value):gsub('\\', '\\\\'):gsub('"', '\\"'):gsub('\n', '\\n'):gsub('\r', '\\r') .. '"'
end
local function read(fn)
    local ok, value = pcall(fn)
    if not ok then value = 'unavailable:' .. tostring(value) end
    if type(value) == 'number' or type(value) == 'boolean' then return tostring(value) end
    return value == nil and 'null' or quoted(value)
end
local rows = {'"source":"retail_read_only_development"', '"page_identity":null', '"full_game_acceptance":false'}
local function field(name, fn) rows[#rows+1] = quoted(name) .. ':' .. read(fn) end
field('mission', function() return vars.missionCode end)
field('sequence', function() return TppSequence.GetCurrentSequenceName() end)
field('helicopter_space', function() return TppMission.IsHelicopterSpace(vars.missionCode) end)
field('development_menu_active', function() return TppUiCommand.IsMbTopMenuItemActive('MBM_DEVELOP') end)
field('gmp', function() return TppMotherBaseManagement.GetGmp() end)
field('developed_equip_count', function() return TppMotherBaseManagement.GetDevelopedEquipCount() end)
for _, resource in ipairs({'CommonMetal', 'MinorMetal', 'PreciousMetal', 'FuelResource', 'BioticResource'}) do
    field('resource_' .. resource, function() return TppMotherBaseManagement.GetResourceUsableCount{resource=resource} end)
end
-- Definition IDs recovered from the owned helicopter development inventory.
-- Grade is the definition's rank, not a development-progress measurement.
-- Requirements met can be true even when an item is already completed.
local helicopter_ids = {32000,32001,32002,32003,32004,32005,33001,34002,34003,34004,34005,34006,34007,35001,35002,35003,36001,36002,36003}
local helicopter = {}
for _, id in ipairs(helicopter_ids) do
    local item = {'"develop_id":' .. id}
    item[#item+1] = '"grade":' .. read(function() return TppMotherBaseManagement.GetEquipDevelopRank(id) end)
    item[#item+1] = '"developed":' .. read(function()
        local result = TppMotherBaseManagement.IsEquipDevelopedFromDevelopID{equipDevelopID=id}
        assert(type(result)=='boolean', 'native completion is not a boolean')
        return result
    end)
    item[#item+1] = '"requirements_met":' .. read(function()
        local result = TppMotherBaseManagement.IsEquipDevelopableWithDevelopID{equipDevelopID=id}
        assert(type(result)=='boolean', 'native eligibility is not a boolean')
        return result
    end)
    helicopter[#helicopter+1] = '{' .. table.concat(item, ',') .. '}'
end
rows[#rows+1] = '"helicopter":[' .. table.concat(helicopter, ',') .. ']'
return '{' .. table.concat(rows, ',') .. '}'
