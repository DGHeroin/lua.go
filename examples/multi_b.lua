print("in b")
function _LuaStateMessage(cmd, data)
    print('in b', cmd, data)
    _LuaState.SendMessage('main', 2, 'i am ok')
end
_LuaState.SetName('b')