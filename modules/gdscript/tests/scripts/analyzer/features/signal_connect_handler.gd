signal found(node: Node)
signal hit(damage: int)

# Narrowing a class parameter is the usual way to write a handler: a class
# parameter is checked when the handler is called.
func on_found(sprite: Sprite2D) -> void:
	print("found ", sprite.get_class())

# Widening is fine, and so is an extra parameter with a default.
func on_hit(amount: float, crit: bool = false) -> void:
	print("hit ", amount, " ", crit)

func on_anything() -> void:
	print("anything")

func test():
	found.connect(on_found)
	hit.connect(on_hit)
	# A handler that wants fewer arguments drops them explicitly.
	hit.connect(on_anything.unbind(1))

	var sprite := Sprite2D.new()
	found.emit(sprite)
	sprite.free()
	hit.emit(3)
