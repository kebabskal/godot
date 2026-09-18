trait Mortal:
	var hp: int
	# Provided by the trait: the class gets it without declaring it.
	var is_alive: bool: get: return hp > 0
	var health: int:
		get:
			return hp
		set(value):
			hp = clampi(value, 0, 100)

class Guy:
	uses Mortal
	var hp: int = 5

	func report() -> String:
		return "alive" if is_alive else "dead"

class Monster:
	var armor: int = 99
	var tag: String = "orc"
	uses Mortal
	# `hp` sits at a different position than in `Guy`; the shared accessor reads it by name.
	var hp: int = 0

class Overrider:
	uses Mortal
	var hp: int = 5
	# Declared by the class, so the class's own wins.
	var is_alive: bool = false

class Boss extends Guy:
	pass

func test():
	var guy := Guy.new()
	var monster := Monster.new()
	print(guy.is_alive, " ", monster.is_alive, " ", guy.report())

	guy.health = 500
	monster.health = -7
	print(guy.hp, " ", monster.hp)

	var mortal: Mortal = monster
	print(mortal.is_alive)

	print(Overrider.new().is_alive)

	var boss := Boss.new()
	print(boss.is_alive, " ", boss.health)
