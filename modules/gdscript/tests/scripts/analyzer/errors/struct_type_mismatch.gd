struct A:
	var x: int = 1

struct B:
	var x: int = 1

func test():
	var a: A = B()
	print(a)
