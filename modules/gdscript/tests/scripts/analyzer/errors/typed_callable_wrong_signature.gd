func test():
	var f: func(int) -> int = func(s: String) -> int: return s.length()
	print(f.call(1))
