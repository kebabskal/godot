# Exercises the inline typed binary operators (`GDSCRIPT_TYPED_BINARY_OPCODES`) and the assign
# peephole that writes an operator result straight into a local. Results must match the generic path.

func param_target(x: int) -> int:
	x = x + 1
	x += 2
	return x

func test():
	# Int arithmetic into a local, including aliasing of target and operand.
	var a := 10
	var b := 3
	a = a + b
	print(a)
	a = a - b
	print(a)
	a = a * b
	print(a)
	a = b - a
	print(a)
	a += 5
	print(a)
	a = a + a
	print(a)

	# Comparisons produce bools, also into a typed local and in conditions.
	var lt := a < b
	print(lt)
	print(a == a, a != b, a <= b, a >= b, a > b)
	if b > a:
		print("greater")

	# Float arithmetic, including division.
	var f := 1.5
	f = f * 4.0
	print(f)
	f = f / 4.0
	print(f)
	f = f - 0.5
	print(f)
	print(f < 2.0, f == 1.0, f >= 1.0)

	# Mixed int/float in both orders promotes to float.
	var g: float = 2
	g = g * 3
	print(g)
	g = 3 * g
	print(g)
	g = g + 1
	print(g)
	g = 1 - g
	print(g)
	g = g / 2
	print(g)
	g = 9 / g
	print(g)

	# Operator result into an untyped local goes through a temporary and still works.
	var u = a + b
	print(u)
	var v = f * 2.0
	print(v)

	# A parameter as the target.
	print(param_target(4))

	# Typed locals keep their type after the operator wrote into them.
	print(typeof(a) == TYPE_INT, typeof(f) == TYPE_FLOAT, typeof(g) == TYPE_FLOAT, typeof(lt) == TYPE_BOOL)

	# Float division by zero follows IEEE like the generic path.
	var z := 0.0
	print(1.0 / z)

	# Int division, modulo (truncating toward zero, like C), bitwise and shifts.
	var n := -7
	@warning_ignore("integer_division")
	print(n / 2, " ", n % 3, " ", 7 / 2, " ", 7 % 3)
	var m := 12
	m = m & 10
	print(m)
	m = m | 1
	print(m)
	m = m ^ 15
	print(m)
	m = m << 3
	print(m)
	m = m >> 2
	print(m)

	# Unary operators into locals and temporaries.
	var neg := -m
	print(neg)
	neg = -neg
	print(neg)
	var nf := -1.25
	nf = -nf
	print(nf)
	var flag := true
	flag = not flag
	print(flag, not flag)
	var bits := 5
	bits = ~bits
	print(bits)

	# Vector2 and Vector3 arithmetic with vectors and scalars in both orders.
	var p := Vector2(1.0, 2.0)
	var q := Vector2(3.0, 5.0)
	p = p + q
	print(p)
	p = p - Vector2(1.0, 1.0)
	print(p)
	p = p * q
	print(p)
	p = p / q
	print(p)
	p = p * 2.0
	print(p)
	p = 0.5 * p
	print(p)
	p = p * 3
	print(p)
	p = 2 * p
	print(p)
	p = p / 4.0
	print(p)
	p = p / 2
	print(p)
	p = -p
	print(p)
	print(p == Vector2(-1.5, -3.0), p != q)
	var r := Vector3(1.0, 2.0, 3.0)
	r = r + Vector3(1.0, 1.0, 1.0)
	r = r * 2.0
	r = 2 * r
	r = r / Vector3(2.0, 2.0, 2.0)
	r = -r
	print(r)
	print(r == Vector3(-4.0, -6.0, -8.0))
