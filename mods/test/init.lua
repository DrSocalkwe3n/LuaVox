-- parent = default:air
--
-- hasHalfTransparency
-- collideBox = {}
-- plantLike = {}
-- nodebox = {}

local node_template = {
    parent = "default:normal" or node_template,
    render = {
        has_half_transparency = false
    },
    collision = {

    },
    events = {

    },
    node_advancement_factory = function(world_id, node_pos)
    local node_advancement = {
        onLoad = function(data)

        end,
        onSave = function()
        return {}
        end
    }

    return node_advancement
    end

}

local instance = {}

--[[
Движок автоматически подгружает ассеты из папки assets
В этом методе можно зарегистрировать ассеты из иных источников
Состояния нод, частицы, анимации, модели, текстуры, звуки, шрифты
]]--
function instance.assetsInit()
end

--[[
*preInit. События для регистрации определений игрового контента
Ноды, воксели, миры, порталы, сущности, предметы
]]--
function instance.lowPreInit()
end

--[[
До вызова preInit будет выполнена регистрация
контента из файлов в папке content
]]--
function instance.preInit()
local node_air = {}

node_air.hasHalfTransparency = false
node_air.collideBox = nil
node_air.render = nil

core.register_node('test0', {})
core.register_node('test1', {})
core.register_node('test2', {})
core.register_node('test3', {})
core.register_node('test4', {})
core.register_node('test5', {})
end

function instance.highPreInit()
end

--[[
На этом этапе можно наложить изменения
на зарегистрированный другими модами контент
]]--
function instance.init()
end

function instance.postInit()
end

function instance.preDeInit()
end

function instance.deInit()
end

function instance.postDeInit()
core.unregister_node('test0')
core.unregister_node('test1')
core.unregister_node('test2')
core.unregister_node('test3')
core.unregister_node('test4')
core.unregister_node('test5')
end

return instance
