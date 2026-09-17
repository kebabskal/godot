var hp: int = 5

# An inline accessor may stay on the declaration's line, the way `func f(): return 1` does.
var alive: bool: get: return hp > 0
var doubled: int: get(): return hp * 2
var clamped: int: set(value): hp = maxi(0, value)

func test():
	print(alive, " ", doubled)
	clamped = -3
	print(hp, " ", alive)
