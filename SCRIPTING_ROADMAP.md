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
- 4c, second round: `OPCODE_CALL_SCRIPT` skips the `self` reference when
  the caller's frame holds the base (self, local, parameter, temporary,
  constant). Saves ~10 ns per call into a RefCounted script object
  (exact class 90 -> 80, virtual via base 133 -> 117). An earlier note
  here called this a no-gain; that was measured on a stale binary
  because the scons shim returns 0 on compile errors. Always verify a
  build by grepping the log for ` error C` and checking the binary's
  timestamp.
- 4c, tried and rejected (measured on a verified build, reverted): a
  precomputed stack template copied in per call. Copying 24 bytes per
  slot costs what the NIL loop and typed init saved. What remains in the
  prologue is spread thinly; further call cost reduction belongs to 4d.
- 4b landed as `OPCODE_GET_SCRIPT_MEMBER` / `OPCODE_SET_SCRIPT_MEMBER`:
  member access on a variable of known script type goes by index with a
  name guard, falling back to the name lookup when stale. Field set+get
  on a typed variable: 77 -> 49 ns (self members were already ~free at
  2 ns per pair via direct slot addressing). Untyped variables stay on
  the name lookup by design; strict mode (item 5) is what makes 4b pay
  off broadly.
- 4b, accessors: a member with an inline setter/getter on a variable of
  known script type is now accessed by a direct slot call of the
  accessor (setter+getter pair 168 -> 118 ns). Regular `set = f` /
  `get = f` accessors stay on the named path, since their parameter may
  be untyped and the named path converts the value first.
- 4d, interpreter part: binary and unary operators on statically known
  int, float, bool, Vector2 and Vector3 operands execute inline in the
  VM (`GDSCRIPT_TYPED_BINARY_OPCODES` / `GDSCRIPT_TYPED_UNARY_OPCODES`),
  reading and writing the Variant payload in place instead of calling a
  validated evaluator through a function pointer, and an assign peephole
  writes the result straight into the target local. This is the
  "unboxed at the operation, boxed in the slot" form of 4d: the 24-byte
  Variant slot stays, but no tag dispatch or indirect call happens on
  the hot path. Editor build, ns per iteration: loop only 16.5 -> 11.9,
  `acc = acc + i` statement 8.1 -> 4.4, int mod/div/neg statements
  15 -> 5.6, vec2 add 10 -> 6.5. Untyped operands are unchanged (by
  design; see item 5). What remains per statement is opcode dispatch and
  operand address decoding; the next steps would be fused
  compare-and-branch and int vector / Color / Vector4 coverage, then a
  real unboxed slot representation only if a profile shows the slot
  size matters.
- Constraint found while extending 4d: `GDScriptFunction::call()` is one
  function with every opcode as a case. On MSVC, each opcode's local
  copies get their own stack slot (bloating the frame that every script
  call recurses through), and past a certain function size MSVC stops
  optimizing it entirely (frame ~50x, ~4x slower everywhere). The inline
  operator set is therefore limited to int, float, bool, Vector2 and
  Vector3, with operands bound by reference, and the `deep_recursion`
  test (depth 1000) guards the frame. Adding Vector2i/Vector4/Color
  would need a separate out-of-line family opcode with an inner switch.
  Also noted: even the upstream MSVC build crashes between 1000 and 1500
  nested calls, so `MAX_CALL_DEPTH` (2048) is not actually reachable on
  Windows; raising `/STACK` to 16 MB in the Windows platform config
  would fix that independently of this work.
- Measurement note: numbers from the editor build drift by up to ~15 ns
  between builds for opcodes that were not touched (code layout of the
  VM's dispatch function). Compare only within one build, min of 3 runs.
- Release build numbers (template_release, `disable_path_overrides=no`
  so `-s` works; min of 2 runs, ns per iteration, `call_bench.gd`):
  loop only 8.7, while loop 7.2, compare+branch 9.8, int add x4 28.2,
  float mul x4 20.3, vec2 add x4 30.5, int mod/div/neg 17.7, self call
  51.8 (0 args 40.5, 4 args 74.4), typed var call 61.2, virtual via base
  82.9, field set+get typed 40.2 vs untyped 81.3, setter+getter 88.1,
  untyped int add x4 54.7. So in release a fused compare-and-branch is
  ~1 ns over the loop, a typed statement is 3-5 ns, and a direct script
  call is ~40 ns of which ~8 ns per argument. The earlier "hang" was the
  template silently dropping `-s`; see the memory note.
- 5 landed as the project setting `debug/gdscript/strict_mode`: the
  untyped/unsafe warnings become errors that `@warning_ignore` cannot
  silence; an explicit `Variant` type is the opt-in for dynamic code and
  `:=` from a typed value stays allowed. With it on, all code is on the
  4b/4d fast paths. Not done: a per-directory variant (the warning
  directory rules could carry it if addons need to stay lax).
- 1 (structs): design in `STRUCTS_DESIGN.md`, with a touch-point survey
  (~160 sites, 13 hard) and a four-increment plan. Increment 1 landed:
  `Variant::STRUCT` (`Struct` copy-on-write value, `StructLayout`
  ref-counted type with a name registry), all Variant behavior, text,
  binary and JSON serialization, unit tests; the engine builds and all
  suites pass. Not yet in it: the editor property editor, C# marshaling
  (the mono module is not built here), the debugger visualizer.
  Increment 2 landed: the GDScript `struct Name:` class member (fields
  are `var` lines with a required static type and constant defaults),
  the name as a type (`var p: Point`, `Array[Point]`, parameters and
  returns, nested structs), the positional constructor `Point(1.0, 2.0)`
  (constant-folded when the arguments are), typed field access compiled
  to `GET/SET_STRUCT_FIELD` (index-based, guarded by the layout's field
  name so a stale script falls back to the generic path), and analyzer
  errors for unknown fields, untyped fields, too many constructor
  arguments and layout mismatches. Layouts are registered under
  `source_path::name`. Two traps for anyone extending this: both
  `GDScriptParser::DataType` and `GDScriptDataType` have hand-written
  copy assignment, so every new field must be added there (a missed
  one silently drops the layout from every copy); and the three struct
  opcodes had to go through `GDS_NOINLINE` helpers, since their inline
  locals grew the `call()` frame enough to fail `deep_recursion`.
  Increment 3 landed: `StructDB` (engine layouts registered at startup
  by plain name, next to `ClassDB`; script layouts keep their
  `path::name` key in the same registry), `TypedStruct<Name>` so a bound
  method's return type carries the layout through
  `PROPERTY_HINT_STRUCT_TYPE`, the first engine structs
  `PhysicsRayResult2D/3D` returned by `intersect_ray_struct` (the
  dictionary `intersect_ray` stays; `hit` is a field instead of an empty
  result), GDScript treating an engine layout name as a type and
  constructor like a script struct (a script struct in scope shadows an
  engine one), a `structs` section in `extension_api.json` with fields,
  types and defaults, plus `struct::Name` return types, and class
  reference pages for `Struct` and `StructLayout`. C#: the source
  generator enums mirror `TYPE_STRUCT`, and the bindings generator skips
  struct-using methods with a warning instead of failing, since the
  mono module is not built here; real marshaling is still open. Traps:
  a new typed wrapper needs `PtrToArg`, `GetTypeInfo` and
  `VariantInternalAccessor` specializations, and a new `PropertyHint`
  must go at the end of the enum or every later value shifts in the
  GDExtension API.
  Increment 4 landed: `func` inside a `struct` body declares a method.
  Methods compile as static functions of the owning script with the
  struct value as an implicit first parameter, so `self` is the value
  and bare field names are its fields (locals and parameters may not
  shadow them). The layout holds each method as a Callable
  (`GDScriptStructMethodCallable`), which is how untyped calls
  (`Variant::callp`) and operator overloads reach the code; typed calls
  compile to `OPCODE_CALL_STRUCT_METHOD` (method index plus a name
  guard, dispatched through the value's layout, so a reloaded script
  cannot call freed code: `GDScript::clear()` drops the layout's
  methods first). Mutation: a method that assigns to a field is
  "mutating" (a syntactic pre-pass in `resolve_struct_methods`, with a
  fixpoint over self-calls; calls on a field count as mutating since
  the callee is unknown without types); the VM copies its final `self`
  back into the caller's slot on return (`_struct_self_writeback`), and
  the compiler stores a temporary base back into a static variable or a
  field of `self`. The analyzer rejects a mutating call on anything
  else (temporaries, constants, properties with setters). Operator
  overloads are methods named `_add _sub _mul _div _mod _neg _lt _le
  _gt _ge`; `==` stays fieldwise. Typed operands compile to the method
  call directly; untyped ones go through `Variant::evaluate`, where
  `variant_op.cpp` registers a struct-left evaluator for those operators
  that looks the overload up on the layout (`Array.sort()` on structs
  uses `_lt`). Not done: mutating calls on array elements
  (`pts[i].scale(2)` is an error; copy to a local first), `await` in
  struct methods (error), lambdas seeing fields (error), engine struct
  methods, a `_to_string` override, and the move-in/move-out
  optimization that would make a mutating call clone-free. Next: the
  array-element write-back through the assignment chain, then engine
  result structs for the remaining physics queries.
  Decision after increment 4: struct methods follow the built-in value
  types and return a new value (`p = p.scaled(2)`), which works on any
  expression including array elements; mutating methods stay legal on
  variables, members and fields but the array-element write-back is not
  built. The error for other receivers points at the convention.
- 2 (no `Dictionary` returns), physics first: every
  `PhysicsDirectSpaceState2D/3D` query except `collide_shape` (already a
  typed `Vector` array) has a struct-returning twin: `intersect_ray_struct`
  -> `PhysicsRayResult`, `intersect_point_struct` and
  `intersect_shape_struct` -> `Array[PhysicsShapeResult]`,
  `cast_motion_struct` -> `PhysicsCastResult` (`safe_fraction`,
  `unsafe_fraction`, both 1.0 on no hit), `get_rest_info_struct` ->
  `PhysicsRestInfo` (`hit` plus the fields), each in 2D and 3D. Typed
  arrays of engine structs: `TypedArray<TypedStruct<Name>>` puts the
  layout name in the array hint, the analyzer resolves it through
  `StructDB` (so `var hits: Array[PhysicsShapeResult3D]` is fully
  typed), the API dump writes `typedarray::struct::Name`, and the class
  reference shows `Struct[]` with the layout named in the description
  (layouts have no class page yet). Element validation is by
  `Variant::STRUCT` only; the layout is not enforced at runtime. Not
  done: the rest of the engine's dictionary returns (a survey is the
  next step), deprecating the dictionary twins, and a doc page per
  engine layout.
  Survey (from `extension_api.json`, 2026-09-16): 176 bound methods
  return a `Dictionary` or an array of them, 150 in core. They split
  into (a) fixed-shape records, the struct candidates: physics results
  (done), `Time` date/time dicts, `TriangleMesh` hits, `OS.get_memory_info`,
  `Engine.get_version_info`, `Image.compute_image_metrics`,
  `Geometry2D.make_atlas`, `Input.get_joy_info`, `IP.get_local_interfaces`,
  `DisplayServer.tts_get_voices`, `AStarGrid2D.get_point_data_in_region`,
  `ProjectSettings.get_global_class_list`, `GraphEdit`/`VisualShader`
  connections, `CodeEdit` completion options, `WebRTCMultiplayerPeer.get_peer`,
  `XRInterface.get_system_info`, `TextServerManager`/`XRServer.get_interfaces`;
  (b) the reflection records `Object.get_property_list/get_method_list/
  get_signal_list`, `ClassDB.class_get_*_list`, `Script.get_script_*_list`,
  `RenderingServer.*_get_shader_parameter_list`: PropertyInfo/MethodInfo
  shapes with nested argument arrays, high impact but the largest change,
  and the `ScriptExtension`/`ScriptLanguageExtension` virtuals that
  produce them; (c) text: `TextServer.shaped_text_get_glyphs` and friends,
  a hot path that wants unboxed struct arrays before converting; (d)
  genuine maps to keep as dictionaries: font OpenType feature/variation
  maps, `CodeHighlighter` colors, `AudioStream` tags, HTTP headers,
  `RegExMatch.get_names`, `Script.get_script_constant_map`,
  `InstancePlaceholder.get_stored_values`, `XRServer.get_trackers`,
  JSON-RPC and glTF `to_dictionary` serialization; (e) editor-only
  (`EditorVCSInterface`, export/import options), left alone.
  Second batch landed: `Time` gets `DateTime`, `Date`, `TimeOfDay` and
  `TimeZoneInfo` layouts with struct twins of every dict method
  (`get_datetime_from_unix_time`, `get_unix_time_from_datetime`, ...;
  the names drop `_dict`), the first bound methods that *take* a struct
  (`Variant::operator TypedStruct<N>()` added for `VariantCaster`),
  `TriangleMesh.intersect_ray_struct`/`intersect_segment_struct` ->
  `TriangleMeshHit`, `OS.get_memory_info_struct` -> `MemoryInfo`,
  `Engine.get_version_info_struct` -> `VersionInfo`. Core layouts
  register in `register_core_types` next to their class and unregister
  before `StructLayout::cleanup()`. `StructLayout::instantiate(Dictionary)`
  fills a value by field name for engine code that still builds a
  dictionary (platform `get_memory_info` overrides). Next in (a):
  `Image.compute_image_metrics`, `Geometry2D.make_atlas`, the
  `GraphEdit` connection records, `ProjectSettings.get_global_class_list`.
- 12 (`for` with two variables) landed: `for key, value in dictionary`
  and `for index, item in` anything else, with optional types on both.
  The VM still yields one value per step; the compiler fills the other
  variable at the top of the body (so `continue` stays correct): a keyed
  get from a hidden copy of the list for dictionaries, a hidden running
  index otherwise. When the list's type is unknown the choice is made at
  runtime with a type test per step. `range()` keeps its allocation-free
  path. Tooling came for free: both variables are suite locals.
- 7 (traits): design in `TRAITS_DESIGN.md`. Upstream closed the big
  mixin PR (#97657) and asked for a minimal version: inner traits,
  `uses`, required bodyless methods, overriding. This fork builds that
  subset with the same spelling: nominal conformance, traits as static
  types for objects and structs, `is`/`as` through one runtime test,
  name-based dispatch first. Default method bodies are increment 2.
  Increment 1 landed: `trait Name:` with required (bodyless) methods,
  contextual `uses A, B` in classes and struct bodies, conformance
  errors at the `uses` line (missing method, parameter count, narrower
  parameter, wider return), traits as types for variables, parameters,
  returns and typed arrays, subclass inheritance of traits, and
  `is` / `as` at runtime for objects and structs through
  `OPCODE_TYPE_TEST_TRAIT`. Trait-typed slots are Variant slots with no
  runtime validation on assignment (typed code is checked statically;
  strict mode makes all code typed). Tooling done per the checklist
  except the class reference, which has nowhere to document a script
  keyword. Trap: `check_type_compatibility()` is static, so anything it
  consults (`uses` lists) must be resolved eagerly with the type.
  Increment 2 landed: default methods. A trait function with a body is
  analyzed once in the trait's context (`self` is the trait type, bare
  calls reach the trait's other methods, the enclosing class's instance
  members are an error) and compiled into every class that uses the
  trait unless the class chain already has the method; subclass
  overrides are seen because the body calls through `self` by name. In
  structs a default becomes a struct method that always writes back,
  since the shared node cannot carry a per-struct "mutates" flag. Two
  traits providing the same default is an error.
  Required properties landed (first part of increment 3): `var hp: int`
  in a trait must be matched exactly by a member variable, script or
  native property (classes) or a field (structs); trait-typed values
  expose it, and default methods use it by bare name
  (`IdentifierNode::TRAIT_PROPERTY`, a named access on `self`). Traits
  still hold no state. Open: signals and constants in traits, traits
  using traits, global `trait_name` files. Next: typed Callables
  (item 6), unless trait composition is needed first.
- Tooling parity is part of "done" for every language feature from
  here on. Checklist: parser/analyzer/compiler; class reference docs;
  editor completion (`gdscript_editor.cpp`: type names, class members,
  members of a value, identifiers in scope); editor lookup
  (`_lookup_symbol_from_base`, for hover and go-to-definition, which the
  language server also uses); language server document symbols
  (`ExtendGDScriptParser::parse_class_symbol`) and the script API dump;
  debugger variable display (the debug adapter's `parse_variant` and the
  inspector's `parse_property`); the syntax highlighter (free: keywords
  come from the tokenizer); the godot-tools VS Code grammar (external, a
  keyword PR). Struct pass landed: struct symbols with field and method
  children in the outline (kind `Struct`, fields `Field`), a `structs`
  section in the script API, completion of struct names as types, of
  fields and methods on a struct-typed value (script or engine layout),
  and of fields and sibling methods inside a struct method; hover and
  go-to-definition for struct names, fields and methods (struct types
  now carry `script_path`); struct values expand field by field in the
  debug adapter; the inspector shows a read-only `EditorPropertyStruct`
  (type name, fields in the tooltip). Lesson: completion cannot lean on
  analyzer results, since the cursor usually leaves a parse error and a
  constant-folded `Point(1.0, 2.0)` reaches the guesser as a bare
  `Struct` value; `gdscript_editor.cpp` rebuilds struct types from the
  parse tree (`_struct_node_type`, `_find_struct_in_scope`) and maps a
  struct value back to its declaration by layout. Also: the completion
  test harness cannot complete after a bare space (`var p: ➡` yields
  nothing, a pre-existing limit), so tests use a partial identifier. Not
  done: an editable struct property editor, and `struct`/`trait` in the
  VS Code grammar.
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
