class Item:
	var name := "sword"
	var hp := 3
	func boost(n: int) -> int:
		return hp + n
	func touch() -> void:
		print("touched " + name)

var side_effects := 0

func noisy(n: int) -> int:
	side_effects += 1
	return n

func find(ok: bool) -> Item:
	return Item.new() if ok else null

func test():
	# `??` picks the left operand unless it is null.
	var missing: Item = null
	var present := Item.new()
	print((missing ?? present).name)
	print((present ?? missing).name)

	# A nullable builtin really holds null, instead of converting it to a zero.
	var none: int? = null
	print(none)
	print(none ?? 7)
	print((3 as int?) ?? 7)

	# Right-associative.
	print(none ?? none ?? 9)

	# The right operand only runs when it is needed.
	side_effects = 0
	print(5 ?? noisy(1), " ", side_effects)
	print(none ?? noisy(2), " ", side_effects)

	# `?.` yields null instead of accessing a null base.
	print(find(true)?.name)
	print(find(false)?.name)

	# A skipped call skips its arguments too.
	side_effects = 0
	print(find(true)?.boost(noisy(10)), " ", side_effects)
	print(find(false)?.boost(noisy(10)), " ", side_effects)

	# As a statement, with no result at all.
	find(false)?.touch()
	find(true)?.touch()

	# `?[]` guards an index the same way.
	var present_dict: Dictionary? = { "k": 1 }
	var missing_dict: Dictionary? = null
	print(present_dict?["k"])
	print(missing_dict?["k"])

	# The usual way back from `T?` to `T`.
	print(find(false)?.name ?? "(none)")
	print(find(true)?.name ?? "(none)")
