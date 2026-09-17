const Lib = preload("generic_class_qualified.notest.gd")

class Guy:
	uses Lib.Damageable

	func take_damage(amount: int) -> void:
		print("ouch ", amount)

func test():
	# A generic class from another file, with its arguments named through the chain.
	var numbers: Lib.Pool[int] = Lib.Pool.new()
	numbers.add(7)
	print(numbers.first() + 1)

	var words: Lib.Pool[String] = Lib.Pool.new()
	words.add("hi")
	print(words.first().to_upper())

	# Traits and enums from the same file keep working.
	var target: Lib.Damageable = Guy.new()
	target.take_damage(3)
	print(Lib.Element.ICE)
