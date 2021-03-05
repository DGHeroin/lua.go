package main

import (
    "github.com/DGHeroin/lua.go"
    "log"
)

type Dog struct {
    Name string
    Age  int
}

func (d *Dog) Say(name string, age int) (string, string, int) {
    log.Println("dog say in go", d.Name, name, age)
    return "kiss", d.Name, d.Age
}

func main() {
    L := lua.NewState()
    L.OpenLibs()

    L.RegisterFunction("GGFunc", func(L *lua.State) int {
        log.Println("call GG.......")
        return 0
    })

    L.PushGoStruct(&Dog{Name: "我是狗1"})
    L.SetGlobal("Dog")

    L.PushGoStruct(&Dog{Name: "我是狗2"})
    L.SetGlobal("Dog2")

//    if err := L.DoString(`
//    function Say(name)
//        print('my name is...', name)
//    end
//    GGFunc('aaa')
//    GGFunc = nil
//    collectgarbage("collect") -- gc 'GGFunc'
//
//    print("before:", Dog.Name, Dog.Age)
//    Dog.Name = '我是猫1'
//    Dog.Age = 5
//    print("after:", Dog.Name, Dog.Age)
//    print(Dog.Say('aaa', 1234))
//    print(Dog2.Say('bbb', 1234))
//`); err != nil {
//        log.Println(err)
//    }
    if err := L.DoFile("examples/test.lua"); err != nil {
        log.Println(err)
    }
    L.GetGlobal("Say")
    L.PushString("k")
    L.Call(1, 0)

}
