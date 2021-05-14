#Lua.go

<img src="logo.png" align="right" width='100' height='100' />

#### 特性
1. 完美支持Lua原生所有特性(当前lua版本5.4.3)
2. 打通Lua-Go之间的调用，通过反射支持导出Go struct给Lua无缝调用
   
   · 支持直接访问 Go struct 的字段
   
   · 支持直接调用 Go struct 的函数
3. 内建 protobuf/msgpack/cjson/serialize 4种序列化库(需调用OpenLibsExt())
4. 已适配 lua_lock/lua_unlock 支持goroutine并发, 无需额外加锁

#### 关于性能
以我的iMac为例
> Go内部调用无敌 45亿次/秒
>> Lua调用内部函数是最快的  260w次/秒
>>> Lua调用Go导出的全局函数 60w次/秒
>>>> Lua调用Go导出的struct 15w次/秒

示例
```
L := lua.NewState(nil)
L.OpenLibs()
L.OpenLibsExt()
if err := L.DoFile("examples/test.lua"); err != nil {
    log.Println(err)
}
L.WaitClose()
```