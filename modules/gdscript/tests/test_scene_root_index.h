/**************************************************************************/
/*  test_scene_root_index.h                                               */
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

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/object/script_language.h"
#include "core/os/os.h"
#include "editor/file_system/editor_scene_root_index.h"
#include "tests/test_macros.h"
#include "tests/test_utils.h"

// The index reads what GDScript files extend, so its tests live with GDScript's.
namespace TestSceneRootIndex {

using Match = EditorSceneRootIndex::Match;

static String dir;

static void write(const String &p_name, const String &p_text) {
	Ref<FileAccess> f = FileAccess::open(dir.path_join(p_name), FileAccess::WRITE);
	REQUIRE(f.is_valid());
	f->store_string(p_text);
}

static String at(const String &p_name) {
	return dir.path_join(p_name);
}

static String scene_with_script(const String &p_type, const String &p_script) {
	return vformat("[gd_scene format=3]\n\n[ext_resource type=\"Script\" path=\"%s\" id=\"1\"]\n\n[node name=\"Root\" type=\"%s\"]\nscript = ExtResource(\"1\")\n", p_script, p_type);
}

// A clock far ahead of the files, so everything read counts as settled and is kept.
static uint64_t later() {
	return (uint64_t)OS::get_singleton()->get_unix_time() + 1000;
}
// A clock no later than the files, so nothing read counts as settled.
static uint64_t same_second() {
	return 0;
}

// Files written in this second and rewritten in the next, so the change shows in the time.
static void wait_for_next_second() {
	OS::get_singleton()->delay_usec(1100000);
}

struct Fixture {
	Fixture() {
		dir = TestUtils::get_temp_path("scene_root_index");
		Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		if (da->dir_exists(dir)) {
			da->change_dir(dir);
			da->erase_contents_recursive();
		}
		da->make_dir_recursive(dir);

		write("enemy.gd", "class_name TestIndexEnemy\nextends Node2D\n");
		write("boss.gd", "extends \"enemy.gd\"\n");
		write("player.gd", "extends Node2D\n");
		write("sprite.tscn", "[gd_scene format=3]\n\n[node name=\"Root\" type=\"Sprite2D\"]\n");
		write("boss.tscn", scene_with_script("Node2D", "boss.gd"));
		write("player.tscn", scene_with_script("Node2D", "player.gd"));
		write("boss_inherited.tscn", "[gd_scene format=3]\n\n[ext_resource type=\"PackedScene\" path=\"boss.tscn\" id=\"1\"]\n\n[node name=\"Boss2\" instance=ExtResource(\"1\")]\n");
		write("builtin.tscn", "[gd_scene format=3]\n\n[sub_resource type=\"GDScript\" id=\"GDScript_1\"]\nscript/source = \"extends Node2D\\n\"\n\n[node name=\"Root\" type=\"Node2D\"]\nscript = SubResource(\"GDScript_1\")\n");
		ScriptServer::add_global_class("TestIndexEnemy", "Node2D", "GDScript", at("enemy.gd"), false, false);
		EditorSceneRootIndex::unix_time_override = later;
	}

	~Fixture() {
		ScriptServer::remove_global_class("TestIndexEnemy");
		ScriptServer::remove_global_class("TestIndexGhost");
		EditorSceneRootIndex::unix_time_override = nullptr;
	}
};

TEST_SUITE("[Modules][GDScript][SceneRootIndex]") {
TEST_CASE_FIXTURE(Fixture, "[SceneRootIndex] Engine class roots") {
	EditorSceneRootIndex index;
	CHECK(index.match(at("sprite.tscn"), "Sprite2D") == Match::MATCH);
	CHECK(index.match(at("sprite.tscn"), "Node2D") == Match::MATCH);
	CHECK(index.match(at("sprite.tscn"), "Control") == Match::NO_MATCH);
	CHECK(index.match(at("boss.tscn"), "Sprite2D") == Match::NO_MATCH);
}

TEST_CASE_FIXTURE(Fixture, "[SceneRootIndex] Script roots, followed one file at a time") {
	EditorSceneRootIndex index;
	// boss.gd extends enemy.gd by path, which declares the class.
	CHECK(index.match(at("boss.tscn"), "TestIndexEnemy") == Match::MATCH);
	CHECK(index.match(at("boss.tscn"), at("enemy.gd")) == Match::MATCH);
	CHECK(index.match(at("boss.tscn"), at("boss.gd")) == Match::MATCH);
	CHECK(index.match(at("player.tscn"), "TestIndexEnemy") == Match::NO_MATCH);
	CHECK(index.match(at("sprite.tscn"), "TestIndexEnemy") == Match::NO_MATCH);
	// An inherited scene's root comes from its base scene.
	CHECK(index.match(at("boss_inherited.tscn"), "TestIndexEnemy") == Match::MATCH);
	CHECK(index.match(at("boss_inherited.tscn"), "Node2D") == Match::MATCH);
}

TEST_CASE_FIXTURE(Fixture, "[SceneRootIndex] What cannot be read from files is unknown, not a mismatch") {
	EditorSceneRootIndex index;
	// A built-in script is inside the scene, not in a file to follow.
	CHECK(index.match(at("builtin.tscn"), "TestIndexEnemy") == Match::UNKNOWN);
	// Its engine class is still known.
	CHECK(index.match(at("builtin.tscn"), "Node2D") == Match::MATCH);
	// A class the editor has not registered yet.
	CHECK(index.match(at("boss.tscn"), "TestIndexNeverDeclared") == Match::UNKNOWN);
	// The editor's class list is behind: it names a file that no longer declares the class.
	ScriptServer::add_global_class("TestIndexGhost", "Node2D", "GDScript", at("player.gd"), false, false);
	CHECK(index.match(at("player.tscn"), "TestIndexGhost") == Match::UNKNOWN);
	// A scene that is not there.
	CHECK(index.match(at("missing.tscn"), "Node2D") == Match::UNKNOWN);
}

TEST_CASE_FIXTURE(Fixture, "[SceneRootIndex] Facts are only kept once their file has settled") {
	EditorSceneRootIndex index;
	// Read in the second the files were written: a second write in that second would not change
	// their time, so nothing is kept.
	EditorSceneRootIndex::unix_time_override = same_second;
	CHECK(index.match(at("boss.tscn"), "TestIndexEnemy") == Match::MATCH);
	CHECK_FALSE(index.has_cached(at("boss.tscn")));
	CHECK_FALSE(index.has_cached(at("boss.gd")));

	EditorSceneRootIndex::unix_time_override = later;
	CHECK(index.match(at("boss.tscn"), "TestIndexEnemy") == Match::MATCH);
	CHECK(index.has_cached(at("boss.tscn")));
	CHECK(index.has_cached(at("boss.gd")));
	CHECK(index.has_cached(at("enemy.gd")));
}

TEST_CASE_FIXTURE(Fixture, "[SceneRootIndex] Files changed behind its back are seen at once") {
	EditorSceneRootIndex index;
	CHECK(index.match(at("boss.tscn"), "TestIndexEnemy") == Match::MATCH);
	CHECK(index.match(at("player.tscn"), "TestIndexEnemy") == Match::NO_MATCH);
	CHECK(index.match(at("sprite.tscn"), "Sprite2D") == Match::MATCH);
	REQUIRE(index.has_cached(at("boss.gd")));

	// Rewritten without telling anyone, as an external tool would.
	wait_for_next_second();
	write("boss.gd", "extends Node2D\n"); // No longer an enemy, one file up the chain.
	write("player.gd", "extends \"enemy.gd\"\n"); // Now one.
	write("sprite.tscn", "[gd_scene format=3]\n\n[node name=\"Root\" type=\"Control\"]\n");

	CHECK(index.match(at("boss.tscn"), "TestIndexEnemy") == Match::NO_MATCH);
	CHECK(index.match(at("boss_inherited.tscn"), "TestIndexEnemy") == Match::NO_MATCH);
	CHECK(index.match(at("player.tscn"), "TestIndexEnemy") == Match::MATCH);
	CHECK(index.match(at("sprite.tscn"), "Sprite2D") == Match::NO_MATCH);

	// A file that is deleted is forgotten.
	DirAccess::remove_absolute(at("sprite.tscn"));
	CHECK(index.match(at("sprite.tscn"), "Control") == Match::UNKNOWN);
	CHECK_FALSE(index.has_cached(at("sprite.tscn")));
}

TEST_CASE_FIXTURE(Fixture, "[SceneRootIndex] Saved facts are checked against the disk too") {
	const String cache = at("index_cache");
	{
		EditorSceneRootIndex index;
		index.load(cache);
		CHECK(index.match(at("boss.tscn"), "TestIndexEnemy") == Match::MATCH);
		index.save();
	}
	{
		EditorSceneRootIndex index;
		index.load(cache);
		CHECK(index.has_cached(at("boss.gd")));
		CHECK(index.match(at("boss.tscn"), "TestIndexEnemy") == Match::MATCH);
	}

	// Changed while the editor was closed.
	wait_for_next_second();
	write("boss.gd", "extends Node2D\n");
	{
		EditorSceneRootIndex index;
		index.load(cache);
		CHECK(index.match(at("boss.tscn"), "TestIndexEnemy") == Match::NO_MATCH);
	}
}
} // TEST_SUITE

} // namespace TestSceneRootIndex

#endif // TOOLS_ENABLED
