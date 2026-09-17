class Pool[T]:
	func add(item: T) -> void:
		print(item)

class IntPool extends Pool[int]:
	pass

func test():
	IntPool.new().add("not an int")
