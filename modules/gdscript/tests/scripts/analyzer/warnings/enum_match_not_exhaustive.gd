enum State {IDLE, WALK, RUN, JUMP}

enum Many {A, B, C, D, E, F, G, H}

func test():
	var s := State.WALK
	match s: # Misses RUN and JUMP.
		State.IDLE:
			pass
		State.WALK:
			pass
	match s: # A guarded branch may not run, so it handles nothing for sure.
		State.IDLE, State.WALK, State.JUMP:
			pass
		State.RUN when randf() > 2.0:
			pass
	match s: # Fine: a wildcard.
		State.IDLE:
			pass
		_:
			pass
	match s: # Fine: a bind takes the rest.
		State.IDLE:
			pass
		var other:
			print(other)
	match s: # Fine: every value.
		State.IDLE, State.WALK:
			pass
		State.RUN, State.JUMP:
			pass
	var m := Many.A
	match m: # Only the first few names are listed.
		Many.A:
			pass
	var n := 1
	match n: # Not an enum.
		1:
			pass
	var untyped = s
	match untyped: # Not known to be an enum.
		State.IDLE:
			pass
