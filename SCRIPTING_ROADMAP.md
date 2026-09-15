# Scripting roadmap for this fork

This fork tracks `godotengine/godot` master with no divergence of its own. All
work happens on feature branches; `master` is kept identical to upstream.

Goal: make GDScript fast enough for simulations with tens of thousands of
objects (the raylib + C# experience) and fix the ergonomic gaps that push
people to C#. C# stays supported but GDScript is the primary language.

## Findings from reading the tree (Sept 2026, 4.8 dev)

- Every VM value is a boxed `Variant`, including typed locals. Validated
  operators are indirect calls through function pointers with `Variant *`
  arguments, so `a + b` on typed ints is never a machine add.
- A script-to-script call goes through the generic `OPCODE_CALL`, then
  `Variant::callp` -> `Object::callp` -> `GDScriptInstance::callp`, which does
  a `StringName` hash lookup while walking the base-class chain, then
  `GDScriptFunction::call` allocates a fresh stack and copies every argument.
  Native engine methods have a validated fast path; script methods do not.
- Cross-object member access on script types goes through named get/set.
- C# engine-to-script calls go through a GC handle and a source-generated
  chain of `StringName` compares; C# to engine uses ptrcall and is fine.
- Both languages pay the same boundary tax: Variant boxing, StringName
  lookup, MethodBind dispatch.

## Decisions

- JIT and AOT export are deferred. Big, risky, and the interpreter-side work
  gets most of the practical win first.
- Structs must be a core `Variant` type, not a GDScript-only construct, so
  engine APIs can return them and C# can consume them.
- Error handling uses multiple returns with `if var ok, v := f():`. No
  `Result` type.
- Interfaces follow the upstream `trait` work rather than a second concept.
- Parked as low priority: namespaces (opt-in only), generators, explicit
  value-semantics qualifiers, rename refactoring (already in the LSP).

## Ordered wishlist

Foundation, in this order:

1. Structs as a core Variant type. Value semantics, operator overloads, no
   inheritance. Unlocks 2, 3 and the perf work.
2. No `Dictionary` returns from engine APIs. Physics first.
3. Fixed multidimensional arrays of real values.
4. Perf, interpreter only for now:
   a. Direct script-to-script calls (vtable slot + guard, no StringName
      lookup).  <- current work
   b. Member access by index across known script types.
   c. Cheaper argument passing / stack setup in `GDScriptFunction::call`.
   d. Typed IR with unboxed registers for int/float/bool/vectors; box only
      at boundaries.

Type system:

5. Strict mode per project. Untyped is an error; `:=` inference stays.
6. Typed Callables, then generic functions, then user-defined generic
   classes. `Array[T].map` infers `Array[U]`.
7. Interfaces via `trait`. Structs can implement them.
8. Nullable types with `?.` and `??`. Only meaningful with strict mode.
9. Typed signals checked at emit and connect.
10. Enums as real types with methods; exhaustive `match`.

Syntax:

11. Multiple return values.
12. `for key, value in dict` and `for i, item in array`.
13. String interpolation, `f"{x}"`.

Editor and tooling:

14. Typed `PackedScene` exports (slot only accepts a given root type).
15. A formatter.

Critical path: 1 -> 5 -> 4d. Items 4a-4c, the syntax group and the editor
group are independent and can proceed in any order.

## Progress

- 4a landed as `OPCODE_CALL_SCRIPT` (slot + name guard, falls back to the
  name lookup when the slot is stale). Measured with
  `modules/gdscript/benchmarks/call_bench.gd`, Windows/MSVC editor build,
  ns per iteration, before -> after:

  | Case                  | Before  | After   |
  |-----------------------|---------|---------|
  | loop only (baseline)  | 17      | 16      |
  | self call             | 112-122 | 97      |
  | typed var call        | 127-141 | 110     |
  | virtual via base      | 165-186 | 142-150 |
  | setter+getter         | 189-204 | 189-207 |

  Only 15-20% off a call. The name lookup was not the dominant cost;
  what remains is `GDScriptFunction::call` itself (stack allocation,
  argument copying, Variant construction), which is item 4c. Expect 4c
  to matter more than 4a did. Setter/getter calls did not move at all.
- 4c, first round: plain-value arguments and returns (int, float, bool,
  vectors; untyped or matching the declared built-in type) are copied
  bitwise instead of through the type check and the Variant copy
  constructor. Editor build, ns per iteration, before -> after: self call
  96 -> 72, 4 args 134 -> 98, typed var call 108 -> 88, setter+getter
  187 -> 164. A 2-argument call is now ~57 ns over the loop, of which
  ~45 ns is fixed per call and ~6 ns per argument.
- 4c, tried and rejected (each measured, no gain, reverted): skipping the
  `self` reference when the caller holds one (atomics are not the cost);
  a precomputed stack template copied in per call (copying 24 bytes per
  slot costs what the NIL loop and typed init saved). What remains is
  spread thinly over the prologue and epilogue; no single item left is
  worth more than a few ns in the interpreter design. Further call cost
  reduction belongs to 4d.
- Release template builds (`target=template_release`) hang on this
  Windows machine when loading any script via `-s`, also at the branch's
  base commit, so it is not caused by this work. Numbers above are from
  the editor build, which adds line tracking and call stack bookkeeping
  to every call.
- Lessons from 4a: the result of a discarded call must never be written
  to the shared `nil` stack slot (GH-70964), and `_ready` must keep
  going through `GDScriptInstance::callp()` so `@onready` runs first.

## Rough performance expectations (times slower than well-written C)

| Axis                          | Today    | 4a-4c   | 4d      |
|-------------------------------|----------|---------|---------|
| Tight numeric loop            | 40-100x  | same    | 8-20x   |
| Script-to-script call         | 100-300x | 30-60x  | 10-20x  |
| Field access, other object    | 200-400x | 20-40x  | 3-5x    |
| Trivial engine call           | 10-30x   | same    | 5-10x   |
| Heavy engine call             | ~1.1x    | same    | same    |

A Node per entity is expensive regardless of language. The target pattern is
a script loop over struct arrays plus MultiMesh / RenderingServer.
