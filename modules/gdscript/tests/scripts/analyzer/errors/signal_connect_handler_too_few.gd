signal hit(damage: int)

func on_anything() -> void:
	pass

func test():
	# At runtime this never runs the handler: "expected 0 arguments, but called with 1".
	hit.connect(on_anything)
