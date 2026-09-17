trait Damageable:
	func take_damage(amount: int) -> void

class Rock:
	var hp := 1

func test():
	var d: Damageable = Rock.new()
	print(d)
