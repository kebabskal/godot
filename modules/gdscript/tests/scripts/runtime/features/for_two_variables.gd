# `for key, value in dictionary` and `for index, item in anything else`.
func test():
	var d := {"a": 1, "b": 2}
	for k, v in d:
		print(k, "=", v)

	var typed: Dictionary[String, int] = {"x": 10, "y": 20}
	var sum := 0
	for key, value in typed:
		sum += value
		print(key.to_upper())
	print(sum)

	# `continue` keeps the index in step.
	for i, item in ["p", "q", "r"]:
		if i == 1:
			continue
		print(i, ":", item)

	for i, n in range(3, 6):
		print(i, "->", n)

	for i: int, ch: String in "hi":
		print(i, ch)

	var packed := PackedInt32Array([7, 8])
	for i, x in packed:
		print(i + x)

	# Not known at compile time: decided when the loop runs.
	var anything: Variant = {"k": "v"}
	for a, b in anything:
		print(a, b)
	anything = [5, 6]
	for a, b in anything:
		print(a, b)

	# Nested loops keep their own index.
	for i, a in [1, 2]:
		for j, b in [10, 20]:
			if j == 1:
				break
			print(i, j, a + b)
