#ifndef _LUA_EXT_LIB_H_
#define _LUA_EXT_LIB_H_
int luaopen_serialize(lua_State *L);
int luaopen_cmsgpack(lua_State *L);
int luaopen_pb(lua_State *L);
#endif