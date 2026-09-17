trait Mortal:
	var hp: int

struct Crate:
	uses Mortal
	var hp: float = 3.0

func test():
	print(Crate())
