func pair(a: int, b: int) -> int:
	return a + b

func test():
	var f: func(int) -> int = pair
	print(f.call(1))
