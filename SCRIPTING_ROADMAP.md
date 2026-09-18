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

14. Typed `PackedScene` exports (slot only accepts a given root type). **Done.**
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
- 6, first part (typed Callables) landed; design in
  `TYPED_CALLABLES_DESIGN.md`. `func(int, String) -> bool` is a type for
  variables, parameters and returns. The signature lives only on the
  analyzer's `DataType` (`has_callable_signature`, `callable_signature`);
  at runtime it is a plain Callable. Lambdas, function references and
  methods (through `MethodInfo`) carry signatures; assignment checks
  argument count (optional and variadic parameters count), contravariant
  parameters and a covariant return; a plain `Callable` fits unchecked.
  `f.call()` checks its arguments and has the declared return type, which
  the compiler validates where the call returns. Not done: lambda
  parameter inference from the expected type, `f(1)` call syntax, and
  generics, which need their own design.
- 6, second part (generic functions) landed; design in
  `GENERICS_DESIGN.md`. `func first[T](items: Array[T]) -> T` binds its type
  parameters from the argument types at each call, checks the arguments
  against the bindings and gives the call the bound return type. Type
  parameters are erased: `TYPE_PARAMETER` compiles to an untyped slot and a
  container that mentions one compiles to a plain container, because the
  runtime compares element types exactly. That erasure is why a type
  parameter may not appear inside a *return* container type (`-> Array[U]`
  would have to build a typed array it cannot name); the error says so and
  points at reification. Next in item 6: reified type parameters, then the
  built-in container methods, then generic classes.
- 6, the sound part of the built-in container methods, landed: the analyzer
  now keeps element types through `Array[T].filter/slice/duplicate/front/
  back/pick_random/get` and `Dictionary[K, V].duplicate/keys/values`,
  matching what the runtime already does (verified against a build, not
  assumed). `map()` stays untyped because it genuinely is; `pop_back()`,
  `pop_front()`, `min()` and `max()` stay `Variant` because they return
  `null` on an empty container, and sharpening them would break the
  pop-until-empty loop. The same knowledge existed only in the completion
  guesser before.
- 6, generic functions returning containers, landed. A generic function may
  now return `Array[T]` / `Dictionary[K, V]`; the container it builds is
  untyped inside the body, and the **call site** converts it into the bound
  type with `assign()`, since that is where the binding is known. Costs one
  extra pass over the result and nothing else: no hidden arguments, no
  calling-convention change, no new opcodes, so a generic function stays an
  ordinary function. A body that produces the wrong element type is reported
  at the call line. Reification by hidden arguments and monomorphisation
  were both considered and rejected; the reasons are in the design.
- 6, generic classes, landed: `class Pool[T]:`, `Pool[int]` annotations,
  and per-instance substitution of members and method signatures. Invariant
  in the arguments; `Pool.new()` has none and fits any binding, so
  construction needs no new syntax. Container-returning methods convert at
  the call site (and so copy); a container *member* read from outside is a
  plain container, which is what erasure really leaves. Trap: applying the
  class's arguments with the shared substitution helper erased the
  function's own type parameters to `Variant` and silently broke generic
  functions, caught only because their tests were in place.
- Short lambdas landed (not previously on the list; asked for because
  block lambdas read badly inside a call). `params => expression` is a new
  token `=>` plus a parser that desugars to `func(params): return expr`;
  one parameter needs no parentheses, `()` and `(a, b)` are handled in
  `parse_grouping()`. Beyond syntax: a lambda argument takes its parameter
  types from the callable the callee expects, with built-in container
  methods describing theirs separately (`MethodInfo` only says `Callable`),
  and `Array[T].map()` is typed from the callable's return type and
  converted at the call site with the machinery added for generic
  containers. Trap found by running it: a lambda's inferred return type is
  not a *hard* type, so requiring hardness silently disabled the whole
  thing for `n => n * 2` while leaving it working for `n => to_text(n)`.
  Second trap: `() => print(x)` desugars to `return print(x)`, and the
  "cannot get the return value of a void call" check lives in *five* places
  in `reduce_call()` (utility functions, built-ins, struct and trait
  methods, scripts). An arrow lambda body passes `p_allow_void` so a body
  that returns nothing simply makes the lambda return nothing.
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
  still hold no state.
  Rest of increment 3 landed: constants (`Trait.NAME`, folded), signals
  (added to every using class; structs cannot use such a trait), and
  traits using traits. Everything works on the closure of included
  traits: lookup, conformance, the runtime trait set and assignability;
  a default in one trait can satisfy a requirement of another. Other
  scripts' traits work as `Lib.Trait` through the normal member lookup.
  Global `trait_name` files are deliberately not done (see the design).
  Item 7 is complete for now; an interface method table for direct
  dispatch stays as a possible optimization.
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
  done: an editable struct property editor.
- VS Code needs no forked extension. Decided by inspecting the installed
  geequlim.godot-tools 2.7.1: everything it knows about the code it asks
  our editor for over LSP/DAP, so completion, hovers, definitions,
  diagnostics and the debugger were already correct. The single static
  piece is its bundled TextMate grammar, and it contributes
  `source.gdscript` without `injectTo`, so an injection grammar can
  extend it from outside. `misc/vscode/gdscript-fork-syntax` is that
  injection: `struct`, `uses`, `=>`, and the type parameters in
  `class Pool[T]` / `func pick[T](...)`. (`trait` already highlights:
  mainline reserves the word and the grammar lists it.) Two lessons, both
  found by tokenizing a sample with vscode-textmate rather than by eye:
  the bundled `func`/`class` declaration rules require `(` or `:`
  immediately after the name, so a type parameter list silently drops the
  name's highlighting; and `L:` injection priority only wins *ties*, so
  `item => x` needed a rule starting at `item`, because inside a call the
  bundled named-argument rule matches `item =` from one token earlier.
- 8 (nullable types) landed: design in `NULLABLE_DESIGN.md`. `T?` on any
  type, `?.`/`?[` safe navigation (each guards its own step, and a skipped
  call skips its arguments), right-associative `??`, and flow narrowing
  that makes `T?` usable. Narrowing covers `!= null`, `== null`, a bare
  truth test, `not`, and `is_instance_valid()`, composed through `and` and
  `or` into the right operand, plus guard clauses via
  `SuiteNode::has_return`; it is keyed by the declaration, is limited to
  locals and parameters, and is dropped on assignment. Enforcement reuses
  the warning machinery: `UNSAFE_NULLABLE_ACCESS` and
  `NULL_ASSIGNED_TO_NON_NULLABLE` are in `is_strict_mode_error()`, default
  to `IGNORE`, and so are errors in strict mode and silent elsewhere.
  Object `T?` is free at runtime; a nullable builtin becomes a Variant
  slot. Two opcodes (`JUMP_IF_NULL`, `JUMP_IF_NOT_NULL`), no
  `GDS_NOINLINE` needed since neither holds locals. Tooling: completion
  after `?.`, after `??` and on a narrowed value all worked untouched
  (tests under `completion/nullable/`); the VS Code injection grammar
  gained `?`, `?.`, `?[` and `??`. Traps are listed under "As built" in
  the design doc; the two worth repeating here are that
  `resolve_datatype()` needs `is_nullable` set *twice* (a class/native
  lookup replaces `result` wholesale, so `Item?` came out as `Item` while
  `int?` worked), and that `REDUNDANT_NULL_CHECK` must not key off
  `is_nullable` alone, since a plain object type is only a promise where
  the promise is enforced. Not done: definite initialization, i.e. the
  `@export var n: Node` and uninitialized-member rules.
- Struct returns from engine APIs now complete. Reported from manual use as
  "the `_struct` API calls don't have a typed return type". The *analyzer* was
  fine (`t.no_such_field` on a `Time`/physics struct result is a proper error),
  so this was editor completion only: `gdscript_editor.cpp` has its own
  `_type_from_property()`, separate from the analyzer's, and it dropped the
  layout for `Variant::STRUCT`, leaving a plain struct type that the member
  lister skips. It now reads `PROPERTY_HINT_STRUCT_TYPE` through `StructDB`
  like the analyzer does. Tests: `completion/struct/engine_struct_members.gd`
  and `engine_struct_physics.gd`. Still missing, and pre-existing for every
  typed container rather than specific to structs: that same function drops
  container element types, so `intersect_shape_struct()` completes as a plain
  `Array` and its elements offer nothing. Resolving an element type needs an
  analyzer instance (`type_from_property_hint_string()` is not static), which
  is why it was not folded in here.
- 9, first part (typed signal connect) landed, from feedback item 6. A
  `Signal`'s `DataType` already carried the signal's `MethodInfo`, so
  `connect()`, `disconnect()` and `is_connected()` now describe their callable
  parameter from it, the same way the built-in container methods describe
  theirs, and a lambda written at the call site takes its parameter types from
  the signal. `hit.connect(d => d.to_upper())` is now an error on `int`
  instead of a runtime surprise. Works for script and native signals alike;
  a bare `Signal`-typed value has no signal identity, so it is left alone, and
  a hand-written parameter type still wins. Not done, and the rest of item 9:
  checking the *arguments* of `emit()` against the signal, and rejecting a
  connected callable whose signature does not fit.
  A third thing, found by running it on real code, turned out to be a defect in
  `emit` rather than in the inference; it is the entry above. Two more, both
  pre-existing and both bigger than this change:
  - Inferred lambda parameters do not survive strict mode. **Fixed**, see the
    entry above; the same deferral also covers the inferred return type.
  - Completion inside an arrow lambda offered members without parentheses.
    **Corrected and fixed**, see the entry above: the first reading of this,
    that completion could not see an inferred lambda parameter at all, was
    wrong and came from a bad test.
- Generic constructors bind the class's type parameters, reported from manual
  use: `Tiles.new(size, func() -> Tile: return Tile.new())` on
  `class Tiles[T]` with `_init(_size: Vector2i, create: func() -> T)` failed
  with `argument 2 should be "func() -> T" but is "func() -> Tile"`. Only a
  *function's* own type parameters were bindable, and a class's were only read
  from an annotated base (`Pool[int].add()`), which a `new()` call does not
  have, so `T` reached the argument check unbound. The class's type parameters
  are now added to the bindable set for a constructor call. Binding is real,
  not erasure: `Pair.new(1, "two")` on `class Pair[T]` now reports the second
  argument. The call's own type still carries no arguments, so `Pool.new()`
  keeps fitting any annotation, which is what makes construction need no new
  syntax. Short lambdas bind too (`Tiles.new(() => Tile.new())`). Not done,
  and the reason feedback item 1 is still open: since the result carries no
  arguments, a binding that contradicts the annotation
  (`var t: Tiles[Other] = Tiles.new(() => Tile.new())`) is not reported.
- 9, second part (`emit` checked against the signal) landed, found by running
  the first part on real code:
  `signal test_signal(array: Array[String])` with
  `test_signal.connect(a => print(a.filter(b => b.begins_with("k"))))` and
  `test_signal.emit(["kebab", "pizza"])` failed at runtime with "Cannot convert
  argument 1 from Array to Array". Typing the handler's parameter made the
  latent bug visible: `Signal.emit` is a varargs builtin whose `MethodInfo`
  says nothing about the signal, so the array literal stayed a plain `Array`
  and the handler, which now declares `Array[String]`, refused it. A
  hand-written `func(a: Array[String])` handler failed identically before any
  of this, so the defect was in `emit`, not in the inference. `emit()` on a
  statically known signal now takes its parameter types from the signal's own
  `MethodInfo` instead of the varargs one, which makes the existing machinery
  do three things at once: array literals are built with the right element
  type (`update_array_literal_element_type`), argument types are checked, and
  arity is checked. `hit.emit("x")` and `hit.emit()` on `signal hit(dmg: int)`
  are now errors. A `Signal` value with no static identity is left alone.
  Lesson worth keeping: inferring a *container* type for a parameter is only
  safe if the value handed in really carries its element type at runtime, so
  the inference and the conversion at the producing end have to land together.
  Still not done in item 9: rejecting a connected callable whose signature does
  not fit the signal.
- Inferred lambda parameters now survive strict mode, which is what made the
  short-lambda and typed-signal work usable in the mode this fork pushes.
  Reported against real code:
  `test_signal.connect(a => print(a.filter(b => b.begins_with("k"))))` under
  strict mode produced four errors -- `a` and `b` "has no static type" and two
  "has no static return type" -- even though every one of those types is
  inferred. The untyped-declaration checks ran while the lambda's *signature*
  was resolved, which is before the call site applies the expected parameter
  types and before the body determines the return type, so they were asking
  for types that did not exist yet. Both checks are now deferred for lambdas
  to the end of `resolve_function_body()` and warn only about what is still
  genuinely unknown. `nums.map(n => n * 2)` was equally broken and is equally
  fixed. Deliberately unchanged: a *named* function without a return type is
  still an error under strict mode, since there a missing annotation means
  "untyped" rather than "work it out", and inferring `void` would change what
  existing code means. That is the answer to feedback item 7: the noise was
  never really `-> void`, it was strict mode discarding inference it already
  had. Test: `analyzer/warnings/untyped_declaration_lambda.gd`, which enables
  the warning and pins both halves -- inferred parameters silent, a standalone
  `func(x)` still reported.
- Completion inside an arrow lambda now offers members with parentheses.
  Chasing "the intellisense can't pick up the values" turned up three separate
  things, only the last of which was real, and the first two are worth
  recording because they cost the most time:
  1. The first tests were invalid. They completed `n.to_` on an `int` and
     expected `to_char()`. **`int` has no methods in Godot** -- there is no
     `bind_method` for `Variant::INT` in `variant_call.cpp` -- so completion
     was right to offer nothing. Any completion test for a builtin has to pick
     a type that actually has methods; `String` is the safe one.
  2. On the corrected tests, completion inside a lambda body works fine,
     including for a parameter whose type was *inferred* from the call. The
     guesser resolves it through the ordinary local lookup, so no new
     machinery was needed. `func(w): return w.to_➡` inside `map()` already
     passed.
  3. What was actually broken: inside an **arrow** lambda the members came
     back without parentheses (`to_upper`, not `to_upper()`), because the
     parser leaves the enclosing call on `completion_call_stack` while parsing
     the body, so `_guess_expecting_callable()` still saw "this argument wants
     a `Callable`" and suppressed them. Inside the body that is the wrong
     question: the call wants the lambda, not what the lambda computes.
     Upstream already asserts the intended behaviour for block lambdas
     (`completion/no_parenthesis_when_callable_is_expected/lambda_body.gd`);
     the arrow path simply never got it. Parsing an arrow body now hides the
     stack and restores it afterwards, so a call written inside the body still
     pushes its own entry.
  Tests: `completion/lambda/`, a matrix of explicit and inferred parameters in
  block and arrow lambdas, standalone and as a call argument.
  Method for next time: when a completion test fails, check what the guesser
  resolved before assuming the type is missing. Both wrong turns here would
  have been caught by asking "does this method even exist on that type".
- Generic constraints landed (feedback item 3), design in `GENERICS_DESIGN.md`.
  `class Registry[T: Named]` and `func label[T: Named](...)`, where the bound
  is a trait, a script class or a native class. The bound is checked where the
  argument is named (`Registry[Rock]` is an error at the annotation) and at a
  call, where inference binds it. Inside the body a bounded `T` offers the
  bound's members and is assignable to the bound; the other direction stays
  closed, since a binding may be narrower. Why this was "necessary" rather
  than nice: under strict mode reaching into an unbounded `T` is an error, so
  without bounds a generic body could only pass values along. Tooling in the
  same pass: completion of the bound after `[T: `, members of a bounded `T`
  in completion and in hover/go-to-definition (one helper each, mirroring the
  analyzer's), and the VS Code injection grammar, verified by tokenizing
  `class Registry[T: Named]:` with vscode-textmate against the bundled
  godot-tools grammar rather than by eye.
  Not done: bounding one parameter by another (`[T, U: T]`), and multiple
  bounds (`[T: Named & Damageable]`), which the trait system could express but
  nothing has asked for. Noticed and left alone, pre-existing: in the bundled
  VS Code grammar a type parameter *used* as a type (`item: T`) tokenizes as
  `variable.other.constant`, since it looks like a constant to a static
  grammar.
- Union bounds landed, asked for as "constraints on addability or something,
  like either float or int or vectors". A nominal bound cannot express
  "supports `+`", since `int`, `float` and `Vector2` share no named type, so a
  bound may list alternatives: `func total[T: int | float | Vector2]`. The rule
  is that the body must work for every binding, so `get_operation_type()` asks
  once per member and answers only when they agree: each member giving back
  itself makes the result `T`, all members giving the same concrete type makes
  it that type, and anything else leaves the operator unavailable, so
  `%` on `int | Vector2` reports invalid operands. Member *lookup* stays closed
  for a union (choosing one alternative would be a guess); a single bound is
  unchanged. Assignment out of a union-bounded `T` needs every member to fit.
  The alternative considered was C#'s spelling, which is worth recording: C#
  could not do generic math at all until .NET 7, when static abstract interface
  members let `INumber<T>` require `static abstract T operator +(T, T)` and the
  standard library added conformances to every numeric type. Doing that here
  would mean a hand-maintained table in the analyzer saying which `Variant`
  types satisfy an `Addable` trait. The union needs no table, covers user
  structs too, and is the smaller primitive that a named `Addable` could later
  be sugar for. Grammar updated for `|` inside the brackets and verified by
  tokenizing, as with the `:`.
- Generic construction now carries its binding (feedback item 1, and the
  soundness hole left by the constructor work). A constructor whose own
  arguments decide every type parameter gives the call that binding, so
  `var tiles := Tiles.new(func() -> Tile: ...)` is a `Tiles[Tile]` and the
  class is named once, and an annotation that contradicts it is an error
  instead of being quietly accepted. `Pool.new()`, which binds nothing, still
  carries no arguments and fits any annotation, so the no-syntax construction
  rule is intact.
  Limit, and a correction to an earlier claim in this file: the binding is read
  off a **declared** type, so `Tiles.new(func() -> Tile: ...)` binds and
  `Tiles.new(() => Tile.new())` does not. An earlier commit message said short
  lambdas bind; what it had actually shown was that they *fit* an annotated
  target, where an unbound `T` substitutes to `Variant` and accepts anything.
  Three ways to close that were tried against a build and all reverted: calling
  `resolve_lambda_body_now()` before binding (a lambda's signature is built in
  `reduce_lambda()` before its body, so the inferred return is not there yet --
  true, but not sufficient on its own); letting an inferred return type into
  `set_callable_signature_from_function()`; and dropping the `is_hard_type()`
  guard in `bind_type_parameters()`. The third did change behaviour, for the
  worse: the compile-time error became a runtime one, because `T` bound to
  something that substituted back to an unsafe type. The real fix needs the
  binding rule for inferred types thought through rather than the guard
  removed, so it is left undone deliberately.
  The requested spelling cannot be had: `var a: Pool[int] = new()` reads well,
  but **bare `new()` is already valid GDScript** and constructs the enclosing
  script. Verified by running it rather than by reading the grammar, since the
  analyzer's error message ("Cannot assign a value of type res://n1.gd") was
  the only hint. Taking it over would silently change existing code, so it is
  refused. `Pool[int].new()` is the other option and stays open; besides the
  subscript-versus-index ambiguity it needs the parser to accept
  `Registry[K, V]`, a comma-separated subscript that is not currently syntax.
- Feedback item 2, reified type parameters: **pinned**, design explored and not
  started. What is settled, each point checked against a build rather than
  assumed:
  - A type is a value in GDScript for classes (`GDScript`), native classes
    (`GDScriptNativeClass`) and **structs** (`StructLayout`, so
    `var x = Tile` already works and prints one). It is *not* a value for
    builtins: `var t = int` is "Builtin type cannot be used as a name on its
    own". So reification can never be uniform, and the honest gate is "bounded
    by anything except a builtin", which is wider than the first reading of
    "object bound only".
  - A struct already works as a bound today: `class Tiles[T: Tile]` with a
    struct `Tile` types `item.x` correctly. Nothing was needed for that.
  - `StructLayout::instantiate()` exists in C++ but is not bound to script, so
    a layout can be inspected and not built from. That is the one missing piece
    for constructing a struct-bound `T`.
  - Spelling: `T.new()` for a class bound and `T(...)` for a struct bound,
    matching how each kind is already constructed. `T.new()` cannot serve both,
    because a `StructLayout` is itself a RefCounted and `new()` on it already
    means "make another layout".
  - `default`, the C# answer, was considered and is not enough on its own: it
    dodges the builtin problem because it denotes a value rather than a type,
    but for a class binding it is `null`, so a grid filled with `default` is a
    grid of nulls. It also needs a new keyword, and `default` is a plausible
    identifier in existing code.
  - `x is T` is not the spelling to aim for even reified: `is` tests an
    instance against a type, and `T` would be the type. `T == Other` is.
  - Cost, the reason this is the largest item on the list: instances must carry
    their bindings at runtime, which is new compiler *and* runtime work and the
    first place "generics are fully erased" stops being true.
- One-line property accessors landed, the first half of feedback item 5
  (`var is_alive: get(): return hp > 1`). It turned out to be one guard: the
  parser already understood `get():` and `parse_suite()` already accepts a
  single-line body, exactly as `func f(): return 1` does, so the only thing
  stopping it was "Property with inline code must go to an indented block".
  Removing that check makes `var alive: bool: get: return hp > 0` work in any
  class, not just in traits. The whole suite passes unchanged, which is the
  evidence that the guard was protecting nothing. Both `get:` and `get():` are
  accepted; only one accessor fits on a line, since the accessor loop stops at
  the newline.
  Grammar: the bundled VS Code grammar reads everything after a `:` in a
  declaration as a type annotation, so `get` *and the whole body* came out as
  type names -- `return` highlighted as a class. The injection now re-scopes
  from the colon and includes `source.gdscript` for the body. Two traps, both
  caught by tokenizing rather than by eye: the rule has to begin at the `:`
  and not at `get`, because injection priority only wins ties and the bundled
  annotation rule starts at the colon (the same lesson `=>` taught); and the
  first attempt silently could not match because a heredoc turned `\b` into a
  literal backspace (0x08) in the JSON. Write grammar edits from a script file,
  as the memory note about heredocs says.
  Still open, the second half of item 5: a trait *providing* a property with an
  accessor body. Traits parse `var` with properties disabled on purpose, since
  a trait property is a requirement; providing one means a `trait_default_properties`
  list next to `trait_default_methods`, resolved in the trait's context and
  compiled into every using class that does not define it, plus an error for
  structs, which have fields rather than properties.
- Qualified generic types (`Lib.Pool[int]`) landed, which was a real hole
  rather than a new feature: `parse_type()` parsed `[...]` only straight after
  the *first* identifier and returned, so the arguments could never attach to a
  dotted name. A generic class in another file was therefore unusable -- the
  annotation was a syntax error, and without one the call `p.add(7)` failed with
  `argument 1 should be "T" but is "int"`. The chain is now parsed first and the
  arguments attach to the whole name, so `Array[int]` is unchanged and
  `Lib.Pool[int]` works, with the binding enforced across files. Found while
  answering whether a "module" file could hold generics.
  Context for the rest of feedback item 9, from the discussion that turned it
  around: a generic `class_name` is the wrong shape, because the editor cannot
  offer a node type that still needs arguments. Naming a *specialisation* is the
  right shape -- `class_name CertainTileMap extends TileMap[CertainTile]` has no
  type parameters, so it behaves like any other class in a node slot or an
  export. That needs `extends` to accept arguments (parser) and member lookup to
  carry the base's bindings along the class chain (analyzer); today they are
  only applied to access through an annotated value.
  Also settled by testing: the "module file" half largely exists already. A
  `.gd` file holding traits, enums, generic classes and inner classes is reached
  as `Lib.Damageable`, `Lib.Element.ICE`, `Lib.Pool[int]`, either through
  `preload` or through `class_name` in a scanned project, which is what
  `TRAITS_DESIGN.md` already recommends instead of `trait_name` files. Marking
  such a file `@abstract` stops it being instantiated. What Godot has no concept
  of is a file that is *not* a class: one file is one class, and the global
  registry maps a name to a script, so a true namespace would need a new
  registry rather than a parser change. Not attempted.
- "Global scope" files, where a file's traits and enums are in scope everywhere
  without a `Lib.` prefix: **pinned**, design settled, spelling undecided.
  - Not through `ScriptServer`. `GlobalScriptClass` maps a name to a script
    *path* and assumes that script's root class is the type, so a trait or enum
    inside a file has nowhere to go in it. Adding a member path there would
    reach C#, the editor class list, the node-creation dialog, `@export` hints,
    the docs generator and `project.godot` serialization, and would hand all of
    them a "global class" that is not a script.
  - The route that fits is GDScript-only: a list of files whose top-level
    declarations are consulted when a name fails to resolve, reusing the member
    lookup that already makes `Lib.Damageable` work. The analyzer consults
    `ScriptServer::is_global_class()` in seven places (type names, identifiers,
    property and inheritance resolution); the new lookup goes beside each, which
    is what keeps the work bounded.
  - The limitation is also the point: such names would not be editor-visible
    node types -- no class-list entry, no node script type, no
    `@export var x: SomeGlobal`. That is what "without the `class_name`
    semantics" asks for, and it is fine for traits and enums, but it means a
    global-scope file cannot replace `class_name` for real node classes.
  - Undecided, and the reason this is pinned: the spelling. A project setting
    listing the files needs no scanning and behaves the same at runtime and in
    the editor; a `@global_scope` annotation reads better but needs the editor
    to scan for it and somewhere to persist the result, the way `class_name`
    goes through `project.godot`; a per-declaration `@global` adds per-member
    bookkeeping on top of that.
  - Either way the fork's tooling-parity rule applies: completion of the names
    as types and as identifiers, hover, go-to-definition, LSP document symbols,
    plus an error when two files export the same name.
- `extends Pool[int]` landed, which closes feedback item 9 in the shape the
  feedback itself proposed: a generic `class_name` is wrong because the editor
  cannot offer a node type that still needs arguments, but naming a
  *specialisation* gives an ordinary class that can take a `class_name`, sit in
  a node slot and be exported. Verified end to end with
  `class_name CertainGrid extends GridBase.Grid[CertainTile]` in a scanned
  project, used from another script by its global name.
  The parser part is small (arguments after the `extends` chain, applied once
  where inheritance resolves, with the same bound check an annotation gets).
  The work was in member lookup: `Pool[int]` carries its arguments on the type,
  which `member_type_for_base()` already handled, but `IntPool` carries them on
  the *step up* to `Pool`, so the chain from the value's class to the declaring
  class has to be walked while accumulating bindings. `bindings_from_class_chain()`
  does that and both lookups use it, methods through `get_function_signature()`
  and members through `member_type_for_base()`. Bindings compose, so
  `class Middle[U] extends Pool[U]` under `class StringStack extends Middle[String]`
  resolves `T` through `U` to `String`.
  Noted honestly and not reproduced: the very first cold-cache editor scan of
  the throwaway test project segfaulted on quit. Four later runs, three of them
  cold, exited 0 with no crash in the log, so it is recorded rather than
  explained away. Worth watching if it recurs when several new `class_name`
  files appear at once.
- Trait-provided properties landed, the second half of feedback item 5, so a
  derived value in a trait no longer has to be a method:
  `trait Mortal: var is_alive: bool: get: return hp > 0`. Built as the
  property-shaped twin of a default method, and that choice is what makes it
  sound: the accessors are analyzed once in the trait's context, so `hp` is a
  by-name `self` access and one compiled accessor is right in every using
  class even though `hp` sits at a different member index in each (checked
  with two classes that place it differently). The alternative, injecting the
  property into each class's member list, looked simpler and was rejected on
  exactly that point: a shared node resolved in class context would carry the
  *first* class's member index into every other one. Design and the rest of
  the rules are in `TRAITS_DESIGN.md`.
  One footgun found by testing rather than by reading, and fixed: a property
  with only a getter accepted `guy.is_alive = false`, which wrote a slot every
  read bypasses, so the assignment appeared to work and did nothing. It is now
  read-only through the flag native getter-only properties already use.
  Tooling turned up a pre-existing gap wider than this feature: nothing a trait
  contributes to a class -- default methods included -- was offered by
  completion on a class value or inside the class, and hover and
  go-to-definition walked past it. Both are fixed for everything a trait
  contributes, and the new LSP fixture markers were checked to fail with the
  lookup fix removed, so they are not decoration.
- 9 (typed signals) is complete: `connect()` now checks the handler, which was
  the last part left. The first design would have reused
  `callable_signatures_compatible()`, and it is worth recording why it could
  not: that check is contravariant, so it rejects
  `func _on_body_entered(body: CharacterBody2D)` for a `Node2D` argument,
  which is how Godot handlers are normally written. The handler check is its
  own, and rejects only what can never work: the wrong number of arguments,
  and a parameter unrelated to the argument in both directions. Arity is the
  valuable half -- verified that a handler taking too few arguments never runs
  at all, leaving one "expected 0 argument(s), but called with 1" line in the
  output -- and the message points at `unbind()`.
  Traits turned out to be the interesting edge. Allowing class narrowing is
  sound because a class parameter is checked when the function is called
  (verified: a `Node2D` passed to a `CharacterBody2D` parameter never enters
  the body). A trait parameter is *not* checked (verified: a `Rock` that does
  not use `Damageable` entered the body and only failed at `take_damage()`),
  which is also why assigning an object to a trait-typed variable already
  needs `as`. So a trait-typed handler parameter is refused with a message
  that gives the fix, and the fix was checked to compile and run. A plain
  `Callable`, including anything through `unbind()` or `bind()`, stays
  unchecked, the same rule as assigning one.
- 14 (typed `PackedScene` exports) landed. `PackedScene[Enemy]` is a type
  whose `instantiate()` makes an `Enemy`; `preload()` of a scene reads the root
  from the scene it just loaded, so it is typed without an annotation; a wrong
  root is a compile error naming both (`PackedScene[Player]` vs
  `PackedScene[Enemy]`). Covariant in the root, unlike `Array[T]`, because a
  scene is only read from. A plain `PackedScene` fits a typed slot unchecked,
  the rule a plain `Callable` already follows. The root has to be a node.
  - Runtime: the argument is erased, so the slot is a plain `PackedScene`, and
    the one place a wrong root could slip in unnoticed is checked instead:
    `instantiate()` on a typed scene lands in an untyped temporary and is
    assigned into the typed one, the same call-site pattern the generic
    container conversion uses. Before that, a wrong scene got through
    `instantiate()` and failed at the first member access as a misleading
    "Invalid access to property 'hp'"; now it fails on the `instantiate()` line.
  - Shared lookup: `SceneState::get_root_type()` reads the root's class and
    script without instancing, following an inherited scene to its base, and
    `SceneState::is_root_of_type()` matches it against a native class, a global
    class name or a script path. The analyzer and the inspector both use them;
    C++ tests cover native, empty and inherited scenes.
  - Editor: a new `PROPERTY_HINT_SCENE_ROOT_TYPE` (added at the end of the enum,
    as the struct hint was, with `PROPERTY_HINT_MAX`'s documented value moved
    up) carries the root, as a global class name where there is one and the
    script path where there is not. The inspector gives such a property an
    `EditorResourcePicker` with the new `scene_root_type`, which refuses a
    wrong scene in `is_resource_allowed()`, the file/Quick Load selection and
    drag-and-drop. Verified by driving the real picker and
    `EditorInspector.instantiate_property_editor()` from a headless editor
    plugin: Player refused, Boss and Enemy accepted, a script-path root
    honoured, the plain export untouched. That run also caught a wrong message
    ("only accepts 'PackedScene'" about a `PackedScene`), now specific to the
    root.
  - Not done: filtering the Quick Load *list*. Wrong scenes are refused on
    every path but still listed; filtering needs each candidate's root, which
    means loading each scene, and wants a cache in `EditorFileSystem` rather
    than a slow dialog.
  - Trap for anyone writing tests with resources: `--gdscript-generate-tests`
    builds its runner without initializing the project, so `res://` is the repo
    root there and the tests folder in the doctest suite. A fixture that names
    a resource by an absolute `res://` path, or an error message that prints
    one, differs between the two. Use paths relative to the scene
    (`path="enemy.notest.gd"` in the `.tscn`) and keep paths out of expected
    messages.
- Lessons from 4a: the result of a discarded call must never be written
  to the shared `nil` stack slot (GH-70964), and `_ready` must keep
  going through `GDScriptInstance::callp()` so `@onready` runs first.
- The documented test command was wrong until now: `--test-case="*GDScript*"`
  skips the completion and LSP suites, which are separate *suites*. Use
  `--test-suite="*GDScript*"`. Found by deliberately breaking a new
  completion test and watching the run stay green.

## Feedback from manual testing (2026-09-17)

First round of hands-on use of the landed features. Each item was reproduced
against a build before being written down; the two that turned out to already
work are recorded as such so they are not "fixed" twice.

1. **Generic construction reads badly.** `var a: Pool[int] = Pool.new()`
   repeats nothing useful, and naming the class twice looks wrong when the
   annotation already fixes the binding. Asked for: `var a: Pool[int] = new()`.
   `Pool[int].new()` is a parse error today ("Builtin type cannot be used as a
   name on its own"). Both spellings are open; a bare `new()` needs the
   analyzer to take the constructed type from the assignment target, which is
   context the expression does not have today. Was already listed under "Not
   done for generic classes" in `GENERICS_DESIGN.md`. **Partly done**, see Progress:
   the constructor's own arguments now bind the class, so the annotation can
   be dropped where they decide it. The asked-for `new()` is refused because
   that spelling already means "construct the enclosing script".
2. **No access to the type parameter inside a generic class.** `print(T)` and
   `T is Something` are "Identifier not declared". This is reification, which
   the design rejected for generic *functions* on purpose. Generic classes are
   the tractable case: an instance could carry its bindings, but only if
   construction knows them, so this is blocked on item 1. Doing 1 without
   thinking about 2 would pick the wrong spelling.
3. **Constraints are necessary, not optional.** `class Pool[T: RefCounted]` is
   a parse error. `GENERICS_DESIGN.md` said "worth having later"; use says
   otherwise, since without a bound a type parameter is opaque and the body
   can do nothing with a `T` but pass it along. Traits are the natural bound. **Done**, see Progress.
4. **Signals in traits: already works.** Verified: a `signal` in a trait is
   added to every using class, and it connects and emits both on the concrete
   class and through a trait-typed value. Landed with the rest of trait
   increment 3; no work needed. What traits cannot do is *require* a signal of
   the using class, which is a different feature and has not been asked for.
5. **Accessor defaults in traits.** *(Done: the one-line accessor form and
   trait-provided properties, see Progress.)* Asked for `var is_alive: get(): return hp > 1`
   in a trait, so a derived property does not have to be a method. Two separate
   gaps: a trait may not declare a property with an accessor body at all
   (traits hold no state, and required properties must be matched exactly by
   the using class), and the one-line accessor spelling does not exist even in
   a plain class ("Property with inline code must go to an indented block") --
   that half is a mainline limitation and would apply to all code, not just
   traits.
6. **Signals do not feed typed Callables.** Verified precisely: passing
   `d => d.whatever()` to a parameter declared `func(int) -> void` correctly
   errors on the `int`, but `hit.connect(d => ...)` leaves `d` untyped, because
   `connect` takes a plain `Callable` in `MethodInfo` and the signal's declared
   parameters never reach the lambda. So short lambdas lose their inferred
   types exactly where they are most used. **Done**, see Progress; building it
   turned up two pre-existing limits that matter more than the feature did:
   strict mode rejects an inferred lambda parameter, and completion cannot see
   one. Both apply to `map()` as much as to signals.
7. **`-> void` is noisy.** Under strict mode a function without a return type
   is an error ("has no static return type"), so every handler and lambda
   carries `-> void`. **Answered and half done.** For lambdas the real problem
   was not the annotation but that strict mode threw away the inference it
   already had; both the parameter and the return-type checks now run after
   the body, so `n => n * 2` and `a => print(a)` need nothing written out.
   For a *named* function the decision is to leave it alone: a missing return
   type there means "untyped", and inferring `void` would silently change what
   existing code means.
8. **Struct-returning engine APIs did not complete.** Fixed, see Progress.
9. **Generic global classes.** Asked for `class_name` on a generic class, so
   generics can be used for components across files. Currently inner classes
   only; `GENERICS_DESIGN.md` lists it under "Not done" together with
   `extends Pool[int]`. Global classes are registered by name with no place to
   put arguments, so this needs design, not just parsing. **Done**, in a better
   shape than asked: a generic `class_name` is not the answer, since the editor
   cannot offer a node type that needs arguments. `class_name X extends Pool[int]`
   names the specialisation instead, and that is an ordinary class.

Order these imply: 6 (small, self-contained, high daily value), then 3, then
1+2 together, then 9, then 5 and 7 which both need a decision first.

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
