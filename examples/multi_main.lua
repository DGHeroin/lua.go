_LuaState.SetName('main')
gVar = 'i am main'
function _LuaStateMessage(cmd, data)
    print('in main', cmd, data)
end
local a = _LuaState.New()
a.OpenLibs()
a.OpenLibsExt()
a.DoFile('multi_a.lua')

local b = _LuaState.New()
b.OpenLibs()
b.OpenLibsExt()
b.DoFile('multi_b.lua')

local c = _LuaState.NewThread()
c.DoFile('multi_c.lua')

_LuaState.SendMessage('a', 1, 'hello')
_LuaState.SendMessage('b', 2, 'world')

_LuaState.WaitClose()