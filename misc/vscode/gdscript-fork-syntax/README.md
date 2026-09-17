# GDScript fork syntax

A VS Code *injection grammar* that teaches the official
[godot-tools](https://marketplace.visualstudio.com/items?itemName=geequlim.godot-tools)
extension how to colour the syntax this fork adds. It is not a fork of that
extension and does not replace it — it layers on top, so godot-tools keeps
updating normally.

Nothing else needs a plugin. Completion, hovers, go-to-definition, signature
help, diagnostics and the debugger all come over LSP/DAP from this fork's own
editor binary, so they already understand structs, traits and generics. Only
the TextMate grammar, which is bundled inside the extension, is static.

## What it adds

| Written                        | Without this                                | With this                         |
| ------------------------------ | ------------------------------------------- | --------------------------------- |
| `struct Hit:`                   | `struct` is a plain identifier              | keyword + type name               |
| `uses Damageable`               | `uses` is a plain identifier                | keyword                           |
| `class Pool[T]:`                | `T` reads as a constant                     | type parameter                    |
| `class Pool[T: Named]:`         | the bound reads as a constant               | type parameter + bound            |
| `func pick[T](...)`             | the function name is not highlighted at all | function name + type parameter    |
| `items.map(item => item.name)`  | `=>` splits into `=` and `>`                | one operator, `item` a parameter  |
| `var target: Node?`             | a stray `?`                                 | part of the type                  |
| `item?.name ?? "(none)"`        | `?.` and `??` are unhighlighted text        | null-safe operators               |

`trait` already highlights without this, because mainline Godot reserves the
word and the bundled grammar lists it.

## Install

There is no build step — copy or link the folder into your extensions
directory and restart VS Code.

```bash
cp -r misc/vscode/gdscript-fork-syntax ~/.vscode/extensions/
```

On Windows, from the repository root:

```bash
cmd //c mklink //d "%USERPROFILE%\.vscode\extensions\gdscript-fork-syntax" "%CD%\misc\vscode\gdscript-fork-syntax"
```

A link is worth preferring: the grammar then tracks the branch you are on.

To check it is live, open a `.gd` file, run **Developer: Inspect Editor Tokens
and Scopes** from the command palette and put the cursor on a `=>`. The scope
list should contain `keyword.operator.arrow.gdscript`.

## Editing the grammar

`syntaxes/gdscript-fork.tmLanguage.json` is injected with an `L:` selector,
meaning its patterns are tried *before* the bundled ones at the same position.
That is what lets it correct tokens godot-tools already had an opinion about.
It only wins ties: a bundled rule that starts earlier in the line still wins,
which is why the arrow rule matches `item =>` rather than just `=>`.
