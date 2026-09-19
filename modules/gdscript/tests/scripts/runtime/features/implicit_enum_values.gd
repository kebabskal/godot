# `.NAME` is a value of whichever enum the context expects.

enum State {IDLE, WALK, RUN}

class Unit:
	var state: State = .IDLE

	func set_state(s: State) -> void:
		state = s

var mode: State = .WALK

func pick(fast: bool) -> State:
	return .RUN if fast else .WALK

func describe(s: State = .IDLE) -> String:
	match s:
		.IDLE:
			return "idle"
		.WALK, .RUN:
			return "moving"
	return "?"

func test():
	var s: State = .RUN
	print(s)
	s = .IDLE
	print(s)
	print(s == .IDLE, " ", .WALK != s)
	print(pick(true), " ", pick(false))
	print(describe(), " ", describe(.WALK))
	print(mode)
	var unit := Unit.new()
	unit.set_state(.RUN)
	print(unit.state)
	unit.state = .WALK
	print(unit.state)
	var list: Array[State] = [.IDLE, .RUN]
	print(list)
	const C: State = .RUN
	print(C)
	# Engine enums, through a property and through a method parameter.
	var node := Node.new()
	node.process_mode = .PROCESS_MODE_DISABLED
	print(node.process_mode == Node.PROCESS_MODE_DISABLED)
	node.set_process_mode(.PROCESS_MODE_ALWAYS)
	print(node.process_mode)
	node.free()
