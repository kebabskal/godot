trait Noisy:
	signal pinged

struct Quiet:
	uses Noisy
	var x: int = 0

func test():
	print(Quiet())
