# Typed Callables (roadmap item 6, first part)

Status: landed. Generic functions and classes are the next part and need
their own design.

## Goal

Let a `Callable` say what it takes and returns, so passing the wrong
function is a compile-time error and the result of `.call()` has a type.
This is the groundwork for generics (`Array[T].map` inferring `Array[U]`).

## Syntax

A function type is written like a function declaration without names:

```
var on_hit: func(int) -> void
func apply(f: func(int, String) -> bool, n: int) -> bool: ...
func make_adder(n: int) -> func(int) -> int: ...
```

`-> R` is optional; without it the return type is unknown (`Variant`), the
same as a function declared without one. `-> void` means no value. Plain
`Callable` stays the untyped form.

The alternative `Callable[[int, String], bool]` was rejected: it reads
worse, and nested brackets collide with the "no nested typed collections"
rule of `Array[...]`.

## Semantics

- A typed callable is still `Variant::CALLABLE`. The signature lives on the
  analyzer's `DataType` only (`has_callable_signature`,
  `callable_signature` with the return type first, plus how many trailing
  parameters are optional and whether it is variadic). Nothing changes at
  runtime and nothing is validated there.
- Sources of signatures: lambdas, references to script functions
  (`my_func`, `obj.my_func`), script and native methods through their
  `MethodInfo`. `Callable(obj, "name")` and `bind()` give plain callables.
- Assignability, target `func(T...) -> R` from source `func(S...) -> Q`:
  the source must accept the target's argument count (optional and
  variadic parameters count), each `S[i]` must accept `T[i]`
  (contravariant), and `Q` must fit `R` (covariant; anything fits `void`
  and `Variant`). A plain `Callable` fits any typed callable, unchecked,
  like an untyped `Array` fits `Array[int]`.
- `f.call(args)` on a typed callable checks the arguments and has the
  declared return type. The callable itself may have come from an
  unchecked source, so the compiler lands the call in an untyped temporary
  and converts it into the typed result (`CallNode::validate_callable_result`):
  a callable that returns something else is a script error at the call
  line, and after that the type holds. This matters because the inline
  typed operators from 4d read the payload without looking at the tag.
  A first version made the return type inferred instead; that produced
  unsafe-argument warnings on `f.call(f.call(x))`.
- `.call()` on a callable that returns `void` yields null and is not an
  error, the same as for a plain Callable (an existing test relies on it).
- Under `await`, a typed callable's `.call()` is left to the generic path:
  any callable may be a coroutine, so its result is never "redundant".

## Not yet

- Inferring a lambda's parameter types from the expected type
  (`arr.map(func(x): ...)`). Comes with generics.
- Calling a typed callable as `f(1)`. It would be unambiguous for typed
  callables, but changes call resolution; worth its own small step.
- Runtime validation of the argument count on unchecked assignment.
