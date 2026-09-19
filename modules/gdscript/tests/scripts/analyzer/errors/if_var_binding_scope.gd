func pair() -> (bool, int):
	return true, 1

func test():
	if var ok, value := pair():
		print(ok, value)
	else:
		print(value)
	print(ok)
