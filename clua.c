#include "clua.h"

#define MT_GOFUNCTION "Lua.GoFunction"
#define MT_GOINTERFACE "Lua.GoInterface"
unsigned int* clua_checkgosomething(lua_State* L, int index, const char *desired_metatable);
extern int g_callgofunction(lua_State* L, unsigned int fid);
extern int g_callgogc(lua_State* L, unsigned int fid);
extern int g_getfield(lua_State* L, unsigned int fid, const char* fieldName);
extern int g_setfield(lua_State* L, unsigned int fid, const char* fieldName);
int gchook_gofunction(lua_State* L);
int gchook_gointerface(lua_State* L);

int interface_index_callback(lua_State* L);
int interface_newindex_callback(lua_State* L);

void c_initstate(lua_State* L) {
    // 函数
    {
            luaL_newmetatable(L, MT_GOFUNCTION);

            lua_pushliteral(L,"__gc");
            lua_pushcfunction(L,&gchook_gofunction);
            lua_settable(L,-3);

            lua_pushliteral(L,"__call");
            lua_pushcfunction(L,&callback_function);
            lua_settable(L,-3);

            lua_pop(L,1);
    }


    // go struct
    {
        luaL_newmetatable(L, MT_GOINTERFACE);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, &gchook_gointerface);
        lua_settable(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, &interface_index_callback);
        lua_settable(L, -3);

        lua_pushliteral(L, "__newindex");
        lua_pushcfunction(L, &interface_newindex_callback);
        lua_settable(L, -3);

        lua_pop(L,1);
    }
}
void c_pushgofunction(lua_State* L, unsigned int fid) {
    unsigned int* fidptr = (unsigned int *)lua_newuserdata(L, sizeof(unsigned int));
    *fidptr = fid;
    luaL_getmetatable(L, MT_GOFUNCTION);
    lua_setmetatable(L, -2);
}
void c_pushgostruct(lua_State* L, unsigned int fid) {
    unsigned int* fidptr = (unsigned int *)lua_newuserdata(L, sizeof(unsigned int));
    *fidptr = fid;
    luaL_getmetatable(L, MT_GOINTERFACE);
    lua_setmetatable(L, -2);
}

unsigned int* check_go_ref_id(lua_State* L, int index, const char*mt) {
    void *p = lua_touserdata(L, index);
    if (p == NULL) { return 0; }
    if (lua_getmetatable(L, index)) {
        luaL_getmetatable(L, mt);
        if (!lua_rawequal(L, -1, -2)) {p = NULL;}
        lua_pop(L, 2);
    }
    return p;
}

int callback_function(lua_State* L) {
    unsigned int* fidptr = check_go_ref_id(L, 1, MT_GOFUNCTION);

    if (fidptr == NULL) { return 0; }
    return g_callgofunction(L, *fidptr);
}
int gchook_gofunction(lua_State* L) {
    unsigned int* fidptr = check_go_ref_id(L, -1, MT_GOFUNCTION);
    if (fidptr == NULL) { return 0; }
    g_callgogc(L, *fidptr);
    return 0;
}
int gchook_gointerface(lua_State* L) {
    unsigned int* fidptr = check_go_ref_id(L, -1, MT_GOINTERFACE);
    if (fidptr == NULL) { return 0; }
    g_callgogc(L, *fidptr);
    return 0;
}
int interface_index_callback(lua_State* L) {
    unsigned int* fidptr = check_go_ref_id(L, 1, MT_GOINTERFACE);
    if (fidptr == NULL) { return 0; }

    char *field_name = (char *)lua_tostring(L, 2);
    if (field_name == NULL) {
        lua_pushnil(L);
        return 1;
    }
    return g_getfield(L, *fidptr, field_name);
}

int interface_newindex_callback(lua_State* L) {
    unsigned int* fidptr = check_go_ref_id(L, 1, MT_GOINTERFACE);
    if (fidptr == NULL) { return 0; }
    char *field_name = (char *)lua_tostring(L, 2);
    if (field_name == NULL) {
        lua_pushnil(L);
        return 1;
    }
    return g_setfield(L, *fidptr, field_name);
}