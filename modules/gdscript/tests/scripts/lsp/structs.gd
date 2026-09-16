extends Node

struct Point:
#      ^^^^^ struct:point -> struct:point
	var x: float = 0.0
#    ^ struct:point:x -> struct:point:x

	func length() -> float:
#     ^^^^^^ struct:point:length -> struct:point:length
		return x
#        ^ -> struct:point:x

func f():
	var p := Point(1.0)
#         ^^^^^ -> struct:point
	print(p.x, p.length())
#        ^ -> struct:point:x
#             ^^^^^^ -> struct:point:length
