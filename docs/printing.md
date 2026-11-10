# Printing and PDF layout

PonchoWriter prints a dedicated native `QTextDocument` snapshot of the canonical Markdown—not the scrolled Edit/Read screen, table-grid placeholders or preview thumbnails. No browser, external converter or new dependency is required.

## Use

1. Finish the current table-cell edit, then press **Cmd+P** on macOS or **Ctrl+P** on Windows/Linux. Invalid or composing table drafts still prevent printing until resolved.
2. In **Print layout**, leave **Move wide tables to appendix** enabled (the default), or disable it to keep tables in the body. Choose **Continue…**. Cancel leaves the document untouched.
3. In the system print dialog, choose a printer or the desktop's PDF destination, paper, body orientation, margins, copies and page ranges. Default margins are 15 mm. PDF controls depend on the platform/printer; this is not a separate Export PDF command.
4. Missing, unsupported, oversized or nonlocal images produce explicit placeholders and a confirmation before output. Cancel is the default; printing never silently substitutes a preview image.
5. Preparation or output failures open a visible **Print failed** error dialog and also update the status footer. Choosing a PDF destination does not guarantee a file was written if preparation subsequently fails.

The appendix preference is remembered locally as `print/wideTablesToAppendix`. Printing does not require saving the note first, materializing its staged images or switching modes. The current buffer is used, including unsaved changes. Source text, document modified state and undo history are not edited by preparation.

## Images

- Relative images resolve against the note's location; unsaved/recovered screenshot attachments resolve through their persistent staging records.
- Once an attachment has been saved, the actual document-relative file takes precedence over its old staged copy. Save As and reopening use the destination note's resources.
- Inline and reference Markdown images are supported. Original decoded pixels are registered in the print document; only their **physical display dimensions** are reduced to fit the page/cell. Aspect ratio is preserved and small images are not enlarged.
- Local-only loading uses the existing decoder and file/path protections. No HTTP, data-URL or default/global Qt resource fallback is used. Images in table appendices are registered in each output section too.

## Mermaid

Mermaid fences currently print their **code-block source**, not rendered diagrams, even if Edit/Read shows a rendered image and `mmdc` is installed. Printing does not consult the preview renderer/cache or start a renderer. To print a diagram as artwork today, export it to a local image and reference that image with ordinary Markdown.

## Tables: width, not row count

**Long but narrow:** remains inline and continues across pages, with the first row repeated as the header. Cells wrap, borders remain visible and headers have a light-gray background. Tall rows can span pages rather than discarding their remaining content.

**Too wide for the chosen body page:** replaced in the body by a reference such as **“See Appendix A — Table 2”**. The native table starts in a separate **landscape** section using the same paper size and margins. Appendices follow the body, in source order.

**Too wide even in landscape:** divided into consecutive column groups. Every group repeats **column 1** and the header; all other columns appear in their original order. Column 1 is repeated for context—it need not be a unique identifier. Each group begins on its own section/page and can continue vertically.

Sparse or ragged Markdown rows that Qt imports as merged native cells are eligible for appendices too. Column groups expand those cells into logical grid positions: rich text is retained once at the cell's anchor, with covered positions left empty. This avoids skipping the appendix or duplicating a merged value across columns; column 1 still repeats for context.

Width classification uses content/font measurements and bounded readable column estimates, not an arbitrary 500-row cutoff. Body text uses the editor's font family at **11 pt**; table text uses **9.5 pt**. Editor zoom, dark theme, horizontal scroll offsets and virtualized grid state do not determine print geometry. Extremely long cell text wraps rather than causing a progressively tiny font.

Fixed table/image format lengths are converted to Qt's platform units before layout. This avoids a Cocoa DPI mismatch that previously expanded table widths by about one third and incorrectly rejected appendix tables despite passing offscreen tests. Point font sizes and physical page coordinates are not scaled twice.

Disabling appendices keeps ordinary wide tables inline with wrapping. It cannot make an arbitrarily dense table printable: if actual native table bounds exceed the selected page, preparation fails explicitly. Enable appendices or select a wider page instead of accepting clipped columns.

## Pages and printer limitations

- Page numbers in print ranges are **global output order**: body pages, then appendix/group pages. Ranges do not refer to source lines or on-screen positions. The final page count is computed after accepting the dialog's geometry, not previewed in the initial dialog.
- Disjoint ranges, appendix-only starts and reverse page order are honored. Copies/collation are left to drivers with native support; otherwise pages are repeated in software with an explicit output budget.
- The initial page layout is established before painting. Mixed-orientation transitions use Qt's orientation change before starting the next page; physical printer/duplex support depends on the driver.
- If a device rejects the layout or a page transition, printing aborts with an error. Pages already sent to a device cannot be recalled. For an unsupported mixed-orientation driver, retry with the **whole job set to Landscape** in the dialog, or inspect a PDF first. A landscape body also makes its appendices landscape.
- “Print job sent” means Qt accepted the drawing job, not that a physical device confirmed successful delivery.
- This pipeline targets Markdown. Arbitrary raw HTML table/styling/image structures are not interpreted (`MarkdownNoHTML`); use Markdown equivalents. It does not print interactive grid controls or require Mermaid/browser rendering.

## Safety bounds

Preparation is synchronous but bounded. Conservative structural checks run **before** native table import, including ragged tables, quotes and adjacent tables; they may reject unusually pipe-dense input rather than risk a huge padded native table.

| Limit | Bound |
| --- | --- |
| Source | 8,388,608 UTF-16 code units / 50,000 source lines |
| Apparent columns / estimated original cells | 1,024 / 100,000 |
| Repeated appendix output | 200,000 cells / 67,108,864 UTF-16 units |
| Local image decode | 20 MiB per file / 24 million pixels |
| Retained original print images | 128 unique images / 64 MiB decoded pixels |
| Generated document | 2,000 pages, checked after body and each new section |
| Software copies | At most 1,000 copies / 10,000 rendered pages |

These are not a whole-process memory cap. Native layout, source snapshots, transient decode and printer spooling have additional costs. Unsupported or over-budget jobs report an error; unavailable images require placeholder confirmation.

## Verification

On macOS arm64 / Qt 6.12.0, 2026-11-10:

- **384 checks passed, zero failures, four normal-run skips**, across thirteen suites. Printing contributes thirteen checks, including setup/cleanup; the attachment suite exercises actual staged/recovered/saved/Save As resolution without source or recovery-file mutation, plus the visible failure dialog.
- Printing tests cover original pixels, blocked fallback resources, native table widths, rendered continuation headers, oversized rows, appendix images, all source cells across column bands, sparse/merged native cells without duplicate values, option-off behavior, preallocation rejection, PDF page sizes, disjoint/global ranges and software copies/collation.
- Independent Apple PDFKit reading/rasterization verified a generated synthetic PDF: **three pages (one portrait/two landscape), a visible red image and all 112 synthetic table cells extractable**.
- Existing native UI regressions passed **eleven checks** (nine bodies), and threaded rendering passed **six checks / 100 mode switches**. The current thirteen-check print suite passed under Cocoa with actual platform DPI and offscreen with a 144-DPI baseline; targeted geometry checks also passed at 72 DPI. The three-check visible-error fixture passed under Cocoa at the earlier checkpoint. The macOS application rebuilt as 0.7.2.
- The print test launcher deliberately does **not** use `QTEST_MAIN`: that macro forces `AA_Use96Dpi`, which masked the production Cocoa 72-vs-96 DPI mismatch in the earlier native test run. Three synthetic margin cases now check actual table bounds and native image advances. All three fail against the old product code under Cocoa and pass after the fix. Table fit tolerances were not relaxed.
- An opt-in native-print-to-file attempt found **no native printer engine**: Qt fell back to PDF and that native-driver check explicitly skipped. Physical printers, duplex and actual native-driver mixed-orientation behavior remain **unverified**. Windows/Linux have not been compiled or runtime-tested here.

To rerun the print suite after `bin/test`, use the executable generated in `build-tests/printing` (`tst_printing.app/Contents/MacOS/tst_printing` on macOS). `PONCHOWRITER_PRINT_TEST_PDF=/absolute/new/path.pdf` optionally retains its generated synthetic PDF; the path must not already exist. On Cocoa, `PONCHOWRITER_NATIVE_PRINT_TEST=1` additionally attempts native print-to-file, explicitly skipping if Qt cannot supply that engine. It never addresses a physical printer or opens a system print dialog.

No private document contents or generated private-input reports are included in the fixtures or documentation.
