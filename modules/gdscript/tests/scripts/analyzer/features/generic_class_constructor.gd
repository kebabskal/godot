class Tile:
	var value: int = 7

class Tiles[T]:
	var items: Array[T] = []

	func _init(create: func() -> T) -> void:
		items.append(create.call())

	func first() -> T:
		return items[0]

func test():
	# The constructor's arguments bind the class's type parameter, so `create` is
	# checked against `func() -> Tile` rather than the unbound `func() -> T`.
	var from_block: Tiles[Tile] = Tiles.new(func() -> Tile: return Tile.new())
	print(from_block.first().value)

	var from_short: Tiles[Tile] = Tiles.new(() => Tile.new())
	print(from_short.first().value)
