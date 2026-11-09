# Tables and horizontal overflow

Available on `feature/table-grid-and-horizontal-overflow` (based on `master` at `23f309d`). No new runtime dependency, browser or spreadsheet engine.

## Edit a table

Supported root-level pipe tables appear as native grids in **Edit**. Click a cell to edit its inline Markdown. Other cells show a conservative rendered preview.

- **Tab / Shift+Tab:** finish the current edit and move between cells.
- **Enter:** commit; **Escape:** discard the current cell draft.
- **⋯:** align the last edited column **left / center / right**, insert a row below, delete a non-header row, insert a column to the right, or delete a column while retaining at least one. Alignment updates the Markdown separator (`:---`, `:---:`, `---:`) as one canonical undo operation; there is no separate alignment shortcut.
- Cells grow horizontally while typing/preediting (up to a 4,096px live width), reveal the cursor and hide their rendered text while the native editor is active. Long menu labels have font-measured space rather than a fixed narrow popup.
- **Source:** reveal the original pipe Markdown in the same document. It does not wrap; use its local horizontal scrollbar. **Grid** returns to the grid, as does moving the source caret/selection outside the table.
- Selecting across a table reveals source so whole-document selections, copying, deletion and undo retain Markdown semantics.

A committed cell or structural operation is one edit on the canonical Markdown undo stack. Undo/redo are intentionally not routed to the document while a cell TextField owns an unfinished edit; finish or cancel it first. In a cell, ordinary Copy/Paste operates on that cell's text, not on the whole table. IME preedit stays in the native TextField until committed/cancelled.

The editor commits pending cells before Save, Save As, mode changes, printing, opening/replacing a document, search and closing. Rejected changes retain their text as session drafts; a pending edit elsewhere cannot be bypassed by successfully committing another cell. Use **Pending table edits** (or the table menu) to copy drafts for recovery or explicitly discard them. Saving/closing is blocked while unresolved drafts remain. These drafts are not additional saved Markdown and are not promised to survive a process crash. Foreign/structural source changes invalidate cell coordinates: recovery text is retained, never silently applied to a different cell.

## Local scrolling

Wide code blocks use unwrapped native text and an independent horizontal scrollbar in **both modes**. Raw table source uses the same overflow controls. Prose still wraps normally. Horizontal wheel/trackpad gestures scroll the hovered block; vertical gestures remain document scrolling outside the grid. Taller grids have their own vertical scrollbar, within a bounded table viewport.

Horizontal scrolling changes presentation only. Code selection, source offsets, syntax colors, canonical undo and IME remain native. Caret navigation reveals the active code column; manually dragging the scrollbar does not immediately snap it back. Offsets are clamped when a wider window makes them unnecessary. A tall block's scrollbar stays at the visible viewport bottom while you are inside that block; you do not have to scroll to its final line. Moving to either end of a code line (Cmd+Left / Cmd+Right on macOS) reveals that end, without changing source or disabling viewport rendering. Code Copy still omits fences/language tags.

**Read** tables remain native selectable `QTextTable` content with local horizontal scrolling. Drag the scrollbar in either direction or use horizontal wheel/trackpad gestures over the table. Scrollbar items retain their identity when native bounds change, so dragging no longer loses its mouse grab after the first movement. Read-table gesture handlers explicitly accept horizontal mouse/touchpad input; vertical gestures continue to the document. Scroll changes also invalidate native glyph positions. Edit grids and code-block behavior are unchanged by this Read-table fix. Switch to Edit to change cells.

## Scope and scaling

- Edit uses a reusable Qt Quick `TableView`: only visible cell delegates/rich previews are materialized. Model/source parsing and column-width measurement still require table-sized work; this is not constant-time editing or a spreadsheet database.
- Grid height is at most 480 logical pixels. The model accepts at most 50,000 cells, 1,024 columns, 4 Mi UTF-16 source units / 8 MiB UTF-8, and 65,536 UTF-16 units per cell. Existing projection limits also bound eligible blocks/source memory.
- Ambiguous or unsupported Markdown retains the source/native-preview fallback. Root pipe tables are not arbitrary HTML tables, nested-table editors or a complete Markdown parser. Clearing an unbordered edge cell can be rejected when it would change table structure; use Source for deliberate structural Markdown changes.
- Cell previews disable HTML and external resource loading, and omit images; original cell Markdown is retained. No formulas, sorting/filtering, merged-cell designer, remote images or arbitrary HTML execution.
- Read tables retain Qt's full native layout; they are **not virtualized**. Very large Read tables and global mode conversions can still be expensive.

## Verification (2026-11-09)

macOS arm64, Qt 6.12: **357 passed, zero failures, three normal-run skips** across twelve suites. Tests cover canonical cell/structural undo, escaped pipes and Unicode, bounded/unsafe input, stale replacement rejection, draft recovery, raw-source transitions, native no-wrap selection/IME, independent offsets and resize clamping. A 1,500-row × 8-column UI fixture checks that fewer than 500 delegates exist rather than 12,008 cell widgets.

Five native Cocoa UI bodies passed (seven checks including setup/cleanup): live cell growth, hidden underlying cell text, alignment/undo, Tab, IME, rejected drafts, virtualization, physical Cmd+Left/Right, scrollbar placement both mid-document and at document start, Read-table continuous mouse dragging both ways, horizontal wheel/touchpad-source events, independent table offsets, mode cleanup and actual glyph movement in a window capture, code Copy, padded panels and large-code scroll stability. Both separate threaded-render bodies passed (six checks), including 100 mode switches. The development app rebuilt as 0.7.2. Windows/Linux compilation and platform interaction remain unverified; this is not a signed/notarized binary release.
