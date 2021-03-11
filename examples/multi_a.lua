print("in a")
function _LuaStateMessage(cmd, data)
    print('in a', cmd, data)
    _LuaState.SendMessage('main', 1, 'i am ok')
end
_LuaState.SetName('a')

