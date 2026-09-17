trait A:
	func hello() -> String:
		return "a"

trait B:
	func hello() -> String:
		return "b"

class Both:
	uses A, B

func test():
	print(Both.new().hello())
