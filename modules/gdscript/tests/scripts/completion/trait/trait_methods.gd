extends Node

trait Damageable:
	func take_damage(amount: int) -> void
	func is_alive() -> bool

func hit(target: Damageable):
	target.➡
