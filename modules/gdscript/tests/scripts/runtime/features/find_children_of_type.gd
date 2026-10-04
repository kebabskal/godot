# `find_children_of_type(T)` on a node: every descendant that is a `T`, as an `Array[T]`.

trait Damageable:
	func take_damage(amount: int) -> void

class Enemy extends Node2D:
	uses Damageable
	var hp := 3
	func take_damage(amount: int) -> void:
		hp -= amount

class Boss extends Enemy:
	pass

class Crate extends Node:
	uses Damageable
	var broken := false
	func take_damage(_amount: int) -> void:
		broken = true

class Level extends Node:
	func enemy_count() -> int:
		return find_children_of_type(Enemy).size() # A bare call: on `self`.

	# A method of its own with the name wins over the built-in.
	class Custom extends Node:
		func find_children_of_type(_type: Variant) -> String:
			return "custom"

func test():
	var level := Level.new()
	var group := Node2D.new()
	level.add_child(group)
	group.add_child(Enemy.new())
	group.add_child(Boss.new())
	level.add_child(Enemy.new())
	level.add_child(Crate.new())
	level.add_child(Sprite2D.new())
	group.add_child(Sprite2D.new())
	level.add_child(Sprite2D.new(), false, Node.INTERNAL_MODE_FRONT) # Internal: not counted.

	var enemies := level.find_children_of_type(Enemy)
	print(enemies.size(), " ", enemies.get_typed_script() == Enemy)
	for enemy in enemies:
		enemy.take_damage(1) # Typed: `enemy` is an Enemy.
	print(enemies.map(func(e: Enemy) -> int: return e.hp))

	print(level.find_children_of_type(Boss).size())
	print(level.find_children_of_type(Enemy, false).size()) # Direct children only.
	print(level.find_children_of_type(Sprite2D).size(), " ", level.find_children_of_type(Sprite2D).get_typed_class_name())
	print(level.find_children_of_type(Node2D).size())

	var hurtable := level.find_children_of_type(Damageable)
	for thing in hurtable:
		thing.take_damage(5)
	print(hurtable.size(), " ", (level.find_children_of_type(Crate)[0] as Crate).broken)

	var typed: Array[Enemy] = level.find_children_of_type(Enemy)
	print(typed.size(), " ", level.enemy_count())
	print(Level.Custom.new().find_children_of_type(Enemy))

	level.free()
