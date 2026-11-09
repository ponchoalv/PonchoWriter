# Language-tagged code fences: KSyntaxHighlighting investigation

Investigated 2026-11-07. **Historical design investigation: language-fence highlighting is now implemented in Edit and Read.** Use the [current syntax-highlighting guide](syntax-highlighting.md) and [installation steps](install.md), not the planning estimates or deployment alternatives below.

## Implemented outcome

The implementation uses standalone **KSyntaxHighlighting 6.13.0**, statically installed under `.deps/install` with embedded resources (`QRC_SYNTAX=ON`), not Kate/KTextEditor or WebEngine. Tagged backtick/tilde fences resolve aliases, native names and safe extensions; unknown/untagged code stays plain. Stateful tokens reset at every fence, including adjacent same-language fences. Read bakes token formats before HTML serialization and leaves code plain when source metadata and Qt's parser disagree. Theme-aware colors preserve writing font/zoom/background, search, source and undo; no autocomplete/LSP or graphical code editor is implied.

Builds now need **CMake 3.24+ before the first bootstrap** and matching Qt 6.7+ Core/Gui/Network (Test for tests), compiler and build tool. The pinned ECM and syntax archives download only during developer builds. `bin/build`/`bin/test` bootstrap only when the private header is missing; use `JOBS=4 ./bin/build-dependencies` explicitly if needed. Everything must use Release (Debug unsupported); delete `.deps` and generated build/test directories when changing kit/compiler/architecture. See the installation guide for Windows dependency-before-qmake commands and optional matching static `PONCHOWRITER_SYNTAX_PREFIX`.

The engine is MIT, but embedded definitions have **mixed GPL/LGPL/other licenses**. Upstream notices belong in `third_party/ksyntaxhighlighting/README.md` and `LICENSES/`; no all-MIT or redistribution-ready claim follows. Historical Arch packaging is disabled pending resource/license/source auditing. Windows/Linux instructions remain source-inspected, not built/tested. The macOS Qt 6.12.0 implementation run passed **208 checks across eight suites** with no failures and rebuilt the app; see [test details](testing.md).

## Historical proposal (superseded where noted)

The remaining design discussion records the original rationale and acceptance goals, not unfinished implementation work or measured delivery times.

### Original recommendation and effort

Use **KDE KSyntaxHighlighting**, the standalone syntax engine used by Kate, rather than embedding Kate/KTextEditor or adding a browser. It supplies language definitions, light/dark themes and a line-oriented lexer suitable for native Qt documents.

Difficulty: **medium** for correct Edit/Read integration; cross-platform packaging adds work. A small Edit-only proof of concept is roughly 1–2 engineering days. A robust implementation covering both modes, multiline state, incremental edits and regressions is roughly 4–7 days, with another 2–5 days for clean-machine macOS/Windows/Linux packaging validation. These are planning estimates, not measured results or delivery promises.

## Fit with this app

- `src/markdownhighlighter.cpp` already recognizes backtick/tilde fences and styles their bodies uniformly. Keep it as the sole document formatting owner; do not attach a second `QSyntaxHighlighter` that competes for block formats/state.
- Wrap `KSyntaxHighlighting::AbstractHighlighter`: `highlightLine(QStringView, const State &)` emits token spans through `applyFormat(int offset, int length, const Format &)`, and returns the next lexer state.
- Share a long-lived `Repository` and cache language resolution. Resolve the first info-string token, using `definitionForName()` (case-insensitive names/alternative names) plus a small verified alias map for tags such as `js`, `ts`, `py`, `sh`, `cpp` and `yml`. Unknown/untagged fences stay plain code. No language guessing, execution or downloads.
- Keep the fence markers muted. Apply token colors/emphasis over the current code background, preserve the user's font/zoom, and apply search/selection styling last. Use matching light/dark token themes without importing editor-wide backgrounds or fixed font sizes.
- Keep Mermaid's existing diagram recognition separate from ordinary language highlighting; a syntax-colored Mermaid fallback must not change what is accepted for rendering.

## The actual hard parts

### Edit: incremental state

A block must retain both the Markdown fence context (marker, length, language) and the embedded lexer state (multiline strings/comments, etc.). Qt propagates rehighlighting when the integer block state changes, not merely when custom userdata changes.

The current numeric fence-state encoding is also read by Backend's fence/link/formatting checks. Either introduce a semantic fence accessor and intern composite states, or explicitly propagate changes until the combined state stabilizes. Changing only `python` to `cpp` must invalidate the whole fence, even when delimiter length is unchanged. Reset state at every fence boundary, including adjacent fences of the same language.

### Read: serialize real formats, not layout-only highlights

`LivePreview::buildReadDocument()` imports Markdown into a scratch `QTextDocument`, styles it, then serializes HTML into the visible document. Tokenize code while language metadata is available and bake token spans using `QTextCursor::mergeCharFormat()` **after base Read styling and before HTML serialization**. A scratch-document `QSyntaxHighlighter` alone does not make its layout formats serialize.

Verify separate-fence boundaries, blank lines and code nested in lists/quotes. If Qt's imported code metadata is insufficient, carry source-fence metadata alongside import. Preserve source/view offsets, canonical Markdown and the existing undo stack.

## Historical dependency and deployment choices

The library does not require a running KDE desktop or the full Kate editor. Its own build uses CMake/Extra CMake Modules; the app can remain qmake and consume an explicitly built, matched library. Alternatively, migrate the app to CMake separately—not as an accidental prerequisite for this feature.

**Superseded selection discussion:** the implementation now pins 6.13.0. Version pinning matters: inspected upstream master (6.33 development) requires Qt 6.10, whereas PonchoWriter currently declares Qt 6.7+. The inspected **6.13.0** release requires Qt 6.6 and demonstrates that a compatible release exists; this is not a final dependency/version selection. Review supported releases, fixes, syntax-definition licenses and distribution notices before choosing one.

The original proposal was to bundle syntax definitions/themes with library resources for offline, predictable behavior and deploy a matching dylib/DLL. **The implemented choice is static linking with embedded resources, so no separate KDE dylib/DLL is deployed.** Matching compiler/architecture/Qt kit and clean-machine resource/signing validation still matter. Windows/Linux packaging is not currently verified for this fork. Do not assume `macdeployqt` automatically deploys every non-Qt dependency.

## Acceptance checklist (not a test-results report)

- Python, C++, JS/TS, JSON, shell and YAML; aliases, unknown/missing tags.
- Multiline strings/comments, language-only edits, adjacent fences, unclosed fences, tilde/backtick lengths and undo/redo.
- Edit/Read token parity after HTML serialization; light/dark themes, selected text, search, font changes and zoom.
- Source unchanged, source/view offsets correct after rendered objects, no extra undo entries.
- Long lines/large documents: no whole-document retokenization on every caret movement.
- Clean-machine deployment with definitions/themes present and graceful fallback if unavailable.

## Separate Mermaid defect confirmed

The installed Mermaid CLI **12.2.0** rejects the renderer's old `-w` option with `error: unknown option '-w'`. This is unrelated to syntax highlighting or missing Chrome: the example diagram rendered once that option was omitted. The compatibility fix uses common CLI flags and lets Qt bound/scale the returned PNG, as it already does. A real `MermaidRenderer` probe produced a 552×77 image in about 1.15 seconds with the installed Node/Chrome setup. The normal process regression now rejects obsolete width switches instead of accepting arbitrary arguments.

Renderer failures are still not exposed with detailed CLI diagnostics in the UI; unsupported syntax, missing PATH and CLI/browser errors can all fall back to source. Better bounded, user-visible diagnostics are a useful follow-up, separate from adding syntax highlighting.

## Sources

- [KSyntaxHighlighting module and CMake usage](https://api.kde.org/ksyntaxhighlighting-index.html)
- [AbstractHighlighter: line/state/token API](https://api.kde.org/ksyntaxhighlighting-abstracthighlighter.html)
- [Repository: language definitions and themes](https://api.kde.org/ksyntaxhighlighting-repository.html)
- [6.13.0 build requirements](https://invent.kde.org/frameworks/syntax-highlighting/-/blob/v6.13.0/CMakeLists.txt)
- [Current development build requirements](https://invent.kde.org/frameworks/syntax-highlighting/-/blob/master/CMakeLists.txt)
- [Mermaid CLI usage](https://github.com/mermaid-js/mermaid-cli)
