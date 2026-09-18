signal hit(damage: int)

func test():
	hit.connect((damage, source) => print(damage, source))
