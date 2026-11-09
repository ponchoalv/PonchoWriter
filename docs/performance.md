# Performance baseline and first optimization

Measured on 2026-11-09. This is an opt-in engineering benchmark, not an always-running FPS overlay or production telemetry. No new application dependencies were added.

## Branch checkpoint

Before application changes, `feature/native-live-preview` was fast-forwarded into the fork's default branch, **`master`** (there was no `main`), and pushed. The checkpoint is `f94d69eeda3e809c6119cc4077cbba287747ed72`. Work continues separately on `performance/baseline-and-cached-preview`. Local user changes to the preview example and attachments were not included in that merge.

## Run the benchmark

With the native desktop available and the normal build dependencies installed:

```sh
bin/perf baseline /tmp/omawrite-baseline.json
# After an application change, using the same benchmark and environment:
bin/perf candidate /tmp/omawrite-candidate.json
bin/perf-compare /tmp/omawrite-baseline.json /tmp/omawrite-candidate.json
```

`bin/perf` is a POSIX shell/qmake/make launcher (macOS/Linux or an appropriate shell/toolchain); it is not a native PowerShell runner. `bin/perf-compare` optionally uses Python 3's standard library. The benchmark C++ uses cross-platform Qt APIs, but Windows/Linux execution has not been verified here.

The launcher builds a separate Release test executable under `build-tests/performance`. It uses the real `Main.qml`, backend, hidden Markdown source, visible projection and native renderer. It generates its own temporary documents/images and uses isolated application settings/recovery with recovery loading disabled. It does not open or save vault notes, send telemetry, or modify personal spelling dictionaries.

Keep the benchmark window visible and do not interact with it. Keep the monitor, resolution/scaling, power mode, font, Qt/dependency versions and background load unchanged between runs. Run at least three times per revision, preferably alternating reference/candidate order. A single pair is exploratory evidence, not statistical proof. Do not run the correctness suites concurrently with timing runs.

The output is invalidated at the start of an execution and published atomically only after a successful, complete run. Build failures/crashes must not leave an old successful report masquerading as the new run. Reports include revision, application source fingerprint, benchmark/configuration/font fingerprint, dirty-source flag, Qt/dependency versions, screen, scaling and graphics API. The comparison tool rejects incompatible configurations.

## What is measured

Four generated scenarios: 400 prose sections, 1,000 list items, a 1,200-line C++ fence, and 20 local images. Each includes a final paragraph for typing.

- Initial document projection to a synchronized follow-up frame, **not process startup time**.
- Twelve physical typing actions, 24 source-caret moves, six mode switches and six resizes: median, sample p95 and maximum elapsed milliseconds.
- Separate 1.5-second Edit and Read scrolling animations: frame submissions, full phase duration, submissions/second, gap median/p95/maximum, and gaps exceeding 1.5 nominal refresh periods.
- Scrolling includes phase-start→first-submission and final-submission→phase-end gaps, and awaits a synchronized submission after the animation ends. Initial/final stalls are not silently discarded.

A render-thread recorder marks a request during `afterSynchronizing` and completes it at `frameSwapped`. The operation measurements are **GUI-operation-to-follow-up-submission proxies**: they include event-loop polling and scheduling, and can include an additional frame. They are not exact input-to-photon latency, display scan-out FPS, or GPU execution time. The scroll animation itself is a workload with its own scheduling cadence; nominal screen refresh does not guarantee an identical submission rate.

The benchmark requires a threaded renderer and rejects Qt's Software/Null/Unknown API backends. An OpenGL/Vulkan API name does not prove a physical GPU: software drivers such as llvmpipe can still implement those APIs. Reports explicitly mark adapter acceleration as unverified. Use `QSG_INFO=1 bin/perf ...` and retain its output to inspect the actual adapter/driver.

Spelling is deliberately disabled for these initial fixtures. Word counting and recovery remain enabled. Warm-up delays are not a guarantee of global background-work quiescence. Add a separate spelling-enabled workload before drawing conclusions about spelling performance. There are no startup/RSS baselines yet, and idle FPS is intentionally not measured: a resting editor should not render continuously.

## Recorded reference and candidate

Raw reports:

- [Metal reference](performance/metal-baseline.json): unmodified checkpoint application source.
- [Metal cached-source candidate](performance/metal-cached-preview.json): source cache change, identical benchmark/configuration fingerprint.

Environment: Qt 6.12.0, macOS 27.2 ARM64, Metal, IBM Plex Mono, 1000×760 logical-pixel window, scale 1, screen `CR270AU` reporting **75 Hz**. Prior native renderer diagnostics identified the Apple M4 Max Metal device; the portable report itself records the API rather than claiming adapter detection.

Single paired run, median follow-up-submission time:

| Scenario | Typing before → after | Caret before → after |
|---|---:|---:|
| Prose (~56k characters) | 358.8 → 338.0 ms | 39.6 → 39.3 ms |
| Lists (~69k characters) | 426.7 → 425.7 ms | 28.6 → 28.7 ms |
| Code (~47k characters) | 480.2 → 476.3 ms | 53.3 → 55.7 ms |
| Images (~2.7k characters) | 13.9 → 13.7 ms | 12.9 → 13.0 ms |

Active scrolling was approximately **74–75 submitted frames/second** for most cases in both runs, with code Edit approximately 74 → 73. There is **no demonstrated general FPS increase**. These scroll rates are already near this screen's 75 Hz refresh rate, so unchanged submission FPS cannot establish that CPU work is unchanged: latency and gap distributions matter too. Some other operations improved while others regressed within a small sample; do not advertise these differences as established gains.

The large-text typing figures expose a remaining responsiveness problem. This first cache removes redundant extraction/parsing; it does not eliminate full projection reconstruction, highlighting or layout. Those paths are the next profiling targets, rather than assuming more GPU work is needed.

## First low-risk application optimization

`LivePreview` now lazily caches canonical Markdown and the source-only `PreviewBlocks` parse:

- Warm caret/selection changes and width changes reuse both.
- Any source notification invalidates the snapshot, including same-length edits and guarded native writes.
- Formatting-only notifications may require one text comparison but do not invalidate the parse when Markdown is unchanged.
- Read construction and Edit image hit tests share parsed blocks; empty parse results are cached too.
- Explicit refresh rereads source, including externally signal-blocked changes, but only reparses changed Markdown.
- Image eligibility/rendering and attachment refresh are **not** cached solely by Markdown; image changes still take effect.
- IME suppression, source/view mapping, undo ownership, resource bounds and fresh-text code-copy guards remain intact.

Private regression counters verify **zero additional source extractions/parses during a warm caret/selection/width sweep**. This is deterministic evidence of removed work, not a claim that complete caret processing is O(1).

## Verification and next steps

- 344 passing normal Qt checks; two native-only checks skipped in the offscreen run.
- Separate native threaded scene-graph tests passed, including 100 Edit/Read toggles in each of two image-bearing tests.
- Both complete native benchmark runs passed all four scenarios.
- Optional comparator checks: `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests -p test_perf_compare.py` (13 checks, including duplicate/missing fixture and scroll-record rejection).
- An intentionally failing qmake launcher returned failure, marked output incomplete and removed its temporary report.
- macOS app rebuilt; Windows/Linux and older macOS remain unverified.

Next: instrument parsing, projection replacement, highlighting and layout separately; use repeated runs to choose the dominant cost. Then consider safely reusing the already-edited native projection for ordinary text edits, followed by viewport-limited decorations or ordered asynchronous persistence if measurements justify them. Do not bypass resource, IME, undo or recovery checks merely to improve a timing number.
