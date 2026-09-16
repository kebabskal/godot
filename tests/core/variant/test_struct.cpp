/**************************************************************************/
/*  test_struct.cpp                                                       */
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


#include "tests/test_macros.h"

TEST_FORCE_LINK(test_struct)

#include "core/variant/struct.h"
#include "core/variant/struct_layout.h"
#include "core/variant/variant.h"

namespace TestStruct {

static Ref<StructLayout> make_point_layout() {
	Ref<StructLayout> layout;
	layout.instantiate();
	layout->set_name("Point");
	CHECK(layout->add_field("x", Variant::FLOAT, 0.0) == 0);
	CHECK(layout->add_field("y", Variant::FLOAT, 0.0) == 1);
	CHECK(layout->add_field("label", Variant::STRING, "origin") == 2);
	return layout;
}

TEST_CASE("[Struct] Layout") {
	Ref<StructLayout> layout = make_point_layout();
	CHECK(layout->get_name() == StringName("Point"));
	CHECK(layout->get_field_count() == 3);
	CHECK(layout->get_field_name(1) == StringName("y"));
	CHECK(layout->get_field_type(2) == Variant::STRING);
	CHECK(layout->find_field("label") == 2);
	CHECK(layout->find_field("nope") == -1);
	CHECK(layout->has_field("x"));
	// Duplicate names are refused.
	ERR_PRINT_OFF;
	CHECK(layout->add_field("x", Variant::INT) == -1);
	ERR_PRINT_ON;
	// A default that does not fit the type is refused.
	ERR_PRINT_OFF;
	CHECK(layout->add_field("bad", Variant::INT, "text") == -1);
	ERR_PRINT_ON;
	// A missing default becomes the type's zero value.
	CHECK(layout->add_field("count", Variant::INT) == 3);
	CHECK(layout->get_field_default_value(3) == Variant(0));
}

TEST_CASE("[Struct] Null struct") {
	Struct s;
	CHECK(s.is_null());
	CHECK(s.get_field_count() == 0);
	CHECK(s.get_layout().is_null());
	CHECK(s.get_struct_name() == StringName());
	CHECK(s.find_field("x") == -1);
	CHECK(s.to_string() == "Struct()");
	CHECK(s == Struct());
	CHECK(s.to_dictionary().is_empty());
}

TEST_CASE("[Struct] Construction and field access") {
	Ref<StructLayout> layout = make_point_layout();

	Struct defaults = layout->instantiate();
	CHECK_FALSE(defaults.is_null());
	CHECK(defaults.get_struct_name() == StringName("Point"));
	CHECK(defaults.get_field_count() == 3);
	CHECK(defaults.get_field(0) == Variant(0.0));
	CHECK(defaults.get_field_by_name("label") == Variant("origin"));

	const Variant x = 1.5;
	const Variant y = 2; // An int converts to the float field.
	const Variant *args[2] = { &x, &y };
	Callable::CallError err;
	Struct p = layout->instantiate(args, 2, err);
	CHECK(err.error == Callable::CallError::CALL_OK);
	CHECK(p.get_field(0) == Variant(1.5));
	CHECK(p.get_field(1).get_type() == Variant::FLOAT);
	CHECK(p.get_field(1) == Variant(2.0));
	CHECK(p.get_field(2) == Variant("origin"));
	CHECK(p.to_string() == "Point(x: 1.5, y: 2.0, label: origin)");

	// Writes validate against the field type.
	CHECK(p.set_field_by_name("label", "moved"));
	CHECK(p.get_field(2) == Variant("moved"));
	ERR_PRINT_OFF;
	CHECK_FALSE(p.set_field(0, "not a number"));
	CHECK_FALSE(p.set_field_by_name("nope", 1));
	ERR_PRINT_ON;
	CHECK(p.get_field(0) == Variant(1.5));

	// Too many arguments, and an argument of the wrong type.
	const Variant z = 3.0;
	const Variant *too_many[4] = { &x, &y, &z, &z };
	layout->instantiate(too_many, 4, err);
	CHECK(err.error == Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS);
	const Variant text = "text";
	const Variant *wrong[1] = { &text };
	ERR_PRINT_OFF;
	layout->instantiate(wrong, 1, err);
	ERR_PRINT_ON;
	CHECK(err.error == Callable::CallError::CALL_ERROR_INVALID_ARGUMENT);
	CHECK(err.argument == 0);

	Dictionary d = p.to_dictionary();
	CHECK(d.size() == 3);
	CHECK(d["x"] == Variant(1.5));
}

TEST_CASE("[Struct] Value semantics and copy-on-write") {
	Ref<StructLayout> layout = make_point_layout();
	Struct a = layout->instantiate();
	CHECK(a.set_field(0, 1.0));

	Struct b = a; // Shares storage.
	CHECK(a.id() == b.id());
	CHECK(a == b);

	CHECK(b.set_field(0, 2.0)); // Clones before writing.
	CHECK(a.id() != b.id());
	CHECK(a.get_field(0) == Variant(1.0));
	CHECK(b.get_field(0) == Variant(2.0));
	CHECK(a != b);

	// Writable field storage also clones shared storage.
	Struct c = a;
	Variant *fields = c.get_fields_ptrw();
	REQUIRE(fields != nullptr);
	fields[1] = 5.0;
	CHECK(a.get_field(1) == Variant(0.0));
	CHECK(c.get_field(1) == Variant(5.0));

	// Assignment shares again.
	c = b;
	CHECK(c.id() == b.id());
	CHECK(c == b);

	// Duplicate never shares.
	Struct dup = b.duplicate();
	CHECK(dup.id() != b.id());
	CHECK(dup == b);
}

TEST_CASE("[Struct] Equality and hashing") {
	Ref<StructLayout> layout = make_point_layout();
	Struct a = layout->instantiate();
	Struct b = layout->instantiate();
	CHECK(a == b);
	CHECK(a.hash() == b.hash());
	CHECK(b.set_field(0, 1.0));
	CHECK(a != b);
	CHECK(a.hash() != b.hash());

	// Same fields, different layout: not equal.
	Ref<StructLayout> other = make_point_layout();
	Struct c = other->instantiate();
	CHECK(a != c);
	CHECK(a.get_layout() != c.get_layout());

	// Null structs are equal to each other only.
	CHECK(Struct() == Struct());
	CHECK(Struct() != a);
}

TEST_CASE("[Struct] Variant integration") {
	Ref<StructLayout> layout = make_point_layout();
	Struct p = layout->instantiate();
	CHECK(p.set_field(0, 1.0));

	Variant v = p;
	CHECK(v.get_type() == Variant::STRUCT);
	CHECK(Variant::get_type_name(Variant::STRUCT) == "Struct");
	CHECK(v.booleanize());
	CHECK(String(v) == "Point(x: 1.0, y: 0.0, label: origin)");

	// Round trip and value semantics through Variant.
	Struct back = v;
	CHECK(back == p);
	Variant w = v;
	CHECK(w == v);
	CHECK(w.hash() == v.hash());
	CHECK(w.identity_compare(v)); // Still sharing storage.
	bool valid = false;
	w.set_named("x", 3.0, valid);
	CHECK(valid);
	CHECK_FALSE(w.identity_compare(v));
	CHECK(w.get_named("x", valid) == Variant(3.0));
	CHECK(valid);
	CHECK(v.get_named("x", valid) == Variant(1.0));
	CHECK(w != v);
	w.get_named("nope", valid);
	CHECK_FALSE(valid);
	w.set_named("nope", 1, valid);
	CHECK_FALSE(valid);

	// Comparison with other types and null.
	CHECK_FALSE(v == Variant());
	CHECK(v != Variant());
	CHECK_FALSE(v == Variant(1));
	CHECK_FALSE(Variant::can_convert(Variant::STRUCT, Variant::INT));
	CHECK(Variant::can_convert(Variant::STRUCT, Variant::STRUCT));
	CHECK_FALSE(Variant::is_type_shared(Variant::STRUCT));

	// Duplicate through Variant, and the default-constructed Variant of the type is a null struct.
	Variant dup = v.duplicate();
	CHECK(dup == v);
	CHECK_FALSE(dup.identity_compare(v));
	Callable::CallError err;
	Variant empty;
	Variant::construct(Variant::STRUCT, empty, nullptr, 0, err);
	CHECK(err.error == Callable::CallError::CALL_OK);
	CHECK(empty.get_type() == Variant::STRUCT);
	CHECK(empty.operator Struct().is_null());
	CHECK(empty.is_zero());
}

} // namespace TestStruct
