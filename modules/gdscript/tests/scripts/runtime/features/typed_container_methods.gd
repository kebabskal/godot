# Built-in container methods keep the element type, matching what they do at runtime.
func test():
	var nums: Array[int] = [3, 1, 2]

	var kept: Array[int] = nums.filter(func(x: int) -> bool: return x > 1)
	print(kept.size(), " ", kept.is_typed())

	var part: Array[int] = nums.slice(0, 2)
	var copy: Array[int] = nums.duplicate()
	print(part.size(), " ", copy.size())

	# The element type flows into inference, so these are ints, not Variants.
	var f := nums.front()
	var b := nums.back()
	print(f + b)

	# Objects keep their class.
	var nodes: Array[Node] = [Node.new()]
	var same: Array[Node] = nodes.duplicate()
	print(same[0].get_class())
	nodes[0].free()

	var scores: Dictionary[String, int] = {"a": 1, "b": 2}
	var names: Array[String] = scores.keys()
	var values: Array[int] = scores.values()
	print(names.size(), " ", values.size())
	print(names.front().to_upper())

	var same_dict: Dictionary[String, int] = scores.duplicate()
	print(same_dict.size())

	# `map` changes the element type, so it stays untyped: still assignable to Array.
	var mapped: Array = nums.map(func(x: int) -> String: return str(x))
	print(mapped.size())

	# Popping still gives a Variant, because it returns null on an empty array.
	var queue: Array[int] = [1]
	var popped = queue.pop_back()
	print(popped, " ", queue.pop_back() == null)
