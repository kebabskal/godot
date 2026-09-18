func test():
	# `preload()` reads the scene's root, so `instantiate()` makes that type, and the
	# annotation can be left out.
	var enemies := preload("typed_packed_scene_enemy.tscn")
	var enemy := enemies.instantiate()
	print(enemy.hp + 1)
	enemy.free()

	# A native root, and a scene assigned to a wider root type.
	var sprites: PackedScene[Sprite2D] = preload("typed_packed_scene_sprite.tscn")
	var nodes: PackedScene[Node2D] = sprites
	var sprite := sprites.instantiate()
	print(sprite.centered)
	sprite.free()
	var node := nodes.instantiate()
	print(node is Sprite2D)
	node.free()
