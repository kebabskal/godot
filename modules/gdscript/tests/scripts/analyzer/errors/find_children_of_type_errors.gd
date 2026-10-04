func test():
	var root := Node.new()
	var a := root.find_children_of_type(int)
	var b := root.find_children_of_type(Resource)
	var c := root.find_children_of_type(Sprite2D, "yes")
	var d := root.find_children_of_type()
	var e: Array[Node3D] = root.find_children_of_type(Sprite2D)
	print(a, b, c, d, e)
	root.free()
