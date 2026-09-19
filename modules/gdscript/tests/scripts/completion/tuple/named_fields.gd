extends Node

func divmod(a: int, b: int) -> (quotient: int, remainder: int):
	return a / b, a % b

func _ready():
	var d := divmod(7, 2)
	d.➡
