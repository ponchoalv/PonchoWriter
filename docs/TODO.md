# Small future improvements

## Code/table overflow — implemented

`feature/table-grid-and-horizontal-overflow` adds unwrapped native code/raw-table paragraphs, per-block horizontal scrolling in both modes, and a virtualized Edit table grid with a Source escape hatch. Prose wrapping and the canonical undo stack remain intact. See [behavior, limits and verification](tables-and-horizontal-overflow.md).

An optional wrap preference is not implemented; Read's native table layout is not virtualized. Revisit those only with measured demand, without adding a browser or spreadsheet engine.
