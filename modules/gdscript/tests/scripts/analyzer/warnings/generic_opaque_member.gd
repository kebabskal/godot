func shout[T](value: T) -> String:
	# `T` is opaque: the function has to work for every binding, so reaching into
	# it is an unsafe access (an error under `debug/gdscript/strict_mode`).
	return value.to_upper()

func test():
	print(shout("hi"))
