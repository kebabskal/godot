signal hit(damage: int)

func on_hit(who: String) -> void:
	print(who)

func test():
	hit.connect(on_hit)
