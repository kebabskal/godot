/**************************************************************************/
/*  physics_direct_space_state_3d.cpp                                     */
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

#include "physics_direct_space_state_3d.h"

#include "core/object/class_db.h"
#include "core/variant/struct_db.h"
#include "core/variant/typed_array.h"

// `PhysicsRayResult3D`: field order, matching `register_struct_layouts()`.
enum {
	RAY_RESULT_HIT,
	RAY_RESULT_POSITION,
	RAY_RESULT_NORMAL,
	RAY_RESULT_FACE_INDEX,
	RAY_RESULT_COLLIDER_ID,
	RAY_RESULT_COLLIDER,
	RAY_RESULT_SHAPE,
	RAY_RESULT_RID,
};

static Ref<StructLayout> ray_result_layout;

// `PhysicsShapeResult3D`.
enum {
	SHAPE_RESULT_RID,
	SHAPE_RESULT_COLLIDER_ID,
	SHAPE_RESULT_COLLIDER,
	SHAPE_RESULT_SHAPE,
};

// `PhysicsRestInfo3D`.
enum {
	REST_INFO_HIT,
	REST_INFO_POINT,
	REST_INFO_NORMAL,
	REST_INFO_RID,
	REST_INFO_COLLIDER_ID,
	REST_INFO_COLLIDER,
	REST_INFO_SHAPE,
	REST_INFO_LINEAR_VELOCITY,
};

// `PhysicsCastResult3D`.
enum {
	CAST_RESULT_SAFE_FRACTION,
	CAST_RESULT_UNSAFE_FRACTION,
};

static Ref<StructLayout> shape_result_layout;
static Ref<StructLayout> rest_info_layout;
static Ref<StructLayout> cast_result_layout;

static Struct _make_shape_result(const PS3DT::ShapeResult &p_result) {
	Struct s = shape_result_layout->instantiate();
	Variant *fields = s.get_fields_ptrw();
	fields[SHAPE_RESULT_RID] = p_result.rid;
	fields[SHAPE_RESULT_COLLIDER_ID] = p_result.collider_id;
	fields[SHAPE_RESULT_COLLIDER] = p_result.get_collider();
	fields[SHAPE_RESULT_SHAPE] = p_result.shape;
	return s;
}

void PhysicsDirectSpaceState3D::register_struct_layouts() {
	ray_result_layout = StructDB::add_layout(PhysicsRayResult3DName,
			{
					{ "hit", Variant::BOOL, false },
					{ "position", Variant::VECTOR3, Vector3() },
					{ "normal", Variant::VECTOR3, Vector3() },
					{ "face_index", Variant::INT, -1 },
					{ "collider_id", Variant::INT, 0 },
					{ "collider", Variant::OBJECT, Variant(), "Object" },
					{ "shape", Variant::INT, 0 },
					{ "rid", Variant::RID, RID() },
			});
	shape_result_layout = StructDB::add_layout(PhysicsShapeResult3DName,
			{
					{ "rid", Variant::RID, RID() },
					{ "collider_id", Variant::INT, 0 },
					{ "collider", Variant::OBJECT, Variant(), "Object" },
					{ "shape", Variant::INT, 0 },
			});
	rest_info_layout = StructDB::add_layout(PhysicsRestInfo3DName,
			{
					{ "hit", Variant::BOOL, false },
					{ "point", Variant::VECTOR3, Vector3() },
					{ "normal", Variant::VECTOR3, Vector3() },
					{ "rid", Variant::RID, RID() },
					{ "collider_id", Variant::INT, 0 },
					{ "collider", Variant::OBJECT, Variant(), "Object" },
					{ "shape", Variant::INT, 0 },
					{ "linear_velocity", Variant::VECTOR3, Vector3() },
			});
	cast_result_layout = StructDB::add_layout(PhysicsCastResult3DName,
			{
					{ "safe_fraction", Variant::FLOAT, 1.0 },
					{ "unsafe_fraction", Variant::FLOAT, 1.0 },
			});
}

void PhysicsDirectSpaceState3D::unregister_struct_layouts() {
	StructDB::remove_layout(PhysicsRayResult3DName);
	StructDB::remove_layout(PhysicsShapeResult3DName);
	StructDB::remove_layout(PhysicsRestInfo3DName);
	StructDB::remove_layout(PhysicsCastResult3DName);
	ray_result_layout.unref();
	shape_result_layout.unref();
	rest_info_layout.unref();
	cast_result_layout.unref();
}

Dictionary PhysicsDirectSpaceState3D::_intersect_ray(RequiredParam<PhysicsRayQueryParameters3D> p_ray_query) {
	EXTRACT_PARAM_OR_FAIL_V(ray_query, p_ray_query, Dictionary());

	PS3DT::RayResult result;
	bool res = intersect_ray(ray_query->get_parameters(), result);

	if (!res) {
		return Dictionary();
	}

	Dictionary d;
	d["position"] = result.position;
	d["normal"] = result.normal;
	d["face_index"] = result.face_index;
	d["collider_id"] = result.collider_id;
	d["collider"] = result.get_collider();
	d["shape"] = result.shape;
	d["rid"] = result.rid;

	return d;
}

TypedStruct<PhysicsRayResult3DName> PhysicsDirectSpaceState3D::_intersect_ray_struct(RequiredParam<PhysicsRayQueryParameters3D> p_ray_query) {
	ERR_FAIL_COND_V(ray_result_layout.is_null(), Struct());
	Struct s = ray_result_layout->instantiate();
	EXTRACT_PARAM_OR_FAIL_V(ray_query, p_ray_query, s);

	PS3DT::RayResult result;
	if (!intersect_ray(ray_query->get_parameters(), result)) {
		return s; // `hit` stays false.
	}

	// Written directly: the values match the field types by construction.
	Variant *fields = s.get_fields_ptrw();
	fields[RAY_RESULT_HIT] = true;
	fields[RAY_RESULT_POSITION] = result.position;
	fields[RAY_RESULT_NORMAL] = result.normal;
	fields[RAY_RESULT_FACE_INDEX] = result.face_index;
	fields[RAY_RESULT_COLLIDER_ID] = result.collider_id;
	fields[RAY_RESULT_COLLIDER] = result.get_collider();
	fields[RAY_RESULT_SHAPE] = result.shape;
	fields[RAY_RESULT_RID] = result.rid;
	return s;
}

TypedArray<Dictionary> PhysicsDirectSpaceState3D::_intersect_point(RequiredParam<PhysicsPointQueryParameters3D> p_point_query, int p_max_results) {
	EXTRACT_PARAM_OR_FAIL_V(point_query, p_point_query, TypedArray<Dictionary>());

	Vector<PS3DT::ShapeResult> ret;
	ret.resize(p_max_results);

	int rc = intersect_point(point_query->get_parameters(), ret.ptrw(), ret.size());

	if (rc == 0) {
		return TypedArray<Dictionary>();
	}

	TypedArray<Dictionary> r;
	r.resize(rc);
	for (int i = 0; i < rc; i++) {
		Dictionary d;
		d["rid"] = ret[i].rid;
		d["collider_id"] = ret[i].collider_id;
		d["collider"] = ret[i].get_collider();
		d["shape"] = ret[i].shape;
		r[i] = d;
	}
	return r;
}

TypedArray<Dictionary> PhysicsDirectSpaceState3D::_intersect_shape(RequiredParam<PhysicsShapeQueryParameters3D> p_shape_query, int p_max_results) {
	EXTRACT_PARAM_OR_FAIL_V(shape_query, p_shape_query, TypedArray<Dictionary>());

	Vector<PS3DT::ShapeResult> sr;
	sr.resize(p_max_results);
	int rc = intersect_shape(shape_query->get_parameters(), sr.ptrw(), sr.size());
	TypedArray<Dictionary> ret;
	ret.resize(rc);
	for (int i = 0; i < rc; i++) {
		Dictionary d;
		d["rid"] = sr[i].rid;
		d["collider_id"] = sr[i].collider_id;
		d["collider"] = sr[i].get_collider();
		d["shape"] = sr[i].shape;
		ret[i] = d;
	}

	return ret;
}

Vector<real_t> PhysicsDirectSpaceState3D::_cast_motion(RequiredParam<PhysicsShapeQueryParameters3D> p_shape_query) {
	EXTRACT_PARAM_OR_FAIL_V(shape_query, p_shape_query, Vector<real_t>());

	real_t closest_safe = 1.0f, closest_unsafe = 1.0f;
	bool res = cast_motion(shape_query->get_parameters(), closest_safe, closest_unsafe);
	if (!res) {
		return Vector<real_t>();
	}
	Vector<real_t> ret;
	ret.resize(2);
	ret.write[0] = closest_safe;
	ret.write[1] = closest_unsafe;
	return ret;
}

TypedArray<Vector3> PhysicsDirectSpaceState3D::_collide_shape(RequiredParam<PhysicsShapeQueryParameters3D> p_shape_query, int p_max_results) {
	EXTRACT_PARAM_OR_FAIL_V(shape_query, p_shape_query, TypedArray<Vector3>());

	Vector<Vector3> ret;
	ret.resize(p_max_results * 2);
	int rc = 0;
	bool res = collide_shape(shape_query->get_parameters(), ret.ptrw(), p_max_results, rc);
	if (!res) {
		return TypedArray<Vector3>();
	}
	TypedArray<Vector3> r;
	r.resize(rc * 2);
	for (int i = 0; i < rc * 2; i++) {
		r[i] = ret[i];
	}
	return r;
}

Dictionary PhysicsDirectSpaceState3D::_get_rest_info(RequiredParam<PhysicsShapeQueryParameters3D> p_shape_query) {
	EXTRACT_PARAM_OR_FAIL_V(shape_query, p_shape_query, Dictionary());

	PS3DT::ShapeRestInfo sri;

	bool res = rest_info(shape_query->get_parameters(), &sri);
	Dictionary r;
	if (!res) {
		return r;
	}

	r["point"] = sri.point;
	r["normal"] = sri.normal;
	r["rid"] = sri.rid;
	r["collider_id"] = sri.collider_id;
	r["shape"] = sri.shape;
	r["linear_velocity"] = sri.linear_velocity;

	return r;
}

TypedArray<TypedStruct<PhysicsShapeResult3DName>> PhysicsDirectSpaceState3D::_intersect_point_struct(RequiredParam<PhysicsPointQueryParameters3D> p_point_query, int p_max_results) {
	TypedArray<TypedStruct<PhysicsShapeResult3DName>> r;
	ERR_FAIL_COND_V(shape_result_layout.is_null(), r);
	EXTRACT_PARAM_OR_FAIL_V(point_query, p_point_query, r);

	Vector<PS3DT::ShapeResult> ret;
	ret.resize(MAX(p_max_results, 0));
	const int rc = intersect_point(point_query->get_parameters(), ret.ptrw(), ret.size());
	r.resize(rc);
	for (int i = 0; i < rc; i++) {
		r[i] = _make_shape_result(ret[i]);
	}
	return r;
}

TypedArray<TypedStruct<PhysicsShapeResult3DName>> PhysicsDirectSpaceState3D::_intersect_shape_struct(RequiredParam<PhysicsShapeQueryParameters3D> p_shape_query, int p_max_results) {
	TypedArray<TypedStruct<PhysicsShapeResult3DName>> r;
	ERR_FAIL_COND_V(shape_result_layout.is_null(), r);
	EXTRACT_PARAM_OR_FAIL_V(shape_query, p_shape_query, r);

	Vector<PS3DT::ShapeResult> ret;
	ret.resize(MAX(p_max_results, 0));
	const int rc = intersect_shape(shape_query->get_parameters(), ret.ptrw(), ret.size());
	r.resize(rc);
	for (int i = 0; i < rc; i++) {
		r[i] = _make_shape_result(ret[i]);
	}
	return r;
}

TypedStruct<PhysicsCastResult3DName> PhysicsDirectSpaceState3D::_cast_motion_struct(RequiredParam<PhysicsShapeQueryParameters3D> p_shape_query) {
	ERR_FAIL_COND_V(cast_result_layout.is_null(), Struct());
	Struct s = cast_result_layout->instantiate();
	EXTRACT_PARAM_OR_FAIL_V(shape_query, p_shape_query, s);

	real_t closest_safe = 1.0f, closest_unsafe = 1.0f;
	if (cast_motion(shape_query->get_parameters(), closest_safe, closest_unsafe)) {
		Variant *fields = s.get_fields_ptrw();
		fields[CAST_RESULT_SAFE_FRACTION] = closest_safe;
		fields[CAST_RESULT_UNSAFE_FRACTION] = closest_unsafe;
	}
	return s; // Both fractions stay 1.0 when nothing is hit or the query fails.
}

TypedStruct<PhysicsRestInfo3DName> PhysicsDirectSpaceState3D::_get_rest_info_struct(RequiredParam<PhysicsShapeQueryParameters3D> p_shape_query) {
	ERR_FAIL_COND_V(rest_info_layout.is_null(), Struct());
	Struct s = rest_info_layout->instantiate();
	EXTRACT_PARAM_OR_FAIL_V(shape_query, p_shape_query, s);

	PS3DT::ShapeRestInfo sri;
	if (!rest_info(shape_query->get_parameters(), &sri)) {
		return s; // `hit` stays false.
	}

	Variant *fields = s.get_fields_ptrw();
	fields[REST_INFO_HIT] = true;
	fields[REST_INFO_POINT] = sri.point;
	fields[REST_INFO_NORMAL] = sri.normal;
	fields[REST_INFO_RID] = sri.rid;
	fields[REST_INFO_COLLIDER_ID] = sri.collider_id;
	fields[REST_INFO_COLLIDER] = ObjectDB::get_instance(sri.collider_id);
	fields[REST_INFO_SHAPE] = sri.shape;
	fields[REST_INFO_LINEAR_VELOCITY] = sri.linear_velocity;
	return s;
}

PhysicsDirectSpaceState3D::PhysicsDirectSpaceState3D() {
}

void PhysicsDirectSpaceState3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("intersect_point", "parameters", "max_results"), &PhysicsDirectSpaceState3D::_intersect_point, DEFVAL(32));
	ClassDB::bind_method(D_METHOD("intersect_ray", "parameters"), &PhysicsDirectSpaceState3D::_intersect_ray);
	ClassDB::bind_method(D_METHOD("intersect_ray_struct", "parameters"), &PhysicsDirectSpaceState3D::_intersect_ray_struct);
	ClassDB::bind_method(D_METHOD("intersect_point_struct", "parameters", "max_results"), &PhysicsDirectSpaceState3D::_intersect_point_struct, DEFVAL(32));
	ClassDB::bind_method(D_METHOD("intersect_shape_struct", "parameters", "max_results"), &PhysicsDirectSpaceState3D::_intersect_shape_struct, DEFVAL(32));
	ClassDB::bind_method(D_METHOD("cast_motion_struct", "parameters"), &PhysicsDirectSpaceState3D::_cast_motion_struct);
	ClassDB::bind_method(D_METHOD("get_rest_info_struct", "parameters"), &PhysicsDirectSpaceState3D::_get_rest_info_struct);
	ClassDB::bind_method(D_METHOD("intersect_shape", "parameters", "max_results"), &PhysicsDirectSpaceState3D::_intersect_shape, DEFVAL(32));
	ClassDB::bind_method(D_METHOD("cast_motion", "parameters"), &PhysicsDirectSpaceState3D::_cast_motion);
	ClassDB::bind_method(D_METHOD("collide_shape", "parameters", "max_results"), &PhysicsDirectSpaceState3D::_collide_shape, DEFVAL(32));
	ClassDB::bind_method(D_METHOD("get_rest_info", "parameters"), &PhysicsDirectSpaceState3D::_get_rest_info);
}
