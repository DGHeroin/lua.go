package main

import (
    "github.com/DGHeroin/lua.go"
    "log"
    "time"
)

type User struct {
    L *lua.State
    ref int
    count int
}

func (u *User) serve() {
    idx := 0
    for {
        u.L.Lock()
        if err := u.L.CallX(u.ref, false, 0, idx); err != nil {
            log.Println(err)
        }
        u.count++
        u.L.Unlock()
        idx++
    }
}
func (u*User) Setcb(ref int) {
    u.ref = ref
    log.Println(ref)
}
func main() {
    L := lua.NewState()
    L.OpenLibs()
    L.OpenLibsExt()
    user := &User{L: L}
    L.RegisterGoStruct("user", user)
    L.DoString(`
print(user)
vv = 0
user.Setcb(function(val)
    vv = vv + 1
end)
`)
    for i := 0; i < 100; i++ {
        go user.serve()
    }
    lastVal := 0
    for {
        time.Sleep(time.Second)
        L.Lock()
        L.GetGlobal("vv")
        t := L.ToInteger(-1)
        log.Println(t - lastVal, user.count)
        user.count = 0
        lastVal = t
        L.Unlock()
    }

    L.WaitClose()
}
