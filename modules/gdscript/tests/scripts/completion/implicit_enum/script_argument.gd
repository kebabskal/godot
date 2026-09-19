extends Node

enum State { IDLE, WALK }

func set_state(speed: float, s: State) -> void:
	pass

func _ready():
	set_state(1.0, .➡)
