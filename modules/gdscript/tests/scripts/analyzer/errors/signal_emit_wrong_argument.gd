signal hit(damage: int)

func test():
	hit.emit("not an int")
