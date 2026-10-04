func test():
	# Arrays stay invariant: an array of sprite scenes is not an array of node scenes, or a
	# node scene could be added to it through the second name.
	var sprites: Array[PackedScene[Sprite2D]] = []
	var nodes: Array[PackedScene[Node2D]] = sprites
	# Real nested collections are still rejected.
	var nested: Array[Array[int]] = []
	var nested_values: Dictionary[String, Array[int]] = {}
	print(nodes, nested, nested_values)
