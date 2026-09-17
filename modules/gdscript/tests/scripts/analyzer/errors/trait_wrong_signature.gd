trait Damageable:
	func take_damage(amount: int) -> void
	func is_alive() -> bool

struct Shield:
	uses Damageable
	var hp: int = 3

	func take_damage(amount: String) -> void:
		hp -= amount.length()

	func is_alive() -> int:
		return hp

func test():
	print(Shield())
