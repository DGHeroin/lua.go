package lua
/*
#include "clua.h"
 */
import "C"

const (
    LUA_MULTRET       = C.LUA_MULTRET
)

const (
    LUA_TNIL           = int(C.LUA_TNIL)
    LUA_TNUMBER        = int(C.LUA_TNUMBER)
    LUA_TBOOLEAN       = int(C.LUA_TBOOLEAN)
    LUA_TSTRING        = int(C.LUA_TSTRING)
    LUA_TTABLE         = int(C.LUA_TTABLE)
    LUA_TFUNCTION      = int(C.LUA_TFUNCTION)
    LUA_TUSERDATA      = int(C.LUA_TUSERDATA)
    LUA_TTHREAD        = int(C.LUA_TTHREAD)
    LUA_TLIGHTUSERDATA = int(C.LUA_TLIGHTUSERDATA)
)
