const Lib = preload("../features/generic_class_qualified.notest.gd")

func test():
	var numbers: Lib.Pool[int] = Lib.Pool.new()
	numbers.add("not an int")
