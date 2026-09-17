func map_all[T, U](items: Array[T], f: func(T) -> U) -> Array[U]:
	return []

func test():
	print(map_all([1], func(x: int) -> int: return x))
