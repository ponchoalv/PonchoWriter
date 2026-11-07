# Testing the native-preview fork

## Automated suites

Run `./bin/test` with Qt 6.7+ and a C++17 compiler. The runner builds separate QtTest targets, uses the offscreen Qt platform, and recognizes macOS bundles and Unix binaries.

| Suite | Purpose |
| --- | --- |
| `tests/tests.pro` | Existing backend/editor/QML regressions, platform-correct Replace dispatch, visible-editor typing, screenshot paste, image naming menu/left and right pointer hit-tests, source undo/redo, Read/Edit and attachment save integration |
| `tests/shortcutlabels.pro` | Platform-formatted F1 key labels, including Cmd/Option versus physical Ctrl |
| `tests/headings/headings.pro` | ATX heading markers, caret/selection activation, code fences, unchanged source, efficient large-selection updates |
| `tests/attachments/attachments.pro` | Persistent staging/recovery, first save/auto-save/Save As, collision and path safety, current-document rename/locate and undo, encoded/exact references and absolute paths |
| `tests/previewblocks/previewblocks.pro` | Conservative structural parsing and native rasterization/fallback |
| `tests/livepreview/livepreview.pro` | UTF-16 mappings, source/view separation, native input/selection/IME, object boundaries, source preservation, local/remote resources, cumulative raster budgets/stale-resource release and read links |
| `tests/mermaid/mermaid.pro` | Optional-renderer fallback, constrained syntax and asynchronous process/cache behavior using a fake local CLI |

The main QML suite registers the same `Omawrite.LivePreview` type as the application. It uses a unique application-data identity per run, so a failed test's recovery snapshot cannot be restored by the next run. Tests must send physical text input to **`previewEditor`**, not the hidden canonical **`sourceEditor`**. When a test changes source directly, allow the queued projection update before sending input.

The relative-file-path regression canonicalizes an existing parent directory, not a nonexistent document. This accounts for macOS `/var` and `/private/var` aliases without weakening the requirement that the document URL be absolute.

## Verified development run

On 2026-11-07, macOS arm64 with Homebrew Qt 6.12.0 and Apple Clang: **193 passed, 0 failed** across seven suites (110 / 16 / 22 / 12 / 7 / 20 / 6, including per-suite setup/cleanup). The macOS app built successfully. Its bundled icon resource matches the committed ICNS; Windows/Linux compilation and a real Mermaid/Chromium installation were not tested. The renderer process tests use a fake CLI.

Raster caches/projections are byte- and count-bounded (32 MiB per cache/projection; up to 64 visual objects per projection). Additional blocks fall back rather than making source inaccessible. These are not a whole-process memory cap: source documents, transient rasterization and the external renderer have separate costs.

Heading regressions compare actual glyph sizes for levels 1–6 against Qt Markdown across pixel/point fonts, light/dark themes and zoom sizes; they also check warm active-marker colors and Edit/Read toggle parity, including Qt's smallest-heading HTML round-trip.

Read/Edit appearance regressions compare actual paragraph and heading gaps, nested tight/loose list spacing, bold/italic, link accent and code backgrounds across themes and zoom sizes. Toggle tests check Mac/Linux/Windows shortcut labels and logical icon centering, including the Material control in the full window.

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

No renderer or external runtime is installed by the test runner. Do not describe tests of the fake CLI as an end-to-end Mermaid/browser test.
