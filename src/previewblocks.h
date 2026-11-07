#pragma once

#include <QColor>
#include <QFont>
#include <QImage>
#include <QList>
#include <QString>
#include <QUrl>

namespace PreviewBlocks {

enum class Kind { Table, Image, Mermaid };

struct Block {
    int start = 0;
    int end = 0;
    Kind kind = Kind::Table;
    QString markdown;
    QString imagePath;
};

// UTF-16, half-open source ranges; the final line ending is not included.
// Tables require pipe-separated, equally sized rows and >=3 hyphens per
// delimiter cell (optional alignment colons). Images must occupy a whole line;
// destinations with spaces require <...>. Up to three leading spaces are allowed.
// Fenced content is ignored except exact, case-sensitive "mermaid" info strings;
// an unclosed Mermaid fence extends to EOF. Reference images are not recognized.
QList<Block> parse(const QString &source);
QImage renderTable(const QString &markdown, const QFont &font,
                   const QColor &foreground, const QColor &background, int maxWidth);
// Local file URLs or relative paths only; no network or data URLs. Input is
// limited to 20 MiB and 24 MP before decode; output fits maxWidth x 1800 without
// upscaling. Invalid, oversized or unsupported images return a null image.
QImage loadImage(const QUrl &url, int maxWidth);

} // namespace PreviewBlocks
