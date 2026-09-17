func total[T: int | float](items: Array[T]) -> T:
	return items[0]

func test():
	var words: Array[String] = ["a"]
	print(total(words))
