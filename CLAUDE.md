# Working in this fork

- This fork mirrors `godotengine/godot` master. `master` must stay identical
  to upstream; do all work on feature branches. To resync:
  `git fetch upstream master && git branch -f master upstream/master && git push origin master`.
- The plan for scripting/GDScript work lives in `SCRIPTING_ROADMAP.md`. Read it
  before starting a scripting task and update it when a decision changes.
- `IMPROVEMENTS.md` is the user-facing summary of what this fork adds over
  mainline, with runnable examples. Update it whenever a user-visible feature
  lands or changes, and keep its roadmap section in sync with
  `SCRIPTING_ROADMAP.md`. Verify every example in it actually runs before
  committing; one wrong claim was caught that way already.
- GDScript tests: build with `scons platform=linuxbsd target=editor tests=yes`
  and run `bin/godot.* --test --test-suite="*GDScript*"`. Use `--test-suite`,
  not `--test-case`: `--test-case="*GDScript*"` silently skips the completion
  and LSP suites, which are separate suites, so editor-side regressions pass
  unnoticed. Script-level tests live in `modules/gdscript/tests/scripts/`, and
  `--gdscript-generate-tests modules/gdscript/tests/scripts` regenerates the
  `.out` expectations (always read the diff: the runner forces every warning
  to "warn", so a new warning shows up across unrelated tests).
  `completion/get_node/local/local.gd` fails before any of our changes.
