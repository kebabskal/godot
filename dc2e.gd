extends SceneTree
class Tile extends RefCounted:
	pass
class Other extends RefCounted:
	pass
class Tiles[T]:
	func _init(create: func() -> T) -> void:
		print(create)
func _init() -> void:
	var tiles: Tiles[Other] = Tiles.new(func() -> Tile: return Tile.new())
	print(tiles)
	quit()
