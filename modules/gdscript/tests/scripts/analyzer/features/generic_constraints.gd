trait Named:
	func get_name() -> String:
		return "?"

class Person:
	uses Named

	func get_name() -> String:
		return "ada"

class Registry[T: Named]:
	var items: Array[T] = []

	func add(item: T) -> void:
		items.append(item)
		# The bound is the point: a value of type `T` can be used, not just passed along.
		print("added ", item.get_name())

	func first_as_bound() -> String:
		var named: Named = items[0]
		return named.get_name()

func label[T: Named](item: T) -> String:
	return item.get_name()

func test():
	var registry: Registry[Person] = Registry.new()
	registry.add(Person.new())
	print(registry.first_as_bound())
	print(label(Person.new()))
