/**************************************************************************/
/*  test_gdscript_formatter.h                                             */
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

#include "../gdscript_formatter.h"
#include "../gdscript_tokenizer.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "tests/test_macros.h"

namespace GDScriptTests {

static String format(const String &p_source) {
	return GDScriptFormatter::format_text(p_source);
}

static void collect_scripts(const String &p_dir, Vector<String> &r_paths) {
	Ref<DirAccess> dir = DirAccess::open(p_dir);
	if (dir.is_null()) {
		return;
	}
	dir->list_dir_begin();
	for (String name = dir->get_next(); !name.is_empty(); name = dir->get_next()) {
		if (name == "." || name == "..") {
			continue;
		}
		const String path = p_dir.path_join(name);
		if (dir->current_is_dir()) {
			collect_scripts(path, r_paths);
		} else if (name.ends_with(".gd")) {
			r_paths.push_back(path);
		}
	}
	dir->list_dir_end();
}

// The tokens of a script, one per line, without positions: what formatting must not change.
static String token_shapes(const String &p_source) {
	GDScriptTokenizerText tokenizer;
	tokenizer.set_source_code(p_source);
	String shapes;
	while (true) {
		const GDScriptTokenizer::Token token = tokenizer.scan();
		if (token.type == GDScriptTokenizer::Token::TK_EOF) {
			break;
		}
		if (token.type == GDScriptTokenizer::Token::NEWLINE) {
			continue; // Lines are never joined or split; a file of blank lines does lose its one newline.
		}
		const bool layout = token.type == GDScriptTokenizer::Token::INDENT || token.type == GDScriptTokenizer::Token::DEDENT;
		shapes += String(token.get_name()) + (layout ? String() : " " + token.source) + "\n";
	}
	return shapes;
}

TEST_SUITE("[Modules][GDScript][Formatter]") {
	TEST_CASE("[GDScript][Formatter] Short lambdas keep their arrow") {
		CHECK(format("var f = n=>n*2\n") == "var f = n => n * 2\n");
		CHECK(format("var g = (a,b)=>a+b\n") == "var g = (a, b) => a + b\n");
		CHECK(format("var h = ()=>print(\"x\")\n") == "var h = () => print(\"x\")\n");
		CHECK(format("print(nums.map(n => n * 2).filter(n => n > 2))\n") == "print(nums.map(n => n * 2).filter(n => n > 2))\n");
		CHECK(format("hit.connect(d => d.to_upper())\n") == "hit.connect(d => d.to_upper())\n");
		CHECK(format("var neg = n => -n\n") == "var neg = n => -n\n");
		CHECK(format("var alive=units.filter(u=>u.hp>0)\n") == "var alive = units.filter(u => u.hp > 0)\n");
	}

	TEST_CASE("[GDScript][Formatter] Operators get one space, unary ones none") {
		CHECK(format("var x=a+b*c-d/e%f**g\n") == "var x = a + b * c - d / e % f ** g\n");
		CHECK(format("x+=1\ny -=  2\nz:=3\n") == "x += 1\ny -= 2\nz := 3\n");
		CHECK(format("if a==b and c!=d or not e:\n\tpass\n") == "if a == b and c != d or not e:\n\tpass\n");
		CHECK(format("var n = -1\nvar m = - x\nf(-1, -2)\nreturn -x\n") == "var n = -1\nvar m = -x\nf(-1, -2)\nreturn -x\n");
		// A sign against a digit is part of the literal, so `-2 ** 2` is 4 and `- 2 ** 2` is -4.
		CHECK(format("print(- 2 ** 2)\nprint(-2 ** 2)\nprint(+ 1)\nprint(- .5)\n") == "print(- 2 ** 2)\nprint(-2 ** 2)\nprint(+ 1)\nprint(-.5)\n");
		CHECK(format("var d = a - -b\nvar e = 2 ** -1\nvar t = !x and ~y\n") == "var d = a - -b\nvar e = 2 ** -1\nvar t = !x and ~y\n");
		CHECK(format("-x\n") == "-x\n");
		CHECK(format("func f(*args, **kw):\n\tpass\n") == "func f(*args, **kw):\n\tpass\n");
		CHECK(format("var s = \"%d\"%x\n") == "var s = \"%d\" % x\n");
		CHECK(format("var q = a if b else -c\n") == "var q = a if b else -c\n");
		CHECK(format("var r = x<<1|y>>2&z^w\n") == "var r = x << 1 | y >> 2 & z ^ w\n");
	}

	TEST_CASE("[GDScript][Formatter] Brackets, commas and dots") {
		CHECK(format("print( a , b )\n") == "print(a, b)\n");
		CHECK(format("var a = [ 1,2 ]\nvar d = { \"k\" : 1 , \"j\":2 }\nvar e = {}\n") == "var a = [1, 2]\nvar d = {\"k\": 1, \"j\": 2}\nvar e = {}\n");
		CHECK(format("x = a [0] . b ( ) . c\n") == "x = a[0].b().c\n");
		CHECK(format("if (a) :\n\tpass\n") == "if (a):\n\tpass\n");
		CHECK(format("var t = (a + b) * c\n") == "var t = (a + b) * c\n");
		CHECK(format("func f(a:int,b:=2,c = 3)->int:\n\treturn a\n") == "func f(a: int, b := 2, c = 3) -> int:\n\treturn a\n");
		CHECK(format("for i in range(3): pass\n") == "for i in range(3): pass\n");
		CHECK(format("a = 1;b = 2\n") == "a = 1; b = 2\n");
		CHECK(format("var l = func(x): return x\n") == "var l = func(x): return x\n");
		CHECK(format("assert(x)\npreload(\"a.gd\")\nsuper()\nsuper.f()\nawait x\n") == "assert(x)\npreload(\"a.gd\")\nsuper()\nsuper.f()\nawait x\n");
		CHECK(format("match x:\n\t1, 2:\n\t\tpass\n\t[var a, _]:\n\t\tpass\n\t_:\n\t\tpass\n") == "match x:\n\t1, 2:\n\t\tpass\n\t[var a, _]:\n\t\tpass\n\t_:\n\t\tpass\n");
	}

	TEST_CASE("[GDScript][Formatter] Slices, subscripts and type parameters keep their colon spacing") {
		CHECK(format("var s = a[1:2]\nvar t = a[1: 2]\nvar u = a[:2]\nvar v = a[ 1 :2 ]\n") == "var s = a[1:2]\nvar t = a[1: 2]\nvar u = a[:2]\nvar v = a[1:2]\n");
		CHECK(format("func pick[T: Node](items: Array[T]) -> T:\n\treturn items[0]\n") == "func pick[T: Node](items: Array[T]) -> T:\n\treturn items[0]\n");
		CHECK(format("var d: Dictionary[String,int] = {}\n") == "var d: Dictionary[String, int] = {}\n");
	}

	TEST_CASE("[GDScript][Formatter] Nullable, safe navigation and enum shorthand") {
		CHECK(format("var t : Node ? = null\n") == "var t: Node? = null\n");
		CHECK(format("var n = t ?. name ?? \"none\"\nvar i = a ?[ 0 ]\n") == "var n = t?.name ?? \"none\"\nvar i = a?[0]\n");
		CHECK(format("state = .IDLE\nf(.IDLE, .RUN)\nif state == .IDLE:\n\tpass\n") == "state = .IDLE\nf(.IDLE, .RUN)\nif state == .IDLE:\n\tpass\n");
		CHECK(format("func pair() -> (bool,int):\n\treturn true, 1\n") == "func pair() -> (bool, int):\n\treturn true, 1\n");
	}

	TEST_CASE("[GDScript][Formatter] Node paths, unique names and annotations stay whole") {
		CHECK(format("@onready var a=$Path/To/Node\n") == "@onready var a = $Path/To/Node\n");
		CHECK(format("var b = $\"Path With Space\"\nvar c = $A/%Unique.name\nvar d = %Btn\n") == "var b = $\"Path With Space\"\nvar c = $A/%Unique.name\nvar d = %Btn\n");
		CHECK(format("var e = $A/B.get_child( 0 ).name\n") == "var e = $A/B.get_child(0).name\n");
		CHECK(format("var f = $A/B+1\n") == "var f = $A/B + 1\n");
		CHECK(format("@export_range(1,10) var f:int=1\n@export var g\n@tool\n") == "@export_range(1, 10) var f: int = 1\n@export var g\n@tool\n");
		CHECK(format("var p = ^\"a/b\"\nvar s = &\"name\"\n") == "var p = ^\"a/b\"\nvar s = &\"name\"\n");
	}

	TEST_CASE("[GDScript][Formatter] Strings, f-strings and comments are left as written") {
		CHECK(format("var s = \"a  +  b\"\n") == "var s = \"a  +  b\"\n");
		CHECK(format("var f = f\"{x+1:.2f} and {y}\"+z\n") == "var f = f\"{x+1:.2f} and {y}\" + z\n");
		CHECK(format("var m = \"\"\"\n  a+b\n\n\n\n  c\"\"\"+d\n") == "var m = \"\"\"\n  a+b\n\n\n\n  c\"\"\" + d\n");
		CHECK(format("x=1 # note  \n") == "x = 1  # note\n");
		CHECK(format("x=1#note\n") == "x = 1  #note\n");
		CHECK(format("\t# indented  \n#  comment #  \n") == "\t# indented\n#  comment #\n");
		CHECK(format("var h = \"#\"+a # real\n") == "var h = \"#\" + a  # real\n");
	}

	TEST_CASE("[GDScript][Formatter] Lines, continuations and blank lines") {
		CHECK(format("x = 1   \n\ty = 2\t\n") == "x = 1\n\ty = 2\n");
		CHECK(format("var t = a+\\\n\tb\n") == "var t = a + \\\n\tb\n");
		CHECK(format("f(\n\ta,\n\t-b,\n)\n") == "f(\n\ta,\n\t-b,\n)\n");
		CHECK(format("\n\nvar a = 1\n\n\n\n\nvar b = 2\n\n\n") == "var a = 1\n\n\nvar b = 2\n");
		CHECK(format("var a = 1\n\n\n\n") == "var a = 1\n");
		CHECK(format("var a = 1") == "var a = 1");
		CHECK(format("var a = 1\n\n  ") == "var a = 1");
		CHECK(format("\n\n\n") == "");
		CHECK(format("") == "");
		CHECK(format("x=1\r\ny=2\r\n") == "x = 1\r\ny = 2\r\n");
	}

	TEST_CASE("[GDScript][Formatter] Code that does not tokenize is left alone") {
		CHECK(format("var s = \"open\n") == "var s = \"open\n");
		CHECK(format("if x:\n\ta=1\n        b=2\n") == "if x:\n\ta=1\n        b=2\n");
		CHECK(format("<<<<<<< HEAD\nx=1\n") == "<<<<<<< HEAD\nx=1\n");
	}

	TEST_CASE("[GDScript][Formatter] Every test script keeps its tokens and is formatted in one pass") {
		Vector<String> paths;
		collect_scripts("modules/gdscript/tests/scripts", paths);
		REQUIRE_MESSAGE(paths.size() > 100, "The GDScript test scripts were not found; run from the repository root.");
		int formatted = 0;
		for (const String &path : paths) {
			const String source = FileAccess::get_file_as_string(path);
			Vector<GDScriptFormatter::Edit> edits;
			if (!GDScriptFormatter::format(source, edits)) {
				continue; // A script that does not tokenize, written to test that.
			}
			formatted++;
			const String once = format(source);
			CHECK_MESSAGE(token_shapes(once) == token_shapes(source), path);
			CHECK_MESSAGE(format(once) == once, path);
		}
		CHECK(formatted > 100);
	}

	TEST_CASE("[GDScript][Formatter] Formatting is idempotent") {
		const String source = "@tool\nclass_name Thing extends Node\n\nstruct Hit:\n\tvar damage: int\n\nsignal hit(d: int)\n\n@export var speed := 1.0\n@onready var label := $UI/Label\n\nfunc _ready() -> void:\n\thit.connect(d => print(f\"{d} damage\"))\n\tvar items: Array[Hit] = []\n\tvar big := items.filter(h => h.damage > 2).map(h => h.damage * 2)\n\tif not big.is_empty() and big[0] > -1:\n\t\tlabel.text = \"%d\" % big[0]  # the first\n\tmatch speed:\n\t\t1.0:\n\t\t\tpass\n\t\t_:\n\t\t\tspeed = .5\n";
		CHECK(format(source) == source);
		CHECK(format(format("var f = n=>n*2\nx=[ 1,2 ]\n")) == format("var f = n=>n*2\nx=[ 1,2 ]\n"));
	}
}

} // namespace GDScriptTests

#endif // TOOLS_ENABLED
