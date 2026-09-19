func pair() -> (bool, int):
	return true, 1

func wrong_count() -> (bool, int):
	return true, 1, 2

func wrong_type() -> (bool, int):
	return true, "x"

func test():
	var a, b, c := pair()
	var x, y := 5
	var s: String, t: int = pair()
	var p: (int, int) = pair()
	print(a, b, c, x, y, s, t, p)
