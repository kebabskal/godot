extends Node

trait Damageable:
#     ^^^^^^^^^^ trait:damageable -> trait:damageable
	func is_alive() -> bool
#     ^^^^^^^^ trait:damageable:is_alive -> trait:damageable:is_alive

class Enemy:
	uses Damageable
#     ^^^^^^^^^^ -> trait:damageable
	func is_alive() -> bool:
		return true

func f(target: Damageable):
#              ^^^^^^^^^^ -> trait:damageable
	print(target.is_alive())
#             ^^^^^^^^ -> trait:damageable:is_alive
