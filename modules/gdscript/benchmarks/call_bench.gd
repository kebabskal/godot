extends SceneTree

class Base:
	var v: int = 0:
		set(x):
			v = x
		get:
			return v
	func add(a: int, b: int) -> int:
		return a + b
	func poly() -> int:
		return 1
	func call_poly() -> int:
		return poly()

class Derived extends Base:
	func poly() -> int:
		return 2

func self_add(a: int, b: int) -> int:
	return a + b

func bench(label: String, f: Callable, n: int) -> void:
	var t := Time.get_ticks_usec()
	f.call(n)
	var dt := Time.get_ticks_usec() - t
	print("%-28s %8.1f ns/iter" % [label, float(dt) * 1000.0 / n])

func run_self(n: int) -> void:
	var acc := 0
	for i in n:
		acc = self_add(acc, i)

func run_typed(n: int) -> void:
	var d: Base = Derived.new()
	var acc := 0
	for i in n:
		acc = d.add(acc, i)

func run_virtual(n: int) -> void:
	var d: Base = Derived.new()
	var acc := 0
	for i in n:
		acc += d.call_poly()

func run_property(n: int) -> void:
	var d := Base.new()
	for i in n:
		d.v = i
		var _x := d.v

func run_loop_only(n: int) -> void:
	var acc := 0
	for i in n:
		acc = acc + i

func _init() -> void:
	var n := 2_000_000
	for round in 2:
		bench("loop only (baseline)", run_loop_only, n)
		bench("self call", run_self, n)
		bench("typed var call", run_typed, n)
		bench("virtual via base", run_virtual, n)
		bench("setter+getter", run_property, n)
	quit()
