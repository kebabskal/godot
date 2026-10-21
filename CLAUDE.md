# Working in this fork

- `master` on the fork carries this fork's changes on top of
  `godotengine/godot` master. To pull in upstream, fetch master from the
  godotengine remote, merge it into `master` (merge, not rebase: the fork has
  dozens of commits that would each need their conflicts resolved) and push it
  to the fork. Remote names differ between clones: in the main Windows
  checkout `origin` is godotengine and `kebabskal` is the fork, so there it is
  `git fetch origin master`, then `git merge origin/master` on `master`, then
  `git push kebabskal master`. Feature branches merge `master` the same way.
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
  `--test gdscript-generate-tests modules/gdscript/tests/scripts` regenerates the
  `.out` expectations (always read the diff: the runner forces every warning
  to "warn", so a new warning shows up across unrelated tests). The old
  `--gdscript-generate-tests` flag (renamed upstream, Oct 2026) still exits 0
  and only prints an error, leaving every `.out` untouched, so an empty diff
  after it proves nothing.
  `completion/get_node/local/local.gd` fails before any of our changes.
