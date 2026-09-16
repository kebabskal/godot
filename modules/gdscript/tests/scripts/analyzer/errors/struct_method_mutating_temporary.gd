struct V:
	var x: int = 0

	func inc() -> void:
		x += 1

func test():
	V().inc()
