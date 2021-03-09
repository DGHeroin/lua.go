#include "clua.h"

#define MT_GOFUNCTION "Lua.GoFunction"
#define MT_GOINTERFACE "Lua.GoInterface"

extern int g_gofunction(lua_State* L, unsigned int fid);
extern int g_gogc(lua_State* L, unsigned int fid);
extern int g_getfield(lua_State* L, unsigned int fid, const char* fieldName);
extern int g_setfield(lua_State* L, unsigned int fid, const char* fieldName);

static int callback_function(lua_State* L);
static int gchook_gofunction(lua_State* L);

static int interface_index_callback(lua_State* L);
static int interface_newindex_callback(lua_State* L);
static int gchook_gointerface(lua_State* L);

static int l_traceback(lua_State *L);

void c_initstate(lua_State* L) {
    // GOFunction
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
    // utils func
    {
        lua_pushcfunction(L, l_traceback);
        lua_setglobal(L, "traceback");
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
unsigned int* check_go_ref(lua_State* L, int index, const char*mt) {
    void *p = lua_touserdata(L, index);
    if (p == NULL) { return 0; }
    if (lua_getmetatable(L, index)) {
        luaL_getmetatable(L, mt);
        if (!lua_rawequal(L, -1, -2)) {p = NULL;}
        lua_pop(L, 2);
    }
    return p;
}
static int callback_function(lua_State* L) {
    unsigned int* fidptr = check_go_ref(L, 1, MT_GOFUNCTION);
    if (fidptr == NULL) { return 0; }
    return g_gofunction(L, *fidptr);
}
static int gchook_gofunction(lua_State* L) {
    unsigned int* fidptr = check_go_ref(L, -1, MT_GOFUNCTION);
    if (fidptr == NULL) { return 0; }
    g_gogc(L, *fidptr);
    return 0;
}
static int gchook_gointerface(lua_State* L) {
    unsigned int* fidptr = check_go_ref(L, -1, MT_GOINTERFACE);
    if (fidptr == NULL) { return 0; }
    g_gogc(L, *fidptr);
    return 0;
}
static int interface_index_callback(lua_State* L) {
    unsigned int* fidptr = check_go_ref(L, 1, MT_GOINTERFACE);
    if (fidptr == NULL) { return 0; }

    char *field_name = (char *)lua_tostring(L, 2);
    if (field_name == NULL) {
        lua_pushnil(L);
        return 1;
    }
    return g_getfield(L, *fidptr, field_name);
}
static int interface_newindex_callback(lua_State* L) {
    unsigned int* fidptr = check_go_ref(L, 1, MT_GOINTERFACE);
    if (fidptr == NULL) { return 0; }
    char *field_name = (char *)lua_tostring(L, 2);
    if (field_name == NULL) {
        lua_pushnil(L);
        return 1;
    }
    return g_setfield(L, *fidptr, field_name);
}
int c_is_gofunction(lua_State *L, int n) {
    return check_go_ref(L, n, MT_GOFUNCTION) != NULL;
}
int c_is_gostruct(lua_State *L, int n) {
    return check_go_ref(L, n, MT_GOINTERFACE) != NULL;
}
unsigned int c_togofunction(lua_State* L, int index) {
    unsigned int *r = check_go_ref(L, index, MT_GOFUNCTION);
    return (r != NULL) ? *r : -1;
}
unsigned int c_togostruct(lua_State *L, int index) {
    unsigned int *r = check_go_ref(L, index, MT_GOINTERFACE);
    return (r != NULL) ? *r : -1;
}
static int callback_c (lua_State* L) {
    int fid = c_togofunction(L, lua_upvalueindex(1));
    return g_gofunction(L, fid);
}
void c_pushcallback(lua_State* L) {
    lua_pushcclosure(L, callback_c, 1);
}

void c_register_lib(lua_State* L, void* fn, const char* name) {
    lua_CFunction func = (lua_CFunction) fn;
    luaL_requiref(L, name, func, 1);
    lua_pop(L, 1);
}
static int l_traceback(lua_State *L) {
    size_t msgsz;
    const char *msg = luaL_checklstring(L, 1, &msgsz);
    int level = (int)luaL_optinteger(L, 2, 1);
    int max = (int)luaL_optinteger(L, 3, 20) + level;

    lua_Debug ar;
    int top = lua_gettop(L);
    if (msg)
        lua_pushfstring(L, "%s\n", msg);
    luaL_checkstack(L, 10, NULL);
    lua_pushliteral(L, "STACK TRACEBACK:");
    int n;
    const char *name;
    while (lua_getstack(L, level++, &ar)) {
        lua_getinfo(L, "Slntu", &ar);
        lua_pushfstring(L, "\n=> %s:%d in ", ar.short_src, ar.currentline);
        if (ar.name)
            lua_pushstring(L, ar.name);
        else if (ar.what[0] == 'm')
            lua_pushliteral(L, "mainchunk");
        else
            lua_pushliteral(L, "?");
        if (ar.istailcall)
            lua_pushliteral(L, "\n(...tail calls...)");
        lua_concat(L, lua_gettop(L) - top);     // <str>

        // varargs
        n = -1;
        while ((name = lua_getlocal(L, &ar, n--)) != NULL) {    // <str|value>
            lua_pushfstring(L, "\n    %s = ", name);        // <str|value|name>
            luaL_tolstring(L, -2, NULL);    // <str|value|name|valstr>
            lua_remove(L, -3);  // <str|name|valstr>
            lua_concat(L, lua_gettop(L) - top);     // <str>
        }

        // arg and local
        n = 1;
        while ((name = lua_getlocal(L, &ar, n++)) != NULL) {    // <str|value>
            if (name[0] == '(') {
                lua_pop(L, 1);      // <str>
            } else {
                if (n <= ar.nparams+1)
                    lua_pushfstring(L, "\n    param %s = ", name);      // <str|value|name>
                else
                    lua_pushfstring(L, "\n    local %s = ", name);      // <str|value|name>
                luaL_tolstring(L, -2, NULL);    // <str|value|name|valstr>
                lua_remove(L, -3);  // <str|name|valstr>
                lua_concat(L, lua_gettop(L) - top);     // <str>
            }
        }

        if (level > max)
            break;
    }
    lua_concat(L, lua_gettop(L) - top);
    return 1;
}