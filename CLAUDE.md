# Working in this fork

- This fork mirrors `godotengine/godot` master. `master` must stay identical
  to upstream; do all work on feature branches. To resync:
  `git fetch upstream master && git branch -f master upstream/master && git push origin master`.
- The plan for scripting/GDScript work lives in `SCRIPTING_ROADMAP.md`. Read it
  before starting a scripting task and update it when a decision changes.
- GDScript tests: build with `scons platform=linuxbsd target=editor tests=yes`
  and run `bin/godot.* --test --test-case="*GDScript*"`. Script-level tests
  live in `modules/gdscript/tests/scripts/`.
