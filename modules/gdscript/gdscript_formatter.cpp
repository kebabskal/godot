/**************************************************************************/
/*  gdscript_formatter.cpp                                                */
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

#include "gdscript_formatter.h"

#ifdef TOOLS_ENABLED

#include "gdscript_tokenizer.h"

#include "core/templates/hash_map.h"

using Token = GDScriptTokenizer::Token;

// A token that ends a value: what a `.`, a `(` or a `[` attaches to, and what a `-` after it is
// subtracted from rather than negating.
static bool _is_value_like(Token::Type p_type) {
	switch (p_type) {
		case Token::IDENTIFIER:
		case Token::LITERAL:
		case Token::CONST_PI:
		case Token::CONST_TAU:
		case Token::CONST_INF:
		case Token::CONST_NAN:
		case Token::SELF:
		case Token::SUPER:
		case Token::UNDERSCORE:
		case Token::PARENTHESIS_CLOSE:
		case Token::BRACKET_CLOSE:
		case Token::BRACE_CLOSE:
		case Token::FSTRING_END:
		case Token::QUESTION_MARK: // `Node?`, the end of a nullable type.
			return true;
		default:
			return false;
	}
}

// A keyword that is followed by a space (`self` and `super` are values, see above).
static bool _is_keyword(Token::Type p_type) {
	switch (p_type) {
		case Token::IF:
		case Token::ELIF:
		case Token::ELSE:
		case Token::FOR:
		case Token::WHILE:
		case Token::BREAK:
		case Token::CONTINUE:
		case Token::PASS:
		case Token::RETURN:
		case Token::MATCH:
		case Token::WHEN:
		case Token::AS:
		case Token::ASSERT:
		case Token::AWAIT:
		case Token::BREAKPOINT:
		case Token::CLASS:
		case Token::CLASS_NAME:
		case Token::TK_CONST:
		case Token::ENUM:
		case Token::EXTENDS:
		case Token::FUNC:
		case Token::TK_IN:
		case Token::IS:
		case Token::NAMESPACE:
		case Token::PRELOAD:
		case Token::SIGNAL:
		case Token::STATIC:
		case Token::STRUCT:
		case Token::TRAIT:
		case Token::VAR:
		case Token::TK_VOID:
		case Token::YIELD:
		case Token::AND:
		case Token::OR:
		case Token::NOT:
			return true;
		default:
			return false;
	}
}

// An operator that can also be a prefix: `a - b` but `-b`, `a * b` but `*args`, `"%d" % x` but `%Name`.
static bool _can_be_unary(Token::Type p_type) {
	switch (p_type) {
		case Token::PLUS:
		case Token::MINUS:
		case Token::STAR:
		case Token::STAR_STAR:
		case Token::PERCENT:
		case Token::TILDE:
		case Token::BANG:
			return true;
		default:
			return false;
	}
}

static bool _opens(Token::Type p_type) {
	return p_type == Token::PARENTHESIS_OPEN || p_type == Token::BRACKET_OPEN || p_type == Token::BRACE_OPEN || p_type == Token::QUESTION_BRACKET;
}

static bool _closes(Token::Type p_type) {
	return p_type == Token::PARENTHESIS_CLOSE || p_type == Token::BRACKET_CLOSE || p_type == Token::BRACE_CLOSE;
}

static bool _is_period(Token::Type p_type) {
	return p_type == Token::PERIOD || p_type == Token::PERIOD_PERIOD || p_type == Token::PERIOD_PERIOD_PERIOD;
}

// Whether `p_next` continues the `$Path/To/Node` that `p_prev` is part of. The parser reads a
// string right after the `$`, else names (keywords included) joined by `/`, with `%` for a unique
// name; anything else, `$A/B+1`, is an operator applied to the node.
static bool _continues_node_path(const Token &p_prev, const Token &p_next) {
	return p_next.type == Token::SLASH || p_next.type == Token::PERCENT || p_next.is_node_name() ||
			(p_prev.type == Token::DOLLAR && p_next.type == Token::LITERAL);
}

struct FormatState {
	struct Bracket {
		Token::Type type = Token::PARENTHESIS_OPEN;
		bool subscript = false; // `a[1:2]` rather than `[1, 2]`: a colon inside is a slice.
	};
	Vector<Bracket> brackets;
	int fstring_depth = 0; // Inside `f"..."`, whose text and fields are left as written.
	bool node_path = false; // Inside `$Path/To/Node`, which the tokenizer splits into pieces.
	bool prev_unary = false; // The previous token is a prefix operator, glued to its operand.
};

// The whitespace to put between two tokens on one line. `p_original` is what was there.
static String _gap(const Token &p_prev, const Token &p_next, const Token *p_after_next, const String &p_original, const FormatState &p_state) {
	if (p_state.fstring_depth > 0) {
		return p_original;
	}
	if (p_prev.type == Token::DOLLAR) {
		return "";
	}
	if (p_state.node_path && p_original.is_empty() && _continues_node_path(p_prev, p_next)) {
		return "";
	}
	// `:=` reads as one operator.
	const bool walrus = p_next.type == Token::COLON && p_after_next != nullptr && p_after_next->type == Token::EQUAL &&
			p_after_next->start_line == p_next.end_line && p_after_next->start_column == p_next.end_column;
	if (walrus) {
		return _opens(p_prev.type) ? "" : " ";
	}
	if (p_prev.type == Token::COLON && p_next.type == Token::EQUAL && p_original.is_empty()) {
		return "";
	}
	// Nothing before what closes or joins.
	switch (p_next.type) {
		case Token::PARENTHESIS_CLOSE:
		case Token::BRACKET_CLOSE:
		case Token::BRACE_CLOSE:
		case Token::COMMA:
		case Token::SEMICOLON:
		case Token::COLON:
		case Token::QUESTION_PERIOD:
		case Token::QUESTION_BRACKET:
		case Token::QUESTION_MARK:
			return "";
		default:
			break;
	}
	if (_is_period(p_next.type) && _is_value_like(p_prev.type)) {
		return ""; // `a.b`; a `.IDLE` after `=` or `,` keeps its space below.
	}
	// Nothing after what opens or joins.
	if (_opens(p_prev.type) || _is_period(p_prev.type) || p_prev.type == Token::QUESTION_PERIOD) {
		return "";
	}
	if (p_prev.type == Token::ANNOTATION) {
		return p_next.type == Token::PARENTHESIS_OPEN ? "" : " ";
	}
	if (p_state.prev_unary) {
		// A sign written against a number is part of the literal to the tokenizer, so `-2 ** 2` is
		// 4 and `- 2 ** 2` is -4: a space between a sign and a digit is kept.
		const bool sign = p_prev.type == Token::MINUS || p_prev.type == Token::PLUS;
		if (sign && p_next.type == Token::LITERAL && !p_next.source.is_empty() && is_digit(p_next.source[0]) && !p_original.is_empty()) {
			return " ";
		}
		return "";
	}
	if (p_next.type == Token::PARENTHESIS_OPEN) {
		switch (p_prev.type) {
			case Token::FUNC: // A lambda.
			case Token::PRELOAD:
			case Token::ASSERT:
			case Token::YIELD:
				return "";
			default:
				return _is_value_like(p_prev.type) ? "" : " ";
		}
	}
	if (p_next.type == Token::BRACKET_OPEN) {
		return _is_value_like(p_prev.type) ? "" : " ";
	}
	if (p_prev.type == Token::COLON) {
		// A slice, `a[1:2]`, is written both ways; a type parameter bound, `[T: int]`, shares the
		// brackets, so what was there stays.
		if (!p_state.brackets.is_empty()) {
			const FormatState::Bracket &innermost = p_state.brackets[p_state.brackets.size() - 1];
			if (innermost.type == Token::BRACKET_OPEN && innermost.subscript) {
				return p_original.is_empty() ? "" : " ";
			}
		}
		return " ";
	}
	// Everything else: a binary operator, a comma, a keyword, a value next to a keyword.
	return " ";
}

bool GDScriptFormatter::format(const String &p_source, Vector<Edit> &r_edits) {
	r_edits.clear();

	GDScriptTokenizerText tokenizer;
	tokenizer.set_source_code(p_source);
	Vector<Token> tokens;
	while (true) {
		Token token = tokenizer.scan();
		if (token.type == Token::TK_EOF) {
			break;
		}
		if (token.type == Token::ERROR || token.type == Token::VCS_CONFLICT_MARKER) {
			return false; // Broken code is not reshaped.
		}
		if (token.type == Token::NEWLINE || token.type == Token::INDENT || token.type == Token::DEDENT || token.type == Token::EMPTY) {
			continue;
		}
		tokens.push_back(token);
	}
	const HashMap<int, GDScriptTokenizer::CommentData> &comments = tokenizer.get_comments();

	// Without the carriage return: the tokenizer's columns count characters, and an edit replaces a
	// line's text in front of whatever line ending it has.
	Vector<String> lines = p_source.split("\n");
	for (int i = 0; i < lines.size(); i++) {
		if (lines[i].ends_with("\r")) {
			lines.write[i] = lines[i].substr(0, lines[i].length() - 1);
		}
	}
	// A source that ends with a newline splits into one more, empty, element that is not a line.
	const int line_count = p_source.ends_with("\n") ? lines.size() - 1 : lines.size();

	// Lines that begin inside a token started on an earlier line (a multiline string): 1-based.
	HashMap<int, int> covered_by;
	for (int i = 0; i < tokens.size(); i++) {
		for (int line = tokens[i].start_line + 1; line <= tokens[i].end_line; line++) {
			covered_by[line] = i;
		}
	}

	FormatState state;
	const Token *prev = nullptr;
	int next_token = 0;
	int blank_run_start = -1; // 0-based first line of the current run of blank lines.
	bool content_seen = false;

	auto end_blank_run = [&](int p_end) {
		// `p_end`: the first line after the run. Leading blank lines go, and all but two of a run.
		if (blank_run_start < 0) {
			return;
		}
		const int keep = content_seen ? 2 : 0;
		if (p_end - blank_run_start > keep) {
			Edit edit;
			edit.remove = true;
			edit.line = blank_run_start + keep;
			edit.end_line = p_end;
			r_edits.push_back(edit);
		}
		blank_run_start = -1;
	};

	for (int l = 0; l < line_count; l++) {
		const int line_number = l + 1;
		const String &line = lines[l];
		const int first = next_token;
		while (next_token < tokens.size() && tokens[next_token].start_line == line_number) {
			next_token++;
		}
		const int *covering_index = covered_by.getptr(line_number);
		const Token *covering = covering_index != nullptr ? &tokens[*covering_index] : nullptr;

		if (first == next_token && covering == nullptr) {
			const String stripped = line.strip_edges(false, true);
			if (stripped.is_empty()) {
				if (blank_run_start < 0) {
					blank_run_start = l;
				}
				continue;
			}
			// A comment on its own line, left as written but for trailing whitespace.
			end_blank_run(l);
			content_seen = true;
			if (stripped != line) {
				Edit edit;
				edit.line = l;
				edit.text = stripped;
				r_edits.push_back(edit);
			}
			continue;
		}
		end_blank_run(l);
		content_seen = true;

		if (covering != nullptr && covering->end_line > line_number) {
			continue; // Wholly inside a multiline string.
		}

		String out;
		int cursor = 0; // Where in `line` the text not yet copied starts.
		if (covering != nullptr) {
			cursor = covering->end_column - 1;
			out = line.substr(0, cursor);
			prev = covering;
		} else {
			while (cursor < line.length() && (line[cursor] == ' ' || line[cursor] == '\t')) {
				cursor++;
			}
			out = line.substr(0, cursor);
		}

		for (int k = first; k < next_token; k++) {
			const Token &token = tokens[k];
			const bool first_on_line = k == first;
			const String original_gap = line.substr(cursor, token.start_column - 1 - cursor);
			if (prev != nullptr && (!first_on_line || covering != nullptr)) {
				const Token *after = k + 1 < next_token ? &tokens[k + 1] : nullptr;
				out += _gap(*prev, token, after, original_gap, state);
			}
			if (token.end_line == token.start_line) {
				out += line.substr(token.start_column - 1, token.end_column - token.start_column);
				cursor = token.end_column - 1;
			} else {
				out += line.substr(token.start_column - 1);
				cursor = line.length();
			}

			// What the next gap needs to know about this token.
			const bool statement_start = first_on_line && covering == nullptr && state.brackets.is_empty();
			state.prev_unary = _can_be_unary(token.type) && (prev == nullptr || statement_start || !_is_value_like(prev->type));
			if (token.type == Token::DOLLAR) {
				state.node_path = true;
			} else if (state.node_path) {
				state.node_path = prev != nullptr && original_gap.is_empty() && _continues_node_path(*prev, token);
			}
			if (_opens(token.type)) {
				FormatState::Bracket bracket;
				bracket.type = token.type == Token::QUESTION_BRACKET ? Token::BRACKET_OPEN : token.type;
				bracket.subscript = token.type == Token::QUESTION_BRACKET || (token.type == Token::BRACKET_OPEN && prev != nullptr && _is_value_like(prev->type) && !state.node_path);
				state.brackets.push_back(bracket);
			} else if (_closes(token.type) && !state.brackets.is_empty()) {
				state.brackets.resize(state.brackets.size() - 1);
			}
			if (token.type == Token::FSTRING_START) {
				state.fstring_depth++;
			} else if (token.type == Token::FSTRING_END && state.fstring_depth > 0) {
				state.fstring_depth--;
			}
			prev = &token;
		}

		// After the last token: an inline comment, or the `\` that continues the line.
		String rest = line.substr(cursor).strip_edges();
		const GDScriptTokenizer::CommentData *comment = comments.getptr(line_number);
		if (comment != nullptr && !comment->new_line) {
			const String text = comment->comment.strip_edges(false, true); // Also drops a carriage return.
			if (rest != text) {
				continue; // Not where it was expected; this line is left alone.
			}
			out += "  " + text;
		} else if (rest == "\\") {
			out += " \\";
		} else if (!rest.is_empty()) {
			continue; // Something the tokenizer swallowed; this line is left alone.
		}

		if (out != line) {
			Edit edit;
			edit.line = l;
			edit.text = out;
			r_edits.push_back(edit);
		}
	}
	// Blank lines at the end go entirely.
	content_seen = false;
	end_blank_run(line_count);

	return true;
}

String GDScriptFormatter::format_text(const String &p_source) {
	Vector<Edit> edits;
	if (!format(p_source, edits)) {
		return p_source;
	}
	Vector<String> lines = p_source.split("\n");
	Vector<String> result;
	int next_edit = 0;
	for (int i = 0; i < lines.size(); i++) {
		if (next_edit < edits.size() && edits[next_edit].remove && i >= edits[next_edit].line) {
			if (i < edits[next_edit].end_line) {
				continue;
			}
			next_edit++;
		}
		if (next_edit < edits.size() && !edits[next_edit].remove && edits[next_edit].line == i) {
			const String ending = lines[i].ends_with("\r") ? "\r" : "";
			result.push_back(edits[next_edit].text + ending);
			next_edit++;
		} else {
			result.push_back(lines[i]);
		}
	}
	return String("\n").join(result);
}

#endif // TOOLS_ENABLED
