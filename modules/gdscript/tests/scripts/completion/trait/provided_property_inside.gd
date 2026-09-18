extends Node

trait Mortal:
	var hp: int
	var is_alive: bool: get: return hp > 0

class Guy:
	uses Mortal
	var hp: int = 5
	func check() -> bool:
		return is_➡
