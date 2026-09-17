trait Named:
	func get_name() -> String:
		return "?"

func label[T: Named](item: T) -> String:
	return item.get_name()

func test():
	print(label(5))
