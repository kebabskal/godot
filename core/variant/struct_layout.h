/**************************************************************************/
/*  struct_layout.h                                                       */
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

#include "core/object/ref_counted.h"
#include "core/object/script_language.h"
#include "core/variant/container_type_validate.h"
#include "core/variant/struct.h"
#include "core/variant/variant.h"

// The type of a `Struct`: its name and its ordered, typed fields with their defaults.
//
// Layouts are shared: engine structs are registered once and script structs are created by the
// script that declares them. Values keep their layout alive. Two layouts are the same type only
// if they are the same object, so a value made from a stale layout (after a script reload) is a
// type mismatch rather than a crash.
class StructLayout : public RefCounted {
	GDCLASS(StructLayout, RefCounted);

public:
	struct Field {
		StringName name;
		ContainerTypeValidate type; // `variant_type == NIL` accepts anything.
		Variant default_value;
	};

private:
	StringName name;
	String source_path; // Script path for script-declared layouts; empty for engine layouts.
	Vector<Field> fields;
	HashMap<StringName, int> field_index;

protected:
	static void _bind_methods();

public:
	void set_name(const StringName &p_name);
	StringName get_name() const;

	void set_source_path(const String &p_path);
	String get_source_path() const;

	// Appends a field. Returns its index, or -1 if the name is taken. `p_default_value` must be
	// acceptable for the field type (it is validated and converted like any assignment).
	int add_field(const StringName &p_name, Variant::Type p_type, const Variant &p_default_value = Variant(), const StringName &p_class_name = StringName(), const Ref<Script> &p_script = Ref<Script>());

	int get_field_count() const;
	const Field &get_field(int p_index) const;
	StringName get_field_name(int p_index) const;
	Variant::Type get_field_type(int p_index) const;
	Variant get_field_default_value(int p_index) const;
	// Field index for a name, or -1.
	int find_field(const StringName &p_name) const;
	bool has_field(const StringName &p_name) const;

	// Converts `r_value` to the field's type where that is allowed (like a typed assignment) and
	// returns false when it cannot be accepted.
	bool validate_field_value(int p_index, Variant &r_value) const;

	// A value with every field at its default.
	Struct instantiate() const;
	// A value with the leading fields set from positional arguments and the rest at their defaults.
	Struct instantiate(const Variant **p_args, int p_argcount, Callable::CallError &r_error) const;

	// Registry of layouts by name. Engine layouts register at startup; script layouts register under a
	// qualified name when their script compiles. Serialized values are resolved through it.
	static void register_layout(const Ref<StructLayout> &p_layout);
	static void unregister_layout(const StringName &p_name);
	static Ref<StructLayout> find_layout(const StringName &p_name);
	// Drops the registry. Called at shutdown.
	static void cleanup();

	StructLayout() {}
};
