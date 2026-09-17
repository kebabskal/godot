class Pool[T]:
	var items: Array[T] = []

	func add(item: T) -> void:
		items.append(item)

func test():
	var ints: Pool[int] = Pool.new()
	ints.add("not an int")
