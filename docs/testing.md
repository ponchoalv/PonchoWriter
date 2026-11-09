# Testing the native-preview fork

## Automated suites

Run `./bin/test` with Qt 6.7+, a C++17 compiler and CMake 3.24+ for the first syntax/spelling-dependency bootstrap. The runner builds separate QtTest targets, uses the offscreen Qt platform, and recognizes macOS bundles and Unix binaries.

| Suite | Purpose |
| --- | --- |
| `tests/tests.pro` | Existing backend/editor/QML regressions, platform-correct Replace dispatch, visible-editor typing, screenshot paste, image naming menu/left and right pointer hit-tests, source undo/redo, Read/Edit and attachment save integration |
| `tests/shortcutlabels.pro` | Platform-formatted F1 key labels, including Cmd/Option versus physical Ctrl |
| `tests/headings/headings.pro` | ATX heading markers, caret/selection activation, code fences, unchanged source, efficient large-selection updates |
| `tests/attachments/attachments.pro` | Persistent staging/recovery, first save/auto-save/Save As, collision and path safety, current-document rename/locate and undo, encoded/exact references and absolute paths |
| `tests/previewblocks/previewblocks.pro` | Conservative structural parsing and native rasterization/fallback |
| `tests/livepreview/livepreview.pro` | UTF-16 mappings, source/view separation, native input/selection/IME, object boundaries, source preservation, local/remote resources, cumulative raster budgets/stale-resource release and read links |
| `tests/mermaid/mermaid.pro` | Optional-renderer fallback, constrained syntax and asynchronous process/cache behavior using a fake local CLI |
| `tests/codehighlighting/codehighlighting.pro` | Language/alias resolution, plain fallback, multiline state and per-fence reset, incremental versus full rehighlight, language-only edits, theme/font/search/source/undo preservation |
| `tests/spellchecking/spellchecking.pro` | Spelling prose classification, cached diagnostics, stale-result/menu-token validation and dictionary/controller behavior |
| `tests/spellchecking/integration.pro` | Spelling projection/object mapping, asynchronous styling, source/undo preservation and IME |
| `tests/spellchecking/main.pro` | Backend and Main.qml spelling controls/actions using an isolated deterministic dictionary |
| `tests/instancebroker/instancebroker.pro` | Real subprocess forwarding, framing/limits, concurrent startup, stale/unresponsive owners, private runtime/fallback directories, queued slow opening and callback lifetime |

The main QML suite registers the same `Omawrite.LivePreview` type as the application. It uses a unique application-data identity per run, so a failed test's recovery snapshot cannot be restored by the next run. Tests must send physical text input to **`previewEditor`**, not the hidden canonical **`sourceEditor`**. When a test changes source directly, allow the queued projection update before sending input.

The relative-file-path regression canonicalizes an existing parent directory, not a nonexistent document. This accounts for macOS `/var` and `/private/var` aliases without weakening the requirement that the document URL be absolute.

## Verified development run

On 2026-11-09, macOS arm64 with Homebrew Qt 6.12.0 and Apple Clang: **344 passed, 0 failed, 2 native-render tests skipped** in the offscreen run across twelve suites (125 / 16 / 23 / 13 / 8 / 39 / 6 / 12 / 48 / 9 / 5 / 40, including per-suite setup/cleanup). The two skipped tests were also run separately with real macOS windows and the threaded scene graph: both passed (100 rendered mode switches). The macOS app built successfully. Its bundled icon resource matches the committed ICNS; Windows/Linux compilation was not tested. The automated renderer process tests use a fake CLI that rejects the obsolete `-w`/`--width` switches. Separately, the installed real Mermaid CLI 12.2.0/Node/Chrome stack was exercised through `MermaidRenderer`: the example flowchart produced a 552×77 PNG in about 1.15 seconds after removing `-w`. This is a single real-render smoke check, not exhaustive browser/platform validation.

Window-routing regressions use isolated application-data profiles: same/relative/symlink targets focus without replacing drafts; file-picker routes preserve untitled text; Save As collisions fail before attachment copying; a cancelled close retains its Backend; accepted close deletes QML before Backend and permits reopening; duplicate recovered URLs keep both drafts; New Window leaves a later orphan snapshot untouched. Native Cocoa runs of these and delayed `QFileOpenEvent` delivery passed (three bodies plus setup/cleanup). An isolated-HOME smoke test also started the actual rebuilt app and accepted three subsequent executable launches for the same/different files, leaving both source files unchanged (offscreen; no Finder focus assertion). The broker's separate subprocess tests include a 3.3-second queued opening callback whose ACK still succeeds. A forwarding process drains native events through a 400 ms quiet interval; this tests events delivered after initial startup, not every possible LaunchServices timing or desktop focus policy. Windows/Linux compilation/runtime remain unverified.

Raster caches/projections are byte- and count-bounded (32 MiB per cache/projection; up to 64 visual objects per projection). Additional blocks fall back rather than making source inaccessible. These are not a whole-process memory cap: source documents, transient rasterization and the external renderer have separate costs.

Heading regressions compare actual glyph sizes for levels 1–6 against Qt Markdown across pixel/point fonts, light/dark themes and zoom sizes; they also check original muted active-marker colors and Edit/Read toggle parity, including Qt's smallest-heading HTML round-trip.

Copy Image tests cover original decoded dimensions (including a 2,001-pixel-high image), exact pixels, staged versus saved/external disk resolution, unchanged source/undo, failed-copy clipboard preservation and shared decoder limits. Read inline/reference images retain their original destinations without exposing synthetic Mermaid resources. A wrapped inline-image regression checks both halves even when the trailing caret moves to another line, plus blank-space rejection. Native macOS image-menu tests copy pixels in Edit/Read from both halves, with Read mutation controls hidden/disabled (two data rows, four checks including setup/cleanup).

Writing-feature tests cover stale asynchronous selection counts/CJK rules, the 100-file local position cache, platform-safe Explorer argument quoting, real footer selection display, per-file Edit/Read restoration/reload/failed-open/Save As, physical Markdown/HTML clipboard shortcuts, and embedded PNG resources in Read-selection HTML. A native macOS UI run of that combined fixture also passed. `open -R` accepted the guide path in a separate Finder command smoke check; Windows/Linux Reveal and compilation remain unverified.

The long-document mode-switch regression starts with the caret at the end, scrolls Read to two different positions, then uses the actual shortcut to return to Edit. It checks that the viewport stays at the reading height, the new caret is near the visible center, and Markdown is unchanged.

Body-style regressions check softer prose versus stronger headings in both themes, regular/nonitalic paragraph fonts through repeated mode toggles, unchanged Markdown and the full window's theme bindings.

Read/Edit appearance regressions compare actual paragraph and heading gaps, nested tight/loose list spacing, bold/italic, link accent and code backgrounds across themes and zoom sizes. Edit list tests retain exact source, stable hanging/wrapped alignment while the prefix is raw/rendered, sequential numbering, nesting/dedented continuations, native Read body-column parity, prefix search/selection reveal and one-undo marker replacement. Exclusions cover code, comments/HTML and tables; regressions cover paragraph interruption, delimiter changes and outer/nested fences. A 1,000-item fixture moves the caret 32 times without resetting the marker model; cached parsing/formatting and passive bindings avoid recreating all decorations. Resize/zoom and selected-prefix IME cancellation are checked. Native Mac pointer/body typing/undo tests pass. Clipboard tests cover Read empty-selection keyboard/menu Copy returning canonical Markdown, selected Read native copy, selected/whole HTML markup in both MIME channels, the HTML context action and focused-editor shortcut fallback. Three dedicated native Mac bodies passed (five checks including setup/cleanup). Toggle tests check Mac/Linux/Windows shortcut labels and logical icon centering, including the Material control in the full window.

Fenced-code panels cover empty/wrapped lines, separate adjacent and list-contained fences, resize and unfinished/unknown-language blocks without source/undo changes. Wrapping IME preedit updates panel geometry without rebuilding the projection. A separate native Mac GUI test checks the actual continuous fill in both modes/themes, plus the half-pixel border and rounded-corner properties. Panel coordinates also cover the native line box and actual glyph ink at 12/20/34 px with 0/4/9 px document margins. A single-line IBM Plex Mono JSON fixture exposed a 4 px upward offset in Read: layout positions already include the document margin, so it must not be subtracted again from the vertical origin. Native Mac captures check the enlarged padding below the final line at 100%/160% zoom in both modes/themes. Padding scales with the font; presentation-only margins keep backgrounds clear of prose, adjacent code panels and document edges. A 300-fence regression checks batched layout notifications and a subsequent caret move, without changing source or undo. Hover Copy uses Qt's clipboard in Edit/Read, with a Read language label (`text` fallback). Tests cover exact native bodies, Unicode/blank lines, unknown tags, containers, separate adjacent fences, unclosed/empty bodies, parser-mismatch fallback and stale/composing/suspended-layout rejection without overwriting the clipboard. Real QML pointer hover/click checks confirm hidden controls outside the panel, preserved selection/focus/source/undo, and native Mac operation. Inline-code backgrounds and token colors remain independent. Background ranges no longer depend on temporary syntax-highlighter states; incomplete layout retains the last valid model. A fault-injection regression clears those states and temporarily invalidates endpoint layout, and fails on the prior implementation. Native click transitions check both panel pixels and every model change, without forcing a refresh. The exact intermittent user click sequence was not reproduced by the simple fixture; the underlying state/lifecycle dependency is tested deterministically.

A separate large-code regression checks the real document height, continuous panel extent, final-line reachability and scroll range through eight physical caret clicks. It uses generated long TOML by default; `OMAWRITE_LARGE_CODE_FIXTURE` optionally reads a note into an **untitled, isolated backend**, never opening that note for autosave. On native macOS, a real 244-line TOML note reproduced a 7,548→4,038 pixel content-height collapse before the fix and passed afterward, as did the generated fixture. `LivePreview` now avoids resetting an unchanged default font: Qt otherwise invalidates the entire document layout even for equal fonts. Genuine font/zoom changes still apply. This is a reproduced document/scroll-range bug, distinct from the intermittent background-only sequence above.

Inactive standalone dashed horizontal rules render across the writing column in Edit; caret entry/selection reveals their original Markdown. Tests cover source/undo preservation, full-width raster endpoints, fence/Setext/HTML exclusions and literal HTML boundaries; a native Mac physical-click fixture passes in both themes. Read remains Qt's native Markdown rendering.

Code-color regressions compare actual Read character formats with the shared tokenizer in both themes for Python, C++, JavaScript, JSON, YAML and shell aliases. They include quoted/list code, adjacent same-language fences, multiline comments, unclosed/empty fences and unknown/untagged plain fallback. An indented literal fence cannot claim the prefix of a later real fence. Read import validates the full sequence before applying source metadata; ambiguous syntax falls back to plain code. Source text and undo stay unchanged. The macOS app statically links the engine; upstream notices/inventory are included in its resources, but binary redistribution remains blocked pending the [resource-license/source audit](../third_party/ksyntaxhighlighting/README.md).

The Windows kit-dispatch shell paths were checked with fake qmake/compiler/CMake tools (MinGW, MSVC and rejected unsupported kit). This is not a Windows compiler or runtime test.

Spelling checks cover UTF-16 prose masks, container-relative indented code and
paragraph continuations, malformed-bracket linearity, installed-language fallback,
cache/generation safety, one publication per completed snapshot, missing dictionaries,
IME deferral, session Ignore and fake Learn failures/success. Projection checks assert
no async rebuild, preserved styling/undo, clean Read, and exact pointer glyph hits
including wrapping and bidi text. Main/QML tests cover native suggestions, one-undo
correction, stale actions, settings and attachment-menu priority. Clipboard image
paste is tested through both keyboard and context menu. Native Learn is deliberately
not exercised: no personal dictionary changes are needed. Native Mac spelling/UI
checks passed separately. If Cocoa declines OS activation after synthetic attachment
popups, that fixture invokes the actual mode-button handler; separate focused tests
retain physical mode-shortcut coverage. This is not a claim of exhaustive native
keyboard/clipboard/IME testing.

## Opt-in performance baseline

Use `bin/perf LABEL OUTPUT.json` for native frame-submission cadence and operation-to-follow-up-frame proxies across prose/list/code/image fixtures. It is separate from `bin/test`: FPS cannot be established by offscreen correctness tests. Compare compatible runs with `bin/perf-compare BEFORE.json AFTER.json` (optional Python 3). See [performance methodology, raw baseline/candidate reports and limitations](performance.md). The initial Metal pair does not establish an FPS improvement.

Three source-cache regressions verify zero additional extraction/parsing across warm caret/selection/width changes, formatting-only parse reuse, same-length/guarded edits, undo/redo, NBSP/emoji fidelity, empty results, image refresh and Read reuse. Expanded checks cover signal-blocked same-length image changes and code-copy rejection, plus IME commit/cancel cache behavior. The optional comparator has 13 stdlib checks, including malformed fixture/scroll records: `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests -p test_perf_compare.py`.

## Native scene-graph regression tests

Offscreen text/document tests do **not** establish that Qt Quick can paint a projection safely. Run this separately on an interactive desktop:

```sh
JOBS=4 ./bin/test --rendered
```

This opens real windows and forces `QSG_RENDER_LOOP=threaded`. Both tests assert that `beforeRendering` actually runs off the GUI thread; software/basic rendering is not a substitute. They capture rendered frames while switching Edit/Read, with local image objects, tables and fenced code. The full-window test also scrolls, resizes and changes zoom. It invokes the same QML handler as the mode button, rather than depending on OS keyboard focus during resizing. Existing physical-shortcut tests remain separate.

To inspect a particular note without attaching auto-save to it:

```sh
OMAWRITE_RENDER_FIXTURE="/absolute/path/to/note.md" JOBS=4 ./bin/test --rendered
```

The fixture is read only; its contents and sibling `img/` files are copied into a temporary profile. No original document, image or recovery file is written.

The 2026-11-07 user crash was in `QTextLine::ascent()` while Qt Quick painted an image after Edit → Read → Edit. The hardening prevents zero-page-size highlighter batching on rich objects/frames, detaches the highlighter until the replacement Edit projection is complete, and completes/checks image-line layout before publication. An invalid object becomes a one-character placeholder instead of reaching the renderer. Valid-image counts are asserted so this fallback cannot silently hide valid fixtures. A zero-page invariant test fails on the previous code and passes with the fix. Layout notifications are coalesced without invalidating rich page geometry (1,000-heading/image restyle: one notification, about 155 ms in a focused local run).

The exact original crash was not reproduced in the native stress tests, including before the fix. These checks validate the suspected layout-invalidation mechanism and exercise the real renderer; they are not proof that every Qt/rendering crash is eliminated. Native Windows/Linux rendering remains untested.

## Optional macOS dictionary smoke check

```sh
./bin/test --native-spelling
```

This uses the installed NSSpellChecker English dictionary, not a fake backend.
It checks correct/misspelled words and suggestions without calling Learn or
changing the personal dictionary. It is separate from the deterministic suites;
it may skip if English is unavailable. The optional target currently supports
macOS only. Windows/Linux still need real-desktop dictionary and UI verification.

## Manual release checks

Use [preview-example.md](preview-example.md), plus a disposable document and image directory.

- Click the book/pencil toggle repeatedly near the bottom of a long document; verify scroll and caret do not jump unexpectedly.
- Enter and leave a table from both sides; select across it forwards/backwards; type, delete, undo and redo. Saved text must remain pipe Markdown.
- Test input-method composition, cancellation and replacement on your actual keyboard/input method, especially beside visual blocks. A synthetic IME test is not full OS/input-method coverage.
- Copy a real screenshot, paste before the first save, name/save the note, quit/reopen, and verify its `img/` reference and PNG. Test recovery in a disposable profile, not against important drafts.
- Save As into a different folder containing a conflicting image. Check the error and original bytes. Test a missing image and a renamed/located reference, then undo/redo.
- Modify an image outside OmaWrite and reopen to refresh its preview; filesystem image watching is not implemented.
- Check F1 labels and physical Cmd+Option+F / Cmd+Ctrl+F on Mac. OS-reserved keys and Fn settings cannot be fully established by synthetic Qt events.
- If you explicitly install/use Mermaid CLI, check a real successful diagram, an invalid diagram and renderer failure/timeout. Fake-CLI tests do not establish compatibility with a particular Node/Puppeteer/Chromium installation.
- Check high-DPI and light/dark appearance, clickable Read links and local image scaling.
- Use [code-highlighting-example.md](code-highlighting-example.md) to check real font/zoom/search/selection/undo behavior in tagged fences and across mode toggles. These are not autocomplete or LSP features.
- Use [spellchecking-example.md](spellchecking-example.md) with an installed dictionary. Check Edit-only underlines, suggestions, undo, session Ignore, language/on-off changes, code/URL exclusion, emoji offsets and image-menu priority. Test Learn only with a word you actually want in that backend's personal dictionary; ordinary tests must not alter the user's dictionary.
- On Windows/Linux, verify the native backend/dictionary enumeration and correction workflow on a real desktop before claiming platform support is runtime-tested. Linux needs system Hunspell dictionaries, not just its development library.

No renderer or external runtime is installed by the test runner. Do not describe tests of the fake CLI as an end-to-end Mermaid/browser test.
