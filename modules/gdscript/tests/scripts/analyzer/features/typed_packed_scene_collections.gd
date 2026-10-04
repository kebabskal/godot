# Typed scenes inside typed collections. `PackedScene[Sprite2D]` is only checked when compiling, so
# it is not a "nested typed collection": at runtime the array is an `Array[PackedScene]`.
# (Roots are engine classes here: the test runner can load one script file under two paths, which
# makes a script root look mismatched. In a project both paths are the same `res://` path.)

class Pool[T]:
	var items: Array[T] = []

@export var sprite_scenes: Array[PackedScene[Sprite2D]] = []
@export var node_by_name: Dictionary[String, PackedScene[Node2D]] = {}

func test():
	var sprite_scene: PackedScene[Sprite2D] = preload("typed_packed_scene_sprite.tscn")

	var scenes: Array[PackedScene[Sprite2D]] = [sprite_scene, sprite_scene]
	var centered := 0
	for scene in scenes:
		var sprite := scene.instantiate() # A Sprite2D, so `.centered` is checked.
		if sprite.centered:
			centered += 1
		sprite.free()
	print(centered)
	var first := scenes[0].instantiate()
	print(first is Sprite2D)
	first.free()

	var nodes: Dictionary[String, PackedScene[Node2D]] = {"sprite": sprite_scene}
	var node := nodes["sprite"].instantiate()
	print(node.position)
	node.free()

	# Each element may be a scene of a subtype, as one typed scene variable may.
	var wide: Array[PackedScene[Node2D]] = [sprite_scene]
	print(wide.size())

	# A generic class is checked when compiling too.
	var pools: Array[Pool[int]] = [Pool.new()]
	print(pools.size())

	# Exported, each slot carries the root, so the inspector filters it.
	for property: Dictionary in get_property_list():
		if property.name == "sprite_scenes" or property.name == "node_by_name":
			var hint: String = property.hint_string
			print(property.name, " ", hint)
