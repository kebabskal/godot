extends Node

trait Mortal:
	var hp: int
	func is_alive() -> bool

func check(target: Mortal):
	target.➡
