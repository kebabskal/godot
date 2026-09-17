# Traits (roadmap item 7)

Status: increments 1 (interfaces), 2 (default methods) and the required-properties part of 3 landed.

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

## As built (increment 1)

- Parser: `TraitNode` as a class member; `uses` is contextual in class
  and struct bodies and parses a list of types; trait functions reuse
  `parse_function`, where a missing `:` already means "no body".
- Analyzer: `DataType::TRAIT` with `trait_type` and `trait_name`
  (`class fqcn::Name`). `uses` lists resolve with the type itself (end of
  class inheritance resolution, end of `resolve_struct`), because
  `check_type_compatibility()` is static and can only read them.
  Conformance runs once all member signatures are resolved, through
  `get_function_signature()`, so inherited script methods and native
  methods count. Calls on a trait-typed base get their signature from the
  trait (`get_function_signature` TRAIT branch). `is` / `as` with a trait
  on either side are left to runtime unless the other side is a plain
  built-in type.
- Runtime: `GDScript::traits` plus `uses_trait()` walking the base chain,
  `StructLayout::add_trait` / `has_trait` (filled by the analyzer on the
  layout it creates), `OPCODE_TYPE_TEST_TRAIT` through `_trait_test()`.
  `as` is the test plus a conditional null. Trait-typed slots compile to
  Variant slots; a struct passed to a trait-typed parameter is a copy, and
  a mutating method called through the trait changes that copy.
- Tooling: trait names complete as types and members, a trait-typed value
  completes the trait's methods only, lookup goes to the signature, the
  language server reports `Interface` symbols and a `traits` / `uses`
  section in the script API.

## As built (increment 2, default methods)

- A trait function with a body is a default. Its body is analyzed once, in
  the trait's context: `self` is the trait type and a bare call resolves to
  the trait's other methods (`CallNode::trait_self_call`). Nothing of the
  enclosing class is reachable except constants and types, because the
  trait may be used by any class; the analyzer runs these bodies under the
  static-context rules and reports a trait-specific error.
- Classes: conformance collects the defaults that no class in the chain
  implements (`ClassNode::trait_default_methods`; own, inherited, script
  base and native methods all win over a default) and the compiler emits
  each one into the using class as an ordinary method. Calls inside the
  body go through `self` by name, so a subclass override is seen. Two used
  traits providing the same default is an error at the `uses` line.
  `get_function_signature()` finds defaults after the real methods, so
  `enemy.describe()` type-checks on the class type too.
- Structs: an unimplemented default is appended to the struct's method
  list and compiled as a struct method (`_parse_function` takes the struct
  as context, since the node belongs to the trait). Calls to the trait's
  other methods go by name through the value, with write-back. The node
  is shared between users, so whether it mutates is not known per struct:
  it always writes back, the compiler copies a constant receiver first,
  and the analyzer does not demand a writable receiver.
- Not possible in a default body: `await` (it may be compiled into a
  struct), instance members or static functions of the enclosing class.

## As built (increment 3, required properties)

- `var name: Type` in a trait body is a required property. It must be
  typed and cannot have a value, a setter or a getter: traits hold no
  state, the using type provides the variable.
- Conformance: a class needs an instance property of that name (a member
  variable anywhere in the chain, a property of a script base, or a native
  property); a struct needs a field. The type must match exactly, since
  the property is read and written through the trait.
- On a trait-typed value, `value.name` is typed from the trait and
  compiled as a named get/set on the Variant slot, which works for objects
  and structs alike. Anything the trait does not declare is an error.
- In a default method, a bare property name has the `TRAIT_PROPERTY`
  identifier source and compiles to a named access on `self`: the instance
  in a class (so setters and getters run), the value in a struct (where
  the typed address turns it into the struct field opcodes).
- Still open in increment 3: signals, constants, traits using traits,
  global `trait_name` files.

## Not doing

- Structural ("duck") conformance.
- State in traits before increment 3.
- Generic traits before item 6 (generics) exists.
