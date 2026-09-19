enum State:
	IDLE
	WALK

	func is_moving() -> bool:
		return self == WALK

	static func first() -> State:
		return IDLE

	static func broken() -> bool:
		return is_moving()

	func WALK() -> void:
		pass

	static func keys() -> Array:
		return []

func test():
	print(State.is_moving())
	print(State.WALK.first())
	print(State.WALK.missing())
