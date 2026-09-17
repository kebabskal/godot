trait Greeter:
	func who() -> String

	func greet() -> String:
		return "hello " + who()
