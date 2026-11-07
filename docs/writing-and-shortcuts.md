# Writing, visual blocks, attachments and shortcuts

This is the user guide for [ponchoalv/omawrite](https://github.com/ponchoalv/omawrite), a fork of [omacom/omawrite](https://github.com/omacom/omawrite). Markdown remains the file format; this is not a rich-text document format or an Obsidian plugin.

## macOS key names

Qt normally maps a source-code `Ctrl` modifier to **Command** on macOS and `Meta` to the physical **Control** key. The F1 reference now formats labels for the running platform. It also calls Alt **Option** on a Mac.

**Find & Replace uses Cmd+Option+F on macOS**, not Cmd+H, which macOS reserves for Hide App. Fullscreen uses the conventional **Cmd+Ctrl+F** on macOS. Linux keeps Ctrl+H and Super+F. F11 remains an alternate fullscreen binding; your keyboard/system may intercept function keys or require Fn.

| Action | macOS | Linux / source convention |
| --- | --- | --- |
| Open | Cmd+O | Ctrl+O |
| Save | Cmd+S | Ctrl+S |
| Save As | Cmd+Shift+S | Ctrl+Shift+S |
| New window | Cmd+N | Ctrl+N |
| Print rendered document | Cmd+P | Ctrl+P |
| Undo | Cmd+Z | Ctrl+Z |
| Redo | Cmd+Shift+Z or Cmd+Y | Ctrl+Shift+Z or Ctrl+Y |
| Fullscreen | Cmd+Ctrl+F or F11 | Super+F or F11 |
| Find | Cmd+F | Ctrl+F |
| Find & Replace | Cmd+Option+F | Ctrl+H |
| Next search match | Cmd+G, or Return in Find | Ctrl+G, or Enter in Find |
| Previous search match | Shift+Return in Find | Shift+Enter in Find |
| Close search | Escape in search panel | Escape in search panel |
| Bold | Cmd+B | Ctrl+B |
| Italic | Cmd+I | Ctrl+I |
| Strikethrough | Cmd+Shift+X | Ctrl+Shift+X |
| Insert Markdown link | Cmd+K | Ctrl+K |
| Copy whole rendered document as HTML | Cmd+Shift+C | Ctrl+Shift+C |
| Increase text size | Cmd+= or Cmd++ | Ctrl+= or Ctrl++ |
| Decrease text size | Cmd+- | Ctrl+- |
| Reset text size | Cmd+0 | Ctrl+0 |
| Change text size with wheel/trackpad | Cmd+scroll | Ctrl+scroll |
| Shortcut reference | Cmd+?, Cmd+/, or F1 | Ctrl+?, Ctrl+/, or F1 |
| Toggle Read/Edit | Cmd+Shift+M | Ctrl+Shift+M |
| Unsaved dialog: Save | Option+S | Alt+S |
| Unsaved dialog: Discard | Option+D | Alt+D |
| Unsaved dialog: Cancel | Option+C or Escape | Alt+C or Escape |

Usual cut/copy/paste/select-all keys come from Qt's text editor. There is no user-configurable keymap or command palette. Labels and automated tests do not establish that every combination is available on every keyboard or desktop configuration.

## Existing writing behavior

- Bold, italic, strike and link markers are caret-aware: source becomes visible when editing the relevant span.
- Inactive headings hide their `#` markers; moving into the heading reveals the source. A selection crossing a heading reveals its markers too.
- Return normally inserts two newlines (a paragraph break); Shift+Return inserts one. Blank lines and fenced code use one newline.
- Return continues lists and blockquotes; Return on an empty marker exits the list/quote.
- Tab / Shift+Tab indent eligible list items by up to four columns, not arbitrary code blocks.
- Selecting words and pasting a URL makes a Markdown link. Other text paste stays plain text.
- Find is literal and case-insensitive, with the existing accent-folding rules. Enter in the replacement field replaces the current match. **All** replaces all matches as one undo command.
- Named documents auto-save after a short pause. Untitled documents wait for an explicit save. Recovery drafts are not silently saved over files. External-change warnings pause auto-save until resolved.
- Font selection is in the footer. Font zoom is remembered, from 50% through 300%.

## Edit and Read

Use the upper-right **book/pencil** button or Cmd+Shift+M (Ctrl+Shift+M on Linux).

**Edit** keeps ordinary Markdown text editable. Supported inactive pipe tables and standalone local image paragraphs become native visual blocks. Click a block to reveal its source; move away to render it again. Selections crossing a block reveal source rather than copying an opaque picture. Headings and existing inline emphasis retain caret-aware markers.

**Read** renders supported Markdown without an editing caret. Links are clickable. This uses Qt's Markdown/HTML subset, not a web browser: arbitrary web CSS/layout is not supported. Remote images are deliberately not fetched. Unsupported or unavailable visual blocks remain usable source; rendering is never a Markdown conversion on save. One canonical source document owns saving, search, formatting and undo.

Try [preview-example.md](preview-example.md) for tables, images, Mermaid and fallback cases.

## Paste screenshots without saving them manually

On macOS, **Cmd+Ctrl+Shift+4** copies a selected screenshot region to the clipboard (an OS shortcut, not OmaWrite's). Then paste with **Cmd+V** into the document.

The app stages a PNG in persistent application data and inserts ordinary Markdown:

```markdown
![Screenshot](img/screenshot-unique-id.png)
```

The next successful save/auto-save copies it to `img/` beside the document. An untitled note keeps the staged image available to recovery; there is no upload or dependency on a disposable temporary file. A failed staging write inserts no image reference.

- Undo removes the reference, not the image file. Redo and references in other documents remain safe.
- Save As to a different folder copies supported local image dependencies and retained undo aliases before saving Markdown.
- Differing destination collisions, missing required images, unsafe traversal paths, or attachment/document destination collisions fail visibly instead of overwriting data. Byte-identical existing images can be reused.
- New names must be portable relative paths. Existing relative references with spaces, Unicode and percent encoding are supported conservatively. Absolute paths stay absolute rather than being copied.
- There is no global attachment database, Finder-move tracking, automatic vault-wide repair or file garbage collection. Old files are intentionally retained. Multi-file saves are not a transaction: a failed save can leave unused image copies, but should not replace a differing image.
- Externally changed image pixels are not watched automatically; reopen the document if its preview is stale.

### Optional image names and missing-image repair

In **Edit**, right-click a standalone image (or its source paragraph) for:

- **Name / folder…** — choose a portable relative destination such as `img/architecture.png`. The app copies the image and updates matching references in the current document as one undo operation; originals are retained for undo and other notes.
- **Locate image…** — select the actual local file for a missing reference. It is staged under a safe relative name and the current document is updated; no vault-wide search/repair is attempted.

The keyboard Menu key or **Shift+F10** opens the same menu with the caret in an image paragraph. This is optional after paste; no naming dialog interrupts normal screenshot paste. Inline images inside text/tables are outside this small context menu's scope.

## Optional Mermaid

Mermaid is not built into Qt. If **`mmdc`** (Mermaid CLI) is already installed and available on the app's **PATH**, the fork can render a constrained set of local diagrams asynchronously. Check from the terminal:

```sh
command -v mmdc
./build/omawrite.app/Contents/MacOS/omawrite docs/preview-example.md
```

Launching from Finder may give a different PATH than your shell. Restart the app after changing renderer availability. OmaWrite does **not** install or download the renderer. Mermaid CLI normally needs its own Node/Puppeteer/Chromium setup: no embedded WebEngine does not mean no external browser runtime.

Rendering is currently enabled only on macOS/Linux; Windows keeps Mermaid source. Document-provided configuration, HTML labels, click actions, styling directives, encoded escapes and remote-resource syntax are rejected by the constrained renderer. This conservative syntax filter is not an OS/browser sandbox; do not treat an external renderer as a general untrusted-code execution boundary. Missing CLI, invalid/incomplete/unsupported syntax and render failures leave source usable. There is no drag-node editor, whiteboard or arbitrary Mermaid graphical round-trip support.

## Build on macOS

Install **Qt 6.7 or newer** and a C++17 compiler; this development checkout used Homebrew Qt 6.12.0 and Apple's command-line tools. See the [step-by-step installation guide](install.md) for Windows, macOS and Linux.

```sh
./bin/build
./bin/test
open build/omawrite.app
```

The macOS bundle includes a multi-resolution `.icns` icon based on the upstream artwork. No icon-generation tool is required for a normal build.

Open a file directly:

```sh
./build/omawrite.app/Contents/MacOS/omawrite /absolute/path/notes.md
```

On Linux, the executable is `./build/omawrite`. `--help` and `--version` work without opening a window. Native macOS file dialogs replace the Linux desktop portal; do not install a portal on macOS just for this app.

## Design reference

See [Native live preview: design and boundaries](live-preview-design.md) for source/presentation separation, attachment safety and Mermaid dependency tradeoffs.
