/**************************************************************************/
/*  editor_scene_root_index.h                                             */
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
#include "core/templates/hash_map.h"

// Answers "is this scene's root node an Enemy?" without loading scenes or scripts, for filtering
// lists of scenes such as Quick Load's.
//
// It never goes stale, however files change (in the editor, by a script, by an external tool):
// - Every fact comes from one file's own contents: a scene's root type, root script and base
//   scene; a script's class name and what it extends. Nothing derived from other files is kept,
//   so the chains (inherited scene to base scene, script to base script) are walked at query
//   time, each link from its own file.
// - Each fact is kept with its file's modification time and checked against the disk on every
//   use, so a change is seen whether or not the editor has rescanned.
// - Modification times have one-second resolution, so a fact read in the same second as the
//   file was written could miss a second write in that second. Such facts are used but not kept.
// - The answer is three-valued. Only NO_MATCH should hide a scene; anything that cannot be
//   established from the files (an unreadable file, a built-in script, a class name the editor
//   has not seen yet) is UNKNOWN, and the final check still happens when a scene is picked.
class EditorSceneRootIndex {
public:
	enum Match {
		MATCH,
		NO_MATCH,
		UNKNOWN,
	};

	// `p_type` is what `PROPERTY_HINT_SCENE_ROOT_TYPE` carries: an engine class, a global class
	// name, or a script path (or UID).
	Match match(const String &p_scene_path, const String &p_type);

	// Drops what is known about a file. Not needed for correctness, since every use checks the
	// disk, but lets the editor discard a fact as soon as it knows the file changed.
	void forget(const String &p_path);

	// Persists facts between sessions. Loaded facts are held to the same check as fresh ones.
	void load(const String &p_cache_path);
	void save();

	// Tests use these to control the clock and to see what was kept.
	static inline uint64_t (*unix_time_override)() = nullptr;
	bool has_cached(const String &p_path) const { return facts.has(p_path); }

private:
	struct Facts {
		uint64_t modified_time = 0;
		bool readable = false;
		// Scenes.
		bool has_root = false;
		String root_type;
		String script_uid; // The root's script, or for a script, the script it extends by path.
		String script_path;
		bool builtin_script = false;
		String base_scene_uid;
		String base_scene_path;
		// Scripts.
		String class_name;
		String base_class; // What a script extends by name, when not by path.
	};

	HashMap<String, Facts> facts;
	String cache_path;
	bool dirty = false;

	Facts _get(const String &p_path);
	static void _read(const String &p_path, Facts &r_facts);
	static String _resolve(const String &p_uid, const String &p_path);
	String _global_class_path(const String &p_name);
};
