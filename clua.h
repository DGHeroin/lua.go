#ifndef _LUA_
#define _LUA_

#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

void c_initstate(lua_State* L);
void c_pushgofunction(lua_State* L, unsigned int fid);
void c_pushgostruct(lua_State* L, unsigned int fid);

int callback_function(lua_State* L);
#endif