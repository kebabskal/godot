trait Damageable:
	func take_damage(amount: int) -> void
	func is_alive() -> bool

class Enemy:
	uses Damageable

	func take_damage(_amount: int) -> void:
		pass

func test():
	print(Enemy.new())
