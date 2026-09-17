# Traits with constants and signals, and traits that use other traits.
trait Named:
	const UNKNOWN := "???"
	var label: String

	func get_label() -> String:
		return label if label != "" else UNKNOWN

trait Mortal:
	signal died(who: String)
	var hp: int

	func take_damage(amount: int) -> void:
		hp -= amount
		if hp <= 0:
			on_death()

	func on_death() -> void

trait Character:
	uses Named, Mortal

	# Provides what `Mortal` requires, using what `Named` provides.
	func on_death() -> void:
		died.emit(get_label())

class Hero:
	uses Character
	var label: String = ""
	var hp: int = 2

func announce(who: String) -> void:
	print(who, " died")

func test():
	var h := Hero.new()
	@warning_ignore("return_value_discarded")
	h.died.connect(announce)
	print(h is Character, " ", h is Named, " ", h is Mortal)

	var m: Mortal = h
	m.take_damage(1)
	print(h.hp, " ", h.get_label())

	h.label = "Link"
	var c: Character = h
	var n: Named = c
	print(n.get_label(), " ", Named.UNKNOWN)
	c.take_damage(5)
