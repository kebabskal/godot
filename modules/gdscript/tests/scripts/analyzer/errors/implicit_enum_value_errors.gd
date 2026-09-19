enum State {IDLE, WALK}

func takes_int(i: int) -> void:
	print(i)

func test():
	var a = .IDLE
	var b: State = .JUMP
	var c: int = .IDLE
	takes_int(.IDLE)
	print(.IDLE)
	var d := .WALK
	print(a, b, c, d)
