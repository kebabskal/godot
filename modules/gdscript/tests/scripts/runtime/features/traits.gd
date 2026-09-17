# Traits as interfaces: required methods, `uses`, traits as types, `is` and `as`.
trait Damageable:
	func take_damage(amount: int) -> void
	func is_alive() -> bool

trait Named:
	func get_label() -> String

class Enemy:
	uses Damageable, Named
	var hp := 10

	func take_damage(amount: int) -> void:
		hp -= amount

	func is_alive() -> bool:
		return hp > 0

	func get_label() -> String:
		return "enemy"

class Boss extends Enemy:
	func get_label() -> String:
		return "boss"

class Rock:
	var hp := 1

struct Shield:
	uses Damageable
	var hp: int = 3

	func take_damage(amount: int) -> void:
		hp -= amount

	func is_alive() -> bool:
		return hp > 0

func hit(target: Damageable, amount: int) -> bool:
	target.take_damage(amount)
	return target.is_alive()

func label_of(thing: Named) -> String:
	return thing.get_label()

func test():
	var e := Enemy.new()
	print(hit(e, 4), " ", e.hp)

	# A subclass inherits the traits of its base.
	var b := Boss.new()
	var d: Damageable = b
	d.take_damage(20)
	print(d.is_alive(), " ", b is Damageable, " ", b is Named, " ", label_of(b))

	# Runtime tests on untyped values.
	var v: Variant = Rock.new()
	@warning_ignore("unsafe_cast")
	var not_damageable := v as Damageable
	print(v is Damageable, " ", not_damageable == null)
	v = e
	@warning_ignore("unsafe_cast")
	var again := v as Damageable
	print(again != null, " ", again.is_alive())

	# Structs can use traits too. A trait-typed parameter receives a copy of the struct.
	var s := Shield()
	print(hit(s, 1), " ", s.hp, " ", s is Damageable)
	var sv: Variant = s
	print(sv is Damageable, " ", sv is Named)

	# Typed collections.
	var all: Array[Damageable] = [e, b]
	var alive := 0
	for t in all:
		if t.is_alive():
			alive += 1
	print(alive)
