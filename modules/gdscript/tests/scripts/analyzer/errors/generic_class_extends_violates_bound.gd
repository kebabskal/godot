trait Named:
	func get_name() -> String

class Registry[T: Named]:
	func add(item: T) -> void:
		print(item.get_name())

class Rock:
	pass

class BadRegistry extends Registry[Rock]:
	pass

func test():
	print(BadRegistry.new())
