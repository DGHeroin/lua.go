package lua

/*
#cgo CFLAGS: -I ${SRCDIR}/inc
#cgo CFLAGS: -I ${SRCDIR}/lua
#cgo windows,!llua LDFLAGS: -L${SRCDIR}/libs/windows -llua -lm -lws2_32
#cgo linux,!llua LDFLAGS: -L${SRCDIR}/libs/linux -llua -lm
#cgo darwin,!llua LDFLAGS: -L${SRCDIR}/libs/macos -llua -lm

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "clua.h"
*/
import "C"
import (
    "log"
    "reflect"
    "sync"
    "sync/atomic"
    "unsafe"
)

type (
    GoFunction        func(L *State) int
    GoFunctionContext struct {
        L    *State
        fun  GoFunction
        snd  interface{}
        name string
    }
    State struct {
        s          *C.lua_State
        registryId uint32
        registerM  sync.Mutex
        registry   map[uint32]interface{} // go object registry to uint32
        call       chan func()
    }
    Error struct {
        code       int
        message    string
        stackTrace []StackEntry
    }
    StackEntry struct {
        Name        string
        Source      string
        ShortSource string
        CurrentLine int
    }
)

func (e *Error) Error() string {
    return e.message
}

var (
    goStates      map[interface{}]*State
    goStatesMutex sync.Mutex
)

func init() {
    goStates = make(map[interface{}]*State, 16)
}
func newState(L *C.lua_State) *State {
    st := &State{
        s:          L,
        registryId: 0,
        registry:   make(map[uint32]interface{}),
        call:       make(chan func()),
    }
    registerGoState(st)
    C.c_initstate(st.s)
    go st.callbackHandle()
    return st
}
func registerGoState(L *State) {
    goStatesMutex.Lock()
    defer goStatesMutex.Unlock()
    goStates[L.s] = L
}
func unregisterGoState(L *State) {
    goStatesMutex.Lock()
    defer goStatesMutex.Unlock()
    delete(goStates, L.s)
}
func getGoState(L *C.lua_State) *State {
    goStatesMutex.Lock()
    defer goStatesMutex.Unlock()
    return goStates[L]
}

// Lua State
func NewState(L *C.lua_State) *State {
    if L == nil {
        L = (C.luaL_newstate())
        if L == nil {
            return nil
        }
    }
    return newState(L)
}
func (L *State) Close() {
    C.lua_close(L.s)
    unregisterGoState(L)
}
func (L *State) DoFile(filename string) error {
    if r := L.LoadFile(filename); r != 0 {
        return &Error{}
    }
    return L.Call(0, LUA_MULTRET)
}
func (L *State) LoadFile(filename string) int {
    Cfilename := C.CString(filename)
    defer C.free(unsafe.Pointer(Cfilename))
    return int(C.luaL_loadfilex(L.s, Cfilename, nil))
}
func (L *State) Type(idx int) int {
    return int(C.lua_type(L.s, C.int(idx)))
}
func (L *State) Call(nargs int, nresults int) (err error) {
    defer func() {
        if err2 := recover(); err2 != nil {
            if _, ok := err2.(error); ok {
                err = err2.(error)
            }
            return
        }
    }()

    r := C.lua_pcallk(L.s, C.int(nargs), C.int(nresults), 0, 0, nil)
    if r != 0 {
        return &Error{
            code:       int(r),
            message:    L.ToString(-1),
            stackTrace: L.StackTrace(),
        }
    }
    return nil
}
func (L *State) DoString(str string) error {
    if r := L.LoadString(str); r != 0 {
        return &Error{
            code:       r,
            message:    L.ToString(-1),
            stackTrace: L.StackTrace(),
        }
    }
    return L.Call(0, 0)
}
func (L *State) LoadString(str string) int {
    Cs := C.CString(str)
    defer C.free(unsafe.Pointer(Cs))
    return int(C.luaL_loadstring(L.s, Cs))
}
func (L *State) GetTop() int { return int(C.lua_gettop(L.s)) }
func (L *State) StackTrace() []StackEntry {
    r := []StackEntry{}
    var d C.lua_Debug
    Sln := C.CString("Sln")
    defer C.free(unsafe.Pointer(Sln))

    for depth := 0; C.lua_getstack(L.s, C.int(depth), &d) > 0; depth++ {
        C.lua_getinfo(L.s, Sln, &d)
        ssb := make([]byte, C.LUA_IDSIZE)
        for i := 0; i < C.LUA_IDSIZE; i++ {
            ssb[i] = byte(d.short_src[i])
            if ssb[i] == 0 {
                ssb = ssb[:i]
                break
            }
        }
        ss := string(ssb)

        r = append(r, StackEntry{C.GoString(d.name), C.GoString(d.source), ss, int(d.currentline)})
    }

    return r
}
func (L *State) OpenLibs() {
    C.luaL_openlibs(L.s)
}
func (L *State) register(f interface{}) uint32 {
    L.registerM.Lock()
    defer L.registerM.Unlock()
    id := atomic.AddUint32(&L.registryId, 1)
    if id == 0 { // 完成一次uint32
        id = atomic.AddUint32(&L.registryId, 1)
    }

    L.registry[id] = f
    return id
}
func (L *State) getRegister(id uint32) interface{} {
    L.registerM.Lock()
    defer L.registerM.Unlock()
    if p, ok := L.registry[id]; ok {
        return p
    }
    return nil
}
func (L *State) delRegister(id uint32) interface{} {
    L.registerM.Lock()
    defer L.registerM.Unlock()
    if p, ok := L.registry[id]; ok {
        delete(L.registry, id)
        return p
    }
    return nil
}
func (L *State) callbackHandle() {
    invoke := func(cb func()) {
        defer func() {
            recover()
        }()
        cb()
    }
    for {
        select {
        case cb := <-L.call:
            if cb != nil {
                invoke(cb)
            }
        }
    }
}
func (L *State) Run(cb func()) {
    defer func() {
        recover()
    }()
    L.call <- cb
}
func (L *State) registerLib(name string, fn unsafe.Pointer) {
    Sln := C.CString(name)
    defer C.free(unsafe.Pointer(Sln))
    C.c_register_lib(L.s, fn, Sln)
}
func (L *State) OpenLibsExt() {
    L.registerLib("serialize", C.luaopen_serialize)
    L.registerLib("cmsgpack", C.luaopen_cmsgpack)
    L.registerLib("pb", C.luaopen_pb)
}
func (L *State) Ref(t int) int {
    return int(C.luaL_ref(L.s, C.int(t)))
}
func (L *State) Unref(t int, ref int) {
    C.luaL_unref(L.s, C.int(t), C.int(ref))
}
func (L *State) RefRegistryIndex() int {
    return L.Ref(C.LUA_REGISTRYINDEX)
}
func (L *State) UnrefRegistryIndex(ref int) {
    L.Unref(C.LUA_REGISTRYINDEX, ref)
}
func (L*State) RawGetiRegistryIndex(ref int)  {
    L.RawGeti(C.LUA_REGISTRYINDEX, ref)
}
// Is
func (L *State) IsGoFunction(index int) bool { return C.c_is_gostruct(L.s, C.int(index)) != 0 }
func (L *State) IsGoStruct(index int) bool   { return C.c_is_gostruct(L.s, C.int(index)) != 0 }
func (L *State) IsBoolean(index int) bool    { return int(C.lua_type(L.s, C.int(index))) == LUA_TBOOLEAN }
func (L *State) IsLightUserdata(index int) bool {
    return int(C.lua_type(L.s, C.int(index))) == LUA_TLIGHTUSERDATA
}
func (L *State) IsNil(index int) bool       { return int(C.lua_type(L.s, C.int(index))) == LUA_TNIL }
func (L *State) IsNone(index int) bool      { return int(C.lua_type(L.s, C.int(index))) == LUA_TNONE }
func (L *State) IsNoneOrNil(index int) bool { return int(C.lua_type(L.s, C.int(index))) <= 0 }
func (L *State) IsNumber(index int) bool    { return C.lua_isnumber(L.s, C.int(index)) == 1 }
func (L *State) IsString(index int) bool    { return C.lua_isstring(L.s, C.int(index)) == 1 }
func (L *State) IsTable(index int) bool     { return int(C.lua_type(L.s, C.int(index))) == LUA_TTABLE }
func (L *State) IsThread(index int) bool    { return int(C.lua_type(L.s, C.int(index))) == LUA_TTHREAD }
func (L *State) IsUserdata(index int) bool  { return C.lua_isuserdata(L.s, C.int(index)) == 1 }
func (L *State) IsLuaFunction(index int) bool {
    return int(C.lua_type(L.s, C.int(index))) == LUA_TFUNCTION
}

// TO
func (L *State) ToBoolean(index int) bool {
    return int(C.lua_tointegerx(L.s, C.int(index), nil)) == 1
}
func (L *State) ToString(index int) string {
    var size C.size_t
    r := C.lua_tolstring(L.s, C.int(index), &size)
    return C.GoStringN(r, C.int(size))
}
func (L *State) ToBytes(index int) []byte {
    var size C.size_t
    b := C.lua_tolstring(L.s, C.int(index), &size)
    return C.GoBytes(unsafe.Pointer(b), C.int(size))
}
func (L *State) ToInteger(index int) int {
    return int(C.lua_tointegerx(L.s, C.int(index), nil))
}
func (L *State) ToNumber(index int) float64 {
    return float64(C.lua_tonumberx(L.s, C.int(index), nil))
}
func (L *State) ToGoFunction(index int) (f GoFunction) {
    if !L.IsGoFunction(index) {
        return nil
    }
    fid := uint32(C.c_togofunction(L.s, C.int(index)))
    if fid < 0 {
        return nil
    }
    ptr := L.getRegister(fid)
    if fn, ok := ptr.(GoFunction); ok {
        return fn
    }
    return nil
}
func (L *State) ToGoStruct(index int) (f interface{}) {
    if !L.IsGoStruct(index) {
        return nil
    }
    fid := uint32(C.c_togostruct(L.s, C.int(index)))
    if fid < 0 {
        return nil
    }
    return L.registry[fid]
}

// Push
func (L *State) PushString(str string) {
    Cstr := C.CString(str)
    defer C.free(unsafe.Pointer(Cstr))
    C.lua_pushlstring(L.s, Cstr, C.size_t(len(str)))
}
func (L *State) PushBytes(b []byte) {
    C.lua_pushlstring(L.s, (*C.char)(unsafe.Pointer(&b[0])), C.size_t(len(b)))
}
func (L *State) PushInteger(n int64) {
    C.lua_pushinteger(L.s, C.lua_Integer(n))
}
func (L *State) PushNil() {
    C.lua_pushnil(L.s)
}
func (L *State) PushNumber(n float64) {
    C.lua_pushnumber(L.s, C.lua_Number(n))
}
func (L *State) PushValue(index int) {
    C.lua_pushvalue(L.s, C.int(index))
}
func (L *State) PushGoFunction(f GoFunction) {
    id := L.register(f)
    C.c_pushgofunction(L.s, C.uint(id))
}
func (L *State) PushGoStruct(p interface{}) {
    id := L.register(p)
    C.c_pushgostruct(L.s, C.uint(id))
}
func (L *State) PushBoolean(b bool) {
    var bint int
    if b {
        bint = 1
    } else {
        bint = 0
    }
    C.lua_pushboolean(L.s, C.int(bint))
}
func (L *State) PushLightUserdata(ud *interface{}) {
    //push
    C.lua_pushlightuserdata(L.s, unsafe.Pointer(ud))
}
func (L *State) PushGoClosure(f GoFunction) {
    L.PushGoFunction(f)
    C.c_pushcallback(L.s)
}

// Global
func (L *State) RegisterFunction(name string, f GoFunction) {
    L.PushGoFunction(f)
    L.SetGlobal(name)
}
func (L *State) GC(what, data int) int { return int(C.lua_gc(L.s, C.int(what), C.int(data))) }
func (L *State) GetGlobal(name string) {
    CName := C.CString(name)
    defer C.free(unsafe.Pointer(CName))
    C.lua_getglobal(L.s, CName)
}
func (L *State) SetGlobal(name string) {
    Cname := C.CString(name)
    defer C.free(unsafe.Pointer(Cname))
    C.lua_setglobal(L.s, Cname)
}
func (L *State) RawGet(index int) {
    C.lua_rawget(L.s, C.int(index))
}
func (L *State) RawGeti(index int, n int) {
    C.lua_rawgeti(L.s, C.int(index), C.longlong(n))
}
func (L *State) RawSet(index int) {
    C.lua_rawset(L.s, C.int(index))
}
func (L *State) RawSeti(index int, n int) {
    C.lua_rawseti(L.s, C.int(index), C.longlong(n))
}

// Table
func (L *State) NewTable() {
    C.lua_createtable(L.s, 0, 0)
}
func (L *State) GetField(index int, k string) {
    Ck := C.CString(k)
    defer C.free(unsafe.Pointer(Ck))
    C.lua_getfield(L.s, C.int(index), Ck)
}

//export g_gofunction
func g_gofunction(L *C.lua_State, fid uint32) int {
    L1 := getGoState(L)
    if fid < 0 {
        return 0
    }
    obj := L1.getRegister(fid)
    if obj == nil {
        return 0
    }
    fn, ok := obj.(GoFunction)
    if !ok {
        return 0
    }
    return fn(L1)
}

//export g_gogc
func g_gogc(L *C.lua_State, fid uint32) int {
    L1 := getGoState(L)
    if fid < 0 {
        return 0
    }
    L1.delRegister(fid)
    return 0
}

//export g_getfield
func g_getfield(L *C.lua_State, fid uint32, fieldName *C.char) int {
    L1 := getGoState(L)
    if fid < 0 {
        return 0
    }
    obj := L1.getRegister(fid)
    if obj == nil {
        return 0
    }
    name := C.GoString(fieldName)
    ele := reflect.ValueOf(obj).Elem()
    fval := ele.FieldByName(name)
    if fval.Kind() == reflect.Ptr {
        fval = fval.Elem()
    }

    switch fval.Kind() {
    case reflect.Bool:
        L1.PushBoolean(fval.Bool())
        return 1
    case reflect.String:
        L1.PushString(fval.String())
        return 1
    case reflect.Int, reflect.Int8, reflect.Int16, reflect.Int32, reflect.Int64:
        L1.PushInteger(fval.Int())
        return 1
    case reflect.Uint, reflect.Uint8, reflect.Uint16, reflect.Uint32, reflect.Uint64:
        L1.PushInteger(int64(fval.Uint()))
        return 1
    case reflect.Float32, reflect.Float64:
        L1.PushNumber(fval.Float())
        return 1
    case reflect.Slice:
        L1.PushBytes(fval.Bytes())
        return 1
    default:
        // todo 检查是否是函数
        fval = reflect.ValueOf(obj).MethodByName(name)
        if fval.Kind() == reflect.Func {
            // 生成一个函数
            L1.PushGoFunction(L1.makeFunc(obj, name, fval))
            return 1
        }
        return 0
    }
}
func (L *State) makeFunc(sender interface{}, funcName string, value reflect.Value) GoFunction {
    return func(L *State) int {
        t := value.Type()
        var (
            inArgs  []reflect.Value
            outArgs []reflect.Value
        )

        for i := 0; i < t.NumIn(); i++ {
            idx := i + 2
            luatype := L.Type(idx)
            k := t.In(i).Kind()

            switch k {
            case reflect.Bool:
                if luatype != LUA_TNUMBER {
                    return 0
                }
                inArgs = append(inArgs, reflect.ValueOf(bool(L.ToInteger(idx) == 1)))
            case reflect.String:
                if luatype != LUA_TSTRING {
                    return 0
                }
                inArgs = append(inArgs, reflect.ValueOf(L.ToString(idx)))
            case reflect.Int, reflect.Int8, reflect.Int16, reflect.Int32, reflect.Int64:
                if luatype == LUA_TNUMBER {
                    inArgs = append(inArgs, reflect.ValueOf(L.ToInteger(idx)))
                } else if luatype == LUA_TFUNCTION {
                    ref := L.RefRegistryIndex()
                    inArgs = append(inArgs, reflect.ValueOf(ref))
                }
            case reflect.Uint, reflect.Uint8, reflect.Uint16, reflect.Uint32, reflect.Uint64:
                if luatype == LUA_TNUMBER {
                    inArgs = append(inArgs, reflect.ValueOf(uint64(L.ToInteger(idx))))
                } else {
                    return 0
                }
            case reflect.Float32, reflect.Float64:
                if luatype != LUA_TNUMBER {
                    return 0
                }
                inArgs = append(inArgs, reflect.ValueOf(L.ToNumber(idx)))
            case reflect.Interface:
                if luatype != LUA_TUSERDATA {
                    return 0
                }
                inArgs = append(inArgs, reflect.ValueOf(L.ToGoStruct(idx)))
            case reflect.Ptr:
                inArgs = append(inArgs, reflect.ValueOf(L.ToGoStruct(idx)))
            case reflect.Slice:
                if luatype != LUA_TSTRING {
                    return 0
                }
                inArgs = append(inArgs, reflect.ValueOf(L.ToBytes(idx)))
            default:
                log.Println("参数不支持", k)
                return 0
            }
        }

        // 输入参数完备
        outArgs = value.Call(inArgs)
        n := 0
        //
        for i, fval := range outArgs {
            switch fval.Kind() {
            case reflect.Bool:
                L.PushBoolean(fval.Bool())
                n++
            case reflect.String:
                L.PushString(fval.String())
                n++
            case reflect.Int, reflect.Int8, reflect.Int16, reflect.Int32, reflect.Int64:
                L.PushInteger(fval.Int())
                n++
            case reflect.Uint, reflect.Uint8, reflect.Uint16, reflect.Uint32, reflect.Uint64:
                L.PushInteger(int64(fval.Uint()))
                n++
            case reflect.Float32, reflect.Float64:
                L.PushNumber(fval.Float())
                n++
            case reflect.Slice:
                L.PushBytes(fval.Bytes())
                n++
            case reflect.Interface:
                L.PushGoStruct(fval.Interface())
                n++
            default:
                log.Printf("第%d个返回值不支持(%v)", i, fval.Kind())
            }
        }
        return n
    }
}

//export g_setfield
func g_setfield(L *C.lua_State, fid uint32, fieldName *C.char) int {
    L1 := getGoState(L)
    if fid < 0 {
        return 0
    }
    obj := L1.getRegister(fid)
    if obj == nil {
        return 0
    }
    name := C.GoString(fieldName)
    vobj := reflect.ValueOf(obj)
    ele := vobj.Elem()
    fval := ele.FieldByName(name)

    if fval.Kind() == reflect.Ptr {
        fval = fval.Elem()
    }
    luatype := int(C.lua_type(L1.s, 3))
    switch fval.Kind() {
    case reflect.Bool:
        if luatype == LUA_TBOOLEAN {
            fval.SetBool(int(C.lua_toboolean(L1.s, 3)) != 0)
            return 1
        }
        return 0
    case reflect.String:
        if luatype == LUA_TSTRING {
            fval.SetString(C.GoString(C.lua_tolstring(L1.s, 3, nil)))
            return 1
        }
        return 0
    case reflect.Int, reflect.Int8, reflect.Int16, reflect.Int32, reflect.Int64:
        if luatype == LUA_TNUMBER {
            fval.SetInt(int64(C.lua_tointegerx(L1.s, 3, nil)))
            return 1
        }
        return 0
    case reflect.Uint, reflect.Uint8, reflect.Uint16, reflect.Uint32, reflect.Uint64:
        if luatype == LUA_TNUMBER {
            fval.SetUint(uint64(C.lua_tointegerx(L1.s, 3, nil)))
            return 1
        }
        return 0
    case reflect.Float32, reflect.Float64:
        if luatype == LUA_TNUMBER {
            fval.SetFloat(float64(C.lua_tonumberx(L1.s, 3, nil)))
            return 1
        }
        return 0
    case reflect.Slice:
        var typeOfBytes = reflect.TypeOf([]byte(nil))
        if luatype == LUA_TSTRING && fval.Type() == typeOfBytes {
            fval.SetBytes(L1.ToBytes(3))
            return 1
        }
        return 0
    default:
        return 0
    }
}

// GoFunctionContext
func (f *GoFunctionContext) Invoke() {

}
