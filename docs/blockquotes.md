# Blockquote live preview

On `feature/table-grid-and-horizontal-overflow`, native Qt text only; no browser, extra editing surface or runtime dependency.

```markdown
> First quoted line.
> Second quoted line.
>
> A separate paragraph.
> > A nested quote.
```

## Edit

- Ordinary explicit quote paragraphs hide their literal `>` prefixes while inactive. There is **no substitute border/fringe**.
- Click anywhere in the paragraph, or select it, to reveal the exact prefix for editing. Search touching that prefix reveals it too. Leaving the paragraph hides it again.
- Prefixes retain their native advance widths; wrapped text keeps hanging indentation and the body does not jump when `>` appears. Nested `> >`/`>>` prose gets deeper indentation.
- Quote prose is **justified** with native paragraph formatting. This is automatic quote styling, not a new global alignment command. Ordinary prose, code and tables keep their own alignment. Removing `>` restores ordinary paragraph alignment.
- The literal characters remain in the canonical Markdown. Copy, selection, UTF-16 mapping, native IME and source undo are unchanged; styling adds no undo operation. There is no new shortcut.

This is conservative paragraph preview, not another complete Markdown parser. Quoted fences, indented code, HTML, lists, tables and ambiguous structures retain source/native fallback. Explicit quote markers are required for the new preview; unmarked lazy continuations are not newly classified.

## Read: keep quoted lines separate

Consecutive explicit quote-prose lines at the same depth retain their visible source line breaks, instead of becoming one space-joined sentence. Blank `>` lines still separate paragraphs, existing hard breaks remain breaks, and ordinary non-quote soft breaks keep normal Markdown behavior. Justification applies to wrapped quote prose; a forced break can end a ragged line, like the last line of a paragraph.

The preparation does not rewrite saved Markdown. After existing source-offset object replacements, disposable unforgeable markers identify supported boundaries during native Markdown import. Exact unique markers and adjacent separator characters are validated before native line-separator restoration. An invalid mapping reimports without markers, so none can escape into the displayed text or clipboard. This also avoids Qt's hard-break import turning every quoted line into a separately spaced paragraph.

Multiline inline-code/link/image/reference syntax, quoted code/HTML/lists/tables and ambiguous contexts are not given synthetic breaks inside their syntax. Angle autolinks (`<https://…>`) and literal comparisons (`one < two`) remain supported prose. No whole-document source/native block-number guesses are used.

## Verification — 2026-11-10

macOS arm64, Qt 6.12: **365 passed, zero failures, four normal-run skips** across twelve suites. New native-document regressions cover hidden/active/selected/searched prefixes, stable hanging geometry, nested quotes, justification reset, edits after a collapsed table/UTF-16 prefix, one source undo, unchanged projection counters on caret movement, incremental-versus-fresh parsing, final Read HTML-round-trip line breaks, autolinks/comparisons and multiline link/image identity.

Eight native Cocoa UI bodies passed (ten checks including setup/cleanup): the new physical quote-body click/native IME commit/one-undo/Read-break test in both themes, plus existing table/modal/copy/scroll/code regressions. Both threaded-render bodies passed separately (six checks, 100 mode switches). The development app rebuilt as 0.7.2. Windows/Linux were not compiled or runtime-tested. `/Applications` copies and private input files were not modified.
