/**************************************************************************/
/*  gdscript_format.cpp                                                   */
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

#include "gdscript_format.h"

#include <cmath>
#include <cstdio>

// Limits that keep a typo from asking for a gigabyte of padding.
static constexpr int MAX_WIDTH = 4096;
static constexpr int MAX_PRECISION = 100;

static bool _is_align(char32_t p_char) {
	return p_char == '<' || p_char == '>' || p_char == '^' || p_char == '=';
}

static bool _is_integer_type(char32_t p_type) {
	return p_type == 'd' || p_type == 'b' || p_type == 'o' || p_type == 'x' || p_type == 'X';
}

static bool _is_float_type(char32_t p_type) {
	return p_type == 'f' || p_type == 'F' || p_type == 'e' || p_type == 'E' || p_type == 'g' || p_type == 'G' || p_type == '%';
}

bool GDScriptFormatSpec::parse(const String &p_spec, GDScriptFormatSpec &r_spec, String &r_error) {
	r_spec = GDScriptFormatSpec();
	const int length = p_spec.length();
	int i = 0;

	if (length >= 2 && _is_align(p_spec[1])) {
		r_spec.fill = p_spec[0];
		r_spec.align = p_spec[1];
		i = 2;
	} else if (length >= 1 && _is_align(p_spec[0])) {
		r_spec.align = p_spec[0];
		i = 1;
	}
	if (i < length && (p_spec[i] == '+' || p_spec[i] == '-' || p_spec[i] == ' ')) {
		r_spec.sign = p_spec[i++];
	}
	if (i < length && p_spec[i] == '#') {
		r_spec.alternate = true;
		i++;
	}
	if (i < length && p_spec[i] == '0') {
		r_spec.zero = true;
		i++;
	}
	if (i < length && is_digit(p_spec[i])) {
		r_spec.width = 0;
		while (i < length && is_digit(p_spec[i])) {
			r_spec.width = r_spec.width * 10 + (p_spec[i++] - '0');
			if (r_spec.width > MAX_WIDTH) {
				r_error = vformat("The width is larger than %d.", MAX_WIDTH);
				return false;
			}
		}
	}
	if (i < length && (p_spec[i] == ',' || p_spec[i] == '_')) {
		r_spec.grouping = p_spec[i++];
	}
	if (i < length && p_spec[i] == '.') {
		i++;
		if (i >= length || !is_digit(p_spec[i])) {
			r_error = R"(Expected a number of digits after ".", as in ".2f".)";
			return false;
		}
		r_spec.precision = 0;
		while (i < length && is_digit(p_spec[i])) {
			r_spec.precision = r_spec.precision * 10 + (p_spec[i++] - '0');
			if (r_spec.precision > MAX_PRECISION) {
				r_error = vformat("The precision is larger than %d.", MAX_PRECISION);
				return false;
			}
		}
	}
	if (i < length) {
		const char32_t type = p_spec[i++];
		if (!_is_integer_type(type) && !_is_float_type(type) && type != 's') {
			r_error = vformat(R"("%s" is not a format type. Use "d", "b", "o", "x" or "X" for integers, "f", "e", "g" or "%%" for numbers, "s" for text.)", String::chr(type));
			return false;
		}
		r_spec.type = type;
	}
	if (i < length) {
		r_error = vformat(R"(Unexpected "%s" in the format spec "%s".)", String::chr(p_spec[i]), p_spec);
		return false;
	}

	if (r_spec.precision >= 0 && _is_integer_type(r_spec.type)) {
		r_error = vformat(R"(An integer ("%s") has no digits after the point to set a precision for.)", String::chr(r_spec.type));
		return false;
	}
	if (r_spec.grouping == ',' && (r_spec.type == 'b' || r_spec.type == 'o' || r_spec.type == 'x' || r_spec.type == 'X')) {
		r_error = R"("," groups decimal digits; use "_" to group binary, octal and hexadecimal digits.)";
		return false;
	}
	if (r_spec.type == 's' && (r_spec.sign != '-' || r_spec.alternate || r_spec.grouping != 0)) {
		r_error = R"(Text ("s") has no sign, "#" prefix or digit grouping.)";
		return false;
	}
	return true;
}

// Inserts `p_separator` every `p_every` digits into the run of digits that starts `p_text`.
static String _group(const String &p_text, char32_t p_separator, int p_every, bool p_hex = false) {
	int digits = 0;
	while (digits < p_text.length() && (p_hex ? is_hex_digit(p_text[digits]) : is_digit(p_text[digits]))) {
		digits++;
	}
	String result;
	for (int i = 0; i < digits; i++) {
		if (i > 0 && (digits - i) % p_every == 0) {
			result += p_separator;
		}
		result += p_text[i];
	}
	return result + p_text.substr(digits);
}

static String _pad(const String &p_sign, const String &p_body, const GDScriptFormatSpec &p_spec, char32_t p_default_align) {
	const String whole = p_sign + p_body;
	if (p_spec.width <= whole.length()) {
		return whole;
	}
	char32_t align = p_spec.align;
	char32_t fill = p_spec.fill;
	if (align == 0 && p_spec.zero) {
		align = '=';
		fill = '0';
	} else if (align == 0) {
		align = p_default_align;
	}
	const int padding = p_spec.width - whole.length();
	switch (align) {
		case '<':
			return whole + String::chr(fill).repeat(padding);
		case '^':
			return String::chr(fill).repeat(padding / 2) + whole + String::chr(fill).repeat(padding - padding / 2);
		case '=':
			return p_sign + String::chr(fill).repeat(padding) + p_body;
		default:
			return String::chr(fill).repeat(padding) + whole;
	}
}

static String _format_double(double p_value, char p_type, int p_precision, bool p_alternate) {
	// Literal formats only, so compilers can check them. The buffer fits the largest double with the
	// largest precision the spec allows.
	char buffer[512];
	switch (p_type) {
		case 'e':
			snprintf(buffer, sizeof(buffer), p_alternate ? "%#.*e" : "%.*e", p_precision, p_value);
			break;
		case 'E':
			snprintf(buffer, sizeof(buffer), p_alternate ? "%#.*E" : "%.*E", p_precision, p_value);
			break;
		case 'g':
			snprintf(buffer, sizeof(buffer), p_alternate ? "%#.*g" : "%.*g", p_precision, p_value);
			break;
		case 'G':
			snprintf(buffer, sizeof(buffer), p_alternate ? "%#.*G" : "%.*G", p_precision, p_value);
			break;
		default:
			snprintf(buffer, sizeof(buffer), p_alternate ? "%#.*f" : "%.*f", p_precision, p_value);
			break;
	}
	return String(buffer);
}

bool GDScriptFormatSpec::format(const Variant &p_value, const GDScriptFormatSpec &p_spec, String &r_result, String &r_error) {
	const Variant::Type value_type = p_value.get_type();
	const bool is_int = value_type == Variant::INT;
	const bool is_float = value_type == Variant::FLOAT;

	char32_t type = p_spec.type;
	if (type == 0) {
		if (is_int) {
			type = 'd';
		} else if (is_float) {
			type = p_spec.precision >= 0 ? 'g' : 0; // Without either, a float prints as `str()` does.
		} else {
			type = 's';
		}
	}

	if (type == 's' || (type == 0 && !is_float)) {
		if (p_spec.sign != '-' || p_spec.alternate || p_spec.grouping != 0) {
			r_error = vformat(R"(A value of type "%s" has no sign, "#" prefix or digit grouping to format.)", Variant::get_type_name(value_type));
			return false;
		}
		String text = p_value.stringify();
		if (p_spec.precision >= 0) {
			text = text.substr(0, p_spec.precision);
		}
		r_result = _pad(String(), text, p_spec, '<');
		return true;
	}

	if (!is_int && !is_float) {
		r_error = vformat(R"(Cannot format a value of type "%s" as a number ("%s").)", Variant::get_type_name(value_type), String::chr(type));
		return false;
	}
	if (_is_integer_type(type) && !is_int) {
		r_error = vformat(R"(Cannot format a float as an integer ("%s"). Round it first, or use "f".)", String::chr(type));
		return false;
	}

	bool negative = false;
	String body;
	String prefix;
	if (_is_integer_type(type)) {
		const int64_t value = p_value;
		negative = value < 0;
		const uint64_t magnitude = negative ? (uint64_t)0 - (uint64_t)value : (uint64_t)value;
		int base = 10;
		int group_every = 3;
		switch (type) {
			case 'b':
				base = 2;
				group_every = 4;
				prefix = "0b";
				break;
			case 'o':
				base = 8;
				group_every = 4;
				prefix = "0o";
				break;
			case 'x':
			case 'X':
				base = 16;
				group_every = 4;
				prefix = type == 'x' ? "0x" : "0X";
				break;
			default:
				break;
		}
		body = String::num_uint64(magnitude, base, type == 'X');
		if (p_spec.grouping != 0) {
			body = _group(body, p_spec.grouping, group_every, base == 16);
		}
		if (!p_spec.alternate) {
			prefix = String();
		}
	} else {
		const double value = p_value;
		negative = std::signbit(value) && !std::isnan(value);
		const double magnitude = std::fabs(value);
		const bool upper = type == 'F' || type == 'E' || type == 'G';
		if (std::isnan(value) || std::isinf(value)) {
			body = std::isnan(value) ? "nan" : "inf";
			if (upper) {
				body = body.to_upper();
			}
			if (type == '%') {
				body += "%";
			}
		} else if (type == 0) {
			body = Variant(magnitude).stringify();
		} else {
			const int precision = p_spec.precision >= 0 ? p_spec.precision : 6;
			switch (type) {
				case 'f':
				case 'F':
					body = _format_double(magnitude, 'f', precision, p_spec.alternate);
					break;
				case 'e':
				case 'E':
					body = _format_double(magnitude, (char)type, precision, p_spec.alternate);
					break;
				case 'g':
				case 'G':
					body = _format_double(magnitude, (char)type, MAX(precision, 1), p_spec.alternate);
					break;
				case '%':
					body = _format_double(magnitude * 100.0, 'f', precision, p_spec.alternate) + "%";
					break;
				default:
					break;
			}
			if (p_spec.grouping != 0) {
				body = _group(body, p_spec.grouping, 3);
			}
		}
	}

	String sign;
	if (negative) {
		sign = "-";
	} else if (p_spec.sign == '+') {
		sign = "+";
	} else if (p_spec.sign == ' ') {
		sign = " ";
	}
	// With `=` (and `0`), padding goes between the sign and prefix and the digits.
	const char32_t align = p_spec.align != 0 ? p_spec.align : (p_spec.zero ? '=' : '>');
	if (align == '=') {
		r_result = _pad(sign + prefix, body, p_spec, '>');
	} else {
		r_result = _pad(String(), sign + prefix + body, p_spec, '>');
	}
	return true;
}
