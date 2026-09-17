signal hit(damage: int)

func test():
	# `d` is an int here, taken from the signal, so this is caught instead of failing at runtime.
	hit.connect(d => d.to_upper())
