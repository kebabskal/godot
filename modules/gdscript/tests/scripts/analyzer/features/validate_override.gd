# `_validate(property)` is an Object virtual the editor's Inspector calls after
# it edits a property, so a script overrides it like any other native virtual.
extends Resource

var validated: Array[StringName] = []

func _validate(property: StringName) -> void:
	validated.append(property)

func test():
	_validate(&"random_seed")
	print(validated)
