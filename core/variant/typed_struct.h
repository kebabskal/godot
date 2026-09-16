/**************************************************************************/
/*  typed_struct.h                                                            */
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

#include "core/variant/struct.h"

#include <type_traits>

// A `Struct` return or argument type annotated with its engine layout, so the binding's type info
// carries the layout name (`PROPERTY_HINT_STRUCT_TYPE`) and typed callers know the fields:
//
//     inline constexpr char PhysicsRayResult3DName[] = "PhysicsRayResult3D";
//     TypedStruct<PhysicsRayResult3DName> intersect_ray_struct(...);
//
// Nothing is checked at runtime; the value is a plain `Struct`, like `TypedArray<T>` is an `Array`.
template <const char *LAYOUT_NAME>
class TypedStruct : public Struct {
public:
	_FORCE_INLINE_ static const char *get_layout_name() { return LAYOUT_NAME; }

	_FORCE_INLINE_ TypedStruct() {}
	_FORCE_INLINE_ TypedStruct(const Struct &p_struct) :
			Struct(p_struct) {}
};

template <typename T>
struct is_typed_struct : std::false_type {};
template <const char *LAYOUT_NAME>
struct is_typed_struct<TypedStruct<LAYOUT_NAME>> : std::true_type {};
