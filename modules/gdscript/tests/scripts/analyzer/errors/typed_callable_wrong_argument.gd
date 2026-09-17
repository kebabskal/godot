func add_one(x: int) -> int:
	return x + 1

func test():
	var f: func(int) -> int = add_one
	print(f.call("one"))
