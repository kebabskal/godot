class Pool[T]:
	var last: T

	func add(item: T) -> void:
		last = item

	func first() -> T:
		return last

# Naming a specialisation: `IntPool` has no type parameters of its own, so it is an
# ordinary class everywhere a class can be used.
class IntPool extends Pool[int]:
	func doubled() -> int:
		return first() * 2

class Middle[U] extends Pool[U]:
	func twice(item: U) -> void:
		add(item)
		add(item)

class StringStack extends Middle[String]:
	pass

func test():
	var numbers := IntPool.new()
	numbers.add(3)
	print(numbers.first() + 1)
	print(numbers.doubled())
	var inherited: int = numbers.last
	print(inherited)

	# Two levels: `StringStack` -> `Middle[String]` -> `Pool[String]`.
	var words := StringStack.new()
	words.twice("hi")
	print(words.first().to_upper())
