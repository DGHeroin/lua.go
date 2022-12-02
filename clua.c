
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdint.h>
#include <stdio.h>
#include "_cgo_export.h"

#define MT_GOFUNCTION "Lua.GoFunction"
#define MT_GOINTERFACE "Lua.GoInterface"

#define GOLUA_DEFAULT_MSGHANDLER "lua_default_msghandler"

static const char GoStateRegistryKey = 'k'; //golua registry key
static const char PanicFIDRegistryKey = 'k';

typedef struct _chunk {
	int size; // chunk size
	char *buffer; // chunk data
	char* toread; // chunk to read
} chunk;

/* taken from lua5.2 source */
void *testudata(lua_State *L, int ud, const char *tname)
{
	void *p = lua_touserdata(L, ud);
	if (p != NULL)
	{  /* value is a userdata? */
		if (lua_getmetatable(L, ud))
		{  /* does it have a metatable? */
			luaL_getmetatable(L, tname);  /* get correct metatable */
			if (!lua_rawequal(L, -1, -2))  /* not the same? */
				p = NULL;  /* value is a userdata with wrong metatable */
			lua_pop(L, 2);  /* remove both metatables */
			return p;
		}
	}
	return NULL;  /* value is not a userdata with a metatable */
}

int clua_isgofunction(lua_State *L, int n)
{
	return testudata(L, n, MT_GOFUNCTION) != NULL;
}

int clua_isgostruct(lua_State *L, int n)
{
	return testudata(L, n, MT_GOINTERFACE) != NULL;
}

unsigned int* clua_checkgosomething(lua_State* L, int index, const char *desired_metatable)
{
	if (desired_metatable != NULL)
	{
		return testudata(L, index, desired_metatable);
	}
	else
	{
		unsigned int *sid = testudata(L, index, MT_GOFUNCTION);
		if (sid != NULL) return sid;
		return testudata(L, index, MT_GOINTERFACE);
	}
}

size_t clua_getgostate(lua_State* L)
{
	size_t gostateindex;
	//get gostate from registry entry
	lua_pushlightuserdata(L,(void*)&GoStateRegistryKey);
	lua_gettable(L, LUA_REGISTRYINDEX);
	gostateindex = (size_t)lua_touserdata(L,-1);
	lua_pop(L,1);
	return gostateindex;
}


//wrapper for callgofunction
int callback_function(lua_State* L)
{
	int r;
	unsigned int *fid = clua_checkgosomething(L, 1, MT_GOFUNCTION);
	size_t gostateindex = clua_getgostate(L);
	//remove the go function from the stack (to present same behavior as lua_CFunctions)
	lua_remove(L,1);
	return golua_callgofunction(gostateindex, fid!=NULL ? *fid : -1);
}

//wrapper for gchook
int gchook_wrapper(lua_State* L)
{
	//printf("Garbage collection wrapper\n");
	unsigned int* fid = clua_checkgosomething(L, -1, NULL);
	size_t gostateindex = clua_getgostate(L);
	if (fid != NULL)
		return golua_gchook(gostateindex,*fid);
	return 0;
}

unsigned int clua_togofunction(lua_State* L, int index)
{
	unsigned int *r = clua_checkgosomething(L, index, MT_GOFUNCTION);
	return (r != NULL) ? *r : -1;
}

unsigned int clua_togostruct(lua_State *L, int index)
{
	unsigned int *r = clua_checkgosomething(L, index, MT_GOINTERFACE);
	return (r != NULL) ? *r : -1;
}

void clua_pushgofunction(lua_State* L, unsigned int fid)
{
	unsigned int* fidptr = (unsigned int *)lua_newuserdatauv(L, sizeof(unsigned int),0);
	*fidptr = fid;
	luaL_getmetatable(L, MT_GOFUNCTION);
	lua_setmetatable(L, -2);
}

static int callback_c (lua_State* L)
{
	int fid = clua_togofunction(L,lua_upvalueindex(1));
	size_t gostateindex = clua_getgostate(L);
	return golua_callgofunction(gostateindex,fid);
}

void clua_pushcallback(lua_State* L)
{
	lua_pushcclosure(L,callback_c,1);
}

void clua_pushgostruct(lua_State* L, unsigned int iid)
{
	unsigned int* iidptr = (unsigned int *)lua_newuserdatauv(L, sizeof(unsigned int),0);
	*iidptr = iid;
	luaL_getmetatable(L, MT_GOINTERFACE);
	lua_setmetatable(L,-2);
}

int default_panicf(lua_State *L)
{
	const char *s = lua_tostring(L, -1);
	printf("Lua unprotected panic: %s\n", s);
	abort();
}

void clua_setgostate(lua_State* L, size_t gostateindex)
{
	lua_atpanic(L, default_panicf);
	lua_pushlightuserdata(L,(void*)&GoStateRegistryKey);
	lua_pushlightuserdata(L, (void*)gostateindex);
	//set into registry table
	lua_settable(L, LUA_REGISTRYINDEX);
}

static int writer (lua_State *L, const void* b, size_t size, void* B) {
	static int count=0;
	(void)L;
	luaL_addlstring((luaL_Buffer*) B, (const char *)b, size);
	return 0;
}

// dump function chunk from luaL_loadstring
int dump_chunk (lua_State *L) {
	luaL_Buffer b;
	luaL_checktype(L, -1, LUA_TFUNCTION);
	lua_settop(L, -1);
	luaL_buffinit(L,&b);
	int _errno;
	_errno = lua_dump(L, writer, &b, 0);
	if (_errno != 0){
	return luaL_error(L, "unable to dump given function, errno:%d", _errno);
	}
	luaL_pushresult(&b);
	return 0;
}

static const char * reader (lua_State *L, void *ud, size_t *sz) {
	chunk *ck = (chunk *)ud;
	if (ck->size > LUAL_BUFFERSIZE) {
		ck->size -= LUAL_BUFFERSIZE;
		*sz = LUAL_BUFFERSIZE;
		ck->toread = ck->buffer;
		ck->buffer += LUAL_BUFFERSIZE;
	}else{
		*sz = ck->size;
		ck->toread = ck->buffer;
		ck->size = 0;
	}
	return ck->toread;
}

// load function chunk dumped from dump_chunk
int load_chunk(lua_State *L, char *b, int size, const char* chunk_name) {
	chunk ck;
	ck.buffer = b;
	ck.size = size;
	int _errno;
	_errno = lua_load(L,
		reader,
		&ck,
		chunk_name, NULL);
	if (_errno != 0) {
		return luaL_error(L, "unable to load chunk, errno: %d", _errno);
	}
	return 0;
}

/* called when lua code attempts to access a field of a published go object */
int interface_index_callback(lua_State *L)
{
	unsigned int *iid = clua_checkgosomething(L, 1, MT_GOINTERFACE);
	if (iid == NULL)
	{
		lua_pushnil(L);
		return 1;
	}

	char *field_name = (char *)lua_tostring(L, 2);
	if (field_name == NULL)
	{
		lua_pushnil(L);
		return 1;
	}

	size_t gostateindex = clua_getgostate(L);

	int r = golua_interface_index_callback(gostateindex, *iid, field_name);

	if (r < 0)
	{
		lua_error(L);
		return 0;
	}
	else
	{
		return r;
	}
}

/* called when lua code attempts to set a field of a published go object */
int interface_newindex_callback(lua_State *L)
{
	unsigned int *iid = clua_checkgosomething(L, 1, MT_GOINTERFACE);
	if (iid == NULL)
	{
		lua_pushnil(L);
		return 1;
	}

	char *field_name = (char *)lua_tostring(L, 2);
	if (field_name == NULL)
	{
		lua_pushnil(L);
		return 1;
	}

	size_t gostateindex = clua_getgostate(L);

	int r = golua_interface_newindex_callback(gostateindex, *iid, field_name);

	if (r < 0)
	{
		lua_error(L);
		return 0;
	}
	else
	{
		return r;
	}
}

int panic_msghandler(lua_State *L)
{
	size_t gostateindex = clua_getgostate(L);
	go_panic_msghandler(gostateindex, (char *)lua_tolstring(L, -1, NULL));
	return 0;
}

void clua_hide_pcall(lua_State *L)
{
//	lua_getglobal(L, "pcall");
//	lua_setglobal(L, "unsafe_pcall");
//	lua_pushnil(L);
//	lua_setglobal(L, "pcall");
//
//	lua_getglobal(L, "xpcall");
//	lua_setglobal(L, "unsafe_xpcall");
//	lua_pushnil(L);
//	lua_setglobal(L, "xpcall");
}

void clua_initstate(lua_State* L)
{
	/* create the GoLua.GoFunction metatable */
	luaL_newmetatable(L, MT_GOFUNCTION);

	// gofunction_metatable[__call] = &callback_function
	lua_pushliteral(L,"__call");
	lua_pushcfunction(L,&callback_function);
	lua_settable(L,-3);

	// gofunction_metatable[__gc] = &gchook_wrapper
	lua_pushliteral(L,"__gc");
	lua_pushcfunction(L,&gchook_wrapper);
	lua_settable(L,-3);
	lua_pop(L,1);

	luaL_newmetatable(L, MT_GOINTERFACE);

	// gointerface_metatable[__gc] = &gchook_wrapper
	lua_pushliteral(L, "__gc");
	lua_pushcfunction(L, &gchook_wrapper);
	lua_settable(L, -3);

	// gointerface_metatable[__index] = &interface_index_callback
	lua_pushliteral(L, "__index");
	lua_pushcfunction(L, &interface_index_callback);
	lua_settable(L, -3);

	// gointerface_metatable[__newindex] = &interface_newindex_callback
	lua_pushliteral(L, "__newindex");
	lua_pushcfunction(L, &interface_newindex_callback);
	lua_settable(L, -3);

	lua_register(L, GOLUA_DEFAULT_MSGHANDLER, &panic_msghandler);
	lua_pop(L, 1);
}


int callback_panicf(lua_State* L)
{
	lua_pushlightuserdata(L,(void*)&PanicFIDRegistryKey);
	lua_gettable(L,LUA_REGISTRYINDEX);
	unsigned int fid = lua_tointeger(L,-1);
	lua_pop(L,1);
	size_t gostateindex = clua_getgostate(L);
	return golua_callpanicfunction(gostateindex,fid);

}

//TODO: currently setting garbage when panicf set to null
GoInterface clua_atpanic(lua_State* L, unsigned int panicf_id)
{
	//get old panicfid
	unsigned int old_id;
	lua_pushlightuserdata(L, (void*)&PanicFIDRegistryKey);
	lua_gettable(L,LUA_REGISTRYINDEX);
	if(lua_isnil(L, -1) == 0)
		old_id = lua_tointeger(L,-1);
	lua_pop(L, 1);

	//set registry key for function id of go panic function
	lua_pushlightuserdata(L, (void*)&PanicFIDRegistryKey);
	//push id value
	lua_pushinteger(L, panicf_id);
	//set into registry table
	lua_settable(L, LUA_REGISTRYINDEX);

	//now set the panic function
	lua_CFunction pf = lua_atpanic(L,&callback_panicf);
	//make a GoInterface with a wrapped C panicf or the original go panicf
	if(pf == &callback_panicf)
	{
		return golua_idtointerface(old_id);
	}
	else
	{
		//TODO: technically UB, function ptr -> non function ptr
		return golua_cfunctiontointerface((GoUintptr *)pf);
	}
}

int clua_callluacfunc(lua_State* L, lua_CFunction f)
{
	return f(L);
}

void* allocwrapper(void* ud, void *ptr, size_t osize, size_t nsize)
{
	return (void*)golua_callallocf((GoUintptr)ud,(GoUintptr)ptr,osize,nsize);
}

lua_State* clua_newstate(void* goallocf)
{
	return lua_newstate(&allocwrapper,goallocf);
}

void clua_setallocf(lua_State* L, void* goallocf)
{
	lua_setallocf(L,&allocwrapper,goallocf);
}

void clua_openbase(lua_State* L)
{
	lua_pushcfunction(L,&luaopen_base);
	lua_pushstring(L,"");
	lua_call(L, 1, 0);
	clua_hide_pcall(L);
}

void clua_openio(lua_State* L)
{
	lua_pushcfunction(L,&luaopen_io);
	lua_pushstring(L,"io");
	lua_call(L, 1, 0);
}

void clua_openmath(lua_State* L)
{
	lua_pushcfunction(L,&luaopen_math);
	lua_pushstring(L,"math");
	lua_call(L, 1, 0);
}

void clua_openpackage(lua_State* L)
{
	lua_pushcfunction(L,&luaopen_package);
	lua_pushstring(L,"package");
	lua_call(L, 1, 0);
}

void clua_openstring(lua_State* L)
{
	lua_pushcfunction(L,&luaopen_string);
	lua_pushstring(L,"string");
	lua_call(L, 1, 0);
}

void clua_opentable(lua_State* L)
{
	lua_pushcfunction(L,&luaopen_table);
	lua_pushstring(L,"table");
	lua_call(L, 1, 0);
}

void clua_openos(lua_State* L)
{
	lua_pushcfunction(L,&luaopen_os);
	lua_pushstring(L,"os");
	lua_call(L, 1, 0);
}

void clua_register_lib(lua_State* L, lua_CFunction func, const char* name)
{
	luaL_requiref(L, name, func, 1);
	lua_pop(L, 1);
}

void clua_hook_function(lua_State *L, lua_Debug *ar)
{
	lua_checkstack(L, 2);
	lua_pushstring(L, "Lua execution quantum exceeded");
	lua_error(L);
}

void clua_setexecutionlimit(lua_State* L, int n)
{
	lua_sethook(L, &clua_hook_function, LUA_MASKCOUNT, n);
}

void clua_lua_insert(lua_State* L, int n) {
	lua_insert(L, n);
}

void clua_lua_remove(lua_State* L, int n) {
	lua_remove(L, n);
}

void clua_lua_replace(lua_State* L, int n) {
	lua_replace(L, n);
}

