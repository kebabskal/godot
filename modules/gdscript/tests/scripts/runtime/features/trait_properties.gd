# Traits with required properties: the user provides the variable, default methods can use it.
trait Mortal:
	var hp: int

	func take_damage(amount: int) -> void:
		hp -= amount

	func is_alive() -> bool:
		return self.hp > 0

class Enemy:
	uses Mortal
	var hp: int = 10

class Armored extends Enemy:
	func take_damage(amount: int) -> void:
		hp -= amount - 2

struct Crate:
	uses Mortal
	var hp: int = 3

func heal(m: Mortal, amount: int) -> int:
	m.hp += amount
	return m.hp

func test():
	var e := Enemy.new()
	e.take_damage(4)
	print(e.hp, " ", e.is_alive())

	var a := Armored.new()
	a.take_damage(4)
	print(a.hp)

	print(heal(e, 10), " ", e.hp)

	var c := Crate()
	c.take_damage(5)
	print(c.hp, " ", c.is_alive())
	# A struct passed as a trait is a copy: the caller's value is unchanged.
	print(heal(c, 1), " ", c.hp)

	var m: Mortal = a
	m.hp = 1
	print(a.hp, " ", m.is_alive())
