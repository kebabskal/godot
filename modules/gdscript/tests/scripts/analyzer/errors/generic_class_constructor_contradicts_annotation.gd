class Tile:
	pass

class Other:
	pass

class Tiles[T]:
	func _init(create: func() -> T) -> void:
		print(create)

func test():
	# The constructor binds `T` to `Tile`, so the annotation cannot say `Other`.
	var tiles: Tiles[Other] = Tiles.new(func() -> Tile: return Tile.new())
	print(tiles)
