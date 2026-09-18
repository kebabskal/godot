trait Mortal:
	var hp: int
	var is_alive: bool: get: return hp > 0

trait Undead:
	var hp: int
	var is_alive: bool: get: return true

class Lich:
	uses Mortal, Undead
	var hp: int = 5

func test():
	pass
