function Say(name)
    print('my name is...', name)
end
GGFunc('aaa')
GGFunc = nil
collectgarbage("collect") -- gc 'GGFunc'

print("before:", Dog.Name, Dog.Age)
Dog.Name = '我是猫1'
Dog.Age = 5
print("after:", Dog.Name, Dog.Age)
print(Dog.Say('aaa', 1234))
print(Dog2.Say('bbb', 1234))