class Pair[T]:
	func _init(first: T, second: T) -> void:
		print(first, second)

func test():
	Pair.new(1, "two")
