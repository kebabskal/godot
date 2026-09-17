# `class Name[T]:` keeps the element type per instance.
class Pool[T]:
	var items: Array[T] = []
	var last: T

	func add(item: T) -> void:
		items.append(item)
		last = item

	func first() -> T:
		return items[0]

	func all() -> Array[T]:
		return items

	func size() -> int:
		return items.size()

class Pair[K, V]:
	var key: K
	var value: V

	func set_both(k: K, v: V) -> void:
		key = k
		value = v

	func describe() -> String:
		return "%s=%s" % [key, value]

func take_ints(pool: Pool[int]) -> int:
	return pool.first()

func test():
	var ints: Pool[int] = Pool.new()
	ints.add(3)
	ints.add(4)
	# The members are typed per instance.
	print(ints.size(), " ", ints.first() + 1, " ", ints.last + 1)

	var words: Pool[String] = Pool.new()
	words.add("hi")
	print(words.first().to_upper(), " ", words.last.length())

	# A container returned from a generic class is converted for the caller.
	var copy: Array[int] = ints.all()
	print(copy, " ", copy.is_typed())

	# Passing to a function that wants a specific binding.
	print(take_ints(ints))

	# Two type parameters.
	var pair: Pair[String, int] = Pair.new()
	pair.set_both("hp", 7)
	print(pair.describe(), " ", pair.value + 1, " ", pair.key.to_upper())
