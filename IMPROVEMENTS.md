# What's different in this fork

This is a fork of Godot's `master` branch focused on **GDScript**: making it
faster, and giving it the type-system features that larger projects keep
asking for.

Everything here is backwards compatible. Existing projects keep working
unchanged; the new features are opt-in, and the speedups apply automatically
to code that already uses static types.

Contents:

- [Faster GDScript](#faster-gdscript)
- [Strict mode](#strict-mode)
- [Structs](#structs)
- [Engine APIs that return structs instead of dictionaries](#engine-apis-that-return-structs-instead-of-dictionaries)
- [Traits](#traits)
- [Typed Callables](#typed-callables)
- [Generic functions](#generic-functions)
- [Generic classes](#generic-classes)
- [Short lambdas](#short-lambdas)
- [`for` loops with two variables](#for-loops-with-two-variables)
- [Nullable types](#nullable-types)
- [Editor support](#editor-support)
- [Roadmap](#roadmap)

---

## Faster GDScript

Typed GDScript runs substantially faster. Nothing in your project changes:
if your code has type hints, it is already on the fast paths.

| What                                  | Before    | After    |
|---------------------------------------|-----------|----------|
| Calling a method on a typed variable  | 127-141   | 88       |
| Calling a method on `self`            | 112-122   | 72       |
| Calling an overridden method          | 165-186   | 117      |
| Reading + writing a field (typed)     | 77        | 49       |
| Reading + writing through a setter/getter | 189-204 | 118    |
| `acc = acc + i` (typed ints)          | 8.1       | 4.4      |
| Integer divide / modulo / negate      | 15        | 5.6      |
| An empty loop iteration               | 16.5      | 11.9     |

Nanoseconds per operation, measured on Windows with an editor build. Each
row was measured immediately before and after the change that affected it.

Four things changed to get there:

- **Script-to-script calls skip the name lookup.** Calling a method used to
  search for it by name on every call. Now the lookup happens once at
  compile time.
- **Field access by index.** Reading `player.health` on a variable of known
  type goes straight to the field instead of searching by name.
- **Cheaper arguments.** Simple values (ints, floats, booleans, vectors)
  are passed and returned without the generic copying machinery.
- **Arithmetic runs inline.** `a + b` on two known ints, floats, booleans,
  `Vector2`s or `Vector3`s is executed directly instead of calling out to a
  generic operator function. Comparisons that feed an `if` are fused with
  the branch.

**Untyped code is deliberately unchanged.** A variable with no type still
goes through the old, slower paths, because the interpreter cannot know
what it holds. This is what strict mode below is for.

For reference, on a release build a direct script call now costs about
40 ns plus 8 ns per argument, and a typed arithmetic statement costs 3-5 ns.

---

## Strict mode

A project setting, `debug/gdscript/strict_mode`, that turns GDScript's
untyped and unsafe warnings into errors that cannot be silenced with
`@warning_ignore`.

With it on, every variable, parameter and return value needs a type, so all
of your code lands on the fast paths described above. Dynamic code is still
possible, but you have to ask for it:

```gdscript
var speed := 10.0              # fine, inferred from a typed value
var health: int = 100          # fine, explicit
var anything: Variant = get_thing()   # fine, explicitly dynamic
var mystery = get_thing()      # error in strict mode
```

Turn it on per project. Existing projects are unaffected until you do.

---

## Structs

A struct is a value with a fixed set of named, typed fields. Unlike a
dictionary, it cannot gain or lose fields, its fields have types, and typos
are caught when you save the file rather than when the code runs.

```gdscript
struct Point:
    var x: float = 0.0
    var y: float = 0.0

func _ready():
    var p := Point(1.0, 2.0)   # positional, remaining fields take defaults
    var q := Point()           # all defaults
    p.y += 1.0
    print(p)                   # Point(x: 1.0, y: 3.0)
```

Structs are **values**, not references. Assigning one copies it, and passing
one to a function gives the function its own copy:

```gdscript
var a := Point(1.0, 1.0)
var b := a
a.x = 5.0
print(b.x)   # 1.0, unaffected
```

The copy is lazy, so passing structs around is as cheap as passing an array.

The name is a type anywhere a type is expected:

```gdscript
var p: Point
var path: Array[Point] = []
func closest(to: Point) -> Point: ...
```

### Methods and operators

Structs can have methods. Follow the convention of the built-in value types
and return a new value, like `Vector2.normalized()` does:

```gdscript
struct Vec:
    var x: float = 0.0
    var y: float = 0.0

    func length_sq() -> float:
        return x * x + y * y

    func scaled(f: float) -> Vec:
        return Vec(x * f, y * f)

    func _add(other: Vec) -> Vec:
        return Vec(x + other.x, y + other.y)

    func _lt(other: Vec) -> bool:
        return length_sq() < other.length_sq()

func _ready():
    var v := Vec(3.0, 4.0)
    print(v.length_sq())        # 25.0
    v = v.scaled(2.0)
    print(v + Vec(1.0, 1.0))    # uses _add

    var points: Array[Vec] = [Vec(3.0, 0.0), Vec(1.0, 0.0)]
    points.sort()               # uses _lt
```

Inside a method, `self` is the struct and the fields are available by name.
The operator methods are `_add`, `_sub`, `_mul`, `_div`, `_mod`, `_neg`,
`_lt`, `_le`, `_gt` and `_ge`. Equality compares fields and needs no method.

A method that assigns to a field changes the variable it was called on:

```gdscript
struct Health:
    var hp: int = 10

    func damage(n: int) -> void:
        hp -= n

var h := Health()
h.damage(3)
print(h.hp)   # 7
```

Such a method can only be called on something that can be written back to,
so `Health().damage(1)` is rejected: it would only change a temporary. The
returning form works everywhere, including on array elements, which is why
it is the recommended style.

Structs can be saved in scenes and resources and sent over the network.

---

## Engine APIs that return structs instead of dictionaries

Many engine calls return a `Dictionary` whose keys you have to remember and
spell correctly. Those calls now have struct-returning twins. The original
dictionary versions are untouched.

**Physics queries.** Every space state query has a `_struct` twin:

```gdscript
var space := get_world_3d().direct_space_state
var hit: PhysicsRayResult3D = space.intersect_ray_struct(query)
if hit.hit:
    print(hit.position, " ", hit.normal, " ", hit.collider)
```

Note `hit.hit`: a miss returns a struct with default values instead of an
empty dictionary, so there is no separate "is it empty" check.

| Method | Returns |
|---|---|
| `intersect_ray_struct` | `PhysicsRayResult2D` / `3D` |
| `intersect_point_struct` | `Array[PhysicsShapeResult2D/3D]` |
| `intersect_shape_struct` | `Array[PhysicsShapeResult2D/3D]` |
| `cast_motion_struct` | `PhysicsCastResult2D/3D` |
| `get_rest_info_struct` | `PhysicsRestInfo2D/3D` |

**Dates and times.** `Time` has a struct version of every dictionary
method, named without `_dict`:

```gdscript
var now: DateTime = Time.get_datetime_from_system()
print(now.year, "-", now.month, "-", now.day, " ", now.hour)

var stamp := Time.get_unix_time_from_datetime(DateTime(2000, 1, 1))
```

The layouts are `DateTime`, `Date`, `TimeOfDay` and `TimeZoneInfo`.

**Others:**

```gdscript
var hit: TriangleMeshHit = mesh.intersect_ray_struct(from, dir)
var mem: MemoryInfo = OS.get_memory_info_struct()
var version: VersionInfo = Engine.get_version_info_struct()
```

All of these work as typed arrays too, so `Array[PhysicsShapeResult3D]`
gives you completion on every element.

---

## Traits

A trait is a set of methods and properties that a class or struct promises
to provide. It is the interface feature GDScript has been missing, and it
follows the syntax Godot's maintainers asked for upstream, so code written
against it should stay valid.

```gdscript
trait Damageable:
    var hp: int                        # the user must have this property

    func take_damage(amount: int) -> void    # required: no body
    func is_alive() -> bool                  # required

class Enemy:
    uses Damageable
    var hp: int = 10

    func take_damage(amount: int) -> void:
        hp -= amount

    func is_alive() -> bool:
        return hp > 0
```

Forgetting a method or giving it the wrong signature is an error at the
`uses` line, not a crash at runtime.

A trait is a type, and works at runtime:

```gdscript
func hit(target: Damageable, amount: int) -> void:
    target.take_damage(amount)      # checked against the trait

func _ready():
    if node is Damageable:
        hit(node, 5)

    var d := node as Damageable     # null if it does not use the trait
    var all: Array[Damageable] = [enemy, boss]
```

Subclasses inherit the traits of their base, and **structs can use traits
too**, so one function can accept both an object and a struct.

### Default methods

A trait method with a body is a default: every user gets it unless it
provides its own. This is where traits stop being just interfaces:

```gdscript
trait Mortal:
    var hp: int

    func take_damage(amount: int) -> void:
        hp -= amount

    func is_alive() -> bool:
        return hp > 0

class Enemy:
    uses Mortal
    var hp: int = 10      # that is the whole class

class Armored extends Enemy:
    func take_damage(amount: int) -> void:   # overrides the default
        hp -= maxi(amount - 2, 0)
```

A default method can use the trait's own properties and methods, and calls
between them respect overrides.

### Constants, signals, and traits built from traits

```gdscript
trait Named:
    const UNKNOWN := "???"
    var label: String

    func get_label() -> String:
        return label if label != "" else UNKNOWN

trait Mortal:
    signal died(who: String)
    var hp: int

    func on_death() -> void

    func take_damage(amount: int) -> void:
        hp -= amount
        if hp <= 0:
            on_death()

trait Character:
    uses Named, Mortal

    # Satisfies Mortal's requirement using what Named provides.
    func on_death() -> void:
        died.emit(get_label())

class Hero:
    uses Character
    var label: String = "Link"
    var hp: int = 3
```

`Hero` gets the `died` signal, both traits' methods, and satisfies `Named`,
`Mortal` and `Character` for `is` checks and typed parameters.

Conformance is by name, not by shape: a class satisfies a trait only if it
says `uses`. A class that happens to have matching methods does **not**
pass an `is` check, which keeps the errors clear and the runtime check
cheap.

A trait from another script works like any other member:

```gdscript
const Lib = preload("res://traits.gd")

class Person:
    uses Lib.Greeter
```

---

## Typed Callables

A `Callable` can now say what it takes and returns, so passing the wrong
function is caught when you save the file:

```gdscript
func apply(f: func(int) -> int, x: int) -> int:
    return f.call(x)

func make_adder(n: int) -> func(int) -> int:
    return func(amount: int) -> int: return amount + n

func _ready():
    var add_ten := make_adder(10)
    print(add_ten.call(5))      # 15, and the result is known to be an int

    var on_hit: func(int) -> void = _handle_hit
```

The return type of `.call()` is known, so results flow into typed code
without a cast. Plain `Callable` still works everywhere and is still
accepted where a typed callable is expected.

---

## Generic functions

A function can take type parameters, so one helper works for every element
type and the caller keeps theirs:

```gdscript
func first[T](items: Array[T]) -> T:
    return items[0]

func get_or[K, V](dict: Dictionary[K, V], key: K, fallback: V) -> V:
    return dict[key] if dict.has(key) else fallback

func _ready():
    var scores: Array[int] = [3, 1, 2]
    var n := first(scores)          # n is an int, not a Variant
    print(n + 1)

    var names: Array[String] = ["ada"]
    print(first(names).to_upper())  # String's members, no cast

    var stock: Dictionary[String, int] = {"apple": 3}
    print(get_or(stock, "pear", 0) + 1)
```

The type parameters are worked out from the arguments at each call, and the
result carries the type through, so `first(names)` really is a `String`.
Mixing them up is caught:

```gdscript
func pick[T](a: T, b: T) -> T:
    return a

pick(1, "two")     # error: argument 2 should be "int" but is "String"
```

Typed callables combine with this, which is where it earns its keep:

```gdscript
func count_where[T](items: Array[T], pred: func(T) -> bool) -> int:
    var total := 0
    for item in items:
        if pred.call(item):
            total += 1
    return total

count_where(scores, func(x: int) -> bool: return x > 1)
```

Inside the function the type parameter is opaque: the body has to work for
every possible `T`, so `T` has no members of its own. Reaching into one is
an unsafe access, which strict mode turns into an error.

A generic function can also build and return collections, so the `map` and
`filter` you would otherwise write once per element type can be written
once:

```gdscript
func map_all[T, U](items: Array[T], f: func(T) -> U) -> Array[U]:
    var out: Array[U] = []
    for item in items:
        out.append(f.call(item))
    return out

func _ready():
    var nums: Array[int] = [1, 2, 3]
    var words: Array[String] = map_all(nums, func(n: int) -> String: return "n%d" % n)
    print(words)    # ["n1", "n2", "n3"], a real Array[String]
```

The returned collection really carries its element type, so it can be passed
straight into anything expecting `Array[String]`. Building it costs one
extra pass over the result, because the element type is only known where the
function is called. If a generic function puts the wrong type in, you get a
clear error at the call, not a mistyped collection.

### Built-in container methods keep their types

Filtering or copying a typed collection used to lose the element type,
forcing a cast. Now the types come through:

```gdscript
var nums: Array[int] = [3, 1, 2]

var big: Array[int] = nums.filter(func(x: int) -> bool: return x > 1)
var part: Array[int] = nums.slice(0, 2)
var copy: Array[int] = nums.duplicate()
var n := nums.front()          # an int

var scores: Dictionary[String, int] = {"a": 1}
var names: Array[String] = scores.keys()
var values: Array[int] = scores.values()
```

Two groups deliberately stay untyped, because sharpening them would be
wrong. `map()` really does return an untyped array, since the element type
changes. And `pop_back()`, `pop_front()`, `min()` and `max()` return `null`
when the container is empty, so they stay `Variant` and the familiar
pop-until-empty loop keeps working.

## Generic classes

A class can take type parameters too, so a container or service is written
once and each instance keeps its own element type:

```gdscript
class Pool[T]:
    var items: Array[T] = []
    var last: T

    func add(item: T) -> void:
        items.append(item)
        last = item

    func first() -> T:
        return items[0]

func _ready():
    var ints: Pool[int] = Pool.new()
    ints.add(3)
    print(ints.first() + 1)        # first() is an int here
    print(ints.last + 1)

    var words: Pool[String] = Pool.new()
    words.add("hi")
    print(words.first().to_upper())   # and a String here

    ints.add("oops")   # error: argument 1 should be "int" but is "String"
```

`Pool.new()` takes its binding from the variable it is assigned to, so there
is no separate construction syntax. The bindings are exact: a `Pool[String]`
is not a `Pool[int]`, and mixing them is an error that names both.

A constructor can use the type parameter as well, and its arguments bind it,
so a `func() -> T` parameter is checked against what you actually pass:

```gdscript
class Tiles[T]:
    var items: Array[T] = []

    func _init(create: func() -> T) -> void:
        items.append(create.call())

    func first() -> T:
        return items[0]

func _ready():
    var tiles: Tiles[Tile] = Tiles.new(() => Tile.new())
    print(tiles.first().name)
```

Where two arguments have to agree, disagreeing is an error:

```gdscript
class Pair[T]:
    func _init(first: T, second: T) -> void:
        print(first, second)

Pair.new(1, "two")   # error: argument 2 should be "int" but is "String"
```

What is not checked is the binding against the annotation: because
`Tiles.new(...)` carries no arguments of its own, a
`var t: Tiles[Other] = Tiles.new(() => Tile.new())` is accepted today.

A type parameter can be **bounded**, which is what lets the class do anything
with the values it holds:

```gdscript
trait Named:
    func get_name() -> String

class Registry[T: Named]:
    var items: Array[T] = []

    func add(item: T) -> void:
        items.append(item)
        print("added ", item.get_name())   # allowed: every T is a Named

func _ready():
    var people: Registry[Person] = Registry.new()
    people.add(Person.new())

    var rocks: Registry[Rock] = Registry.new()
    # error: Type parameter "T" of "Registry" is bound to "Rock", which is not a "Named".
```

The bound can be a trait, one of your classes, or an engine class
(`[T: Node2D]` lets the body read `item.position`). Without a bound a type
parameter is opaque: you can store it and hand it back, but not call anything
on it, and under strict mode reaching into one is an error. A bounded `T` also
goes wherever the bound goes, so `var named: Named = item` is fine; the
reverse is not, because the binding may be narrower than the bound.

Functions take bounds too: `func label[T: Named](item: T) -> String`.

A method returning `Array[T]` gives the caller a real `Array[T]`, built at
the call, so it is a copy rather than a reference to the class's own array.
A `var items: Array[T]` read from outside the class is a plain `Array`: the
class is compiled once, so the array it holds has no element type at
runtime, and saying otherwise would be a promise the value cannot keep.

## Short lambdas

GDScript's lambdas are blocks, which reads badly inside a call. There is now
an arrow form for the common case of a single expression:

```gdscript
var names := items.map(item => item.name)
var alive := units.filter(u => u.hp > 0)
units.sort_custom((a, b) => a.hp < b.hp)
var done := () => print("finished")
```

`params => expression` is exactly `func(params): return expression`, so
captures and everything else behave as before. One parameter needs no
parentheses; zero or several do. A body that returns nothing, such as a
`print()`, is fine: the lambda simply returns nothing too.

This holds under strict mode too. A lambda's parameters and return type are
inferred, so strict mode asks for nothing that the compiler already worked
out; a lambda with nothing to infer from is still reported, and a named
function still needs its return type written out.

The parameter types come from the call, so they are known without writing
them out:

```gdscript
var nums: Array[int] = [1, 2, 3]

nums.map(n => n * 2)        # n is an int
nums.map(n => n.to_upper()) # error: "to_upper" not found in base "int"
```

And the result of `map` is a typed array, taken from what the lambda
returns:

```gdscript
var texts: Array[String] = items.map(item => item.name)   # Array[String]
var totals: Array[int] = items.map(item => item.hp * 2)   # Array[int]
```

That works for a named function too (`nums.map(to_text)`). `map` builds an
untyped array internally, so the typed one is produced where the call
happens, which costs one extra pass over the result. If the lambda returns
something that does not fit, you get a clear error at the call.

### Signal handlers are typed too

A signal's declared parameters reach the lambda you connect to it, so the
short form keeps its types there as well:

```gdscript
signal hit(damage: int, who: String)
signal healed(amount: int)

func _ready():
    hit.connect((d, w) => print(w, " took ", d * 2))
    healed.connect(a => print("healed ", a * 2))

    healed.connect(a => a.to_upper())   # error: "to_upper" not found in base "int"
```

This works for the engine's own signals too, so `child_entered_tree.connect(n => ...)`
gives you a `Node`. Writing a parameter type by hand still wins, and a plain
`Callable` variable is connected unchecked as before.

`emit` is held to the same signature, so the value you send matches what the
handler is promised:

```gdscript
signal changed(items: Array[String])
signal hit(damage: int)

func _ready():
    changed.connect(a => print(a.filter(b => b.begins_with("k"))))
    changed.emit(["kebab", "pizza"])   # built as Array[String], not a plain Array

    hit.emit("oops")   # error: argument 1 should be "int" but is "String"
    hit.emit()         # error: too few arguments for "emit()" call
```

The array literal matters: a handler that knows the element type rejects a
plain `Array` at runtime, so `emit` builds the typed one, the same way a call
to an ordinary function does.

## `for` loops with two variables

Iterating a dictionary no longer needs a lookup inside the loop, and
iterating anything else can give you the index:

```gdscript
for key, value in inventory:
    print(key, " x", value)

for i, item in items:
    print(i, ": ", item)

for i, ch in "hello":
    print(i, ch)
```

Types are allowed on either variable, and the element types of typed
collections flow through:

```gdscript
var stock: Dictionary[String, int] = {"apple": 3}
for name, count in stock:
    print(name.to_upper(), count + 1)   # name is a String, count is an int
```

---

## Nullable types

A `?` after a type means "or null".

```gdscript
var target: Node? = null
var score: int? = null
var owners: Array[Node?] = []

func find(id: String) -> Item?:
    return inventory.get(id)
```

This is new information, not new behaviour: an object-typed variable could
always hold null, and GDScript simply had no way to say whether that was
intended. Now it does, and the two operators below let you act on it.

### `?.` — reach in only if it is there

`a?.b` gives null when `a` is null, instead of "Attempt to call a function on
a null instance". It works for properties, methods and indexing.

```gdscript
print(find("sword")?.item_name)     # sword
print(find("shield")?.item_name)    # <null>

find("shield")?.repair()            # does nothing
```

A skipped call skips its arguments too, so nothing in `a?.m(expensive())`
runs when `a` is null.

Each `?.` guards its own step. In `a?.b.c` only the `.b` is guarded; write
`a?.b?.c`. This is on purpose — otherwise a single `?` would silently change
the meaning of code far to its right.

### `??` — the fallback

`a ?? b` is `a` unless it is null, and `b` is only evaluated if it is needed.
It chains, and it is how you get from `Item?` back to `Item`.

```gdscript
print(find("shield")?.item_name ?? "(empty)")   # (empty)

var score: int? = null
print(score ?? 0)                                # 0
```

### Checking for null tells the compiler, too

A `T?` you can never use would be pointless, so a test that can only pass for
a non-null value makes it non-null from there on:

```gdscript
func describe(item: Item?) -> String:
    if item == null:
        return "(empty)"
    return item.item_name       # `Item` here, not `Item?`

func is_healthy(item: Item?) -> bool:
    return item != null and item.hp > 0
```

`!= null`, `== null`, a bare `if item:`, `not item` and `is_instance_valid()`
all count, and they compose through `and` and `or` — including into the
right-hand side, which is what makes the second example work. A guard clause
that returns narrows the rest of the function. This applies to local
variables and parameters; for a member, copy it into a local first.

### What is checked

By default nothing here is an error, so existing projects are unaffected:
mainline GDScript lets any object variable hold null and plenty of code
relies on it. Two warnings, both off by default, say otherwise:

- **`UNSAFE_NULLABLE_ACCESS`** — reaching into a value declared `T?` without
  `?.` and without checking it.
- **`NULL_ASSIGNED_TO_NON_NULLABLE`** — putting a value that may be null into
  a slot whose type has no `?`.

Turn them on in **Project Settings → Debug → GDScript → Warnings**, or turn
on [strict mode](#strict-mode), where both are errors:

```gdscript
var enemy: Node = null      # error in strict mode: use `Node?`
var target: Node? = get_node_or_null(^"Boss")
target.queue_free()         # error in strict mode: use `?.` or check it
```

### The cost

For an object type, `T?` is free: the slot was already able to hold null, and
the guarantee is entirely compile-time. A nullable builtin is different —
an `int` slot cannot hold null, so `int?` is stored the way an untyped value
is, and gives up the fast paths until you narrow it back to `int`.

---

## Editor support

Every feature above works in the script editor and in external editors
through the language server: completion, go-to-definition, hover and the
document outline. Structs and traits appear in the outline with their
fields, properties and methods, and completion knows what a struct-typed or
trait-typed variable offers.

While debugging, struct values expand field by field in editors that use
the debug adapter, such as VS Code. In the built-in remote inspector they
show their type with the fields in the tooltip; a full inspector editor for
structs is still to come.

### VS Code

Keep using the official **godot-tools** extension — there is no forked
version to install, and you should not need one. Everything the extension
shows you about your code it asks this fork's editor for over the language
server, so completion, hovers, go-to-definition and errors already know
about structs, traits and generics.

The one thing the extension answers by itself is syntax colouring, from a
grammar file baked into it. That grammar predates these features, so
`struct`, `uses`, `=>` and type parameters come out looking like ordinary
identifiers. [`misc/vscode/gdscript-fork-syntax`](misc/vscode/gdscript-fork-syntax)
fixes that: copy it into `~/.vscode/extensions/` and restart. It layers on
top of godot-tools rather than replacing it, so the extension still updates
from the marketplace as usual.

---

## Roadmap

Next, in order:

1. **Typed `PackedScene` exports.** `@export var enemy: PackedScene[Enemy]`,
   with the scene picker filtered to scenes whose root matches, so dropping
   the wrong scene into a slot is caught in the editor.
2. **Typed signals**: connecting types the handler and `emit` is checked
   against the signal (above); what is left is rejecting a handler whose
   signature does not fit the signal.
3. **Enums as real types**, with methods and exhaustive `match`.
4. **Multiple return values**, including `if var ok, value := parse(text):`
   for error handling.
5. **String interpolation**, `f"{name} has {hp} HP"`.

Further out: fixed multidimensional arrays of real numbers, a formatter,
and direct dispatch for trait methods if profiling asks for it.

Deliberately not planned: a JIT or ahead-of-time compilation, and
structural (duck-typed) trait conformance.
