# Omawrite

A dead-simple Markdown writing app built with Qt Quick and C++ that automatically follows system dark/light mode.

This is the [ponchoalv fork](https://github.com/ponchoalv/omawrite) of
[omacom/omawrite](https://github.com/omacom/omawrite). See the
[writing and shortcut guide](docs/writing-and-shortcuts.md),
[native preview design](docs/live-preview-design.md),
[example document](docs/preview-example.md), and
[test guide](docs/testing.md),
[language highlighting guide](docs/syntax-highlighting.md),
[desktop spelling guide](docs/spellchecking.md),
[file/window routing guide](docs/opening-files-and-windows.md), and
[Windows, macOS and Linux installation guide](docs/install.md).

## Fork additions

- Native Edit/Read toggle with inactive table/local-image previews and caret-aware heading markers. Markdown stays the source of truth with one undo history.
- Clipboard screenshot paste, durable recovery-safe staging, `img/` materialization and collision-safe attachment handling across Save As.
- Theme-aware language-tagged code-fence highlighting in Edit and Read, using standalone KSyntaxHighlighting rather than Kate.
- Optional constrained Mermaid rendering through an already-installed external `mmdc`; no embedded WebEngine or automatic runtime downloads.
- macOS F1 key labels, nonconflicting Replace/fullscreen shortcuts and a bundled app icon.
- Optional image name/folder and Locate actions in the image context menu; current-document changes are undoable.
- Minimal Edit-only spelling underlines and suggestions using Sonnet Core with native macOS/Windows dictionaries or system Hunspell on Linux; no autocorrect or dictionary downloads.
- Single-process multiwindow routing: focus an already-open file, open other files separately, and protect owned Save As targets. Native macOS file-open events and cross-platform local IPC share the same router.
- Lightweight selection word counts, per-file position memory, cross-platform Reveal, and selection-aware HTML copying. See the [writing guide](docs/writing-and-shortcuts.md); code-block overflow remains a [future TODO](docs/TODO.md).

The screenshots below are from upstream, before these fork additions.

<img width="2948" height="3227" alt="screenshot-2026-06-23_15-24-08" src="https://github.com/user-attachments/assets/4e930c0d-edda-4046-b444-a59eff523329" />
<img width="2948" height="3227" alt="screenshot-2026-06-23_15-23-23" src="https://github.com/user-attachments/assets/8ced7c26-961b-4ded-b263-84403001a951" />


## Install

The **upstream** app is available through the Omarchy Package Repository as `omawrite` and is included in new Omarchy installations (from Quattro forward). These fork features require building this checkout; they are not part of that upstream package.

## Command line

`omawrite FILE ...` routes files to the running app: existing documents are focused, other files get separate windows. Without filenames, it starts an untitled window if closed or activates the running app. Cmd/Ctrl+N always creates an untitled window. On macOS use `open -a /path/to/omawrite.app /path/to/note.md`, without `-n` or `--args`; see [opening files](docs/opening-files-and-windows.md).
`omawrite --help` and `omawrite --version` answer in the terminal without opening a window.

## Shortcuts

On macOS, use **Cmd** instead of **Ctrl** in the list below unless noted.
The `F1` reference displays platform-specific key names, including Option on Mac.

- `Ctrl+S` saves. Unsaved documents use a native macOS dialog or the Linux XDG desktop portal picker.
- `Ctrl+Shift+S` saves as.
- `Ctrl+O` opens a Markdown file through the platform file picker.
- `Ctrl+P` opens the system print dialog.
- `Ctrl+N` opens a new Omawrite window.
- `Ctrl+Z`, `Ctrl+Shift+Z`, and `Ctrl+Y` handle undo and redo.
- `Super+F` toggles fullscreen on Linux; `Cmd+Ctrl+F` on macOS. `F11` is also available.
- `Ctrl+F` searches the document. Use `Enter` or `Ctrl+G` for the next match and `Shift+Enter` for the previous match.
- `Ctrl+H` opens find and replace on Linux; `Cmd+Option+F` on macOS (leaving `Cmd+H` for Hide App).
- `Ctrl+Shift+M` (`Cmd+Shift+M` on macOS) toggles Read/Edit.
- `Ctrl+B`, `Ctrl+I`, and `Ctrl+Shift+X` toggle bold, italic, and strikethrough Markdown. `Ctrl+K` inserts a link.
- `Ctrl+=` and `Ctrl+-` change the text size, `Ctrl+0` returns to the default.
  `Ctrl+scroll` does the same.
- `Ctrl+Shift+C` copies the rendered document as HTML, with plain text as a fallback.
- `Ctrl+?`, `Ctrl+/`, or `F1` shows the keyboard shortcut reference.
- In the unsaved-changes dialog, `Alt+C`, `Alt+D`, and `Alt+S` choose Cancel,
  Discard, and Save.

Once a document has been saved to a file, edits are auto-saved to that file a
moment after you stop typing, so `Ctrl+S` is rarely needed. Untitled drafts are
never auto-saved — they wait for an explicit save so Omawrite never picks a file
name for you. Unsaved drafts are recovered after an abnormal exit. Omawrite also
watches open files and warns before an external change can replace local work;
auto-save pauses until you reload, keep your version, or save. A recovered draft
is never auto-saved over its file — save it to keep it.

The font button in the footer picks the writing font from any installed text font,
and Omawrite remembers the choice. IBM Plex Mono is the default.

Text follows the desktop text size — `omarchy display text size`, or GNOME's
`text-scaling-factor` — and re-flows without a restart. The default of 12px leaves
Omawrite at the size it is designed around; larger and smaller sizes scale from there.
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
./build/omawrite
```

On macOS, launch the app with `open build/omawrite.app`; the executable is
`build/omawrite.app/Contents/MacOS/omawrite`. Homebrew Qt supplies `qmake6`.
The bundle includes `assets/omawrite.icns`, generated from the upstream SVG artwork;
no icon converter is needed to build.

`bin/test` runs the Qt test suites with the offscreen platform, so they do not
require an active graphical session. It handles both Unix executables and macOS
app bundles. To open a file with the local Linux build:

```sh
./build/omawrite path/to/document.md
```

### Binary distribution status

Local development builds are supported. **No prebuilt release is being published.**
The syntax engine is MIT, but embedded language definitions have mixed additional terms. Sonnet adds LGPL obligations, including static-library relinking/source requirements; see [Sonnet notices](third_party/sonnet/README.md).
See [provenance and license/source audit status](third_party/ksyntaxhighlighting/README.md).
The historical Arch `pkgbuild/PKGBUILD` / `bin/install` packaging route is disabled
for this fork until that audit and a maintained fork recipe are complete. Upstream
Omarchy packages are unaffected.
