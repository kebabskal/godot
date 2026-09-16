# Structs as a core Variant type (roadmap item 1)

Status: draft for discussion, September 2026. Nothing here is implemented.

## Why a core type

The roadmap wants three things from structs, and only a core `Variant`
type gives all three:

- Engine APIs can return them instead of `Dictionary` (item 2, physics
  queries first), and GDExtension and C# can consume them.
- Scripts get a cheap value type for simulation data: a struct array
  plus MultiMesh is the target pattern.
- Static typing knows the fields, so field access compiles to an index
  (like 4b) and arithmetic on fields stays on the 4d fast path.

A GDScript-only construct (a class with value semantics) would give the
last two but not the first.

## Decisions carried over from the roadmap

Value semantics. Operator overloads. No inheritance. Interfaces via the
upstream `trait` work, later.

## Representation

`Variant::STRUCT`, added at the end of the `Type` enum before
`VARIANT_MAX`, with `needs_deinit` true.

Payload: a pointer to a shared, copy-on-write block.

```
struct StructData {
    SafeRefCount refcount;
    Ref<StructLayout> layout;
    Variant fields[];            // layout->fields.size() entries
};
```

Copying a struct value bumps the refcount; a write goes through
`ensure_unique()`, which clones the block when it is shared. That is the
`String` / packed array model, and it makes passing structs to functions
and storing them in arrays cheap while keeping value semantics.

Fields are `Variant` in the first version. Unboxed typed storage (an
`int` field occupying 8 bytes, not 24) is a later optimization behind
the same API; nothing in the design depends on the boxed layout.

## Layouts

```
class StructLayout : public RefCounted {
    StringName name;             // "Point", or "PhysicsRayResult"
    struct Field {
        StringName name;
        Variant::Type type;      // NIL means any
        StringName class_name;   // for OBJECT / nested STRUCT
        Ref<Script> script;
        Variant default_value;
    };
    Vector<Field> fields;
    HashMap<StringName, int> field_index;
    // Later: operator overloads and methods, as Callables or script functions.
};
```

Ownership: a struct value keeps its layout alive through the `Ref`.
Engine structs are registered once in a `StructDB` (name -> layout) at
startup, next to `ClassDB`. Script structs are created by the script
that declares them; the script holds the layout, values hold refs, so a
layout outlives the script if values still exist (same as a script
resource today).

Identity: two layouts are the same type only if they are the same
object. A hot-reloaded script produces a new layout; existing values keep
the old one and stay valid, and typed code compares by pointer, so a
stale value is a type error rather than a crash. This mirrors what 4a
and 4b do for methods and members.

## Core behavior

- Construction: `StructLayout::instantiate(args)` fills fields from
  positional arguments, the rest from defaults. `Variant::construct`
  for `STRUCT` needs a layout, so the generic path takes
  `(StructLayout, args...)`.
- `get_named` / `set_named`: field by name (index lookup in the layout).
  `set_named` on a shared block clones first. Unknown field: invalid.
- Equality and hash: fieldwise, layout must match.
- `booleanize`: true. `to_string`: `Point(x: 1, y: 2)`.
- `duplicate`: fieldwise; `duplicate(true)` deep-copies fields.
- Serialization: text form `Struct("Point", {"x": 1, "y": 2})` in
  `.tscn`/`.tres`, a new tag in the binary format, and JSON as a
  dictionary (lossy, like today's engine dictionaries). Layouts are
  resolved by name on load; a script layout is written with its script
  path so it can be found.
- Operators: `==`, `!=`. User overloads come with the GDScript methods
  work below.

## GDScript

Declaration, at class level like `enum`:

```
struct Point:
    var x: float = 0.0
    var y: float = 0.0
```

Fields must be typed (a struct is a typed thing by definition; `Variant`
is allowed explicitly). Defaults are constant expressions. No
inheritance, no `extends`.

Use:

```
var p := Point(1.0, 2.0)     # positional constructor
var q := Point()              # defaults
p.x += 1.0                    # field write: copies if shared, then writes
var pts: Array[Point] = []    # typed array of structs
func f(a: Point) -> Point:    # by value; the callee cannot change the caller's value
```

Typing: a new `DataType::Kind::STRUCT` carrying the layout. Field access
on a known struct type compiles to `OPCODE_GET_STRUCT_FIELD` /
`OPCODE_SET_STRUCT_FIELD` with the field index and a layout guard
(same shape as 4b). Typed arrays need `ContainerTypeValidate` to carry a
layout next to `class_name` / `script`.

Methods on structs and operator overloads come in a second step:

```
struct Vec:
    var x: float
    func length() -> float: ...
    func _add(other: Vec) -> Vec: ...
```

Inside a method, `self` is the struct value the method was called on; a
mutating method writes through to the caller's variable (the call site
passes the variable's slot, and the write clones if shared). This is the
C# rule and it is what people expect from `p.normalize()`.

## Engine structs (item 2)

A registration macro so engine code can declare a layout in one place:

```
STRUCT_LAYOUT(PhysicsRayResult,
    FIELD(Vector3, position), FIELD(Vector3, normal),
    FIELD(RID, rid), FIELD(int, collider_id), FIELD(Object, collider), ...)
```

New struct-returning methods are added next to the dictionary ones
(`intersect_ray_struct` or a versioned name) so nothing breaks; the
dictionary versions can be deprecated later. The layout appears in
`extension_api.json` under a new `structs` section so GDExtension and the
C# source generator can emit a matching value type.

## What it does not do

- No unboxed field storage in the first version.
- No struct inheritance, no default/copy hooks, no destructors.
- No inspector editor at first; exported struct fields show up once the
  inspector gets a struct property editor.

## Increments

Each is a separate, testable landing:

1. Core: type, layout, data block, construct, named access, equality,
   hash, string, duplicate, text and binary serialization, JSON. Unit
   tests in `tests/core/variant`.
2. GDScript: declaration, type, constructor call, field access with the
   index opcodes, typed arrays, strict-mode integration, script tests.
3. Engine: `StructDB`, the registration macro, the first physics query
   result, `extension_api.json`, and the C# side.
4. Methods and operator overloads on script structs.

## Sizing (from a survey of every place keyed on `Variant::Type`)

About 160 edit sites engine-wide for the new type; only ~13 of them fail
to compile if missed, the rest are `default:`-guarded switches or tables
indexed by type that compile fine and misbehave at runtime. The ones
that bite silently: `needs_deinit` in `variant.h`, `type_init_function_table`
and the three opcode label tables in `gdscript_vm.cpp` (must mirror the
enums in `gdscript_function.h`), `VariantInternal::initialize`, and the
four C# source-generator mirrors (`GodotEnums.cs`, `MarshalType.cs`,
`MarshalUtils.cs`).

| Area | Sites |
|---|---|
| core/variant (type, tables, ops, calls, setget, utility, hash, string) | ~50 |
| Serialization (text, binary, JSON) plus two file-format version bumps | ~20 |
| Object system, GDExtension interface, API dump, docs, core tests | ~20 |
| Editor (inspector editor, type pickers, DAP, icon) | ~12 |
| GDScript, Variant-type side | ~25 |
| GDScript, `struct` declaration side | ~10 |
| C# glue and source generators | ~22 |

Two traps found: `GDScriptParser::get_builtin_type()` turns every type
name into a keyword, so the new type must be excluded there or its name
chosen so it cannot collide; and `Variant::is_type_shared()` is what
separates value semantics from `Dictionary`/`Array` reference semantics
and feeds `duplicate`, deep copy and the inspector, so it must say
"not shared" for structs.

Increment 1 is the ~50 core sites plus serialization and tests. Editor
and C# can lag behind it without breaking anything as long as the
hard-failure sites in each are handled.
