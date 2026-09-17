# Generic functions: the type parameters are bound at each call.
func first[T](items: Array[T]) -> T:
	return items[0]

func pick[T](a: T, b: T, use_first: bool) -> T:
	return a if use_first else b

func get_or[K, V](dict: Dictionary[K, V], key: K, fallback: V) -> V:
	return dict[key] if dict.has(key) else fallback

func count_where[T](items: Array[T], pred: func(T) -> bool) -> int:
	var total := 0
	for item in items:
		if pred.call(item):
			total += 1
	return total

func find_by[T](items: Array[T], pred: func(T) -> bool, fallback: T) -> T:
	for item in items:
		if pred.call(item):
			return item
	return fallback

class Animal:
	var name := "animal"

class Dog extends Animal:
	func bark() -> String:
		return "woof"

func test():
	var numbers: Array[int] = [3, 1, 2]
	var n := first(numbers)
	print(n + 1, " ", n is int)

	var words: Array[String] = ["hi", "there"]
	print(first(words).to_upper())

	print(pick(10, 20, true), " ", pick("a", "b", false))

	var scores: Dictionary[String, int] = {"a": 1}
	print(get_or(scores, "a", 0) + 5, " ", get_or(scores, "z", -1))

	print(count_where(numbers, func(x: int) -> bool: return x > 1))

	# The bound type keeps its members.
	var dogs: Array[Dog] = [Dog.new()]
	print(first(dogs).bark())
	print(find_by(dogs, func(d: Dog) -> bool: return d.name == "animal", null) != null)

	# A local can still use the type parameter.
	print(collect(numbers))

func collect[T](items: Array[T]) -> String:
	var seen: Array[T] = []
	for item in items:
		seen.append(item)
	return str(seen.size())
