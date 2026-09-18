trait Mortal:
	var hp: int
	var is_alive: bool: get: return hp > 0

struct Blob:
	uses Mortal
	var hp: int = 1

func test():
	pass
