# Mode transitions, the visible caret and code shortcuts

Implemented on `fix/mode-transitions-caret-and-pairs`, following `feature/incremental-native-editing` at `d35b347`. Qt 6.7+ public APIs only; no new dependencies or browser renderer. Windows/Linux have not been compiled or runtime-tested here.

## What changed

- **Read → Edit retains the existing native Edit document and its attached syntax highlighter.** It no longer clears/reinserts every line and queues a whole-document re-highlight. Source changes made while reading are reconciled into that retained projection. The hidden Markdown source still exclusively owns save/undo.
- Read preparation batches scratch/import formatting with `QTextDocument::setLayoutEnabled(false)`, then reenables layout before attaching the completed native document. This is not the unsafe zero-page-size rich-layout shortcut.
- An unchanged Read document is reused. Source, font/theme/width and explicit resource refresh invalidate it. Edit image resources are separately refreshed on return, even when Read consumed a resize/refresh notification.
- The mode's read-only notification occurs inside the selection guard, before document attachment and ready/position restoration. Native document attachment also invalidates Qt Quick's text rendering state.
- Long documents retain viewport-aware glyph rendering. An independent clipped QML caret follows the native cursor rectangle, focus, selection, OS blink settings and IME cursor-visibility attributes. It is not affected by Qt's internal offscreen-caret visibility latch.

No code was found that closes/recreates the OS window during mode switching. The old rebuild stall was reproduced; the exact reported disappearance and intermittent blank-frame sequence were not independently reproduced. Native regressions now check unchanged window visibility, a caret after distant scrolling/clicking, and actual Read glyph pixels **before another scroll**.

## Auto-pairing in Edit

Typing `[` or `{` inserts its matching closer at a safe boundary. Straight single/double quotes are also paired, except inside words/contractions or after an escape. A selection is surrounded and remains selected. Typing an existing closer moves past it. Backspace between an empty matched pair removes both as one canonical undo action.

Pairing does not transform paste or IME commit text, does not operate in Read/overwrite mode, and conservatively leaves modified-key/word-interior input alone. This is simple pairing, not a language-aware completion engine. Backticks are handled by the explicit commands below, not automatically paired on typing.

## Code commands

| Action | macOS | Windows/Linux |
|---|---|---|
| Toggle inline code | **Cmd+T** | **Ctrl+T** |
| Toggle fenced code block | **Cmd+Shift+T** | **Ctrl+Shift+T** |

No existing OmaWrite shortcut used these combinations. They appear in F1. Both commands are Edit-only, finish composition and synchronize the projection before reading selection, preserve the body selection, and use one source undo action.

Inline code uses one backtick normally, or a longer delimiter when the body contains backticks. Markdown padding is added when necessary. With no selection it inserts an empty pair and puts the caret inside. A single surrounding code span can be removed; multiple unrelated spans or literal backticks are wrapped rather than stripped.

Fenced code uses at least three backticks, longer than runs in the selection. **The opening and closing fences occupy separate lines.** Mid-paragraph selections receive the needed outside newlines; existing body line breaks remain intact. With no selection the caret is on an empty line between the fences. It does not guess a language tag. A single selected/surrounding block toggles off; a selection spanning separate blocks is wrapped safely instead. Undo restores the original text, including any added line breaks.

## Matched native measurement

One paired run, three Edit → Read → Edit cycles followed by twelve typed keys. Both versions used the same test harness and frozen, read-only copy of the manually edited scratch note: **10,027 newline characters, 815,767 UTF-8 bytes**, SHA-256 `1be079fbcd539277e779a1337d67712516fee60d53cc56cef909d5f366359ab1`. The original note was never attached for saving.

Environment: macOS arm64, Qt 6.12.0, 1000×760 window, IBM Plex Mono 20 px, DPR 1, `Q27G2G3R3B` at 144 Hz, threaded native rendering. The isolated window never accepts OS keyboard focus. Spelling was disabled/restored; normal word counting/recovery remained enabled.

| Operation → marked follow-up frame submission | Previous `d35b347` | Retained documents |
|---|---:|---:|
| First Edit → Read | 1,147 ms | 598 ms |
| Subsequent unchanged Edit → Read | 1,138–1,152 ms | 3–5 ms |
| Read → Edit, three switches | 15,120–15,263 ms | 6.6–10.5 ms |

First Read after an edit or invalidation still requires document-sized conversion; cached timing is **not** promised then. These are frame-submission proxies, not display scan-out latency, a general FPS claim or a statistical proof. Keeping two native presentations trades memory for avoiding reconstruction.

Raw reports: [previous](performance/mode-transitions/previous-10k.json), [retained documents](performance/mode-transitions/retained-10k.json).

To reproduce after building the tests, run the native `tst_omawrite` executable with `tenThousandLineNativeTypingBenchmark` and set:

```sh
QSG_RENDER_LOOP=threaded
OMAWRITE_INCREMENTAL_PERF=1
OMAWRITE_MODE_PERF=1
OMAWRITE_INCREMENTAL_PERF_FIXTURE=/absolute/path/to/a/frozen-10000-line-copy.md
OMAWRITE_INCREMENTAL_PERF_SCREEN=Q27G2G3R3B # Use an actually attached screen name.
OMAWRITE_INCREMENTAL_PERF_OUTPUT=/tmp/mode-result.json
OMAWRITE_INCREMENTAL_PERF_LABEL=my-build
```

Export these variables before invoking the executable. Compare identical fixture hashes, harness, actual display, font and window size. Do not automate typing into an app the writer is currently using.

## Verification

- **351 passed, 0 failed, 3 skipped** across twelve normal suites. Skips: two native rendering bodies and the opt-in benchmark.
- Separate native macOS tests passed: distant-scroll caret and Read pixels, physical code shortcuts/undo/IME, large-code scrolling, hover copy, and both image-paste rows: eight checks including setup/cleanup.
- Both threaded renderer bodies passed separately: six checks, including 100 rendered mode switches.
- Both native benchmark runs passed; macOS app rebuilt as 0.7.2.
- Regression fixtures refetch the active document on `QQuickTextDocument::textDocumentChanged`; Edit and Read intentionally no longer have the same pointer. Spelling still appears only in Edit.

The built app is `build/omawrite.app`; this update does not replace `/Applications/omawrite.app` or terminate existing app instances.
