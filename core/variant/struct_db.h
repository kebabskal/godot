/**************************************************************************/
/*  struct_db.h                                                            */
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
#include "core/templates/list.h"
#include "core/variant/struct_layout.h"

#include <initializer_list>

// Engine-declared struct layouts: registered once at startup, next to `ClassDB`, and looked up by
// plain name. Script layouts share the registry under a qualified `path::name`; this is the engine
// side of it, and what the extension API dump enumerates.
class StructDB {
public:
	struct FieldDef {
		StringName name;
		Variant::Type type = Variant::NIL;
		Variant default_value;
		StringName class_name; // For `OBJECT` fields.
	};

	// Creates and registers a layout. Returns null (and reports an error) if the name is taken or a
	// field is invalid, so a broken declaration is caught at startup.
	static Ref<StructLayout> add_layout(const StringName &p_name, std::initializer_list<FieldDef> p_fields);
	static Ref<StructLayout> get_layout(const StringName &p_name);
	static bool has_layout(const StringName &p_name);
	static void remove_layout(const StringName &p_name);
	// The names of the engine layouts, sorted.
	static void get_layout_list(List<StringName> *r_list);
};
