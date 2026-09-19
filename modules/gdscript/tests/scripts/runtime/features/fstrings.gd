# f-strings: `{expression}` fields, with Python's format specs after `:`.

func test():
	var name := "Bob"
	var hp := 42
	var ratio := 0.755
	var price := 1234.5
	var neg := -7
	print(f"{name} has {hp} HP")
	print(f"{hp:05}")
	print(f"{price:,.2f}")
	print(f"{ratio:.1%}")
	print(f"{name:<6}|")
	print(f"{name:>6}|")
	print(f"{name:^7}|")
	print(f"{name:*^9}")
	print(f"{hp:x} {hp:#x} {hp:X} {hp:#b} {hp:o}")
	print(f"{5:08b}")
	print(f"{neg:+} {hp:+} {hp: }")
	print(f"{neg:05}")
	print(f"{neg:=6}")
	print(f"{1234567:,}")
	print(f"{1234567:_}")
	print(f"{0xdeadbeef:_x}")
	print(f"{3.14159:.2f}")
	print(f"{3.14159:.3}")
	print(f"{12345.678:e}")
	print(f"{0.000123:g}")
	print(f"{2.5:f}")
	print(f"{hp:.2f}")
	print(f"{hp=}")
	print(f"{hp = }")
	print(f"{price=:.1f}")
	print(f"{{literal}} {hp}")
	print(f"{ {'a': 1}['a'] }")
	print(f"{f'{hp}'}")
	print(f"{name:.2}")
	print(f"{-0.0:.1f}")
	print(f"{INF:.2f}")
	print(f"{1234.5678:>12,.2f}|")
	print(f"{-42:+08.1f}")
	print(f"{0.5:%}")
	print(f"{7:#o}")
	# What Python has no word for: Godot's own values, and a float with no spec.
	print(f"{price} {0.1 + 0.2} {Vector2(1, 2)} {true}")
	print(f"{Vector2(1, 2):>12}|")
	print(f"{hp if hp > 10 else 0} {[1, 2][1]} {name.length()}")
	print(f'{name}' + f"")
	print(f"plain")
	print(f"""two
{hp} lines""")
	print(f"tab\t{hp}")
	var typed: String = f"{hp:03}"
	print(typed.length())
