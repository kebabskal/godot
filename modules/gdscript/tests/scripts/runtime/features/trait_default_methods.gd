# Traits with default methods: compiled into every class or struct that does not provide its own.
trait Describable:
	func get_label() -> String

	func describe() -> String:
		return "<%s>" % get_label()

	func shout() -> String:
		return self.describe().to_upper()

trait Damageable:
	func take_damage(amount: int) -> void
	func is_alive() -> bool

	func kill() -> int:
		var hits := 0
		while is_alive():
			take_damage(1)
			hits += 1
		return hits

class Enemy:
	uses Describable, Damageable
	var hp := 3

	func get_label() -> String:
		return "enemy"

	func take_damage(amount: int) -> void:
		hp -= amount

	func is_alive() -> bool:
		return hp > 0

class Boss extends Enemy:
	# Overrides the default; `shout()` sees it through `self`.
	func describe() -> String:
		return "boss!"

struct Shield:
	uses Damageable
	var hp: int = 2

	func take_damage(amount: int) -> void:
		hp -= amount

	func is_alive() -> bool:
		return hp > 0

func test():
	var e := Enemy.new()
	print(e.describe(), " ", e.shout())

	var b := Boss.new()
	print(b.describe(), " ", b.shout())
	var d: Describable = b
	print(d.describe())

	print(e.kill(), " ", e.hp, " ", e.is_alive())
	var k: Damageable = b
	print(k.kill(), " ", b.hp)

	# A default method compiled into a struct writes its changes back to the variable.
	var s := Shield()
	print(s.kill(), " ", s.hp, " ", s.is_alive())
	print(Shield().kill())
