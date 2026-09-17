# Generic functions that build and return containers.
func map_all[T, U](items: Array[T], f: func(T) -> U) -> Array[U]:
	var out: Array[U] = []
	for item in items:
		out.append(f.call(item))
	return out

func keep[T](items: Array[T], pred: func(T) -> bool) -> Array[T]:
	var out: Array[T] = []
	for item in items:
		if pred.call(item):
			out.append(item)
	return out

func pair_up[K, V](keys: Array[K], values: Array[V]) -> Dictionary[K, V]:
	var out: Dictionary[K, V] = {}
	for i in keys.size():
		out[keys[i]] = values[i]
	return out

class Animal:
	var name := "animal"

func test():
	var nums: Array[int] = [1, 2, 3]

	var words: Array[String] = map_all(nums, func(n: int) -> String: return "n%d" % n)
	print(words, " ", words.is_typed())

	var kept: Array[int] = keep(nums, func(n: int) -> bool: return n > 1)
	print(kept, " ", kept.is_typed())

	# The element type is checked when the result is converted.
	var doubled := map_all(nums, func(n: int) -> int: return n * 2)
	print(doubled)

	var table: Dictionary[String, int] = pair_up(words, nums)
	print(table.size(), " ", table["n1"])

	# Object element types survive too.
	var animals: Array[Animal] = map_all(nums, func(_n: int) -> Animal: return Animal.new())
	print(animals.size(), " ", animals[0].name)

	# Nesting a generic call inside another keeps working.
	print(keep(map_all(nums, func(n: int) -> int: return n * 10), func(n: int) -> bool: return n > 10))
