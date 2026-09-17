# Lambda parameters take their type from the call, and `map()` types its result.
class Item:
	var name := "thing"
	var hp := 3

func to_text(n: int) -> String:
	return "n%d" % n

func test():
	var nums: Array[int] = [1, 2, 3]

	# The result of `map` is a real typed array, from the lambda's return type.
	var texts: Array[String] = nums.map(n => to_text(n))
	print(texts, " ", texts.is_typed())

	var doubled: Array[int] = nums.map(n => n * 2)
	print(doubled, " ", doubled.is_typed())

	# The parameter is typed, so its members are known.
	var items: Array[Item] = [Item.new(), Item.new()]
	var names: Array[String] = items.map(item => item.name)
	print(names, " ", names.is_typed())

	var health: Array[int] = items.map(item => item.hp)
	print(health)

	# A named function works the same way.
	var named: Array[String] = nums.map(to_text)
	print(named, " ", named.is_typed())

	# `filter` already keeps the element type and needs no conversion.
	var big: Array[int] = nums.filter(n => n > 1)
	print(big)

	# Sorting with a typed comparison.
	var sorted_items := items.duplicate()
	sorted_items.sort_custom((a, b) => a.hp < b.hp)
	print(sorted_items.size())

	# An untyped array still maps to an untyped one.
	var loose := [1, 2]
	print(loose.map(n => n))
