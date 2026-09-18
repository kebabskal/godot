extends Node

trait Greeter:
	func greet() -> String:
		return "hi"

class Guy:
	uses Greeter

func _ready():
	var g := Guy.new()
	g.gre➡
