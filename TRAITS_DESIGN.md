# Traits (roadmap item 7)

Status: design draft. Increment 1 in progress.

## Goal

Interfaces for GDScript: a named set of method signatures that classes and
structs can declare they satisfy, usable as a static type and testable at
runtime. Follows the upstream `trait` work instead of inventing a second
concept (roadmap decision).

## Where upstream is

The comprehensive trait PR (godotengine/godot#97657: `.gdt` files,
`trait_name`, variables, signals, enums, static members, inner classes,
`@tool` propagation) was closed. Maintainers asked for a minimal version
first: inner traits, `uses`, required (bodyless) methods, method
overriding, everything inside the GDScript module. This design is that
minimal version, with the same spelling, so code written for it stays valid
if upstream lands theirs.

## Syntax

Declaration, at class level like `enum` and `struct`:

```
trait Damageable:
    func take_damage(amount: int) -> void   # required: no body
    func is_alive() -> bool
```

Use, in a class (after `extends`) or in a struct body:

```
extends Node
uses Damageable, Saveable

struct Shield:
    uses Damageable
    var hp: int = 10
    func take_damage(amount: int) -> void: ...
    func is_alive() -> bool: ...
```

`uses` is a contextual keyword: it is only special as the first token of a
class-level or struct-level statement, so existing identifiers named `uses`
keep working. `trait` is already a reserved word.

As a type:

```
var target: Damageable = enemy
func hit(t: Damageable) -> void: t.take_damage(5)
var all: Array[Damageable]
if node is Damageable: ...
var d := node as Damageable      # null when it does not match
```

A trait from another script is reached like any other class member:
`const Combat = preload("combat.gd")` then `Combat.Damageable`. Global
traits (`trait_name` files) are out of scope for now.

## Semantics

- Nominal, not structural: a type satisfies a trait only if it says `uses`.
  Errors are clearer and the runtime check is a set lookup.
- A class that `uses` a trait must provide every required method with a
  compatible signature: same parameter count (extra optional parameters
  allowed), parameter types equal or wider, return type equal or narrower.
  The method may be the class's own, inherited from a script base, or a
  native method of the base class.
- Traits are inherited: a subclass of a class that uses a trait also has it.
- A trait-typed value can hold an object or a struct. Calls on it are
  statically typed against the trait's signatures and dispatched by name
  (`OPCODE_CALL`, which already reaches script methods and struct methods).
  An interface method table (slot per trait method, like 4a's vtable) is a
  later optimization.
- Only the trait's own methods are visible on a trait-typed value. Anything
  else needs `as` to a concrete type.

## Analyzer

- `GDScriptParser::TraitNode` (a `ClassNode::Member::TRAIT`), holding the
  method signatures. `DataType::Kind::TRAIT` with `trait_type`.
- Type compatibility: a class, script or struct type is assignable to a
  trait type if it (or a base) uses the trait. A trait type is assignable
  to `Variant` and to itself. Trait to concrete type needs `as`.
- `is` / `as` with a trait type resolve to a runtime test.
- Assigning an untyped (`Variant`) value to a trait-typed slot is unsafe,
  like any other narrowing: a warning normally, an error in strict mode.

## Runtime

- Trait identity is a qualified name, `script_path::TraitName`, like struct
  layouts: it survives reloads and needs no object.
- `GDScript` keeps the set of trait names it uses; the test walks the base
  script chain. `StructLayout` keeps the same set for structs.
- One new opcode, `OPCODE_TYPE_TEST_TRAIT`, through an out-of-line helper
  (the VM frame constraint from 4d applies). `as` compiles to the test plus
  a conditional null. Trait-typed slots are `VARIANT` at runtime: no
  validation on assignment, arguments or returns in the first increment.
  The analyzer guarantees typed code, and strict mode makes all code typed.
  A validating `OPCODE_ASSIGN_TYPED_TRAIT` can follow if needed.

## Increments

1. Interfaces: `trait` with required methods, `uses` on classes and
   structs, conformance checking, traits as types, `is` / `as`, tooling
   (completion, lookup, LSP symbols), tests, docs.
2. Default methods: a trait function with a body is compiled into each
   using class unless the class overrides it. `self` is the using class.
3. Other members as upstream settles them: required properties, signals,
   constants; traits using traits; global `trait_name` files.
4. Interface method tables for direct dispatch, if profiles ask for it.

## Not doing

- Structural ("duck") conformance.
- State in traits before increment 3.
- Generic traits before item 6 (generics) exists.
