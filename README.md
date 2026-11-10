# PonchoWriter

A dead-simple Markdown writing app built with Qt Quick and C++ that automatically follows system dark/light mode.

See [the PonchoWriter rename, upgrade and compatibility guide](docs/ponchowriter-identity.md).

Created and maintained by **Alfonso Alvarez**: [ponchoalv/PonchoWriter](https://github.com/ponchoalv/PonchoWriter). Application contributions use the [MIT license](LICENSE); required upstream and dependency attribution is in [COPYRIGHT.md](COPYRIGHT.md). See the
[writing and shortcut guide](docs/writing-and-shortcuts.md),
[native preview design](docs/live-preview-design.md),
[example document](docs/preview-example.md), and
[test guide](docs/testing.md),
[language highlighting guide](docs/syntax-highlighting.md),
[desktop spelling guide](docs/spellchecking.md),
[file/window routing guide](docs/opening-files-and-windows.md), and
[Windows, macOS and Linux installation guide](docs/install.md).

## Features

- Native Edit/Read toggle, editable table grids with a Markdown escape hatch, local-image previews and caret-aware heading markers. Markdown stays the source of truth with one undo history.
- [Local horizontal scrolling](docs/tables-and-horizontal-overflow.md) for code and tables, with virtualized Edit cells and native selectable Read tables.
- Clipboard screenshot paste, durable recovery-safe staging, `img/` materialization and collision-safe attachment handling across Save As.
- Theme-aware language-tagged code-fence highlighting in Edit and Read, using standalone KSyntaxHighlighting rather than Kate.
- Optional constrained Mermaid rendering through an already-installed external `mmdc`; no embedded WebEngine or automatic runtime downloads.
- macOS F1 key labels, nonconflicting Replace/fullscreen shortcuts and a bundled app icon.
- Optional image name/folder and Locate actions in the image context menu; current-document changes are undoable.
- Minimal Edit-only spelling underlines and suggestions using Sonnet Core with native macOS/Windows dictionaries or system Hunspell on Linux; no autocorrect or dictionary downloads.
- Single-process multiwindow routing: focus an already-open file, open other files separately, and protect owned Save As targets. Native macOS file-open events and cross-platform local IPC share the same router.
- Persistent incremental native Edit projection, resumable metadata/object patches and viewport-aware decorations/glyph rendering; [10,000-line measurement and limits](docs/incremental-native-editing.md).
- Retained Edit/Read documents for fast return to editing, a viewport-safe caret, conservative bracket/quote pairs and [inline/fenced code shortcuts](docs/mode-transitions-and-code-shortcuts.md).
- [Blockquote live preview](docs/blockquotes.md): reveal `>` on click, justified quote prose and preserved quoted line breaks in Read.
- Edit Cmd/Ctrl-click or context **Open Link**; Read retains single-click links.
- Lightweight selection word counts, per-file position memory, cross-platform Reveal, and selection-aware HTML copying. See the [writing guide](docs/writing-and-shortcuts.md); code-block overflow remains a [future TODO](docs/TODO.md).

<img width="128" height="128" alt="PonchoWriter journal icon" src="pkgbuild/ponchowriter.svg" />

## Install

Build PonchoWriter from this repository; see the [platform installation guide](docs/install.md). No prebuilt installer or distribution package is promised. Existing distribution packages for other applications are not PonchoWriter releases.

## Command line

`ponchowriter FILE ...` routes files to the running app: existing documents are focused, other files get separate windows. Without filenames, it starts an untitled window if closed or activates the running app. Cmd/Ctrl+N always creates an untitled window. On macOS use `open -a /path/to/ponchowriter.app /path/to/note.md`, without `-n` or `--args`; see [opening files](docs/opening-files-and-windows.md).
`ponchowriter --help` and `ponchowriter --version` answer in the terminal without opening a window.

## Shortcuts

On macOS, use **Cmd** instead of **Ctrl** in the list below unless noted.
The `F1` reference displays platform-specific key names, including Option on Mac.

- `Ctrl+S` saves. Unsaved documents use a native macOS dialog or the Linux XDG desktop portal picker.
- `Ctrl+Shift+S` saves as.
- `Ctrl+O` opens a Markdown file through the platform file picker.
- `Ctrl+P` opens the system print dialog.
- `Ctrl+N` opens a new PonchoWriter window.
- `Ctrl+Z`, `Ctrl+Shift+Z`, and `Ctrl+Y` handle undo and redo.
- `Super+F` toggles fullscreen on Linux; `Cmd+Ctrl+F` on macOS. `F11` is also available.
- `Ctrl+F` searches the document. Use `Enter` or `Ctrl+G` for the next match and `Shift+Enter` for the previous match.
- `Ctrl+H` opens find and replace on Linux; `Cmd+Option+F` on macOS (leaving `Cmd+H` for Hide App).
- `Ctrl+Shift+M` (`Cmd+Shift+M` on macOS) toggles Read/Edit.
- `Ctrl+B`, `Ctrl+I`, and `Ctrl+Shift+X` toggle bold, italic, and strikethrough Markdown. `Ctrl+K` inserts a link. `Ctrl+T` toggles inline code; `Ctrl+Shift+T` adds/removes a fenced block with fences on separate lines. On macOS use Cmd.
- `Ctrl+=` and `Ctrl+-` change the text size, `Ctrl+0` returns to the default.
  `Ctrl+scroll` does the same.
- `Ctrl+Shift+C` copies the rendered document as HTML, with plain text as a fallback.
- `Ctrl+?`, `Ctrl+/`, or `F1` shows the keyboard shortcut reference.
- In the unsaved-changes dialog, `Alt+C`, `Alt+D`, and `Alt+S` choose Cancel,
  Discard, and Save.

Once a document has been saved to a file, edits are auto-saved to that file a
moment after you stop typing, so `Ctrl+S` is rarely needed. Untitled drafts are
never auto-saved — they wait for an explicit save so PonchoWriter never picks a file
name for you. Unsaved drafts are recovered after an abnormal exit. PonchoWriter also
watches open files and warns before an external change can replace local work;
auto-save pauses until you reload, keep your version, or save. A recovered draft
is never auto-saved over its file — save it to keep it.

The font button in the footer picks the writing font from any installed text font,
and PonchoWriter remembers the choice. IBM Plex Mono is the default.

Text follows the desktop text size — `omarchy display text size`, or GNOME's
`text-scaling-factor` — and re-flows without a restart. The default of 12px leaves
PonchoWriter at the size it is designed around; larger and smaller sizes scale from there.
`Ctrl+=`, `Ctrl+-`, and `Ctrl+scroll` set your own size on top of that, from 50% to
300% in browser-sized steps. It is remembered for the next window you open.

## Requirements

- **Qt 6.7 or newer**: `qt6-base` and `qt6-declarative` (including Qt Quick Controls, Quick Dialogs and QtTest for tests).
- Linux: `xdg-desktop-portal` and a portal backend. macOS and Windows use native dialogs.
- A matching C++17 compiler/build kit, **CMake 3.24+**, and a native build tool for the first pinned KSyntaxHighlighting/Sonnet Core dependency build. Code highlighting needs no runtime downloads.
- Linux spelling: `pkg-config` and Hunspell development files at build time, plus installed language dictionaries at runtime.
- See [installation instructions](docs/install.md); Windows/Linux builds have not been verified on this Mac.

The IBM Plex Mono font is bundled under the SIL Open Font License 1.1; see
`fonts/OFL.txt`. The font is copyright IBM Corp.

## Build and install

For a local build, install the Qt requirements above, a C++17 compiler such as
`gcc`, `make`, and CMake 3.24+. The scripts bootstrap the pinned standalone
syntax engine and Sonnet Core/native spelling backend into `.deps/install` if absent (or run `./bin/build-dependencies`
explicitly). This downloads checksum-verified KDE source archives at build time.
The build scripts use `qmake6` (or `qmake`), and the tests use QtTest. All projects
currently use Release to match the private static library, including Windows' CRT.

```sh
./bin/build
./bin/test
./build/ponchowriter
```

On macOS, launch the app with `open build/ponchowriter.app`; the executable is
`build/ponchowriter.app/Contents/MacOS/ponchowriter`. Homebrew Qt supplies `qmake6`.
The bundle includes the original PonchoWriter journal/P icon as `assets/ponchowriter.icns` (vector source: `pkgbuild/ponchowriter.svg`); no icon converter is needed to build.

`bin/test` runs the Qt test suites with the offscreen platform, so they do not
require an active graphical session. It handles both Unix executables and macOS
app bundles. To open a file with the local Linux build:

```sh
./build/ponchowriter path/to/document.md
```

### Binary distribution status

Local development builds are supported. **No prebuilt release is being published.**
The syntax engine is MIT, but embedded language definitions have mixed additional terms. Sonnet adds LGPL obligations, including static-library relinking/source requirements; see [Sonnet notices](third_party/sonnet/README.md).
See [provenance and license/source audit status](third_party/ksyntaxhighlighting/README.md).
The historical Arch `pkgbuild/PKGBUILD` / `bin/install` packaging route is disabled
for this project until that audit and a maintained package recipe are complete. Upstream
Omarchy packages are unaffected.
