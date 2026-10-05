/**************************************************************************/
/*  problems_dock.cpp                                                     */
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

#include "problems_dock.h"

#include "core/config/project_settings.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_uid.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/object/editor_language.h"
#include "core/object/script_language.h"
#include "core/os/os.h"
#include "editor/docks/filesystem_dock.h"
#include "editor/editor_data.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/run/editor_run_bar.h"
#include "editor/script/script_editor_plugin.h"
#include "editor/settings/editor_command_palette.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/tree.h"
#include "scene/main/scene_tree.h"
#include "scene/main/timer.h"
#include "scene/resources/packed_scene.h"

ProblemsDock *ProblemsDock::singleton = nullptr;

// Time spent checking files per frame, so a full scan never stalls the editor.
static constexpr uint64_t FRAME_BUDGET_USEC = 8000;

Vector<StringName> ProblemsDock::get_missing_required_properties(Object *p_object) {
	Vector<StringName> missing;
	if (p_object == nullptr) {
		return missing;
	}
	List<PropertyInfo> properties;
	p_object->get_property_list(&properties);
	for (const PropertyInfo &property : properties) {
		if ((property.usage & PROPERTY_USAGE_REQUIRED) && is_required_value_missing(p_object->get(property.name))) {
			missing.push_back(property.name);
		}
	}
	return missing;
}

bool ProblemsDock::_is_scene_path(const String &p_path) {
	const String extension = p_path.get_extension().to_lower();
	return extension == "tscn" || extension == "scn"; // Not imported scenes: they are rebuilt from their source.
}

bool ProblemsDock::_is_resource_path(const String &p_path) {
	const String extension = p_path.get_extension().to_lower();
	return extension == "tres" || extension == "res";
}

String ProblemsDock::_node_label(Node *p_root, Node *p_node) {
	return p_node == p_root ? String(p_root->get_name()) : String(p_root->get_name()) + "/" + String(p_root->get_path_to(p_node));
}

bool ProblemsDock::_is_script_type(const StringName &p_type) {
	return ClassDB::is_parent_class(p_type, SNAME("Script"));
}

// Every file worth checking, with its modification time, and for each file the scenes that depend
// on it, so a change to a script or a sub-scene re-checks the scenes using it.
void ProblemsDock::_collect_files(EditorFileSystemDirectory *p_dir, HashMap<String, uint64_t> &r_files, HashMap<String, Vector<String>> &r_dependents) const {
	for (int i = 0; i < p_dir->get_subdir_count(); i++) {
		_collect_files(p_dir->get_subdir(i), r_files, r_dependents);
	}
	for (int i = 0; i < p_dir->get_file_count(); i++) {
		const String path = p_dir->get_file_path(i);
		const bool is_scene = _is_scene_path(path);
		if (!(is_scene || _is_resource_path(path) || _is_script_type(p_dir->get_file_type(i)))) {
			continue;
		}
		r_files[path] = p_dir->get_file_modified_time(i);
		if (is_scene) {
			for (const String &dependency : p_dir->get_file_deps(i)) {
				r_dependents[dependency].push_back(path);
			}
		}
	}
}

void ProblemsDock::_enqueue(const String &p_path) {
	if (queued.has(p_path)) {
		return;
	}
	if (queue.is_empty()) {
		scan_total = 0;
	}
	queued.insert(p_path);
	queue.push_back(p_path);
	scan_total++;
	set_process(true);
}

void ProblemsDock::rescan() {
	results.clear();
	tree_dirty = true;
	initial_scan_done = true;
	_filesystem_changed();
}

void ProblemsDock::_rescan_pressed() {
	rescan();
}

// Finds what changed on disk since the last check: new and modified files are queued, deleted ones
// dropped. A changed script re-checks every script, since a `class_name` dependency is invisible to
// the file system, and every scene that uses it, directly or through another scene.
void ProblemsDock::_filesystem_changed() {
	if (!initial_scan_done) {
		return; // The first scan starts once the editor has settled; it will see everything then.
	}
	EditorFileSystemDirectory *root = EditorFileSystem::get_singleton()->get_filesystem();
	if (root == nullptr) {
		return;
	}
	HashMap<String, uint64_t> files;
	HashMap<String, Vector<String>> dependents;
	_collect_files(root, files, dependents);

	Vector<String> removed;
	for (const KeyValue<String, FileResult> &E : results) {
		if (!files.has(E.key)) {
			removed.push_back(E.key);
		}
	}
	bool script_changed = false;
	for (const String &path : removed) {
		results.erase(path);
		script_changed = script_changed || (!_is_scene_path(path) && !_is_resource_path(path));
		tree_dirty = true;
	}

	Vector<String> changed;
	for (const KeyValue<String, uint64_t> &E : files) {
		const HashMap<String, FileResult>::Iterator result = results.find(E.key);
		if (!result || result->value.modified_time != E.value) {
			changed.push_back(E.key);
			script_changed = script_changed || (!_is_scene_path(E.key) && !_is_resource_path(E.key));
		}
	}
	changed.sort();

	// Scripts first: they are quick, and scenes are only as right as the scripts they use.
	Vector<String> to_check;
	HashSet<String> seen;
	if (script_changed) {
		for (const KeyValue<String, uint64_t> &E : files) {
			if (!_is_scene_path(E.key) && !_is_resource_path(E.key)) {
				to_check.push_back(E.key);
			}
		}
		to_check.sort();
		for (const String &path : to_check) {
			seen.insert(path);
		}
	}
	for (const String &path : changed) {
		if (!seen.has(path) && !_is_scene_path(path)) {
			to_check.push_back(path);
			seen.insert(path);
		}
	}
	Vector<String> scenes;
	Vector<String> pending = changed;
	for (int i = 0; i < pending.size(); i++) {
		const String &path = pending[i];
		if (_is_scene_path(path) && !seen.has(path)) {
			scenes.push_back(path);
			seen.insert(path);
		}
		const HashMap<String, Vector<String>>::Iterator users = dependents.find(path);
		if (users) {
			for (const String &user : users->value) {
				if (!seen.has(user)) {
					pending.push_back(user); // Checked as a scene when its turn comes.
				}
			}
		}
	}
	for (const String &path : to_check) {
		_enqueue(path);
	}
	for (const String &path : scenes) {
		_enqueue(path);
	}
	_update_counts();
}

void ProblemsDock::_process_queue() {
	const uint64_t start = OS::get_singleton()->get_ticks_usec();
	while (!queue.is_empty()) {
		const String path = queue[0];
		queue.remove_at(0);
		queued.erase(path);
		_check_file(path);
		tree_dirty = true;
		if (OS::get_singleton()->get_ticks_usec() - start > FRAME_BUDGET_USEC) {
			break;
		}
	}
	if (queue.is_empty()) {
		set_process(false);
	}
	// Redrawing the tree for every file would cost more than checking it.
	const uint64_t now = OS::get_singleton()->get_ticks_msec();
	if (tree_dirty && (queue.is_empty() || now - last_tree_update_msec > 500)) {
		_update_tree();
	}
	_update_counts();
}

void ProblemsDock::_check_file(const String &p_path) {
	EditorFileSystem *efs = EditorFileSystem::get_singleton();
	EditorFileSystemDirectory *dir = nullptr;
	int index = -1;
	dir = efs->find_file(p_path, &index);
	if (dir == nullptr || index < 0) {
		results.erase(p_path);
		return;
	}
	FileResult result;
	result.modified_time = dir->get_file_modified_time(index);
	if (_is_script_type(dir->get_file_type(index))) {
		_check_script(p_path, result.problems);
	} else if (_is_scene_path(p_path)) {
		_check_scene(p_path, result.problems);
	} else {
		_check_resource(p_path, result.problems);
	}
	results[p_path] = result;
}

// The check the script editor runs on an open script: parse and analyze, nothing is reloaded.
void ProblemsDock::_check_script(const String &p_path, Vector<Problem> &r_problems) const {
	ScriptLanguage *language = nullptr;
	const String extension = p_path.get_extension().to_lower();
	for (int i = 0; i < ScriptServer::get_language_count(); i++) {
		if (ScriptServer::get_language(i)->get_extension() == extension) {
			language = ScriptServer::get_language(i);
			break;
		}
	}
	if (language == nullptr || language->get_editor_language() == nullptr) {
		return; // No source validator here (C# is checked by its own build).
	}
	Error read_error = OK;
	const String source = FileAccess::get_file_as_string(p_path, &read_error);
	if (read_error != OK) {
		Problem problem;
		problem.message = TTR("The script could not be read.");
		problem.file = p_path;
		r_problems.push_back(problem);
		return;
	}
	List<EditorLanguage::ScriptError> errors;
	List<EditorLanguage::Warning> warnings;
	language->get_editor_language()->validate(source, p_path, &errors, &warnings, nullptr, nullptr);
	for (const EditorLanguage::ScriptError &error : errors) {
		Problem problem;
		problem.message = error.message;
		// An error can belong to a script this one depends on, which is checked on its own.
		if (!error.path.is_empty() && error.path != p_path) {
			problem.message = vformat(TTR("%s (in %s)"), error.message, error.path.get_file());
		}
		problem.file = p_path;
		problem.line = MAX(error.start_line, 1);
		problem.column = MAX(error.start_column, 1);
		r_problems.push_back(problem);
	}
	for (const EditorLanguage::Warning &warning : warnings) {
		Problem problem;
		problem.severity = SEVERITY_WARNING;
		problem.message = warning.string_code.is_empty() ? warning.message : vformat("%s (%s)", warning.message, warning.string_code);
		problem.file = p_path;
		problem.line = MAX(warning.start_line, 1);
		problem.column = MAX(warning.start_column, 1);
		r_problems.push_back(problem);
	}
}

void ProblemsDock::_check_dependencies(const String &p_path, Vector<Problem> &r_problems) const {
	List<String> dependencies;
	ResourceLoader::get_dependencies(p_path, &dependencies, true);
	for (const String &dependency : dependencies) {
		// "uid::type::path", or just a path. The UID wins when it is known, as when loading.
		String path = dependency.get_slice("::", 0);
		if (dependency.get_slice_count("::") >= 3) {
			const ResourceUID::ID uid = ResourceUID::get_singleton()->text_to_id(path);
			if (uid != ResourceUID::INVALID_ID && ResourceUID::get_singleton()->has_id(uid)) {
				path = ResourceUID::get_singleton()->get_id_path(uid);
			} else {
				path = dependency.get_slice("::", 2);
			}
		}
		if (path.is_empty() || FileAccess::exists(path)) {
			continue;
		}
		Problem problem;
		problem.message = vformat(TTR("Missing dependency: \"%s\" does not exist."), path);
		problem.file = p_path;
		r_problems.push_back(problem);
	}
}

void ProblemsDock::_check_scene(const String &p_path, Vector<Problem> &r_problems) const {
	_check_dependencies(p_path, r_problems);
	if (!r_problems.is_empty()) {
		return; // Loading it would only add errors about the same thing.
	}

	// A required property on a scene's root is normally set where the scene is instantiated, so it is
	// checked there. The main scene is never instantiated by another, so its root is checked here.
	String main_scene = GLOBAL_GET("application/run/main_scene");
	main_scene = ResourceUID::ensure_path(main_scene);
	const bool root_is_checked = main_scene == p_path;

	const Ref<PackedScene> scene = ResourceLoader::load(p_path, "PackedScene");
	if (scene.is_null()) {
		Problem problem;
		problem.message = TTR("The scene could not be loaded.");
		problem.file = p_path;
		r_problems.push_back(problem);
		return;
	}

	// An open scene is read from the editor, so unsaved edits count. Node paths are only stored in
	// the file, so those are as of the last save.
	EditorData &editor_data = EditorNode::get_editor_data();
	for (int i = 0; i < editor_data.get_edited_scene_count(); i++) {
		Node *root = editor_data.get_edited_scene_root(i);
		if (root != nullptr && root->get_scene_file_path() == p_path) {
			_check_scene_nodes(p_path, root, root, root_is_checked, r_problems);
			_check_scene_node_paths(p_path, root, scene->get_state(), r_problems);
			return;
		}
	}

	Node *root = scene->instantiate(PackedScene::GEN_EDIT_STATE_INSTANCE);
	if (root == nullptr) {
		Problem problem;
		problem.message = TTR("The scene could not be instantiated.");
		problem.file = p_path;
		r_problems.push_back(problem);
		return;
	}
	_check_scene_nodes(p_path, root, root, root_is_checked, r_problems);
	_check_scene_node_paths(p_path, root, scene->get_state(), r_problems);
	memdelete(root);
}

// Nodes that belong to this scene: its own, and the roots of the scenes it instantiates. What is
// inside an instance belongs to that scene and is reported there.
void ProblemsDock::_check_scene_nodes(const String &p_path, Node *p_root, Node *p_node, bool p_root_is_checked, Vector<Problem> &r_problems) const {
	if (p_node == p_root || p_node->get_owner() == p_root) {
		const NodePath path = p_root->get_path_to(p_node);
		const bool is_instance = p_node != p_root && !p_node->get_scene_file_path().is_empty();
		if (!is_instance) {
			// An instance's own warnings come from its scene, and are reported for that scene.
			for (const String &warning : p_node->get_configuration_warnings()) {
				Problem problem;
				problem.severity = SEVERITY_WARNING;
				problem.message = warning.replace("\n", " ");
				problem.file = p_path;
				problem.node = path;
				problem.node_label = _node_label(p_root, p_node);
				r_problems.push_back(problem);
			}
		}
		if (p_node != p_root || p_root_is_checked) {
			for (const StringName &property : get_missing_required_properties(p_node)) {
				Problem problem;
				problem.message = vformat(TTR("Required property \"%s\" is not set."), property);
				problem.file = p_path;
				problem.node = path;
				problem.node_label = _node_label(p_root, p_node);
				problem.property = property;
				r_problems.push_back(problem);
			}
		}
	}
	for (int i = 0; i < p_node->get_child_count(false); i++) {
		_check_scene_nodes(p_path, p_root, p_node->get_child(i, false), p_root_is_checked, r_problems);
	}
}

// Node paths stored in the scene file that lead nowhere. A node reference in an export is stored as a
// path too, so this finds the ones whose node was renamed, moved or deleted.
void ProblemsDock::_check_scene_node_paths(const String &p_path, Node *p_root, const Ref<SceneState> &p_state, Vector<Problem> &r_problems) const {
	if (p_state.is_null()) {
		return;
	}
	for (int i = 0; i < p_state->get_node_count(); i++) {
		Node *node = p_root->get_node_or_null(p_state->get_node_path(i));
		if (node == nullptr) {
			continue;
		}
		const int depth = p_root->get_path_to(node).get_name_count() - (node == p_root ? 1 : 0);
		for (int j = 0; j < p_state->get_node_property_count(i); j++) {
			const Variant value = p_state->get_node_property_value(i, j);
			Vector<NodePath> paths;
			if (value.get_type() == Variant::NODE_PATH) {
				paths.push_back(value);
			} else if (value.get_type() == Variant::ARRAY) {
				const Array array = value;
				for (const Variant &element : array) {
					if (element.get_type() == Variant::NODE_PATH) {
						paths.push_back(element);
					}
				}
			}
			for (const NodePath &target : paths) {
				if (target.is_empty() || target.is_absolute() || target.get_name_count() == 0) {
					continue;
				}
				// A path that leaves the scene can only be resolved where it is instantiated.
				int up = 0;
				while (up < target.get_name_count() && target.get_name(up) == SNAME("..")) {
					up++;
				}
				if (up > depth) {
					continue;
				}
				Vector<StringName> names;
				for (int k = 0; k < target.get_name_count(); k++) {
					names.push_back(target.get_name(k));
				}
				if (node->get_node_or_null(NodePath(names, false)) != nullptr) {
					continue;
				}
				Problem problem;
				problem.severity = SEVERITY_WARNING;
				const StringName property = p_state->get_node_property_name(i, j);
				problem.message = vformat(TTR("Property \"%s\" points to \"%s\", which does not exist."), property, String(target));
				problem.file = p_path;
				problem.node = p_root->get_path_to(node);
				problem.node_label = _node_label(p_root, node);
				problem.property = property;
				r_problems.push_back(problem);
			}
		}
	}
}

void ProblemsDock::_check_resource(const String &p_path, Vector<Problem> &r_problems) const {
	_check_dependencies(p_path, r_problems);
	if (!r_problems.is_empty()) {
		return;
	}
	// Only a resource with a script can have required properties; loading the others is not needed.
	int index = -1;
	EditorFileSystemDirectory *dir = EditorFileSystem::get_singleton()->find_file(p_path, &index);
	if (dir == nullptr || index < 0 || String(dir->get_file_resource_script_class(index)).is_empty()) {
		return;
	}
	const Ref<Resource> resource = ResourceLoader::load(p_path);
	for (const StringName &property : get_missing_required_properties(resource.ptr())) {
		Problem problem;
		problem.message = vformat(TTR("Required property \"%s\" is not set."), property);
		problem.file = p_path;
		problem.property = property;
		r_problems.push_back(problem);
	}
}

// The scene being edited is re-checked from the editor shortly after it changes, so fixing something
// in the inspector clears it here without saving.
void ProblemsDock::_edited_scene_changed() {
	if (initial_scan_done) {
		edited_scene_timer->start();
	}
}

void ProblemsDock::_recheck_edited_scene() {
	Node *root = EditorNode::get_singleton()->get_edited_scene();
	if (root == nullptr || root->get_scene_file_path().is_empty()) {
		return;
	}
	const String path = root->get_scene_file_path();
	const HashMap<String, FileResult>::Iterator existing = results.find(path);
	FileResult result;
	if (existing) {
		result.modified_time = existing->value.modified_time; // Still the file on disk; saving re-checks it.
	}
	_check_scene(path, result.problems);
	results[path] = result;
	_update_tree();
	_update_counts();
}

Vector<ProblemsDock::Problem> ProblemsDock::get_problems() const {
	Vector<String> paths;
	for (const KeyValue<String, FileResult> &E : results) {
		paths.push_back(E.key);
	}
	paths.sort();
	Vector<Problem> problems;
	for (const String &path : paths) {
		problems.append_array(results[path].problems);
	}
	return problems;
}

void ProblemsDock::_update_counts() {
	error_count = 0;
	warning_count = 0;
	for (const KeyValue<String, FileResult> &E : results) {
		for (const Problem &problem : E.value.problems) {
			if (problem.severity == SEVERITY_ERROR) {
				error_count++;
			} else {
				warning_count++;
			}
		}
	}
	errors_button->set_text(itos(error_count));
	warnings_button->set_text(itos(warning_count));

	if (!queue.is_empty()) {
		status_label->set_text(vformat(TTR("Checking %d of %d files..."), scan_total - queue.size(), scan_total));
	} else if (!initial_scan_done) {
		status_label->set_text(TTR("Waiting for the project to load..."));
	} else {
		status_label->set_text(vformat(TTR("%d files checked."), results.size()));
	}

	if (is_inside_tree()) {
		Ref<Texture2D> icon;
		if (error_count > 0) {
			icon = get_editor_theme_icon(SNAME("StatusError"));
		} else if (warning_count > 0) {
			icon = get_editor_theme_icon(SNAME("StatusWarning"));
		}
		set_dock_icon(icon);
		set_force_show_icon(icon.is_valid());
	}
	if (EditorRunBar::get_singleton()) {
		EditorRunBar::get_singleton()->update_problems_button();
	}
}

void ProblemsDock::_update_tree() {
	tree_dirty = false;
	last_tree_update_msec = OS::get_singleton()->get_ticks_msec();

	// Keep the files the user collapsed collapsed.
	HashSet<String> collapsed;
	for (TreeItem *item = tree->get_root() ? tree->get_root()->get_first_child() : nullptr; item; item = item->get_next()) {
		if (item->is_collapsed()) {
			collapsed.insert(item->get_metadata(0));
		}
	}

	tree->clear();
	shown.clear();
	TreeItem *root = tree->create_item();

	const String filter = search_box->get_text().strip_edges();
	const bool show_errors = errors_button->is_pressed();
	const bool show_warnings = warnings_button->is_pressed();
	const Ref<Texture2D> error_icon = get_editor_theme_icon(SNAME("StatusError"));
	const Ref<Texture2D> warning_icon = get_editor_theme_icon(SNAME("StatusWarning"));
	const Color error_color = get_theme_color(SNAME("error_color"), EditorStringName(Editor));
	const Color warning_color = get_theme_color(SNAME("warning_color"), EditorStringName(Editor));
	const Color location_color = get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor));

	// Files with errors first, then by path.
	Vector<String> paths;
	for (const KeyValue<String, FileResult> &E : results) {
		if (!E.value.problems.is_empty()) {
			paths.push_back(E.key);
		}
	}
	struct ErrorsFirst {
		const HashMap<String, FileResult> *results = nullptr;
		bool has_error(const String &p_path) const {
			for (const Problem &problem : (*results)[p_path].problems) {
				if (problem.severity == SEVERITY_ERROR) {
					return true;
				}
			}
			return false;
		}
		bool operator()(const String &p_a, const String &p_b) const {
			const bool a = has_error(p_a);
			const bool b = has_error(p_b);
			return a != b ? a : p_a < p_b;
		}
	};
	paths.sort_custom<ErrorsFirst>(ErrorsFirst{ &results });

	for (const String &path : paths) {
		TreeItem *file_item = nullptr;
		int file_errors = 0;
		int file_warnings = 0;
		for (const Problem &problem : results[path].problems) {
			if ((problem.severity == SEVERITY_ERROR && !show_errors) || (problem.severity == SEVERITY_WARNING && !show_warnings)) {
				continue;
			}
			String location = problem.line > 0 ? vformat(TTR("line %d"), problem.line) : problem.node_label;
			if (problem.property != StringName()) {
				location += (location.is_empty() ? String() : " " + String::chr(0x00B7) + " ") + String(problem.property);
			}
			if (!filter.is_empty() && !problem.message.containsn(filter) && !path.containsn(filter) && !location.containsn(filter)) {
				continue;
			}
			if (file_item == nullptr) {
				file_item = tree->create_item(root);
				file_item->set_metadata(0, path);
				file_item->set_icon(0, EditorNode::get_singleton()->get_class_icon(EditorFileSystem::get_singleton()->get_file_type(path), "File"));
				file_item->set_collapsed(collapsed.has(path));
				file_item->set_selectable(1, false);
			}
			TreeItem *item = tree->create_item(file_item);
			const bool is_error = problem.severity == SEVERITY_ERROR;
			item->set_icon(0, is_error ? error_icon : warning_icon);
			item->set_text(0, problem.message);
			item->set_tooltip_text(0, problem.message);
			item->set_text(1, location);
			item->set_custom_color(1, location_color);
			item->set_metadata(0, shown.size());
			shown.push_back(problem);
			if (is_error) {
				file_errors++;
			} else {
				file_warnings++;
			}
		}
		if (file_item != nullptr) {
			String counts;
			if (file_errors > 0) {
				counts = vformat(TTRN("%d error", "%d errors", file_errors), file_errors);
			}
			if (file_warnings > 0) {
				counts += (counts.is_empty() ? "" : ", ") + vformat(TTRN("%d warning", "%d warnings", file_warnings), file_warnings);
			}
			file_item->set_text(0, path.trim_prefix("res://"));
			file_item->set_tooltip_text(0, path);
			file_item->set_text(1, counts);
			file_item->set_custom_color(1, file_errors > 0 ? error_color : warning_color);
		}
	}
}

void ProblemsDock::_filter_changed(const String &p_text) {
	_update_tree();
}

void ProblemsDock::_filter_toggled(bool p_pressed) {
	_update_tree();
}

void ProblemsDock::_item_mouse_selected(const Vector2 &p_pos, MouseButton p_button) {
	if (p_button == MouseButton::LEFT) {
		_item_activated();
	}
}

void ProblemsDock::_item_activated() {
	TreeItem *item = tree->get_selected();
	if (item == nullptr) {
		return;
	}
	const Variant metadata = item->get_metadata(0);
	if (metadata.get_type() == Variant::STRING) {
		Problem problem; // A file row: open the file.
		problem.file = metadata;
		_navigate(problem);
		return;
	}
	const int index = metadata;
	if (index >= 0 && index < shown.size()) {
		_navigate(shown[index]);
	}
}

void ProblemsDock::_navigate(const Problem &p_problem) {
	const String &path = p_problem.file;
	if (!ResourceLoader::exists(path) && !FileAccess::exists(path)) {
		return;
	}
	EditorNode *editor = EditorNode::get_singleton();

	if (_is_script_type(EditorFileSystem::get_singleton()->get_file_type(path))) {
		const Ref<Resource> script = ResourceLoader::load(path);
		if (script.is_null()) {
			return;
		}
		ScriptEditor::get_singleton()->edit(script, MAX(p_problem.line - 1, 0), MAX(p_problem.column - 1, 0));
		if (editor->get_editor_selection_history()->get_current() != script->get_instance_id()) {
			editor->push_item(script.ptr(), "", true);
		}
		ScriptEditor::get_singleton()->focus_script_editor(script);
		return;
	}

	if (_is_scene_path(path)) {
		// Opening a scene that is already open switches to it; a broken one offers to fix its dependencies.
		editor->open_scene(path);
		Node *root = editor->get_edited_scene();
		if (root == nullptr || root->get_scene_file_path() != path || p_problem.node.is_empty()) {
			return;
		}
		Node *node = root->get_node_or_null(p_problem.node);
		if (node == nullptr) {
			return;
		}
		EditorSelection *selection = editor->get_editor_selection();
		selection->clear();
		selection->add_node(node);
		editor->push_item(node, p_problem.property);
		return;
	}

	FileSystemDock::get_singleton()->navigate_to_path(path);
	if (p_problem.property != StringName()) {
		const Ref<Resource> resource = ResourceLoader::load(path);
		if (resource.is_valid()) {
			editor->push_item(resource.ptr(), p_problem.property);
		}
	}
}

void ProblemsDock::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			EditorFileSystem::get_singleton()->connect("filesystem_changed", callable_mp(this, &ProblemsDock::_filesystem_changed));
			EditorNode::get_singleton()->connect("scene_changed", callable_mp(this, &ProblemsDock::_edited_scene_changed));
			// A commit changes the history, an undo or a redo the version.
			EditorUndoRedoManager::get_singleton()->connect("history_changed", callable_mp(this, &ProblemsDock::_edited_scene_changed));
			EditorUndoRedoManager::get_singleton()->connect("version_changed", callable_mp(this, &ProblemsDock::_edited_scene_changed));
			get_tree()->connect(SceneStringName(node_configuration_warning_changed), callable_mp(this, &ProblemsDock::_edited_scene_changed).unbind(1));
			set_process(true); // Waits for the first scan of the file system.
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			rescan_button->set_button_icon(get_editor_theme_icon(SNAME("Reload")));
			errors_button->set_button_icon(get_editor_theme_icon(SNAME("StatusError")));
			warnings_button->set_button_icon(get_editor_theme_icon(SNAME("StatusWarning")));
			search_box->set_right_icon(get_editor_theme_icon(SNAME("Search")));
			_update_tree();
		} break;

		case NOTIFICATION_PROCESS: {
			if (!initial_scan_done) {
				// Start once the editor has scanned the file system and opened the last session's scenes.
				EditorFileSystem *efs = EditorFileSystem::get_singleton();
				if (efs->is_scanning() || efs->get_filesystem() == nullptr || !EditorNode::get_singleton()->is_editor_ready()) {
					return;
				}
				initial_scan_done = true;
				_filesystem_changed();
				if (queue.is_empty()) {
					set_process(false);
					_update_counts();
				}
				return;
			}
			_process_queue();
		} break;
	}
}

void ProblemsDock::_bind_methods() {
}

ProblemsDock::ProblemsDock() {
	singleton = this;

	set_name(TTRC("Problems"));
	set_icon_name("StatusWarning");
	set_dock_shortcut(ED_SHORTCUT_AND_COMMAND("bottom_panels/toggle_problems_bottom_panel", TTRC("Toggle Problems Dock")));
	set_default_slot(EditorDock::DOCK_SLOT_BOTTOM);
	set_available_layouts(EditorDock::DOCK_LAYOUT_HORIZONTAL | EditorDock::DOCK_LAYOUT_FLOATING);

	VBoxContainer *vb = memnew(VBoxContainer);
	vb->set_custom_minimum_size(Size2(0, 90 * EDSCALE));
	add_child(vb);

	HBoxContainer *top = memnew(HBoxContainer);
	vb->add_child(top);

	rescan_button = memnew(Button);
	rescan_button->set_theme_type_variation("FlatButton");
	rescan_button->set_tooltip_text(TTRC("Check the whole project again."));
	rescan_button->set_accessibility_name(TTRC("Rescan"));
	rescan_button->connect(SceneStringName(pressed), callable_mp(this, &ProblemsDock::_rescan_pressed));
	top->add_child(rescan_button);

	status_label = memnew(Label);
	status_label->set_h_size_flags(SIZE_EXPAND_FILL);
	status_label->set_focus_mode(FOCUS_ACCESSIBILITY);
	top->add_child(status_label);

	search_box = memnew(LineEdit);
	search_box->set_custom_minimum_size(Size2(200 * EDSCALE, 0));
	search_box->set_placeholder(TTRC("Filter Problems"));
	search_box->set_accessibility_name(TTRC("Filter Problems"));
	search_box->set_clear_button_enabled(true);
	search_box->connect(SceneStringName(text_changed), callable_mp(this, &ProblemsDock::_filter_changed));
	top->add_child(search_box);

	errors_button = memnew(Button);
	errors_button->set_toggle_mode(true);
	errors_button->set_pressed(true);
	errors_button->set_text("0");
	errors_button->set_theme_type_variation("EditorLogFilterButton");
	errors_button->set_tooltip_text(TTRC("Toggle visibility of errors."));
	errors_button->set_accessibility_name(TTRC("Errors"));
	errors_button->connect(SceneStringName(toggled), callable_mp(this, &ProblemsDock::_filter_toggled));
	top->add_child(errors_button);

	warnings_button = memnew(Button);
	warnings_button->set_toggle_mode(true);
	warnings_button->set_pressed(true);
	warnings_button->set_text("0");
	warnings_button->set_theme_type_variation("EditorLogFilterButton");
	warnings_button->set_tooltip_text(TTRC("Toggle visibility of warnings."));
	warnings_button->set_accessibility_name(TTRC("Warnings"));
	warnings_button->connect(SceneStringName(toggled), callable_mp(this, &ProblemsDock::_filter_toggled));
	top->add_child(warnings_button);

	tree = memnew(Tree);
	tree->set_v_size_flags(SIZE_EXPAND_FILL);
	tree->set_hide_root(true);
	tree->set_columns(2);
	tree->set_column_expand(0, true);
	tree->set_column_expand_ratio(0, 3);
	tree->set_column_expand(1, true);
	tree->set_column_clip_content(1, true);
	tree->set_select_mode(Tree::SELECT_ROW);
	tree->set_accessibility_name(TTRC("Problems"));
	tree->connect("item_mouse_selected", callable_mp(this, &ProblemsDock::_item_mouse_selected));
	tree->connect("item_activated", callable_mp(this, &ProblemsDock::_item_activated));
	vb->add_child(tree);

	edited_scene_timer = memnew(Timer);
	edited_scene_timer->set_wait_time(0.5);
	edited_scene_timer->set_one_shot(true);
	edited_scene_timer->connect("timeout", callable_mp(this, &ProblemsDock::_recheck_edited_scene));
	add_child(edited_scene_timer);

	status_label->set_text(TTRC("Waiting for the project to load..."));
}

ProblemsDock::~ProblemsDock() {
	singleton = nullptr;
}
