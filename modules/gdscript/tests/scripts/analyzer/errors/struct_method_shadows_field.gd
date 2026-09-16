struct V:
	var x: int = 0

	func with(x: int) -> V:
		return V(x)

func test():
	print(V().with(1))
