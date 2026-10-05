/**************************************************************************/
/*  problems_dock.h                                                       */
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

#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "editor/docks/editor_dock.h"

class Button;
class EditorFileSystemDirectory;
class Label;
class LineEdit;
class SceneState;
class Timer;
class Tree;
class TreeItem;

// Lists what is wrong across the whole project, kept up to date as files change: script errors and
// warnings, node configuration warnings and required properties left empty in every scene, node paths
// that point nowhere, and missing dependencies. Nothing is run; scenes are instantiated the way the
// editor opens them, and an open scene is read from the editor, unsaved edits included.
class ProblemsDock : public EditorDock {
	GDCLASS(ProblemsDock, EditorDock);

public:
	enum Severity {
		SEVERITY_ERROR,
		SEVERITY_WARNING,
	};

	struct Problem {
		Severity severity = SEVERITY_ERROR;
		String message;
		String file; // Where clicking takes you.
		int line = 0; // In a script, one-based; 0 if none.
		int column = 0;
		NodePath node; // In a scene, from its root; empty if none.
		String node_label; // The same, as shown: starting with the root's name.
		StringName property; // Shown in the inspector when the node or resource is opened.
	};

private:
	static ProblemsDock *singleton;

	struct FileResult {
		uint64_t modified_time = 0;
		Vector<Problem> problems;
	};

	HashMap<String, FileResult> results;
	Vector<String> queue;
	HashSet<String> queued;
	int scan_total = 0; // Files queued since the queue was last empty, for the progress text.
	bool initial_scan_done = false;
	uint64_t last_tree_update_msec = 0;
	bool tree_dirty = false;

	int error_count = 0;
	int warning_count = 0;

	Vector<Problem> shown; // Problems in the tree, indexed by item metadata.

	Button *rescan_button = nullptr;
	Label *status_label = nullptr;
	LineEdit *search_box = nullptr;
	Button *errors_button = nullptr;
	Button *warnings_button = nullptr;
	Tree *tree = nullptr;
	Timer *edited_scene_timer = nullptr;

	static bool _is_scene_path(const String &p_path);
	static bool _is_resource_path(const String &p_path);
	static String _node_label(Node *p_root, Node *p_node);
	static bool _is_script_type(const StringName &p_type);
	void _collect_files(EditorFileSystemDirectory *p_dir, HashMap<String, uint64_t> &r_files, HashMap<String, Vector<String>> &r_dependents) const;
	void _enqueue(const String &p_path);
	void _process_queue();
	void _check_file(const String &p_path);

	void _check_script(const String &p_path, Vector<Problem> &r_problems) const;
	void _check_dependencies(const String &p_path, Vector<Problem> &r_problems) const;
	void _check_scene(const String &p_path, Vector<Problem> &r_problems) const;
	void _check_scene_nodes(const String &p_path, Node *p_root, Node *p_node, bool p_root_is_checked, Vector<Problem> &r_problems) const;
	void _check_scene_node_paths(const String &p_path, Node *p_root, const Ref<SceneState> &p_state, Vector<Problem> &r_problems) const;
	void _check_resource(const String &p_path, Vector<Problem> &r_problems) const;

	void _filesystem_changed();
	void _edited_scene_changed();
	void _recheck_edited_scene();
	void _rescan_pressed();

	void _update_tree();
	void _update_counts();
	void _filter_changed(const String &p_text = String());
	void _filter_toggled(bool p_pressed);
	void _item_mouse_selected(const Vector2 &p_pos, MouseButton p_button);
	void _item_activated();
	void _navigate(const Problem &p_problem);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	static ProblemsDock *get_singleton() { return singleton; }

	// Names of the properties of `p_object` marked PROPERTY_USAGE_REQUIRED that are still empty.
	static Vector<StringName> get_missing_required_properties(Object *p_object);

	void rescan();
	int get_error_count() const { return error_count; }
	int get_warning_count() const { return warning_count; }
	bool is_scanning() const { return !queue.is_empty() || !initial_scan_done; }
	Vector<Problem> get_problems() const;

	ProblemsDock();
	~ProblemsDock();
};
