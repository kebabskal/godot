/**************************************************************************/
/*  gdscript_format.h                                                     */
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

#include "core/string/ustring.h"
#include "core/variant/variant.h"

// The format spec of an f-string field, `f"{price:>10,.2f}"`: Python's format mini-language, which
// Rust and C# spell nearly the same way.
//
//     [[fill]align][sign][#][0][width][grouping][.precision][type]
//
// align: `<` left, `>` right, `^` centre, `=` pad after the sign. sign: `+`, `-`, or ` `.
// `#`: `0b`/`0o`/`0x` prefixes. `0`: pad with zeros after the sign. grouping: `,` or `_`.
// type: `d` `b` `o` `x` `X` for integers; `f` `F` `e` `E` `g` `G` `%` for numbers; `s` for text.
struct GDScriptFormatSpec {
	char32_t fill = ' ';
	char32_t align = 0; // 0: numbers right, everything else left.
	char32_t sign = '-';
	bool alternate = false;
	bool zero = false;
	int width = -1;
	char32_t grouping = 0;
	int precision = -1;
	char32_t type = 0;

	// Reads a spec; on failure `r_error` says what is wrong with it.
	static bool parse(const String &p_spec, GDScriptFormatSpec &r_spec, String &r_error);
	// Formats a value; on failure (a spec that does not fit the value's type) `r_error` says why.
	static bool format(const Variant &p_value, const GDScriptFormatSpec &p_spec, String &r_result, String &r_error);
};
