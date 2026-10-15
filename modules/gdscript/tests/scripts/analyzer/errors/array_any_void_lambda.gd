class Pad:
	func hit(p: Vector2) -> bool:
		return p.x > 0
	func mark(_p: Vector2) -> void:
		pass

func test():
	var pads := [Pad.new()]
	# `any()` tests what the callable returns; a lambda that returns nothing never counts.
	print(not pads.any(func(pad) -> void: pad.hit(Vector2.ONE)))
