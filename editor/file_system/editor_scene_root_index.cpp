/**************************************************************************/
/*  editor_scene_root_index.cpp                                           */
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

#include "editor_scene_root_index.h"

#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_uid.h"
#include "core/object/class_db.h"
#include "core/object/script_language.h"
#include "core/os/os.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/resource_format_text.h"

// How far chains are followed before giving up, against cycles in broken projects.
static constexpr int MAX_CHAIN = 64;

// Script files are told apart from scenes by extension, as the languages register them.
static ScriptLanguage *_language_for(const String &p_path) {
	const String ext = p_path.get_extension().to_lower();
	for (int i = 0; i < ScriptServer::get_language_count(); i++) {
		if (ScriptServer::get_language(i)->get_extension() == ext) {
			return ScriptServer::get_language(i);
		}
	}
	return nullptr;
}

String EditorSceneRootIndex::_resolve(const String &p_uid, const String &p_path) {
	// As loading does: the UID wins while the editor knows it, the written path otherwise. This
	// runs at query time, so a file that moved is found where it is now.
	if (!p_uid.is_empty()) {
		const ResourceUID::ID id = ResourceUID::get_singleton()->text_to_id(p_uid);
		if (id != ResourceUID::INVALID_ID && ResourceUID::get_singleton()->has_id(id)) {
			return ResourceUID::get_singleton()->get_id_path(id);
		}
	}
	return p_path;
}

void EditorSceneRootIndex::_read(const String &p_path, Facts &r_facts) {
	ScriptLanguage *language = _language_for(p_path);
	if (language) {
		String base_path;
		r_facts.readable = language->get_script_file_extends(p_path, &r_facts.class_name, &base_path, &r_facts.base_class);
		r_facts.script_path = base_path;
		return;
	}

	if (p_path.get_extension().to_lower() == "tscn") {
		ResourceLoaderText::SceneRoot root;
		if (ResourceFormatLoaderText::get_scene_root(p_path, root) != OK) {
			return;
		}
		r_facts.readable = true;
		r_facts.has_root = root.has_root;
		r_facts.root_type = root.type;
		r_facts.script_uid = root.script_uid;
		r_facts.script_path = root.script_path;
		r_facts.builtin_script = root.builtin_script;
		r_facts.base_scene_uid = root.base_scene_uid;
		r_facts.base_scene_path = root.base_scene_path;
		return;
	}

	// A binary scene has no cheap reader, so it is loaded (fresh from disk, not from the cache,
	// which could hold an older version). Rare in projects, and kept like any other fact.
	const Ref<PackedScene> scene = ResourceLoader::load(p_path, "PackedScene", ResourceFormatLoader::CACHE_MODE_IGNORE);
	if (scene.is_null()) {
		return;
	}
	const Ref<SceneState> state = scene->get_state();
	r_facts.readable = true;
	r_facts.has_root = state->get_node_count() > 0;
	if (!r_facts.has_root) {
		return;
	}
	r_facts.root_type = state->get_node_type(0);
	for (int i = 0; i < state->get_node_property_count(0); i++) {
		if (state->get_node_property_name(0, i) == CoreStringName(script)) {
			const Ref<Resource> script = state->get_node_property_value(0, i);
			if (script.is_valid()) {
				r_facts.builtin_script = script->is_built_in();
				r_facts.script_path = script->get_path();
			}
			break;
		}
	}
	const Ref<SceneState> base = state->get_base_scene_state();
	if (base.is_valid()) {
		r_facts.base_scene_path = base->get_path();
	} else if (state->get_node_instance(0).is_valid()) {
		r_facts.base_scene_path = state->get_node_instance(0)->get_path();
	}
}

EditorSceneRootIndex::Facts EditorSceneRootIndex::_get(const String &p_path) {
	const uint64_t modified_time = FileAccess::exists(p_path) ? FileAccess::get_modified_time(p_path) : 0;
	if (modified_time == 0) {
		if (facts.erase(p_path)) {
			dirty = true;
		}
		return Facts(); // Gone, or not a file: unreadable.
	}

	const Facts *known = facts.getptr(p_path);
	if (known && known->modified_time == modified_time) {
		return *known;
	}

	Facts fresh;
	fresh.modified_time = modified_time;
	_read(p_path, fresh);

	// Keep the facts only if the file has settled: written in an earlier second than now (with
	// a second's slack for file systems whose clock differs a little), and unchanged while it
	// was read. Otherwise another write in the same second would leave the time as it is.
	const uint64_t now = unix_time_override ? unix_time_override() : (uint64_t)OS::get_singleton()->get_unix_time();
	const bool settled = modified_time + 2 <= now && FileAccess::get_modified_time(p_path) == modified_time;
	if (settled) {
		facts[p_path] = fresh;
		dirty = true;
	} else if (facts.erase(p_path)) {
		dirty = true;
	}
	return fresh;
}

String EditorSceneRootIndex::_global_class_path(const String &p_name) {
	// The editor's list of global classes is only as current as its last scan, so the file it
	// names is asked whether it still declares the class. A class the list does not have yet
	// (declared since) cannot be found at all: the caller treats an empty result as unknown.
	if (!ScriptServer::is_global_class(p_name)) {
		return String();
	}
	const String path = ScriptServer::get_global_class_path(p_name);
	const Facts script = _get(path);
	if (!script.readable || script.class_name != p_name) {
		return String();
	}
	return path;
}

EditorSceneRootIndex::Match EditorSceneRootIndex::match(const String &p_scene_path, const String &p_type) {
	// What the root has to be: a script (by path), or an engine class.
	String required_script;
	if (p_type.begins_with("uid://") || p_type.is_absolute_path()) {
		required_script = ResourceUID::ensure_path(p_type);
		if (required_script.is_empty()) {
			return UNKNOWN;
		}
	} else if (!ClassDB::class_exists(p_type)) {
		required_script = _global_class_path(p_type);
		if (required_script.is_empty()) {
			return UNKNOWN;
		}
	}

	// The root: its engine class, and its script, the nearest one along the inheritance of scenes.
	String native;
	String script;
	String scene = p_scene_path;
	for (int depth = 0; native.is_empty(); depth++) {
		if (depth == MAX_CHAIN || scene.is_empty()) {
			return UNKNOWN;
		}
		const Facts f = _get(scene);
		if (!f.readable) {
			return UNKNOWN;
		}
		if (!f.has_root) {
			// A scene with no nodes has no root to match; a base scene without one is broken.
			return depth == 0 ? NO_MATCH : UNKNOWN;
		}
		if (script.is_empty()) {
			if (f.builtin_script && !required_script.is_empty()) {
				return UNKNOWN; // What it extends is written inside the scene, not in a file to read.
			}
			script = _resolve(f.script_uid, f.script_path);
		}
		native = f.root_type;
		scene = _resolve(f.base_scene_uid, f.base_scene_path);
	}

	if (required_script.is_empty()) {
		if (!ClassDB::class_exists(native)) {
			return UNKNOWN; // An extension class not loaded right now, say.
		}
		return ClassDB::is_parent_class(native, p_type) ? MATCH : NO_MATCH;
	}

	// Walk up from the root's script, one file at a time.
	for (int depth = 0; depth < MAX_CHAIN; depth++) {
		if (script.is_empty()) {
			return NO_MATCH; // Reached an engine class without passing the required script.
		}
		if (script.simplify_path() == required_script.simplify_path()) {
			return MATCH;
		}
		const Facts f = _get(script);
		if (!f.readable) {
			return UNKNOWN;
		}
		if (f.script_path.begins_with("uid://")) {
			script = _resolve(f.script_path, String());
			if (script.is_empty()) {
				return UNKNOWN; // A UID the editor has not seen yet.
			}
		} else if (!f.script_path.is_empty()) {
			script = f.script_path;
		} else if (ClassDB::class_exists(f.base_class)) {
			script = String();
		} else {
			script = _global_class_path(f.base_class);
			if (script.is_empty()) {
				return UNKNOWN;
			}
		}
	}
	return UNKNOWN;
}

void EditorSceneRootIndex::forget(const String &p_path) {
	if (facts.erase(p_path)) {
		dirty = true;
	}
}

void EditorSceneRootIndex::load(const String &p_cache_path) {
	cache_path = p_cache_path;
	facts.clear();
	dirty = false;
	Ref<FileAccess> f = FileAccess::open(cache_path, FileAccess::READ);
	if (f.is_null()) {
		return;
	}
	const Array entries = f->get_var();
	for (const Variant &v : entries) {
		const Array e = v;
		if (e.size() != 12) {
			continue; // Written by a different version: those facts are read again.
		}
		Facts facts_entry;
		facts_entry.modified_time = e[1];
		facts_entry.readable = e[2];
		facts_entry.has_root = e[3];
		facts_entry.root_type = e[4];
		facts_entry.script_uid = e[5];
		facts_entry.script_path = e[6];
		facts_entry.builtin_script = e[7];
		facts_entry.base_scene_uid = e[8];
		facts_entry.base_scene_path = e[9];
		facts_entry.class_name = e[10];
		facts_entry.base_class = e[11];
		facts[e[0]] = facts_entry;
	}
}

void EditorSceneRootIndex::save() {
	if (!dirty || cache_path.is_empty()) {
		return;
	}
	Array entries;
	for (const KeyValue<String, Facts> &kv : facts) {
		const Facts &e = kv.value;
		entries.push_back(Array{ kv.key, e.modified_time, e.readable, e.has_root, e.root_type, e.script_uid, e.script_path, e.builtin_script, e.base_scene_uid, e.base_scene_path, e.class_name, e.base_class });
	}
	Ref<FileAccess> f = FileAccess::open(cache_path, FileAccess::WRITE);
	if (f.is_valid()) {
		f->store_var(entries);
		dirty = false;
	}
}
