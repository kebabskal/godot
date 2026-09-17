trait Mortal:
	var hp: int

class Rock:
	uses Mortal

func test():
	print(Rock.new())
