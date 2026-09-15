extends SceneTree

# Micro-benchmark for the script call path. Run headless:
#   godot --headless -s modules/gdscript/benchmarks/call_bench.gd
# Use a `template_release` build for numbers that matter; editor builds add
# line tracking and call stack bookkeeping to every call.

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

class Plain extends Object:
	func add(a: int, b: int) -> int:
		return a + b

func self_add(a: int, b: int) -> int:
	return a + b

func self_zero() -> int:
	return 1

func self_four(a: int, b: int, c: int, d: int) -> int:
	return a + b + c + d

func self_untyped(a, b):
	return a + b

func self_void(_a: int) -> void:
	pass

func self_locals(a: int, b: int) -> int:
	var l0 := a
	var l1 := b
	var l2 := l0
	var l3 := l1
	var l4 := l2
	var l5 := l3
	var l6 := l4
	var l7 := l5
	return l6 + l7

# Same statement count as `self_locals` but only one local: separates the cost of
# extra stack slots from the cost of the extra opcodes.
func self_stmts(a: int, b: int) -> int:
	var l := a
	l = b
	l = a
	l = b
	l = a
	l = b
	l = a
	l = b
	return l + a

func bench(label: String, f: Callable, n: int) -> void:
	var t := Time.get_ticks_usec()
	f.call(n)
	var dt := Time.get_ticks_usec() - t
	print("%-28s %8.1f ns/iter" % [label, float(dt) * 1000.0 / n])

func run_loop_only(n: int) -> void:
	var acc := 0
	for i in n:
		acc = acc + i

func run_self(n: int) -> void:
	var acc := 0
	for i in n:
		acc = self_add(acc, i)

func run_self_zero(n: int) -> void:
	var acc := 0
	for i in n:
		acc += self_zero()

func run_self_four(n: int) -> void:
	var acc := 0
	for i in n:
		acc = self_four(acc, i, i, i)

func run_self_untyped(n: int) -> void:
	var acc := 0
	for i in n:
		acc = self_untyped(acc, i)

func run_self_void(n: int) -> void:
	for i in n:
		self_void(i)

func run_self_locals(n: int) -> void:
	var acc := 0
	for i in n:
		acc = self_locals(acc, i)

func run_self_stmts(n: int) -> void:
	var acc := 0
	for i in n:
		acc = self_stmts(acc, i)

func run_typed(n: int) -> void:
	var d: Base = Derived.new()
	var acc := 0
	for i in n:
		acc = d.add(acc, i)

func run_plain_object(n: int) -> void:
	var p := Plain.new()
	var acc := 0
	for i in n:
		acc = p.add(acc, i)
	p.free()

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

func _init() -> void:
	var build := "editor" if OS.has_feature("editor") else ("release" if OS.has_feature("template_release") else "debug")
	print("build: %s" % build)
	var n := 2_000_000
	for round in 2:
		bench("loop only (baseline)", run_loop_only, n)
		bench("self call", run_self, n)
		bench("self call, 0 args", run_self_zero, n)
		bench("self call, 4 args", run_self_four, n)
		bench("self call, untyped args", run_self_untyped, n)
		bench("self call, void", run_self_void, n)
		bench("self call, 8 locals", run_self_locals, n)
		bench("self call, 8 stmts 1 local", run_self_stmts, n)
		bench("typed var call", run_typed, n)
		bench("typed var call (Object)", run_plain_object, n)
		bench("virtual via base", run_virtual, n)
		bench("setter+getter", run_property, n)
	quit()
