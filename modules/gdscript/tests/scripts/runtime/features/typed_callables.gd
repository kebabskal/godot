# Typed callables: `func(int) -> int` as a type, checked at compile time.
func twice(f: func(int) -> int, x: int) -> int:
	return f.call(f.call(x))

func add_one(x: int) -> int:
	return x + 1

func describe(n: int, prefix: String = "#") -> String:
	return prefix + str(n)

func make_adder(n: int) -> func(int) -> int:
	return func(x: int) -> int: return x + n

func run(job: func() -> void) -> void:
	job.call()

func test():
	print(twice(add_one, 5))
	print(twice(func(x: int) -> int: return x * 3, 2))

	var add_ten := make_adder(10)
	print(add_ten.call(1))

	# A function with an optional parameter fits a shorter signature.
	var label: func(int) -> String = describe
	print(label.call(7))

	# Untyped lambdas and plain Callables still fit.
	var loose: func(int) -> int = func(x): return x - 1
	print(loose.call(3))
	var plain: Callable = add_one
	var typed_again: func(int) -> int = plain
	print(typed_again.call(1))

	run(func() -> void: print("ran"))

	var callbacks: Array[Callable] = [add_one, add_ten]
	print(callbacks.size())
