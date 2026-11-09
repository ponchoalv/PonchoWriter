# Incremental native editing and the 10,000-line check

Implemented on `feature/incremental-native-editing`, based directly on `master` at `f94d69e`. The earlier `performance/baseline-and-cached-preview` experiment remains parked: it was **not merged** into this branch.

## What changed

- **Persistent Edit document:** ordinary native typing updates the canonical Markdown range and mapping in place. It does not clear/reinsert the entire visible document or detach/recreate its highlighter. Canonical source still owns undo, saving, search and recovery.
- **Resumable metadata:** structural and list parsers retain checkpoints and rescan from the affected region until state converges. Fence-interior edits have a local fast path; source offsets use retained suffix metadata. Structural line-count changes and syntax changes that propagate legitimately require more work.
- **Object patches:** tables, local images, Mermaid and rules are reconciled as bounded source/view intervals, patched right-to-left. Caret/selection activation reveals source without replacing unrelated prose. Existing UTF-16 mappings, resource limits, composition deferral and safe image fallback remain.
- **Local styling/layout:** stable block-format list-prefix metadata avoids absolute-position churn. Code state and selection rehighlighting batch native layout changes. Code-panel spacing uses retained endpoints rather than scanning/reformatting every paragraph on a caret move.
- **Viewport work:** list labels and code controls are generated for the visible region plus overscan. The native TextEdit uses Qt Quick's public `ItemObservesViewport` flag above 10,000 source characters. Direct QTextDocument edits bypass `TextEdit::setText()`, which normally enables this optimization; without it Qt can generate glyph nodes for the whole long document. This is not a custom renderer or a Qt-private API.
- **Lifecycle safety:** structural projection notifications wait for stable extents; a separate readiness notification restores remembered positions even when reopening identical text. Invalid image slots remain repairable, including Read. Theme/font/global refresh and Read import can still do whole-document work.

This is still C++17/Qt's event-driven document architecture, not an ECS/DOD rewrite. QString edits, canonical backend bookkeeping, word counting, global operations and some structural metadata work remain document-sized. It is not a constant-time typing or whole-process memory guarantee.

## Link interaction

In **Edit**, ordinary clicking positions the caret. **Cmd-click on macOS / Ctrl-click elsewhere**, or right-click **Open Link**, opens a link using the operating system. Blank space is not treated as the nearest link. Image menus keep priority. **Read retains ordinary single-click links.** Existing safe link normalization/resolution is reused; no embedded browser or new dependency was added.

## Matched native measurement

The writer's scratch note was read into an **untitled isolated backend**, never opened for autosave or modified on disk. It had acquired manual edits since creation: **10,011 newline characters / 815,337 UTF-8 bytes**, not exactly the original 10,000 lines. SHA-256:

```text
743a6f26c5d298e652fe83b36b83e502e84e3efdcfea0cc9d6e98e1a5ff66106
```

Both runs used the same benchmark function, fixture hash, native full Main.qml window (1000×760), IBM Plex Mono 20 px, Qt 6.12.0, macOS arm64, Metal / Apple M4 Max, **Q27G2G3R3B at 144 Hz**, DPR 1. Spelling was disabled/restored for this core-editing comparison; normal word-count/recovery bookkeeping remained. Each run delivered 12 synthetic native QTest X-key events near the end, verifying exact canonical text after every key. Internal caret/focus was established outside timing. The benchmark window is created hidden, then shown without accepting OS keyboard focus; it does not type into the writer's app.

| Typing → marked follow-up frame submission | `master` (`f94d69e`) | Incremental candidate |
| --- | ---: | ---: |
| Median of 12 keys | 12,371.7 ms | 10.1 ms |
| Maximum | 12,500.6 ms | 14.3 ms |

Raw samples: [reference](performance/incremental-native/master-10k.json), [candidate](performance/incremental-native/incremental-10k.json).

This is **one matched pair on one large mixed Markdown note**, not a statistical guarantee or a general FPS result. The previous full-reconstruction path was exceptionally slow here. The timer includes event handling and a requested frame synchronized after queued work, but is not photon/display scan-out latency or GPU execution time. Neither all documents nor startup, Read import, global theme/font changes, every IME or other desktops are characterized by this table. Earlier runs on mismatched displays and failed input-isolation runs are excluded.

### Reproduce

Build normal suites first (`bin/test`). On an interactive macOS desktop, use the main test executable; Unix builds have the corresponding non-bundle executable:

```sh
OMAWRITE_INCREMENTAL_PERF=1 \
OMAWRITE_INCREMENTAL_PERF_FIXTURE='/absolute/path/to/a/disposable-large-note.md' \
OMAWRITE_INCREMENTAL_PERF_SCREEN=Q27G2G3R3B \
OMAWRITE_INCREMENTAL_PERF_LABEL=incremental-native \
OMAWRITE_INCREMENTAL_PERF_OUTPUT=/tmp/omawrite-10k.json \
QSG_INFO=1 \
build-tests/tst_omawrite.app/Contents/MacOS/tst_omawrite \
  tenThousandLineNativeTypingBenchmark
```

Choose an actual connected display name or omit the screen selector. A requested display is verified before/during/after timing; desktop window policies can cause failure rather than a comparable report. The fixture must contain at least 10,000 newline characters. Default correctness runs skip this opt-in benchmark. For a reference, compile the **same current test harness** against `f94d69e` in a separate worktree; do not overwrite a working checkout or mix application changes into that reference. Remove old output files and require successful process exit and all assertions before accepting a report. This small opt-in test is separate from the parked branch's `bin/perf` tooling.

## Verification and limits

2026-11-09, macOS arm64 / Qt 6.12.0:

- **346 checks passed, 0 failed**, twelve offscreen suites; three bodies skipped (two native renderer checks and the opt-in benchmark).
- Structural parser differential regression: 600 seeded UTF-16 edits compared against a fresh parse; 10,000-line fence-interior convergence. Native document/block identity, canonical undo, list parser equivalence, composition, source/object boundaries and viewport-bounded decorations have regression coverage.
- Viewport threshold crossing, Read/Edit and shrinking documents are covered. Real native glyph pixels are checked after distant scrolls, in addition to text/height/model assertions.
- Seven native UI bodies (including two image data rows): **9 checks passed**, covering Edit link click/context/blank-space behavior, code hover/padding/scroll glyphs, clipboard, image menus and reading-position lifecycle.
- Two separate native threaded-render bodies: **6 checks passed**, including 100 mode switches with images. Each matched benchmark run passed its body plus setup/cleanup.
- App rebuilt successfully as `build/omawrite.app` (version remains 0.7.2). Homebrew Qt newer-deployment-target linker warnings remain; this is not proof of older macOS compatibility.

Windows/Linux code paths use the same public Qt APIs but were **not compiled or runtime-tested here**. No user app was killed/replaced, no dictionary was learned, no external renderer was installed, and the original scratch note's hash remained unchanged. Existing static syntax/spelling dependency redistribution/license audit requirements remain in force.
