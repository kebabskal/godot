# Exercises `OPCODE_CALL_SCRIPT`: script-to-script calls resolved through method slots
# instead of a name lookup. Every case here must dispatch exactly like a name-based call.

class Base:
	var log: Array[String] = []
	var value: int = 0:
		set(v):
			log.append("set %d" % v)
			value = v
		get:
			log.append("get")
			return value

	func describe() -> String:
		return "base"

	# Calls a method that subclasses override: must dispatch to the most-derived one.
	func describe_via_base() -> String:
		return "via base: " + describe()

	func only_in_base(a: int, b: int = 10) -> int:
		return a + b

	func no_return() -> void:
		log.append("no_return")

	func fib(n: int) -> int:
		if n < 2:
			return n
		return fib(n - 1) + fib(n - 2)

class Derived extends Base:
	func describe() -> String:
		return "derived"

	func call_inherited() -> int:
		return only_in_base(1) + self.only_in_base(1, 2)

class Unrelated:
	func describe() -> String:
		return "unrelated"

	func extra() -> String:
		return "extra"

func helper_defined_later(x: int) -> int:
	return x * 2

func test():
	# Self call to a method defined later in the file.
	print(helper_defined_later(21))

	var b := Base.new()
	var d := Derived.new()

	# Virtual dispatch through a call site compiled in the base class.
	print(b.describe_via_base())
	print(d.describe_via_base())

	# Inherited method resolved by walking the base chain, with and without default args.
	print(d.call_inherited())

	# Typed variable holding a subclass instance.
	var as_base: Base = d
	print(as_base.describe())
	print(as_base.only_in_base(5))

	# Void call and recursion.
	b.no_return()
	print(b.log)
	print(b.fib(15))

	# Inline setter/getter go through the same path.
	d.value = 3
	print(d.value)
	print(d.log)

	# Stale-slot safety: an object whose script changed at runtime must still resolve by name.
	var swapped: Base = Base.new()
	swapped.set_script(Unrelated)
	print(swapped.describe())
