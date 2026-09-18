trait Mortal:
	var hp: int
	var is_alive: bool: get: return hp > 0

class Guy:
	uses Mortal
	var hp: int = 5

func test():
	var guy := Guy.new()
	# Every read goes through the getter, so a write would land nowhere.
	guy.is_alive = false
