extends Node

trait Named:
	func get_name() -> String:
		return "?"

class Registry[T: Named]:
	func label(item: T) -> String:
		return item.get_➡
