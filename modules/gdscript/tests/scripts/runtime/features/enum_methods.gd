# Enum methods: the `enum Name:` block form. A method without `static` takes the value as
# `self`; inside, the enum's values need no prefix. Values stay plain integers at runtime.

const Other = preload("enum_methods_other.notest.gd")

enum State:
	IDLE
	WALK, RUN
	JUMP = 10

	func is_moving() -> bool:
		return self == WALK or self == RUN

	func next() -> State:
		match self:
			IDLE:
				return WALK
			WALK:
				return RUN
			RUN:
				return JUMP
			JUMP:
				return IDLE
		return IDLE

	func step(times: int, skip_jump := false) -> State:
		var result := self
		for _i in times:
			result = result.next()
			if skip_jump and result == JUMP:
				result = next_after(result)
		return result

	static func next_after(state: State) -> State:
		return state.next()

	static func from_speed(speed: float) -> State:
		if speed <= 0.0:
			return IDLE
		return WALK if speed < 5.0 else RUN

class Inner:
	enum Mode:
		A
		B

		func flip() -> Mode:
			return B if self == A else A

	func outer_state_moving() -> bool:
		return State.WALK.is_moving()

var current: State = State.RUN

func test():
	var s := State.WALK
	print(s.is_moving())
	print(State.IDLE.is_moving())
	print(s.next())
	print(s.next().next().next())
	print(State.from_speed(0.0), " ", State.from_speed(2.0), " ", State.from_speed(9.0))
	print(State.IDLE.step(2))
	print(State.IDLE.step(3, true))
	print(State.next_after(State.RUN))
	print(current.is_moving())
	var states: Array[State] = [State.IDLE, State.RUN]
	for state in states:
		print(state.is_moving())
	print(Inner.Mode.A.flip())
	print(Inner.new().outer_state_moving())
	print(Other.Dir.LEFT.opposite())
	print(typeof(s) == TYPE_INT)
	# Enum methods are reached through the enum, not listed as the class's methods.
	print(get_method_list().filter(func(m: Dictionary) -> bool: return "." in m.name).size())
