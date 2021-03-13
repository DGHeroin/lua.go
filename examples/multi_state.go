package main

import (
    "github.com/DGHeroin/lua.go"
    "log"
    "os"
)

func main()  {
    L := lua.NewState(nil)
    L.OpenLibs()
    L.OpenLibsExt()

    if err := L.DoFile(os.Args[1]); err != nil {
        log.Println(err)
    }

}
