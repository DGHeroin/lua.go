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
void c_popgofunction(lua_State* L,void* ud);
void c_pushgostruct(lua_State* L, unsigned int fid);

int c_is_gofunction(lua_State *L, int n);
int c_is_gostruct(lua_State *L, int n);

unsigned int c_togofunction(lua_State* L, int index);
unsigned int c_togostruct(lua_State *L, int index);

void c_pushcallback(lua_State* L);

#endif