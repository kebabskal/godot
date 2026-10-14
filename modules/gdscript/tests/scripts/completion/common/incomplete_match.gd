extends Node

# A match with nothing to test (half typed) must not crash the analyzer.
func _input(event: InputEvent) -> void:
	match
	var mme := event as InputEventMouseMotion
	mme.➡
