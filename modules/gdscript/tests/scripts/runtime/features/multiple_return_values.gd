# Several values returned as one, and unpacked.

class Enemy:
	var hp := 3

func parse(text: String) -> (bool, int):
	if not text.is_valid_int():
		return false, 0
	return true, text.to_int()

func divmod(a: int, b: int) -> (quotient: int, remainder: int):
	return a / b, a % b

func find(hp: int) -> (bool, Enemy):
	if hp <= 0:
		return false, null
	var enemy := Enemy.new()
	enemy.hp = hp
	return true, enemy

func maybe(hp: int) -> Enemy?:
	return find(hp)[1] if hp > 0 else null

func next_item(i: int) -> (bool, int):
	return i < 3, i * 10

func untyped():
	return 1, "two"

func test():
	var ok, value := parse("42")
	print(ok, " ", value)
	var _, n := parse("x")
	print(n)
	var r := parse("7")
	print(r)
	var d := divmod(17, 5)
	print(d, " ", d.quotient, " ", d.remainder)
	var q: int, rem: int = divmod(9, 4)
	print(q, " ", rem)

	var a := 1
	var b := 2
	a, b = b, a
	print(a, " ", b)
	var arr := [0, 0]
	arr[0], arr[1] = divmod(7, 2)
	print(arr)
	_, a = parse("5")
	print(a)

	if var found, enemy := find(4):
		print(found, " ", enemy.hp)
	if var found, enemy := find(0):
		print("unreachable ", found, enemy)
	else:
		print("not found")
	if var enemy := maybe(6):
		print(enemy.hp)
	if var enemy := maybe(0):
		print("unreachable ", enemy)

	var i := 0
	while var more, item := next_item(i):
		print(item)
		i += 1

	var x, y = [10, 20]
	print(x + y)
	var s, t = untyped()
	print(s, " ", t)
	var p: (bool, int) = parse("3")
	print(p)
	var one, two := 1, "2"
	print(one, " ", two)
