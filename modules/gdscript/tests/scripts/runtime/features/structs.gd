struct Point:
	var x: float = 0.0
	var y: float = 0.0
	var label: String = "p"

struct Line:
	var a: Point
	var b: Point = Point(1.0, 1.0)

func describe(p: Point) -> String:
	return "%s(%s, %s)" % [p.label, p.x, p.y]

func move(p: Point, dx: float) -> Point:
	p.x += dx # Changes the callee's copy only.
	return p

func test():
	var p := Point(1.0, 2.0)
	print(p)
	print(p.x + p.y)

	# Value semantics: a copy is independent of the original.
	var q := p
	q.x = 10.0
	print(p.x, " ", q.x)
	var moved := move(p, 5.0)
	print(p.x, " ", moved.x)

	# Defaults, equality, and a typed variable without an initializer.
	print(describe(Point()))
	print(Point(1.0, 2.0) == p, " ", Point() == p)
	var d: Point
	print(d.label)

	# Typed arrays of structs; writing through an element writes back into the array.
	var pts: Array[Point] = [Point(0.0, 0.0), Point(1.0, 1.0)]
	pts[1].y = 5.0
	print(pts[1].y)
	var total := 0.0
	for pt in pts:
		total += pt.x + pt.y
	print(total)

	# Nested structs.
	var line := Line()
	print(line.b.x)
	line.a.x = 3.0
	print(line.a.x)
	print(line)

	# Non-constant arguments go through the constructor opcode at runtime.
	var k := 2.0
	var r := Point(k, k * 2.0, "r")
	print(r)

	# Through a Variant the generic path is used and value semantics still hold.
	var v: Variant = p
	v.label = "u"
	print(v.label, " ", p.label)
	print(typeof(p) == TYPE_STRUCT)
