package main

import (
    "github.com/DGHeroin/lua.go"
    "os"
)

func main()  {
    L := lua.NewState(nil)
    L.OpenLibs()
    L.OpenLibsExt()

    L.DoFile(os.Args[1])
    select {

    }
}
