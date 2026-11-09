# Native live preview: design and boundaries

This fork adds visual Markdown features without changing the document format or embedding Qt WebEngine. See [the user guide](writing-and-shortcuts.md) for supported behavior and shortcuts.

## Principles

- The source Markdown document is authoritative. Saving must never serialize a rendered presentation back into Markdown.
- One source undo history. Showing or hiding a preview is not an edit.
- Edit mode reveals source in the active structural block; Read mode does not permit edits.
- Selection and cursor positions are UTF-16 source offsets. Any visual projection needs explicit source/view mapping, including backward selections and block boundaries.
- Unsupported or invalid syntax remains readable source. An incomplete diagram must not interrupt writing with a modal error.
- Avoid multiline source hiding through tiny fonts or floating pictures over the editor. Such tricks break selection, hit testing, scrolling, and accessibility.

## Persistent Edit projection

Ordinary typing edits the visible native document in place and applies the exact range to canonical Markdown. Resumable structural/list checkpoints converge from the affected region; source/view object intervals are reconciled right-to-left, without clearing unrelated prose or replacing the highlighter. Canonical source remains the sole undo/save authority. Global resets and Read's native Markdown/HTML import remain separate full-document operations. Edit and Read now retain separate resource-denying native documents; the Edit highlighter stays attached during Read. Read is prepared offscreen with layout disabled during formatting, enabled before attachment, and reused until source/style/width/resource invalidation. Mode read-only changes are guarded before document attachment. A separate IME-aware caret avoids Qt's offscreen cursor latch without disabling viewport glyph rendering. See [mode-transition details and measurement](mode-transitions-and-code-shortcuts.md).

List prefix metadata belongs to stable view block formats, not absolute source-position hashes. Code/selection styling batches layout changes. Code/list decorations are viewport-filtered; above 10,000 source characters the visible item explicitly enables public Qt Quick viewport observation because direct QTextDocument mutation bypasses TextEdit's normal large-text setter. Rich image layouts are never optimized by suspending page size to zero. Failed image placeholders retain repair state.

This reduces ordinary editing work, not every source-copy/backend/global operation. See [implementation boundaries, native measurements and reproduction](incremental-native-editing.md).

## Structural blocks

The initial target is pipe tables, standalone Markdown image paragraphs, and explicitly tagged Mermaid fences. Ordinary Markdown and inline formatting retain their existing behavior. Headings hide their markers outside the active heading; moving the caret into a heading reveals its Markdown.

Qt's native Markdown renderer understands tables, local pictures and links. It is not a browser: arbitrary CSS, JavaScript and web layouts are outside scope. General table-cell editing and graphical Mermaid node editing are also outside scope; activate the block and edit its source instead.

A rendered structural block needs a deterministic source activation position, not a claim of pixel-perfect table-cell cursor mapping. Selections touching a preview must reveal source before editing. Changes in representation must preserve a source-based scroll anchor where possible. Input-method composition must not be rebuilt underneath an unfinished composition.

## Pictures and persistence

Clipboard images use ordinary PNG attachments, referenced with standard Markdown such as:

```markdown
![Screenshot](img/screenshot-example.png)
```

An unsaved document stages attachments in persistent app-data storage, not disposable system temporary storage. Recovery must preserve enough metadata to resolve those references. Saving materializes the attachments before committing Markdown that refers to them. Failed attachment writes must not produce a newly saved broken reference.

Names must be relative and safe; never silently overwrite another image. Undo removes the reference, not the image file: redo and other references may still need it. Save As must preserve relative image resolution or fail visibly rather than silently break links.

Renaming/moving through the app can update matching references in the current document. Moving through Finder cannot be tracked reliably by an internal ID alone. A manual Locate operation is preferable to guessing. There is no global image database, vault-wide repair, automatic upload, or automatic deletion of orphaned attachments.

## Mermaid

Qt does not render Mermaid natively. An optional external Mermaid CLI can render diagrams to local images. This avoids bundling WebEngine but does **not** make Mermaid browser-free: the usual CLI uses Node/Puppeteer/Chromium externally.

A renderer should use fixed executable arguments, never shell evaluation of document text; enforce time and output bounds, cache by source and rendering options, discard stale results, and show a source fallback when unavailable. External rendering is an optional capability, not a reason for the app to download executables or remote images automatically.

## Verification priorities

1. Exact source preservation through rendering, toggles, selection, undo/redo and save.
2. Backward and cross-block selections; deletion at preview boundaries.
3. Smart paste, formatting shortcuts, search inside collapsed blocks and input-method composition.
4. Local image resolution, collision handling, Save As, save failure and crash recovery.
5. Missing Mermaid renderer, malformed syntax, stale asynchronous rendering, theme/scale changes.
6. Startup, typing latency, large-document layout and scroll stability.

Automated coverage and remaining manual checks are recorded in the user guide. Architectural goals here are not substitutes for a passing test or a statement that every edge case is supported.
