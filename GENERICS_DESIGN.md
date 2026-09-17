# Generics (roadmap item 6, second part)

Status: all four increments landed: generic functions, containers of type parameters, the sound part of the built-in container methods, and generic classes.

## Goal

Write a utility function once and keep the element types:

```
func first[T](items: Array[T]) -> T:
    return items[0]

var n := first(scores)      # scores is Array[int], so n is int
```

Today the same function has to return `Variant` and every caller casts.

## The constraint that shapes this

Type parameters are erased: one compiled function serves every caller, and
`T` is a Variant slot at runtime. That is fine for scalars, because the
value the caller passed in is returned unchanged.

It is **not** fine for containers. A typed array carries its element type at
runtime, and Godot refuses to mix the two:

```
func make() -> Array:
    return []

var strings: Array[String] = make()
# Runtime error: Trying to assign an array of type "Array"
#                to a variable of type "Array[String]".
```

So a generic function that *builds* a container cannot honestly claim to
return `Array[T]`: what it actually built is an untyped `Array`.

Increment 1 rejected that case. Increment 2 solves it at the **call site**
instead of in the callee, which needs no runtime type information at all:
see below.

## Syntax

Type parameters go in brackets after the function name, matching `Array[T]`:

```
func first[T](items: Array[T]) -> T: ...
func get_or[K, V](dict: Dictionary[K, V], key: K, fallback: V) -> V: ...
func find_by[T](items: Array[T], pred: func(T) -> bool) -> T: ...
```

## Semantics

- **Binding.** At each call, every type parameter is bound by matching the
  argument types against the parameter types, structurally: `T` against the
  whole argument, `Array[T]` against `Array[int]`, `func(T) -> U` against a
  typed callable. A parameter bound twice must agree; a parameter that no
  argument mentions becomes `Variant` and the call is marked unsafe.
- **Checking.** Arguments are checked against the bound types, so passing an
  `Array[int]` and a `String` where both are `T` is an error.
- **Result.** The call's type is the return type with the bindings applied,
  which is the whole point: `first(scores)` is an `int`.
- **Inside the body** a type parameter is opaque. A value of type `T` can be
  passed where `T` is expected, stored in a `T` variable and returned, but it
  has no members and no operators: the function must work for every possible
  binding. Assigning it to a `Variant` is allowed and unsafe, as usual.
- `x is T` and `x as T` are errors: a type parameter has no runtime identity.

## Containers of type parameters

A generic function may return `Array[T]` or `Dictionary[K, V]`. Inside the
body the container it builds is untyped, because the binding does not exist
there. The **caller** knows the binding, so the call site converts:

```
var out: Array[String] = map_all(nums, to_text)
```

compiles to the call, then an empty `Array[String]`, then `assign()` from
the returned array, which converts element by element.

That costs one extra pass over the result, which is the same order as the
work the function did to build it. In exchange there are no hidden
arguments, no change to how functions are called, and no new opcodes, so a
generic function is still an ordinary function that anything can call.

If the function put the wrong type in, `assign()` reports it at the call
line and leaves an empty container, rather than handing back something
mistyped.

Everywhere else a type parameter is fine. A parameter `Array[T]` matches a container the
caller already built. A local `var seen: Array[T] = []` is sound too, even
though the array it builds is untyped at runtime: the analyzer only lets
values of type `T` into it, and it cannot escape into a concrete container,
because `Array[T]` is not compatible with `Array[int]` while `T` is opaque.
The return type is the only place a bound type meets a real typed slot.

Scalar positions are unrestricted:

```
func first[T](items: Array[T]) -> T                 # fine
func get_or[K, V](d: Dictionary[K, V], k: K, v: V) -> V   # fine
func for_each[T](items: Array[T], f: func(T) -> void) -> void   # fine
```

## Runtime

Nothing changes. A type parameter compiles to an untyped slot, generic
functions compile once, and calls are ordinary calls. This increment is
entirely in the analyzer.

## As built (increment 1)

- `DataType::Kind::TYPE_PARAMETER`, resolved when a type name matches a type
  parameter of the function being resolved. Opaque: only the same type
  parameter is compatible with it, so the body has to work for every binding.
- Binding walks the parameter type against the argument type structurally,
  through container element types and callable signatures. The first binding
  of a parameter wins; a later argument is then checked against it, which is
  what produces the error for `pick(1, "two")`.
- `get_function_signature()` reports the type parameters of the function a
  call resolved to, and `reduce_call()` binds, substitutes into the parameter
  types before checking the arguments, and substitutes into the return type.
- Two things erasure forced, both found by running the code rather than by
  reading it:
  - A parameter declared `Array[T]` must compile to a **plain** `Array`, not
    an "array of Variant". The runtime check compares element types exactly,
    so "array of Variant" rejects the caller's `Array[int]`. The compiler
    drops the element type whenever it mentions a type parameter.
  - The dictionary index check reads `index_type.builtin_type`, which is
    `NIL` for a type parameter and collided with the `null`-index case.
    Type parameters are now compared by the ordinary compatibility rule.
- Reaching into a `T` (`value.to_upper()`) is an unsafe access: a warning
  normally, an error under strict mode. That is the same treatment any
  unknown type gets, so it needed no special handling.
- Completion substitutes too, through its own guesser, since the line under
  the cursor usually does not parse. Type parameters also complete as type
  names inside their function.

Noticed while testing, pre-existing and left alone: the completion guesser
drops the declared element type of a local initialised with an array literal
(`var words: Array[String] = ["a"]` is guessed as plain `Array`), so
completion on a generic call's result is only precise when the argument's
element type survives. The analyzer itself is unaffected.

## As built (increment 2)

- The return-type restriction is gone. `reduce_call()` notices when the
  declared return type mentions a type parameter inside a container and the
  binding makes every element type concrete, and marks the call.
- The compiler lands such a call in an untyped temporary, builds the bound
  container, and calls `assign()` on it. `assign()` already exists for both
  `Array` and `Dictionary`, converts elements, preserves the class name for
  object elements and reports a genuine mismatch loudly. All of that was
  checked against a running build before the design was chosen.
- Rejected on the way: passing the bound types to the callee as hidden
  arguments. It would avoid the copy, but it changes the arity of a generic
  function, which then has to interact with `Callable`, dynamic calls,
  virtual dispatch and varargs, and it invents a calling convention upstream
  does not have. Monomorphising per binding was rejected too: a script in
  another file can call a generic function with a binding the defining
  script never saw, so the specialisations cannot all be known at compile
  time.

## As built (increment 3, the sound part)

The built-in container methods now keep their element types in the analyzer,
because the runtime already keeps them. Checked against a running build
rather than assumed:

| Method | Runtime result | Inferred |
|---|---|---|
| `Array[T].filter/slice/duplicate/duplicate_deep` | still `Array[T]` | `Array[T]` |
| `Array[T].front/back/pick_random/get` | the element | `T` |
| `Dictionary[K, V].duplicate/duplicate_deep` | still `Dictionary[K, V]` | same |
| `Dictionary[K, V].keys` / `.values` | `Array[K]` / `Array[V]` | same |

Two families are deliberately left as `Variant`:

- `map()` really does return an untyped array, since the element type
  changes. Claiming `Array[U]` would be a lie; this needs increment 2.
- `pop_back()`, `pop_front()`, `min()` and `max()` return `null` on an empty
  container *silently*. Sharpening them would turn the ordinary
  `var item := queue.pop_back()` loop into a runtime error the first time the
  queue runs dry. `front()`, `back()` and `pick_random()` already raise an
  error when empty, so those are safe to sharpen.

## As built (increment 4, generic classes)

```
class Pool[T]:
    var items: Array[T] = []
    func add(item: T) -> void: items.append(item)
    func first() -> T: return items[0]

var ints: Pool[int] = Pool.new()
```

- `class Name[T]:` shares the type parameter parsing with functions. Inside
  the class, `T` resolves through the enclosing *class* chain as well as the
  enclosing function.
- `Pool[int]` as a type annotation keeps its arguments in the same storage a
  container's element types use, so nothing new is needed to carry them.
- Member access substitutes the arguments: `ints.add(x)` wants an `int`,
  `ints.first()` gives one, and `ints.last` is one. Method calls go through
  the same binding code as generic functions, which now merges the class's
  arguments with the function's own.
- Generic classes are **invariant**: a `Pool[String]` is not a `Pool[int]`.
  A value with no arguments, which is what `Pool.new()` is, still fits
  either, so construction needs no new syntax.
- A method returning `Array[T]` is converted at the call site like a generic
  function's, so `ints.all()` really is an `Array[int]`. That conversion
  copies, so the caller does not get a reference to the class's own array.
- A **member variable** of type `Array[T]` is different: there is no call to
  convert at, and converting on every read would be worse than the problem.
  Read from outside, `ints.items` is a plain `Array`, which is what the
  value actually is. Scalar members like `var last: T` substitute normally.
- Trap: the shared substitution helper turns an unbound type parameter into
  `Variant`. Using it to apply the *class's* arguments before the function's
  own were worked out therefore erased `T` and silently broke every generic
  function. It now has a mode that leaves unbound parameters alone.

## Not done for generic classes

- `Pool[int].new()`. Construction infers the binding from the declared type
  instead, which covers the same ground without new expression syntax.
- Generic global classes (`class_name`), and `extends Pool[int]`.

## Increments

1. Generic functions: binding, checking, substitution, the restriction.
2. Containers of type parameters, converted at the call site. Done.
3. Generic inference for the built-in container methods, so `Array.map()`
   itself infers its result. Their signatures come from `MethodInfo`, which
   has no notion of type parameters, so this needs a table of the known
   generic built-ins.
4. Generic classes. Done.

## Not doing

- Constraints (`T: Node`). Worth having later; traits are the natural
  spelling once there is a reason.
- Explicit type arguments at the call site (`first[int](x)`). Inference
  covers the cases that motivated this, and the syntax collides with
  indexing.
