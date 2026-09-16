struct V:
	var x: int = 0

	func _add(other: V) -> V:
		x += other.x
		return self

func test():
	print(V(1) + V(2))
