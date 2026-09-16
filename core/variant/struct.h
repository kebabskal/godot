/**************************************************************************/
/*  struct.h                                                              */
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

#include "core/string/string_name.h"
#include "core/templates/vector.h"
#include "core/typedefs.h"

class Dictionary;
class StructLayout;
class StructPrivate;
class Variant;
template <typename T>
class Ref;

// A value of a `StructLayout`: a fixed set of named, typed fields.
//
// Value semantics with copy-on-write: copying a `Struct` shares the storage, and the first write
// through a shared copy clones it, so passing structs around and keeping them in arrays is cheap
// while assignments never alias. A default-constructed `Struct` has no layout and no fields.
class Struct {
	mutable StructPrivate *_p = nullptr;

	void _ref(const Struct &p_from) const;
	void _unref() const;
	// Copy-on-write: makes this value's storage unshared before a write.
	void _make_unique();

public:
	Struct();
	explicit Struct(const Ref<StructLayout> &p_layout);
	Struct(const Struct &p_from);
	~Struct();

	void operator=(const Struct &p_from);

	// True when this value has no layout (the default-constructed struct).
	bool is_null() const;
	Ref<StructLayout> get_layout() const;
	// The layout's name, or an empty name for a null struct.
	StringName get_struct_name() const;
	// Identity of the shared storage; equal for copies that still share it. Used for identity
	// comparison, never for equality.
	uint64_t id() const;

	int get_field_count() const;
	StringName get_field_name(int p_index) const;
	// Field index for a name, or -1.
	int find_field(const StringName &p_name) const;
	bool has_field(const StringName &p_name) const;

	Variant get_field(int p_index) const;
	// Validates (and converts, where the layout allows) the value against the field's type. Returns
	// false and leaves the struct unchanged when the value is not acceptable or the index is invalid.
	bool set_field(int p_index, const Variant &p_value);
	Variant get_field_by_name(const StringName &p_name, bool *r_valid = nullptr) const;
	bool set_field_by_name(const StringName &p_name, const Variant &p_value);

	// Direct access to the field storage, for callers that already validated what they write.
	// `get_fields_ptrw()` clones shared storage first. Both are null for a null struct.
	const Variant *get_fields_ptr() const;
	Variant *get_fields_ptrw();

	// The fields as name -> value.
	Dictionary to_dictionary() const;

	// A copy whose storage is not shared with this one. With `p_deep`, fields that are arrays,
	// dictionaries or structs are duplicated recursively.
	Struct duplicate(bool p_deep = false) const;

	bool operator==(const Struct &p_other) const;
	bool operator!=(const Struct &p_other) const;
	bool recursive_equal(const Struct &p_other, int p_recursion_count) const;

	uint32_t hash() const;
	uint32_t recursive_hash(int p_recursion_count) const;

	String to_string() const;
};
