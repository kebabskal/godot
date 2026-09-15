# Exercises `OPCODE_GET_SCRIPT_MEMBER` / `OPCODE_SET_SCRIPT_MEMBER`: member access on a variable of a
# known script type resolved by index instead of by name. Every case must behave like a name lookup.

class Base:
	var plain: int = 1
	var typed_float: float = 0.0
	var untyped = null
	var with_accessors: int = 0:
		set(v):
			events.append("set %d" % v)
			with_accessors = v
		get:
			events.append("get")
			return with_accessors
	var events: Array[String] = []

class Derived extends Base:
	var extra: String = "extra"

class Unrelated:
	var plain: String = "unrelated"

class Holder:
	var inner: Base = null

func test():
	var b := Base.new()
	var d := Derived.new()

	# Plain members, read and write.
	b.plain = 5
	print(b.plain)
	b.plain += 2
	print(b.plain)

	# Inherited member through a subclass, and a member of the subclass itself.
	d.plain = 7
	print(d.plain)
	d.extra = "changed"
	print(d.extra)

	# Subclass instance held in a base-typed variable (inherited slots keep their index).
	var as_base: Base = d
	as_base.plain = 8
	print(d.plain)

	# Typed member assigned a convertible value is converted, like a name lookup does.
	b.typed_float = 3
	print(b.typed_float)
	print(typeof(b.typed_float) == TYPE_FLOAT)

	# Untyped member takes anything.
	b.untyped = "text"
	print(b.untyped)
	b.untyped = [1, 2]
	print(b.untyped)

	# Members with accessors still go through the accessors, and a convertible value is converted
	# to the member type before the setter sees it.
	b.with_accessors = 3
	print(b.with_accessors)
	@warning_ignore("narrowing_conversion")
	b.with_accessors = 2.7
	print(b.events)

	# Chained access through a member of a known type.
	var holder := Holder.new()
	holder.inner = Base.new()
	holder.inner.plain = 9
	print(holder.inner.plain)

	# Stale-index safety: an object whose script changed at runtime must still resolve by name.
	var swapped: Base = Base.new()
	swapped.set_script(Unrelated)
	print(swapped.plain)
