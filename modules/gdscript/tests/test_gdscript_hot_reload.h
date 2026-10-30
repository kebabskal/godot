/**************************************************************************/
/*  test_gdscript_hot_reload.h                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "../gdscript.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "tests/test_macros.h"
#include "tests/test_utils.h"

namespace TestGDScriptHotReload {

static Ref<GDScript> compile(const String &p_source) {
	GDScriptLanguage::get_singleton()->init();
	Ref<GDScript> script;
	script.instantiate();
	script->set_source_code(p_source);
	REQUIRE(script->reload(true) == OK);
	return script;
}

static void recompile(const Ref<GDScript> &p_script, const String &p_source) {
	p_script->set_source_code(p_source);
	REQUIRE(p_script->reload(true) == OK);
}

static Object *instantiate(const Ref<GDScript> &p_script, const StringName &p_native) {
	Object *object = ClassDB::instantiate(p_native);
	REQUIRE(object != nullptr);
	object->set_script(p_script);
	REQUIRE(object->get_script_instance() != nullptr);
	return object;
}

static Ref<RefCounted> instantiate(const Ref<GDScript> &p_script) {
	Ref<RefCounted> object;
	object.instantiate(); // Referenced first, or the script's first use of `self` frees it.
	object->set_script(p_script);
	REQUIRE(object->get_script_instance() != nullptr);
	return object;
}

// What a function of `p_object` returns, which is where a member of the wrong type stops the typed
// opcodes.
static Variant call(Object *p_object, const StringName &p_method) {
	Callable::CallError error;
	return p_object->callp(p_method, nullptr, 0, error);
}

TEST_SUITE("[Modules][GDScript][HotReload]") {
	TEST_CASE("[HotReload] A new typed member gets its default, not null") {
		Ref<GDScript> script = compile("extends RefCounted\nvar kept := 1\n");
		Ref<RefCounted> object = instantiate(script);
		object->set("kept", 7);

		recompile(script, R"(extends RefCounted
var kept := 1
@export var depth := 1.5
@export var plain: float
var names: Array[String]
func doubled() -> float:
	return depth * 2.0
func negated() -> float:
	return -depth
)");

		CHECK(object->get("depth").get_type() == Variant::FLOAT);
		CHECK(float(object->get("depth")) == 1.5f);
		CHECK(object->get("plain").get_type() == Variant::FLOAT);
		CHECK(object->get("names").get_type() == Variant::ARRAY);
		CHECK(Array(object->get("names")).is_typed());
		// The two opcodes that failed: a typed binary and a typed unary one.
		CHECK(call(object.ptr(), "doubled").get_type() == Variant::FLOAT);
		CHECK(float(call(object.ptr(), "doubled")) == 3.0f);
		CHECK(float(call(object.ptr(), "negated")) == -1.5f);
		// And the old member kept what it was set to.
		CHECK(int(object->get("kept")) == 7);
	}

	TEST_CASE("[HotReload] A new member's default is computed") {
		Ref<GDScript> script = compile("extends RefCounted\nvar base_value := 2\n");
		Ref<RefCounted> object = instantiate(script);
		object->set("base_value", 5);

		recompile(script, R"(extends RefCounted
var base_value := 2
@export var curve := make_curve()
var from_kept := base_value * 10
func make_curve() -> Resource:
	return Resource.new()
)");

		CHECK(Object::cast_to<Resource>(object->get("curve")) != nullptr);
		// Other initializers see the member as it is, edited, not its default.
		CHECK(int(object->get("from_kept")) == 50);
	}

	TEST_CASE("[HotReload] Only the new members' initializers run") {
		Ref<GDScript> script = compile(R"(extends RefCounted
var counted := count()
func count() -> int:
	set_meta("runs", get_meta("runs", 0) + 1)
	return 1
)");
		Ref<RefCounted> object = instantiate(script);
		REQUIRE(int(object->get_meta("runs")) == 1);

		recompile(script, R"(extends RefCounted
var counted := count()
var added := 2
func count() -> int:
	set_meta("runs", get_meta("runs", 0) + 1)
	return 1
)");

		CHECK(int(object->get("added")) == 2);
		CHECK(int(object->get_meta("runs")) == 1);

		// Reloading without new members runs nothing either.
		recompile(script, R"(extends RefCounted
var counted := count()
var added := 2
func count() -> int:
	set_meta("runs", get_meta("runs", 0) + 1)
	return 2
)");
		CHECK(int(object->get_meta("runs")) == 1);
	}

	TEST_CASE("[HotReload] A retyped member") {
		Ref<GDScript> script = compile(R"(extends RefCounted
var number: int = 1
var loose = 1.0
var other = "text"
)");
		Ref<RefCounted> object = instantiate(script);
		object->set("number", 5);
		object->set("loose", 4.0);

		recompile(script, R"(extends RefCounted
var number: float = 2.5
var loose: float = 8.0
var other: float = 3.0
func sum() -> float:
	return number + loose + other
)");

		// The value no longer fits, so it starts over.
		CHECK(object->get("number").get_type() == Variant::FLOAT);
		CHECK(float(object->get("number")) == 2.5f);
		CHECK(float(object->get("other")) == 3.0f);
		// The value already fits the new type.
		CHECK(float(object->get("loose")) == 4.0f);
		CHECK(float(call(object.ptr(), "sum")) == 9.5f);
	}

	TEST_CASE("[HotReload] Members move and go") {
		Ref<GDScript> script = compile("extends RefCounted\nvar a := 1\nvar b := 2\nvar c := 3\n");
		Ref<RefCounted> object = instantiate(script);
		object->set("a", 10);
		object->set("c", 30);

		recompile(script, R"(extends RefCounted
@export_group("Moved")
var fresh := 4.5
var c := 3
var a := 1
)");

		CHECK(int(object->get("a")) == 10);
		CHECK(int(object->get("c")) == 30);
		CHECK(float(object->get("fresh")) == 4.5f);
		bool valid = true;
		object->get("b", &valid);
		CHECK_FALSE(valid);
	}

	TEST_CASE("[HotReload] Members of inner classes, and those inherited") {
		Ref<GDScript> script = compile(R"(extends RefCounted
class Base:
	var kept := 1
class Derived extends Base:
	pass
)");
		Ref<GDScript> derived = script->get_subclasses()["Derived"];
		REQUIRE(derived.is_valid());
		Ref<RefCounted> object = instantiate(derived);
		object->set("kept", 6);

		recompile(script, R"(extends RefCounted
class Base:
	var kept := 1
	var from_base := 1.5
	func read_base() -> float:
		return from_base * 2.0
class Derived extends Base:
	var from_derived := 2.5
)");

		CHECK(int(object->get("kept")) == 6);
		CHECK(float(object->get("from_base")) == 1.5f);
		CHECK(float(object->get("from_derived")) == 2.5f);
		CHECK(float(call(object.ptr(), "read_base")) == 3.0f);
	}

	TEST_CASE("[HotReload] Scripts inheriting across files, through reload_scripts()") {
		GDScriptLanguage::get_singleton()->init();
		const String dir = TestUtils::get_temp_path("gdscript_hot_reload");
		Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		da->make_dir_recursive(dir);
		const String base_path = dir.path_join("base.gd");
		const String derived_path = dir.path_join("derived.gd");

		const auto write = [](const String &p_path, const String &p_text) {
			Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
			REQUIRE(f.is_valid());
			f->store_string(p_text);
		};
		write(base_path, "extends RefCounted\nvar kept := 1\n");
		write(derived_path, vformat("extends \"%s\"\nvar own := 2\n", base_path));

		Ref<GDScript> base = ResourceLoader::load(base_path);
		Ref<GDScript> derived = ResourceLoader::load(derived_path);
		REQUIRE(base.is_valid());
		REQUIRE(derived.is_valid());
		Ref<RefCounted> object = instantiate(derived);
		object->set("kept", 6);
		object->set("own", 8);

		write(base_path, "extends RefCounted\nvar kept := 1\nvar added := make()\nfunc make() -> RefCounted:\n\treturn RefCounted.new()\nfunc read() -> Object:\n\treturn added\n");
		GDScriptLanguage::get_singleton()->reload_scripts(Array{ base });

		CHECK(int(object->get("kept")) == 6);
		CHECK(int(object->get("own")) == 8);
		CHECK(Object::cast_to<RefCounted>(object->get("added")) != nullptr);
		CHECK(Object::cast_to<RefCounted>(call(object.ptr(), "read")) != nullptr);

		// The base did not change this time, so its members are left alone.
		Ref<RefCounted> added = object->get("added");
		write(derived_path, vformat("extends \"%s\"\nvar own := 2\nvar second := 2.5\n", base_path));
		GDScriptLanguage::get_singleton()->reload_scripts(Array{ derived });
		CHECK(float(object->get("second")) == 2.5f);
		CHECK(Ref<RefCounted>(object->get("added")) == added);

		da->remove(base_path);
		da->remove(derived_path);
	}

	TEST_CASE("[HotReload] A lambda in a new member survives the next reload") {
		Ref<GDScript> script = compile("extends RefCounted\n");
		Ref<RefCounted> object = instantiate(script);

		const String source = "extends RefCounted\nvar f := func(): return 3\nfunc run() -> int:\n\treturn f.call()\n";
		recompile(script, source);
		CHECK(int(call(object.ptr(), "run")) == 3);

		recompile(script, source);
		CHECK(int(call(object.ptr(), "run")) == 3);
	}

	TEST_CASE("[SceneTree][HotReload] A new @onready member") {
		Ref<GDScript> script = compile("extends Node\n");
		Node *ready_node = Object::cast_to<Node>(instantiate(script, "Node"));
		Node *waiting_node = Object::cast_to<Node>(instantiate(script, "Node"));
		SceneTree::get_singleton()->get_root()->add_child(ready_node);
		REQUIRE(ready_node->is_ready());

		recompile(script, R"(extends Node
@onready var parent_name: String = get_parent().name
@onready var scale_value := 2.5
var runs := 0
func _ready() -> void:
	runs += 1
)");

		// `_ready()` has passed for this one, so nothing else would set them.
		CHECK(String(ready_node->get("parent_name")) == String(SceneTree::get_singleton()->get_root()->get_name()));
		CHECK(float(ready_node->get("scale_value")) == 2.5f);
		CHECK(int(ready_node->get("runs")) == 0);

		// This one gets them when it is ready, like any other node.
		CHECK(waiting_node->get("scale_value").get_type() == Variant::FLOAT);
		CHECK(float(waiting_node->get("scale_value")) == 0.0f);
		SceneTree::get_singleton()->get_root()->add_child(waiting_node);
		CHECK(float(waiting_node->get("scale_value")) == 2.5f);
		CHECK(int(waiting_node->get("runs")) == 1);

		memdelete(ready_node);
		memdelete(waiting_node);
	}

	TEST_CASE("[SceneTree][HotReload] A new member that makes a node") {
		Ref<GDScript> script = compile("extends Node\nvar existing := Node.new()\n");
		Node *node = Object::cast_to<Node>(instantiate(script, "Node"));
		Node *existing = Object::cast_to<Node>(node->get("existing"));
		REQUIRE(existing != nullptr);

		recompile(script, "extends Node\nvar existing := Node.new()\nvar added := Node.new()\n");

		// The existing node is still the one, with no second one made and thrown away.
		CHECK(Object::cast_to<Node>(node->get("existing")) == existing);
		Node *added = Object::cast_to<Node>(node->get("added"));
		CHECK(added != nullptr);

		memdelete(added);
		memdelete(existing);
		memdelete(node);
	}
}

} // namespace TestGDScriptHotReload
