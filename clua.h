#ifndef _LUA_
#define _LUA_

#include "inc.h"
#include "c-lib.h"

void c_initstate(lua_State* L);

// go function
void c_pushgofunction(lua_State* L, unsigned int fid);
int c_is_gofunction(lua_State *L, int n);
unsigned int c_togofunction(lua_State* L, int index);
// go struct
void c_pushgostruct(lua_State* L, unsigned int fid);
int c_is_gostruct(lua_State *L, int n);
unsigned int c_togostruct(lua_State *L, int index);
// others
void c_pushcallback(lua_State* L);
void c_register_lib(lua_State* L, void* fn, const char* name);
#endif