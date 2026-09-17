trait Named:
	func get_name() -> String:
		return "?"

class Rock:
	pass

class Registry[T: Named]:
	var items: Array[T] = []

func test():
	var registry: Registry[Rock] = Registry.new()
	print(registry)
