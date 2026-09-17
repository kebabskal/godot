func odd[T: int | Vector2](value: T) -> bool:
	# `%` works for an int but not for a Vector2, so the body cannot use it.
	return value % 2 == 0

func test():
	print(odd(3))
