signal found(node: Node)

trait Damageable:
	func take_damage(amount: int) -> void

# A trait parameter is not checked on the way in, unlike a class parameter.
func on_found(target: Damageable) -> void:
	target.take_damage(1)

func test():
	found.connect(on_found)
