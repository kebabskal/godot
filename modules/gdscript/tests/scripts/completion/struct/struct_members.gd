extends Node

struct Point:
	var x: float = 0.0
	var y: float = 0.0

	func length() -> float:
		return x

func _ready():
	var p := Point(1.0, 2.0)
	p.➡
