# A trait declared in another script is reached like any other member of that script.
const Lib = preload("./traits_lib.notest.gd")

class Person:
	uses Lib.Greeter

	func who() -> String:
		return "world"

func test():
	var g: Lib.Greeter = Person.new()
	print(g.greet(), " ", g is Lib.Greeter)
