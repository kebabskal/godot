# Nullable types

Roadmap item 8. `T?`, `?.` and `??`, plus the flow analysis that makes them
worth having.

## The problem

GDScript today has two rules and no way to say otherwise:

- An object-typed slot is **always** nullable. `var n: Node = null` is legal,
  and `var n: Node` with no initializer *is* null. Every `.` on an object is
  a potential "Attempt to call a function on a null instance".
- A builtin-typed slot is **never** nullable. `var i: int = null` is an error.

So the type system already knows about nullability; it just does not let you
write it down, and it never checks the object case. Strict mode made every
slot typed, which is what makes fixing this worth doing: once the types are
there, null is the remaining hole.

## Surface

### `T?`

`?` after any type means "or null".

```gdscript
var target: Node? = null
var parsed: int? = text.to_int_or_null()
var owners: Array[Node?] = []
func find(id: int) -> Item?:
	return null
```

`T` (no `?`) means not null. That claim is only *enforced* in strict mode —
see below — but it is always what the analyzer reasons with, so `?.` and `??`
give the same types either way.

### `?.`

`a?.b` evaluates `a` once. If it is null the whole expression is null;
otherwise it is `a.b`. Same for `a?.m()` (the call is skipped, arguments are
not evaluated) and `a?[i]`.

```gdscript
var name := get_node_or_null(^"Boss")?.name       # String?
player?.take_damage(10)                            # no-op if null
```

**Each `?.` guards its own step only.** `a?.b.c` does not short-circuit the
`.c` — if `a` is null, `a?.b` is null and `.c` then fails. Write `a?.b?.c`.
This is deliberate: a chain-wide guard makes the reach of a single `?`
depend on tokens far to its right. The analyzer closes the trap instead, by
complaining about the `.c` on a nullable value.

### `??`

`a ?? b` is `a` when `a` is not null, otherwise `b`. `b` is evaluated only
when needed, and `??` is right-associative, so `a ?? b ?? c` chains.

```gdscript
var label := item?.name ?? "(none)"
```

The result type is `b`'s type when that is not nullable, so `??` is the
standard way to get from `T?` back to `T`.

## Narrowing

Without narrowing, `T?` would be a value you can never use. After a test
that can only succeed when the value is not null, it has type `T`:

```gdscript
if target != null:
	target.queue_free()        # Node here, not Node?

if target == null:
	return
target.queue_free()            # Node: the null case left the block

var t := target
if not t:
	return
t.queue_free()
```

Narrowing applies to **locals and parameters only**. A member would need to
survive every call in between, and nothing here tries to prove that; write
`var t := target` first. Locals are safe because only an assignment in the
same function can change one, and a lambda captures by value.

What counts as a test: an identifier compared to `null` with `==` or `!=`
(either way round), used as a truth value, negated with `not`, or passed to
`is_instance_valid()`. These compose through `and` and `or`, including into
the right-hand operand — `x != null and x.hp > 0` and `x == null or x.hp == 0`
both work, since the right side only runs when the left did not settle it.
An assignment to the local drops what was proved about it.

A narrowing also outlives the `if` when the branch cannot fall through:
`if x == null: return` leaves `x` non-null for the rest of the block. That
rests on the parser's `SuiteNode::has_return`, so a block that exits by
`break` or `continue` instead does not narrow yet.

## Enforcement

Outside strict mode nothing new is an error: `T?` and `T` accept null alike,
and a `.` on a nullable value is the usual unsafe-access warning. Existing
projects keep working, including `var n: Node` left null by the editor.

In **strict mode** the non-null claim is real:

| | |
| --- | --- |
| `var n: Node = null` | error: assign to `Node?`, or give it a value |
| `n.foo` where `n: Node?` | error: use `?.`, `??`, or narrow it first |
| `var n: Node` with no initializer | error (step 2): it would start null |
| `@export var n: Node` | error (step 2): the editor can leave it empty |

The last two are the breaking ones, so they land separately, after the rest
is in and tested.

## Representation

`DataType::is_nullable`. Note that `DataType` has a hand-written
`operator=`; a new field must be added there or it silently vanishes from
every copy. (This has bitten this fork twice already — see
`SCRIPTING_ROADMAP.md`.)

At runtime:

- **Object types.** Nothing changes. `Node` and `Node?` are both a `Node`
  slot, which already permits null. The non-null guarantee is static only.
- **Builtins.** `int?` cannot be an int slot, so it compiles to a Variant
  slot, exactly like an untyped value. This is the honest cost of a nullable
  builtin, and narrowing back to `int` gets the fast paths back.

`GDScriptDataType` (the runtime type) is therefore built from a nullable
builtin as "unset", in `_gdtype_from_datatype`.

## Bytecode

Two new opcodes, `OPCODE_JUMP_IF_NULL` and `OPCODE_JUMP_IF_NOT_NULL`: a
Variant type check against `NIL` and a jump. They hold no locals, so unlike
the struct opcodes they do not need `GDS_NOINLINE` (roadmap 4d).

`a ?? b` and `a?.b` both compile like the ternary: a result temporary, the
guard, and a patched jump over the path not taken.

A null test is a **null** test, not `is_instance_valid()`. A freed object is
not null, and `?.` will not save you from one, just as `!= null` does not.

## Order of work

1. `T?` in types, `DataType::is_nullable`, the two opcodes, `??`. **Done.**
2. `?.` on attributes, calls and subscripts. **Done.**
3. Narrowing. **Done.**
4. Strict-mode errors for null assignment and nullable dereference. **Done.**
5. Definite initialization (the `@export` / uninitialized-member rules). Not started.

## As built

Enforcement rides on the existing warning machinery rather than on a separate
strict-mode check: `UNSAFE_NULLABLE_ACCESS` and `NULL_ASSIGNED_TO_NON_NULLABLE`
are listed in `GDScriptWarning::is_strict_mode_error()`, so they are errors in
strict mode, silent by default elsewhere, and can be switched on per project
like any other warning. `REDUNDANT_NULL_CHECK` is an ordinary warning.

Traps found while building it, in the order they bit:

- **`DataType::operator=` is hand-written.** `is_nullable` had to be added to
  it, as `struct_layout` did before. The roadmap says this twice now.
- **The token enum is serialized.** `?.`, `?[` and `??` were inserted into it,
  which needs `TOKENIZER_VERSION` in `gdscript_tokenizer_buffer.h` bumped, or
  previously exported `.gdc` files decode as garbage.
- **`resolve_datatype()` had to set `is_nullable` twice.** Setting it on the
  fresh `result` covers the early returns, but looking up a class, script or
  native name replaces `result` wholesale, so `Item?` silently came out as
  `Item` while `int?` worked. Set again just before the final return.
- **`?[` is an opening bracket.** `parse_precedence` switches the tokenizer to
  multiline mode for `(` and `[`; without `QUESTION_BRACKET` in that list, the
  matching `pop_multiline()` underflowed.
- **`update_const_expression_builtin_type()` converts constants to the
  declared type.** `var n: int? = null` tried to build an `int` from null and
  failed; a nullable target keeps a null constant as it is.
- **A discarded `a?.method()` has no result address.** Writing the null to the
  shared `nil` slot is GH-70964 all over again, so that write is skipped.
- **`REDUNDANT_NULL_CHECK` cannot use `is_nullable` alone.** A plain object
  type is *not* a promise outside strict mode, so `find()?.name`, where `find()`
  returns `Item`, was reported as a redundant guard even though it returns null.
  `type_can_be_null()` is the distinction: a type promises non-null only where
  the promise is enforced.
- Narrowing goes through `and` **and** `or`: `x == null or x.hp == 0` needs the
  left operand's false-narrowings applied to the right one, which is the mirror
  of what `and` needs. Only `and` was handled at first, and the test caught it.
