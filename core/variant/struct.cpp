/**************************************************************************/
/*  struct.cpp                                                            */
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

#include "struct.h"

#include "core/os/mutex.h"
#include "core/templates/hashfuncs.h"
#include "core/templates/local_vector.h"
#include "core/templates/safe_refcount.h"
#include "core/variant/dictionary.h"
#include "core/variant/struct_layout.h"
#include "core/variant/variant.h"

class StructPrivate {
public:
	SafeRefCount refcount;
	Ref<StructLayout> layout;
	LocalVector<Variant> fields;
};

// --- Struct ---

void Struct::_ref(const Struct &p_from) const {
	if (p_from._p == nullptr) {
		_unref();
		return;
	}
	// Take the reference first (thread safe), then drop ours.
	if (!p_from._p->refcount.ref()) {
		return;
	}
	if (p_from._p == _p) {
		_p->refcount.unref();
		return;
	}
	_unref();
	_p = p_from._p;
}

void Struct::_unref() const {
	if (_p == nullptr) {
		return;
	}
	if (_p->refcount.unref()) {
		memdelete(_p);
	}
	_p = nullptr;
}

void Struct::_make_unique() {
	if (_p == nullptr || _p->refcount.get() == 1) {
		return;
	}
	StructPrivate *unique = memnew(StructPrivate);
	unique->refcount.init();
	unique->layout = _p->layout;
	unique->fields = _p->fields;
	_unref();
	_p = unique;
}

Struct::Struct() {}

Struct::Struct(const Ref<StructLayout> &p_layout) {
	if (p_layout.is_null()) {
		return;
	}
	_p = memnew(StructPrivate);
	_p->refcount.init();
	_p->layout = p_layout;
	const int count = p_layout->get_field_count();
	_p->fields.resize(count);
	for (int i = 0; i < count; i++) {
		_p->fields[i] = p_layout->get_field_default_value(i);
	}
}

Struct::Struct(const Struct &p_from) {
	_ref(p_from);
}

Struct::~Struct() {
	_unref();
}

void Struct::operator=(const Struct &p_from) {
	if (this == &p_from) {
		return;
	}
	_ref(p_from);
}

bool Struct::is_null() const {
	return _p == nullptr;
}

Ref<StructLayout> Struct::get_layout() const {
	return _p != nullptr ? _p->layout : Ref<StructLayout>();
}

StringName Struct::get_struct_name() const {
	return _p != nullptr ? _p->layout->get_name() : StringName();
}

uint64_t Struct::id() const {
	return (uint64_t)(uintptr_t)_p;
}

int Struct::get_field_count() const {
	return _p != nullptr ? (int)_p->fields.size() : 0;
}

StringName Struct::get_field_name(int p_index) const {
	ERR_FAIL_NULL_V(_p, StringName());
	return _p->layout->get_field_name(p_index);
}

int Struct::find_field(const StringName &p_name) const {
	return _p != nullptr ? _p->layout->find_field(p_name) : -1;
}

bool Struct::has_field(const StringName &p_name) const {
	return find_field(p_name) >= 0;
}

Variant Struct::get_field(int p_index) const {
	ERR_FAIL_NULL_V(_p, Variant());
	ERR_FAIL_INDEX_V(p_index, (int)_p->fields.size(), Variant());
	return _p->fields[p_index];
}

bool Struct::set_field(int p_index, const Variant &p_value) {
	ERR_FAIL_NULL_V(_p, false);
	ERR_FAIL_INDEX_V(p_index, (int)_p->fields.size(), false);
	Variant value = p_value;
	if (!_p->layout->validate_field_value(p_index, value)) {
		return false;
	}
	_make_unique();
	_p->fields[p_index] = value;
	return true;
}

Variant Struct::get_field_by_name(const StringName &p_name, bool *r_valid) const {
	const int index = find_field(p_name);
	if (r_valid != nullptr) {
		*r_valid = index >= 0;
	}
	return index >= 0 ? _p->fields[index] : Variant();
}

bool Struct::set_field_by_name(const StringName &p_name, const Variant &p_value) {
	const int index = find_field(p_name);
	return index >= 0 && set_field(index, p_value);
}

const Variant *Struct::get_fields_ptr() const {
	return _p != nullptr ? _p->fields.ptr() : nullptr;
}

Variant *Struct::get_fields_ptrw() {
	if (_p == nullptr) {
		return nullptr;
	}
	_make_unique();
	return _p->fields.ptr();
}

Dictionary Struct::to_dictionary() const {
	Dictionary result;
	if (_p == nullptr) {
		return result;
	}
	for (uint32_t i = 0; i < _p->fields.size(); i++) {
		result[_p->layout->get_field_name(i)] = _p->fields[i];
	}
	return result;
}

Struct Struct::duplicate(bool p_deep) const {
	if (_p == nullptr) {
		return Struct();
	}
	Struct result(_p->layout);
	for (uint32_t i = 0; i < _p->fields.size(); i++) {
		const Variant &field = _p->fields[i];
		if (p_deep && (field.get_type() == Variant::ARRAY || field.get_type() == Variant::DICTIONARY || field.get_type() == Variant::STRUCT)) {
			result._p->fields[i] = field.duplicate(true);
		} else {
			result._p->fields[i] = field;
		}
	}
	return result;
}

bool Struct::operator==(const Struct &p_other) const {
	return recursive_equal(p_other, 0);
}

bool Struct::operator!=(const Struct &p_other) const {
	return !recursive_equal(p_other, 0);
}

bool Struct::recursive_equal(const Struct &p_other, int p_recursion_count) const {
	if (_p == p_other._p) {
		return true;
	}
	if (_p == nullptr || p_other._p == nullptr || _p->layout != p_other._p->layout) {
		return false;
	}
	if (p_recursion_count > MAX_RECURSION) {
		ERR_PRINT("Max recursion reached");
		return true;
	}
	p_recursion_count++;
	for (uint32_t i = 0; i < _p->fields.size(); i++) {
		if (!_p->fields[i].hash_compare(p_other._p->fields[i], p_recursion_count, false)) {
			return false;
		}
	}
	return true;
}

uint32_t Struct::hash() const {
	return recursive_hash(0);
}

uint32_t Struct::recursive_hash(int p_recursion_count) const {
	if (_p == nullptr) {
		return hash_murmur3_one_32(0);
	}
	if (p_recursion_count > MAX_RECURSION) {
		ERR_PRINT("Max recursion reached");
		return 0;
	}
	p_recursion_count++;
	uint32_t h = hash_murmur3_one_64((uint64_t)(uintptr_t)_p->layout.ptr());
	for (uint32_t i = 0; i < _p->fields.size(); i++) {
		h = hash_murmur3_one_32(_p->fields[i].recursive_hash(p_recursion_count), h);
	}
	return hash_fmix32(h);
}

String Struct::to_string() const {
	if (_p == nullptr) {
		return "Struct()";
	}
	String result = String(_p->layout->get_name()) + "(";
	for (uint32_t i = 0; i < _p->fields.size(); i++) {
		if (i > 0) {
			result += ", ";
		}
		result += String(_p->layout->get_field_name(i)) + ": " + _p->fields[i].operator String();
	}
	return result + ")";
}

// --- StructLayout ---

void StructLayout::set_name(const StringName &p_name) {
	name = p_name;
}

StringName StructLayout::get_name() const {
	return name;
}

void StructLayout::set_source_path(const String &p_path) {
	source_path = p_path;
}

String StructLayout::get_source_path() const {
	return source_path;
}

int StructLayout::add_field(const StringName &p_name, Variant::Type p_type, const Variant &p_default_value, const StringName &p_class_name, const Ref<Script> &p_script) {
	ERR_FAIL_COND_V_MSG(p_name == StringName(), -1, "A struct field needs a name.");
	if (field_index.has(p_name)) {
		return -1;
	}
	Field field;
	field.name = p_name;
	field.type.variant_type = p_type;
	field.type.class_name = p_class_name;
	field.type.script = p_script;
	field.type.where = "struct field";
	if (p_default_value.get_type() == Variant::NIL && p_type != Variant::NIL && p_type != Variant::OBJECT) {
		Callable::CallError err;
		Variant::construct(p_type, field.default_value, nullptr, 0, err);
	} else {
		field.default_value = p_default_value;
		ERR_FAIL_COND_V_MSG(!field.type.validate(field.default_value, "set the default of"), -1, vformat("The default value of struct field \"%s\" does not match its type.", p_name));
	}
	const int index = fields.size();
	fields.push_back(field);
	field_index[p_name] = index;
	return index;
}

int StructLayout::get_field_count() const {
	return fields.size();
}

const StructLayout::Field &StructLayout::get_field(int p_index) const {
	CRASH_BAD_INDEX(p_index, fields.size());
	return fields[p_index];
}

StringName StructLayout::get_field_name(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, fields.size(), StringName());
	return fields[p_index].name;
}

Variant::Type StructLayout::get_field_type(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, fields.size(), Variant::NIL);
	return fields[p_index].type.variant_type;
}

Variant StructLayout::get_field_default_value(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, fields.size(), Variant());
	return fields[p_index].default_value;
}

int StructLayout::find_field(const StringName &p_name) const {
	const int *index = field_index.getptr(p_name);
	return index != nullptr ? *index : -1;
}

bool StructLayout::has_field(const StringName &p_name) const {
	return field_index.has(p_name);
}

bool StructLayout::validate_field_value(int p_index, Variant &r_value) const {
	ERR_FAIL_INDEX_V(p_index, fields.size(), false);
	return fields[p_index].type.validate(r_value, "assign");
}

Struct StructLayout::instantiate() const {
	return Struct(Ref<StructLayout>(const_cast<StructLayout *>(this)));
}

Struct StructLayout::instantiate(const Variant **p_args, int p_argcount, Callable::CallError &r_error) const {
	r_error.error = Callable::CallError::CALL_OK;
	if (p_argcount > fields.size()) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
		r_error.expected = fields.size();
		return Struct();
	}
	Struct result = instantiate();
	for (int i = 0; i < p_argcount; i++) {
		if (!result.set_field(i, *p_args[i])) {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = i;
			r_error.expected = fields[i].type.variant_type;
			return Struct();
		}
	}
	return result;
}

static HashMap<StringName, Ref<StructLayout>> *struct_layout_registry = nullptr;
static Mutex struct_layout_registry_mutex;

void StructLayout::register_layout(const Ref<StructLayout> &p_layout) {
	ERR_FAIL_COND(p_layout.is_null());
	ERR_FAIL_COND_MSG(p_layout->name == StringName(), "A struct layout needs a name to be registered.");
	MutexLock lock(struct_layout_registry_mutex);
	if (struct_layout_registry == nullptr) {
		struct_layout_registry = memnew((HashMap<StringName, Ref<StructLayout>>));
	}
	(*struct_layout_registry)[p_layout->name] = p_layout;
}

void StructLayout::unregister_layout(const StringName &p_name) {
	MutexLock lock(struct_layout_registry_mutex);
	if (struct_layout_registry != nullptr) {
		struct_layout_registry->erase(p_name);
	}
}

Ref<StructLayout> StructLayout::find_layout(const StringName &p_name) {
	MutexLock lock(struct_layout_registry_mutex);
	if (struct_layout_registry == nullptr) {
		return Ref<StructLayout>();
	}
	const Ref<StructLayout> *layout = struct_layout_registry->getptr(p_name);
	return layout != nullptr ? *layout : Ref<StructLayout>();
}

void StructLayout::cleanup() {
	MutexLock lock(struct_layout_registry_mutex);
	if (struct_layout_registry != nullptr) {
		memdelete(struct_layout_registry);
		struct_layout_registry = nullptr;
	}
}

void StructLayout::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_name"), &StructLayout::get_name);
	ClassDB::bind_method(D_METHOD("get_field_count"), &StructLayout::get_field_count);
	ClassDB::bind_method(D_METHOD("get_field_name", "index"), &StructLayout::get_field_name);
	ClassDB::bind_method(D_METHOD("get_field_type", "index"), &StructLayout::get_field_type);
	ClassDB::bind_method(D_METHOD("get_field_default_value", "index"), &StructLayout::get_field_default_value);
	ClassDB::bind_method(D_METHOD("find_field", "name"), &StructLayout::find_field);
	ClassDB::bind_method(D_METHOD("has_field", "name"), &StructLayout::has_field);
}
