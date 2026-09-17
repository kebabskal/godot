# enable UNTYPED_DECLARATION

signal changed(items: Array[String])

func test() -> void:
	var nums: Array[int] = [1, 2, 3]

	# Inferred from the call, so nothing is untyped here: the parameters come from
	# the container and the signal, and the return types from the bodies.
	print(nums.map(n => n * 2))
	changed.connect(a => print(a.filter(b => b.begins_with("k"))))
	changed.emit(["kebab", "pizza"])

	# Nothing to infer from, so these are still reported.
	var standalone := func(x): print(x)
	standalone.call(1)
