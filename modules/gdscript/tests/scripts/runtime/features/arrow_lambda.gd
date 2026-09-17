# `params => expression`, the short form of `func(params): return expression`.
class Item:
	var name := "thing"
	var hp := 3

func apply(f: Callable, value: Variant) -> Variant:
	return f.call(value)

func test():
	var double := x => x * 2
	print(double.call(4))

	var add := (a, b) => a + b
	print(add.call(2, 3))

	var greet := () => "hi"
	print(greet.call())

	# The usual place: as a call argument, with no indented block in sight.
	var nums := [1, 2, 3]
	print(nums.map(n => n * 10))
	print(nums.filter(n => n > 1))
	print(nums.reduce((acc, n) => acc + n, 0))

	var items := [Item.new(), Item.new()]
	print(items.map(item => item.name))

	# Captures work like any other lambda.
	var factor := 3
	print(nums.map(n => n * factor))

	# Nested and chained.
	print(apply(x => x + 1, 41))
	print(nums.map(n => n + 1).filter(n => n > 2))

	# A ternary body is taken whole.
	print(nums.map(n => "big" if n > 2 else "small"))

	# A body that returns nothing is fine; the lambda just returns nothing too.
	var shout := () => print("done")
	shout.call()
	var each := n => print("n=", n)
	each.call(7)
