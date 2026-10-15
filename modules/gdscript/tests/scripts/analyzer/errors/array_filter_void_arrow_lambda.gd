class Pad:
	func mark(_p: Vector2) -> void:
		pass

func test():
	var pads: Array[Pad] = [Pad.new()]
	# The arrow form infers the lambda's return type from its body, so a void call there is
	# caught the same way as a written `-> void`.
	print(pads.filter(pad => pad.mark(Vector2.ONE)))
