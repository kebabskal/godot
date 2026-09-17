class Pair[K, V]:
	var key: K
	var value: V

func test():
	var p: Pair[int] = Pair.new()
	print(p)
