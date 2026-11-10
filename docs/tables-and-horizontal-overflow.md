# Tables and horizontal overflow

Available on `feature/table-grid-and-horizontal-overflow` (based on `master` at `23f309d`). No new runtime dependency, browser or spreadsheet engine.

## One grid in Edit and Read

Supported root-level pipe tables use the **same virtualized Qt Quick TableView component in both modes**. Read is read-only: no cell editor, row/column actions or Source button. Links and autolinks open through the same URL policy as ordinary Read links.

The square **⛶** button opens the same component in a window-sized modal, with more width **and** height. Edit remains editable there; Read remains read-only. Only one editor is active: the inline grid is suspended while expanded. Closing hands the shared scroll position back to the inline grid. Resizing does not recreate the active cell editor. A removed/replaced table closes its old modal; callbacks from the old document cannot edit an identically positioned table in a new document.

## Edit a table

Click a cell to edit its inline Markdown. Other cells show a conservative rendered preview.

- **Tab / Shift+Tab:** finish the current edit and move between cells.
- **Enter:** commit; **Escape:** explicitly discard the current cell draft. In the expanded view, finish/discard the cell before closing the view.
- **Drag across cells** to select a rectangle (one or several rows/columns). Start typing to buffer one value, then **Enter/Tab** to fill every selected cell with that value as **one canonical undo**. **Backspace/Delete** with grid focus clears the rectangle, also as one undo. IME can start the edit directly; composition is not transformed. Read has no mutation selection/editor.
- Rectangle coordinates carry a source revision. Foreign edits/undo invalidate selection; rejected/stale buffers remain recoverable drafts rather than writing into different cells. Finish or cancel the native cell editor before clearing a grid selection.
- **⋯:** align the current column **left / center / right**, insert a row below, delete a non-header row, insert a column to the right, or delete a column while retaining at least one. Alignment updates the Markdown separator (`:---`, `:---:`, `---:`) as one canonical undo operation.
- Cells grow horizontally while typing/preediting (up to a 4,096px live width), reveal the cursor and hide their rendered text while the native editor is active.
- **Source:** reveal the original unwrapped pipe Markdown with its local scrollbar. **Grid** returns to the grid, as does moving the source caret/selection outside the table. Source/actions are deliberately absent in Read.
- Selecting across an Edit table reveals source so whole-document selections, copying, deletion and undo retain Markdown semantics.

A committed cell or structural operation is one edit on the canonical Markdown undo stack. Undo/redo are intentionally not routed to the document while a cell TextField owns an unfinished edit; finish or cancel it first. In a cell, ordinary Copy/Paste operates on that cell's text. IME preedit stays native until committed/cancelled.

Pending cells are checked before Save, Save As, mode changes, printing, opening/replacing a document, search, document close and modal close. Invalid changes or unfinished composition can block closing the expanded view. There is no click-outside auto-dismiss that loses input. Rejected changes remain session drafts; another cell cannot bypass them. Use **Pending table edits** (or **⋯**) to copy recovery text or explicitly discard it. These drafts are not saved Markdown or crash-persistent storage. Foreign/structural source changes invalidate coordinates: recovery text is retained, never silently applied to another cell.

## Read copying

The grid is a cell viewport, not a native selectable QTextTable. Individual rich cell text is not drag-selectable as document prose. Whole-document Copy with no prose selection still returns canonical Markdown. A Read prose selection crossing a grid slot exports that **whole table's** text and rich table contents, not a placeholder or screenshot; Copy as HTML uses the same table reconstruction and existing theme/font/image sanitization. Table-cell HTML and image resources remain disabled, including repeated adjacent images. Unsupported tables retain their native fallback and native selection behavior.

## Local scrolling

Wide code blocks and raw table source use unwrapped native text and independent horizontal scrollbars in **both modes**; prose still wraps. Each supported table grid has attached native horizontal and, when needed, vertical scrollbars. Drag either handle all the way to either end, continue past the end, then drag back without releasing. Native TableView origin/end-extent changes are respected, including uneven column widths; wheel/caret clamping uses the actual visible-area range, not just the declared content width. Read accepts horizontal mouse/trackpad gestures. Inline grid height is bounded to 480 logical pixels; expand for a larger viewport.

Code scrolling changes presentation only: selection, offsets, syntax colors, canonical undo and IME remain native. Caret navigation reveals the active code column; manually dragging does not immediately snap back. Offsets clamp when a wider window makes them unnecessary. A tall block's scrollbar stays at its visible viewport bottom. Cmd+Left / Cmd+Right on macOS reveals either code-line end. Code Copy still omits fences/language tags. These working Edit/code controls are unchanged by the shared Read grid.

## Performance and bounds

- Both grids reuse visible delegates and lazy rich previews, not one widget per cell. A 1,500×8 fixture checks fewer than 500 delegates rather than 12,008; a 300×8 expanded fixture checks bounded delegate creation.
- Raw cell widths are cached independently of rich HTML. An edit measures changed cells, reuses unchanged widths, and only requests column layout when a measured width changes. Live width growth/layout requests are coalesced. Literal safe cells avoid Markdown-document construction; Markdown/autolinks keep the resource-free parser.
- A candidate already validated by a cell/structural operation can be reused **only after explicit canonical ACK and exact expected-source equality**. Foreign edits/undo use the full validation path. There is no optimistic second authoritative table.
- Typing on the line immediately before a table no longer reveals and re-collapses its entire source on every key. Destructive boundary edits still reveal it safely. Unchanged table headers converge without rescanning the whole table; previous-line-sensitive horizontal rules are still revisited.
- Raw Source mode reuses bounded native row-width measurements and skips unchanged overflow normalization. A one-row edit updates affected measurements; the widest offscreen row still determines the exact maximum. Font/format/topology changes invalidate safely; IME uses fresh native geometry. Caches retain at most 8,192 row records / 65,536 format ranges, at most 256 per row, with uncached fallback. They do not cache line geometry or remove document-sized source/canonical work.
- Cold sizing still scans the table. Canonical snapshots, structural parsing and commits still include table/document-sized work. This is not constant-time editing or a spreadsheet database.
- Bounds: 50,000 cells, 1,024 columns, 4 Mi UTF-16 source units / 8 MiB UTF-8, and 65,536 UTF-16 units per cell. Projection byte/object limits also apply. Ambiguous, unsupported or over-budget tables retain source/native fallback; such Read fallbacks are **not virtualized**.
- No formulas, sorting/filtering, merged-cell designer, remote cell images or arbitrary HTML execution. Clearing an unbordered outer cell adds the necessary outer pipe so the row/header shape survives. Invalid or over-budget bulk changes are rejected atomically, including the existing whole-table limits.

## Verification (2026-11-10)

macOS arm64, Qt 6.12: **369 passed, zero failures, four normal-run skips** across twelve suites. Tests cover canonical undo/ACK, width and validation caches, safe rich/literal cells, rejected/foreign drafts, raw-source transitions, exact Read-slot identity around identical tables/Mermaid/forged resources, stale document/session revocation and native image-layout completeness.

Nine native Cocoa UI bodies passed (eleven checks including setup/cleanup): rectangle drag/fill/clear, IME-first input, one undo, both scrollbar endpoints in Edit/Read, quote behavior, and Read/modal read-only controls, expanded dimensions/virtualization, Edit modal IME/resize/one-undo/rejected close, links/autolinks in both views, selected Read text/HTML containing tables, repeated-image export safety, live field growth/alignment, physical two-way dragging/wheel input and independent offsets, code navigation/hover Copy/padding/large-code stability. If Cocoa refuses OS activation, the modal cancellation fixture invokes the actual Escape shortcut handler; active native windows and offscreen use physical Escape. Both separate threaded-render bodies passed (six checks, 100 mode switches). The development app rebuilt as 0.7.2; Windows/Linux remain uncompiled/runtime-untested.

### Measured typing follow-up

One matched generated **1,361×28-cell** pair against `88a74fc`, same built-in Retina display (120 Hz, DPR 2), Qt 6.12/threaded rendering, IBM Plex Mono 20px, spelling disabled, 12 keys per area:

| Median key-to-follow-up frame submission | Prior `88a74fc` | This follow-up |
| --- | ---: | ---: |
| Prose immediately before table | 1,024.13 ms | 12.65 ms |
| Raw table Source | 16.97 ms | 16.51 ms |
| Distant prose | 7.46 ms | 12.10 ms |

The severe adjacent-table stall is reproduced and removed in this pair. Raw/distant timings show no consistent large improvement; frame cadence/noise matters. These are **submission proxies**, not display latency/FPS or statistical guarantees. Raw-source deterministic tests instead prove unchanged rows avoid repeated width measurement/normalization while matching exhaustive native widths. Whole-document work remains. A separate read-only private-input pair also reproduced the adjacent stall and verified the input unchanged; its contents/copies/reports are not archived.

Synthetic reports: [before baseline](benchmarks/table-typing-2026-11-10-before-baseline.json) / [candidate](benchmarks/table-typing-2026-11-10-before-candidate.json), [raw baseline](benchmarks/table-typing-2026-11-10-raw-baseline.json) / [candidate](benchmarks/table-typing-2026-11-10-raw-candidate.json), [distant baseline](benchmarks/table-typing-2026-11-10-distant-baseline.json) / [candidate](benchmarks/table-typing-2026-11-10-distant-candidate.json).

### Earlier measured commit improvement (2026-11-09)

One matched synthetic **1,361×28-cell** pair on macOS/Qt 6.12, IBM Plex Mono 20px, the same 144 Hz screen/DPR 1 and threaded renderer, measured 12 buffered appended keys and 12 canonical Tab commits each:

| Median follow-up frame-submission proxy | Prior `05b3531` | Shared grid + caches |
| --- | ---: | ---: |
| Buffered key | 4.43 ms | 4.35 ms |
| Tab commit | 24.87 ms | 18.13 ms |

This is about **27% lower Tab latency in this pair**, not display latency, FPS, a statistical guarantee, or evidence of a large buffered-typing improvement. Spelling was disabled. Every sample asserts typing has not changed canonical source and Tab has committed it without pending drafts. [Baseline report](benchmarks/table-grid-2026-11-09-baseline.json) · [candidate report](benchmarks/table-grid-2026-11-09-candidate.json). Only generated synthetic input is archived. A separate private-input smoke run also passed; that input was read into an isolated untitled backend, never saved back, and verified unchanged. Its contents, copies and raw reports are not committed.

Reproduce after building `tests/tests.pro`:

```sh
QSG_RENDER_LOOP=threaded PONCHOWRITER_TABLE_PERF=1 \
  PONCHOWRITER_TABLE_PERF_OUTPUT=/tmp/table-performance.json \
  build-tests/tst_ponchowriter.app/Contents/MacOS/tst_ponchowriter tableGridNativeEditingBenchmark
```

The default fixture is generated. Set `PONCHOWRITER_TABLE_PERF_AREA=cells|before|raw|distant` (default `cells`): `cells` measures 12 buffered keys and 12 canonical Tab commits; other areas measure 12 canonical keys at the named location. Optional `PONCHOWRITER_TABLE_PERF_FIXTURE` reads a private local file without opening it for save/autosave; do not archive that input, recovery artifacts or contents. Reports omit the fixture path and cell text. Optional `PONCHOWRITER_TABLE_PERF_SCREEN` enforces a named native screen. Windows/Linux executable paths differ.
