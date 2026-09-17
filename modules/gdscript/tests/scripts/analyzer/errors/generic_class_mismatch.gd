class Pool[T]:
	var last: T

func test():
	var words: Pool[String] = Pool.new()
	var ints: Pool[int] = words
	print(ints)
