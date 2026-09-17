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

#include "core/io/json.h"
#include "core/io/marshalls.h"
#include "core/math/triangle_mesh.h"
#include "core/os/time.h"
#include "core/variant/struct.h"
#include "core/variant/struct_db.h"
#include "core/variant/struct_layout.h"
#include "core/variant/variant.h"
#include "core/variant/variant_parser.h"

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

TEST_CASE("[Struct] Registry") {
	Ref<StructLayout> layout = make_point_layout();
	CHECK(StructLayout::find_layout("Point").is_null());
	StructLayout::register_layout(layout);
	CHECK(StructLayout::find_layout("Point") == layout);
	StructLayout::unregister_layout("Point");
	CHECK(StructLayout::find_layout("Point").is_null());
	ERR_PRINT_OFF;
	StructLayout::register_layout(Ref<StructLayout>()); // Refused, no crash.
	ERR_PRINT_ON;
}

TEST_CASE("[Struct] StructDB") {
	CHECK_FALSE(StructDB::has_layout("TestEngineStruct"));
	Ref<StructLayout> layout = StructDB::add_layout("TestEngineStruct",
			{
					{ "hit", Variant::BOOL, false },
					{ "position", Variant::VECTOR3, Vector3(1, 2, 3) },
					{ "collider", Variant::OBJECT, Variant(), "Object" },
			});
	REQUIRE(layout.is_valid());
	CHECK(layout->get_source_path().is_empty());
	CHECK(layout->get_field_count() == 3);
	CHECK(StructDB::get_layout("TestEngineStruct") == layout);
	CHECK(StructLayout::find_layout("TestEngineStruct") == layout);

	List<StringName> names;
	StructDB::get_layout_list(&names);
	CHECK(names.find("TestEngineStruct") != nullptr);

	Struct value = layout->instantiate();
	CHECK(value.get_field_by_name("position") == Variant(Vector3(1, 2, 3)));
	CHECK(value.get_field_by_name("collider") == Variant());

	ERR_PRINT_OFF;
	CHECK(StructDB::add_layout("TestEngineStruct", {}).is_null()); // Name taken.
	CHECK(StructDB::add_layout("TestBadStruct", { { "x", Variant::INT, "not an int" } }).is_null()); // Bad default.
	ERR_PRINT_ON;
	CHECK_FALSE(StructDB::has_layout("TestBadStruct"));

	StructDB::remove_layout("TestEngineStruct");
	CHECK_FALSE(StructDB::has_layout("TestEngineStruct"));

	// The physics servers register their result layouts with the server types.
	Ref<StructLayout> ray = StructDB::get_layout("PhysicsRayResult3D");
	REQUIRE(ray.is_valid());
	CHECK(ray->get_field_name(0) == "hit");
	CHECK(ray->get_field_type(0) == Variant::BOOL);
	CHECK(ray->find_field("face_index") >= 0);
	CHECK(StructDB::get_layout("PhysicsRayResult2D").is_valid());
	for (const char *name : { "PhysicsShapeResult2D", "PhysicsShapeResult3D", "PhysicsRestInfo2D", "PhysicsRestInfo3D", "PhysicsCastResult2D", "PhysicsCastResult3D" }) {
		CHECK_MESSAGE(StructDB::has_layout(name), name);
	}
	CHECK(StructDB::get_layout("PhysicsRestInfo3D")->get_field_default_value(0) == Variant(false)); // `hit`.
	CHECK(StructDB::get_layout("PhysicsCastResult3D")->get_field_default_value(0) == Variant(1.0)); // `safe_fraction`.
	for (const char *name : { "DateTime", "Date", "TimeOfDay", "TimeZoneInfo", "TriangleMeshHit", "MemoryInfo", "VersionInfo" }) {
		CHECK_MESSAGE(StructDB::has_layout(name), name);
	}
}

TEST_CASE("[Struct] Time layouts") {
	Time *time = Time::get_singleton();
	REQUIRE(time != nullptr);

	const Struct epoch = time->get_datetime_from_unix_time(0);
	CHECK(epoch.get_struct_name() == "DateTime");
	CHECK(epoch.get_field_by_name("year") == Variant(1970));
	CHECK(epoch.get_field_by_name("weekday") == Variant(4)); // Thursday.
	CHECK(epoch.get_field_by_name("dst") == Variant(false));
	CHECK(time->get_unix_time_from_datetime(time->get_datetime_from_unix_time(946684800)) == 946684800);
	CHECK(time->get_datetime_string_from_datetime(time->get_datetime_from_datetime_string("2001-02-03T04:05:06"), true) == "2001-02-03 04:05:06");
	CHECK(time->get_datetime_from_datetime_string("2001-02-03T04:05:06").get_field_by_name("weekday") == Variant(6)); // Saturday.
	CHECK(time->get_date_from_unix_time(86400).get_field_by_name("day") == Variant(2));
	CHECK(time->get_time_from_unix_time(3661).get_field_by_name("minute") == Variant(1));
	// A user struct with the same field names is accepted by name; missing fields take the epoch defaults.
	Ref<StructLayout> partial;
	partial.instantiate();
	partial->add_field("year", Variant::INT, 2000);
	partial->add_field("month", Variant::INT, 1);
	partial->add_field("day", Variant::INT, 2);
	CHECK(time->get_unix_time_from_datetime(TypedStruct<DateTimeName>(partial->instantiate())) == 946684800 + 86400);
	ERR_PRINT_OFF;
	CHECK(time->get_unix_time_from_datetime(Struct()) == 0); // No layout.
	ERR_PRINT_ON;
	CHECK(time->get_time_zone_info_from_system().get_struct_name() == "TimeZoneInfo");
}

TEST_CASE("[Struct] TriangleMesh hits") {
	Ref<TriangleMesh> mesh;
	mesh.instantiate();
	Vector<Vector3> faces;
	faces.push_back(Vector3(-1, 0, -1));
	faces.push_back(Vector3(1, 0, -1));
	faces.push_back(Vector3(0, 0, 1));
	REQUIRE(mesh->create_from_faces(faces));

	const Struct hit = mesh->intersect_ray_struct(Vector3(0, 1, 0), Vector3(0, -1, 0));
	CHECK(hit.get_struct_name() == "TriangleMeshHit");
	CHECK(hit.get_field_by_name("hit") == Variant(true));
	CHECK(hit.get_field_by_name("face_index") == Variant(0));
	CHECK(Vector3(hit.get_field_by_name("position")).is_equal_approx(Vector3(0, 0, 0)));

	const Struct miss = mesh->intersect_segment_struct(Vector3(0, 1, 0), Vector3(0, 0.5, 0));
	CHECK(miss.get_field_by_name("hit") == Variant(false));
	CHECK(miss.get_field_by_name("face_index") == Variant(-1));
}

// Serialization resolves layouts by name, so these register the layout for the duration of the test.
struct RegisteredPoint {
	Ref<StructLayout> layout = make_point_layout();
	RegisteredPoint() { StructLayout::register_layout(layout); }
	~RegisteredPoint() { StructLayout::unregister_layout("Point"); }
};

TEST_CASE("[Struct] Text serialization") {
	RegisteredPoint registered;
	Struct p = registered.layout->instantiate();
	CHECK(p.set_field(0, 1.5));
	CHECK(p.set_field(2, "moved"));

	String text;
	CHECK(VariantWriter::write_to_string(p, text) == OK);
	CHECK(text.begins_with("Struct(\"Point\", {"));
	CHECK(text.ends_with("})"));
	CHECK(text.contains("\"x\": 1.5"));
	CHECK(text.contains("\"label\": \"moved\""));

	VariantParser::StreamString stream;
	stream.s = text;
	Variant parsed;
	String err_str;
	int err_line = 0;
	CHECK(VariantParser::parse(&stream, parsed, err_str, err_line) == OK);
	CHECK(parsed.get_type() == Variant::STRUCT);
	CHECK(parsed.operator Struct() == p);

	// Missing fields keep their defaults; an unknown type or field is an error; the null struct round-trips.
	stream = VariantParser::StreamString();
	stream.s = "Struct(\"Point\", {\"y\": 2.0})";
	CHECK(VariantParser::parse(&stream, parsed, err_str, err_line) == OK);
	CHECK(parsed.operator Struct().get_field(1) == Variant(2.0));
	CHECK(parsed.operator Struct().get_field(2) == Variant("origin"));
	stream = VariantParser::StreamString();
	stream.s = "Struct(\"Nope\", {})";
	CHECK(VariantParser::parse(&stream, parsed, err_str, err_line) != OK);
	stream = VariantParser::StreamString();
	stream.s = "Struct(\"Point\", {\"nope\": 1})";
	CHECK(VariantParser::parse(&stream, parsed, err_str, err_line) != OK);
	stream = VariantParser::StreamString();
	stream.s = "Struct()";
	CHECK(VariantParser::parse(&stream, parsed, err_str, err_line) == OK);
	CHECK(parsed.get_type() == Variant::STRUCT);
	CHECK(parsed.operator Struct().is_null());
	String null_text;
	CHECK(VariantWriter::write_to_string(Struct(), null_text) == OK);
	CHECK(null_text == "Struct()");
}

TEST_CASE("[Struct] Binary serialization") {
	RegisteredPoint registered;
	Struct p = registered.layout->instantiate();
	CHECK(p.set_field(1, 4.25));
	CHECK(p.set_field(2, "binary"));
	Variant v = p;

	int len = 0;
	CHECK(encode_variant(v, nullptr, len) == OK);
	Vector<uint8_t> buffer;
	buffer.resize(len);
	CHECK(encode_variant(v, buffer.ptrw(), len) == OK);

	Variant decoded;
	int used = 0;
	CHECK(decode_variant(decoded, buffer.ptr(), buffer.size(), &used) == OK);
	CHECK(used == len);
	CHECK(decoded.get_type() == Variant::STRUCT);
	CHECK(decoded == v);
}

TEST_CASE("[Struct] JSON serialization") {
	RegisteredPoint registered;
	Struct p = registered.layout->instantiate();
	CHECK(p.set_field(0, 7.0));
	Variant json = JSON::from_native(p);
	CHECK(json.get_type() == Variant::DICTIONARY);
	CHECK(Dictionary(json)["type"] == Variant("Struct"));
	CHECK(Dictionary(json)["name"] == Variant("Point"));
	Variant back = JSON::to_native(json);
	CHECK(back.get_type() == Variant::STRUCT);
	CHECK(back == Variant(p));
}

} // namespace TestStruct
