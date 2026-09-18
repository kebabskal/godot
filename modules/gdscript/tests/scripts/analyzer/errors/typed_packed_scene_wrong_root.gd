func test():
	# The scene's root is a Sprite2D, which is not a Node3D.
	var nodes: PackedScene[Node3D] = preload("../features/typed_packed_scene_sprite.tscn")
	print(nodes)
