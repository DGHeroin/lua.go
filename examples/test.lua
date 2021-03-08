require "cmsgpack"
require "serialize"
require "pb"

function Say(name)
    print('my name is...', name)
end
--GGFunc('aaa')
--GGFunc = nil
--collectgarbage("collect") -- gc 'GGFunc'
--
--print("before:", Dog.Name, Dog.Age)
--Dog.Name = '我是猫1'
--Dog.Age = 5
--print("after:", Dog.Name, Dog.Age)
--print(Dog.Say('aaa', 1234))
--print(Dog2.Say('bbb', 1234))
--
---- local s = Dog2.GetSelf()
---- s.Say('ccc', 666)
--for i=1,10 do
--    Dog.Tell(Dog2)
--end
--
--
--Dog = nil
--Dog2 = nil

Dog.Say('aaa', 1234)
Dog.Say('aaa', 1234)

collectgarbage("collect") -- gc 'Dog'

Dog.Say('aaa', 1234)
