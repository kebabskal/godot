/**************************************************************************/
/*  struct_db.cpp                                                            */
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

#include "struct_db.h"

Ref<StructLayout> StructDB::add_layout(const StringName &p_name, std::initializer_list<FieldDef> p_fields) {
	ERR_FAIL_COND_V_MSG(p_name == StringName(), Ref<StructLayout>(), "An engine struct layout needs a name.");
	ERR_FAIL_COND_V_MSG(has_layout(p_name), Ref<StructLayout>(), vformat("An engine struct layout named \"%s\" is already registered.", p_name));

	Ref<StructLayout> layout;
	layout.instantiate();
	layout->set_name(p_name);
	for (const FieldDef &field : p_fields) {
		ERR_FAIL_COND_V_MSG(layout->add_field(field.name, field.type, field.default_value, field.class_name) < 0, Ref<StructLayout>(), vformat("Cannot add field \"%s\" to engine struct layout \"%s\": duplicated name or invalid default value.", field.name, p_name));
	}
	StructLayout::register_layout(layout);
	return layout;
}

Ref<StructLayout> StructDB::get_layout(const StringName &p_name) {
	Ref<StructLayout> layout = StructLayout::find_layout(p_name);
	if (layout.is_valid() && !layout->get_source_path().is_empty()) {
		return Ref<StructLayout>(); // A script layout that happens to have no path prefix is not an engine one.
	}
	return layout;
}

bool StructDB::has_layout(const StringName &p_name) {
	return get_layout(p_name).is_valid();
}

void StructDB::remove_layout(const StringName &p_name) {
	if (has_layout(p_name)) {
		StructLayout::unregister_layout(p_name);
	}
}

void StructDB::get_layout_list(List<StringName> *r_list) {
	List<Ref<StructLayout>> layouts;
	StructLayout::get_registered_layouts(&layouts);
	for (const Ref<StructLayout> &layout : layouts) {
		if (layout->get_source_path().is_empty()) {
			r_list->push_back(layout->get_name());
		}
	}
	r_list->sort_custom<StringName::AlphCompare>();
}
