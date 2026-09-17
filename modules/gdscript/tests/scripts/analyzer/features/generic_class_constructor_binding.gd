class Tile:
	var value: int = 7

class Tiles[T]:
	var items: Array[T] = []

	func _init(create: func() -> T) -> void:
		items.append(create.call())

	func first() -> T:
		return items[0]

class Pool[T]:
	var items: Array[T] = []

	func add(item: T) -> void:
		items.append(item)

func test():
	# The constructor's own arguments bind `T`, so the class is named once.
	var tiles := Tiles.new(func() -> Tile: return Tile.new())
	print(tiles.first().value)

	# A constructor that binds nothing still fits whatever it is assigned to.
	var ints: Pool[int] = Pool.new()
	var words: Pool[String] = Pool.new()
	ints.add(1)
	words.add("x")
	print(ints.items, words.items)
