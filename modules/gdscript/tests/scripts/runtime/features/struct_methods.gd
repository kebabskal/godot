# Methods and operator overloads on script structs.
struct Vec:
	var x: float = 0.0
	var y: float = 0.0

	func length_sq() -> float:
		return x * x + y * y

	func scaled(f: float) -> Vec:
		return Vec(x * f, y * f)

	func scale(f: float) -> void:
		x *= f
		y *= f

	func flip() -> void:
		scale(-1.0)

	func describe(prefix: String = "v") -> String:
		return "%s(%s, %s)" % [prefix, x, self.y]

	func _add(other: Vec) -> Vec:
		return Vec(x + other.x, y + other.y)

	func _mul(f: float) -> Vec:
		return scaled(f)

	func _neg() -> Vec:
		return Vec(-x, -y)

	func _lt(other: Vec) -> bool:
		return length_sq() < other.length_sq()

struct Line:
	var start: Vec = Vec()
	var end: Vec = Vec()

	func grow(f: float) -> void:
		end.scale(f)

var member := Vec(1.0, 1.0)
static var shared := Vec(2.0, 0.0)

func mutate_param(p: Vec) -> void:
	p.scale(100.0)

func test():
	var v := Vec(3.0, 4.0)
	print(v.length_sq())
	print(v.describe())
	print(v.describe("p"))
	v.scale(2.0)
	print(v)
	v.flip()
	print(v)

	# Mutating methods write through to a member and a static variable, but a parameter is a copy.
	member.scale(3.0)
	print(member)
	shared.scale(0.5)
	print(shared)
	mutate_param(v)
	print(v)

	# Operators with known types.
	var a := Vec(1.0, 2.0)
	var b := Vec(10.0, 20.0)
	print(a + b)
	print(a * 3.0)
	print(-a)
	print(a < b, " ", b < a)

	# Methods and operators through Variant.
	var u: Variant = a
	print(u + b)
	@warning_ignore("unsafe_method_access")
	print(u.length_sq())
	@warning_ignore("unsafe_method_access")
	u.scale(2.0)
	print(u)

	# Sorting uses `_lt`.
	var arr: Array[Vec] = [Vec(3.0, 0.0), Vec(1.0, 0.0), Vec(2.0, 0.0)]
	arr.sort()
	print(arr)

	# A nested struct field mutated inside a method.
	var line := Line(Vec(1.0, 1.0), Vec(2.0, 2.0))
	line.grow(2.0)
	print(line)

	# A copy taken before a mutation keeps its value.
	var c := Vec(1.0, 1.0)
	var d := c
	c.scale(5.0)
	print(c, " ", d)
