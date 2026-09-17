# A "module" file: traits, enums and generic classes for other scripts to use.
enum Element { FIRE, ICE }

trait Damageable:
	func take_damage(amount: int) -> void

class Pool[T]:
	var items: Array[T] = []

	func add(item: T) -> void:
		items.append(item)

	func first() -> T:
		return items[0]
