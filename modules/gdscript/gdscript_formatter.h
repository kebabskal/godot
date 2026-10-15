/**************************************************************************/
/*  gdscript_formatter.h                                                  */
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

#ifdef TOOLS_ENABLED

#include "core/string/ustring.h"
#include "core/templates/vector.h"

// Normalizes the whitespace of a script without changing its tokens: one space around a binary
// operator and after a comma, none inside brackets or around `.`, two before an inline comment,
// no trailing whitespace, and at most two blank lines in a row. Indentation, strings, f-strings,
// node paths and comments are left as written, and so is a script that does not tokenize. Served
// to external editors over the language server (`textDocument/formatting`).
class GDScriptFormatter {
public:
	struct Edit {
		int line = 0; // 0-based.
		// `remove`: the lines [line, end_line) go, line endings included. Otherwise `text` replaces
		// the line's text in front of its line ending.
		bool remove = false;
		int end_line = 0;
		String text;
	};

	// The edits that format `p_source`, in order and non-overlapping. False when it does not
	// tokenize (an unterminated string, inconsistent indentation): nothing is edited then.
	static bool format(const String &p_source, Vector<Edit> &r_edits);
	// The formatted source, or `p_source` as given when it does not tokenize.
	static String format_text(const String &p_source);
};

#endif // TOOLS_ENABLED
