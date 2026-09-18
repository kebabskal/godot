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

trait Healthy:
#     ^^^^^^^ trait:healthy -> trait:healthy
	var hp: int
	var is_healthy: bool: get: return hp > 10
#    ^^^^^^^^^^ trait:healthy:is_healthy -> trait:healthy:is_healthy
	func heal() -> void:
#     ^^^^ trait:healthy:heal -> trait:healthy:heal
		pass

class Patient:
	uses Healthy
	var hp: int = 5

func g(patient: Patient):
	print(patient.is_healthy)
#              ^^^^^^^^^^ -> trait:healthy:is_healthy
	patient.heal()
#        ^^^^ -> trait:healthy:heal
