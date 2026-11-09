#include "previewblocks.h"
#include "markdownhighlighter.h"
#include "attachmenthelper.h"

#include <QAbstractTextDocumentLayout>
#include <QFile>
#include <QFontMetricsF>
#include <QImageReader>
#include <QPainter>
#include <QRegularExpression>
#include <QTextDocument>
#include <QTextOption>
#include <cmath>

namespace {

struct Line {
    int start;
    int end;
    QString text;
};

bool allowedIndent(const QString &text) {
    int spaces = 0;
    while (spaces < text.size() && text.at(spaces) == QLatin1Char(' '))
        ++spaces;
    return spaces <= 3 && (spaces == text.size() || text.at(spaces) != QLatin1Char('\t'));
}

// Split only on real pipes, not escaped pipes or pipes in matched code spans.
QStringList cells(const QString &line) {
    if (!allowedIndent(line))
        return {};
    // These start other Markdown blocks rather than table rows.
    static const QRegularExpression otherBlock(
        QStringLiteral("^(?:>|#{1,6}(?:\\s|$)|[-+*]\\s|[0-9]{1,9}[.)]\\s)"));
    if (otherBlock.match(line.trimmed()).hasMatch())
        return {};
    const auto spans = MarkdownHighlighter::codeSpans(line);
    QList<int> pipes;
    int span = 0;
    for (int i = 0; i < line.size(); ++i) {
        while (span < spans.size() && i >= spans.at(span).start + spans.at(span).length)
            ++span;
        if (span < spans.size() && i >= spans.at(span).start)
            continue;
        if (line.at(i) == QLatin1Char('\\')) {
            ++i;
        } else if (line.at(i) == QLatin1Char('|')) {
            pipes.append(i);
        }
    }
    if (pipes.isEmpty())
        return {};
    QStringList result;
    int start = 0;
    for (int pipe : pipes) {
        result.append(line.mid(start, pipe - start).trimmed());
        start = pipe + 1;
    }
    result.append(line.mid(start).trimmed());
    if (line.left(pipes.first()).trimmed().isEmpty())
        result.removeFirst();
    if (line.mid(pipes.last() + 1).trimmed().isEmpty() && !result.isEmpty())
        result.removeLast();
    return result;
}

bool delimiter(const QStringList &row) {
    static const QRegularExpression re(QStringLiteral("^:?-{3,}:?$"));
    if (row.isEmpty())
        return false;
    for (const QString &cell : row) {
        if (!re.match(cell).hasMatch())
            return false;
    }
    return true;
}

bool punctuation(QChar c) {
    const ushort u = c.unicode();
    return (u >= 33 && u <= 47) || (u >= 58 && u <= 64)
        || (u >= 91 && u <= 96) || (u >= 123 && u <= 126);
}

QString unescape(const QString &text) {
    QString result;
    for (int i = 0; i < text.size(); ++i) {
        if (text.at(i) == QLatin1Char('\\') && i + 1 < text.size()
            && punctuation(text.at(i + 1)))
            ++i;
        result += text.at(i);
    }
    return result;
}

bool standaloneImage(const QString &line, QString *path) {
    if (!allowedIndent(line))
        return false;
    const QString text = line.trimmed();
    if (!text.startsWith(QStringLiteral("![")))
        return false;
    int i = 2;
    int brackets = 1;
    for (; i < text.size(); ++i) {
        if (text.at(i) == QLatin1Char('\\')) {
            ++i;
        } else if (text.at(i) == QLatin1Char('[')) {
            ++brackets;
        } else if (text.at(i) == QLatin1Char(']') && --brackets == 0) {
            break;
        }
    }
    if (i + 1 >= text.size() || text.at(++i) != QLatin1Char('('))
        return false;
    ++i;
    while (i < text.size() && text.at(i).isSpace())
        ++i;
    const bool angled = i < text.size() && text.at(i) == QLatin1Char('<');
    if (angled)
        ++i;
    const int start = i;
    int depth = 0;
    for (; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (c == QLatin1Char('\\') && i + 1 < text.size()
            && punctuation(text.at(i + 1))) {
            ++i;
            continue;
        }
        if (angled) {
            if (c == QLatin1Char('>'))
                break;
            if (c == QLatin1Char('<'))
                return false;
        } else {
            if (c.isSpace() || (c == QLatin1Char(')') && depth == 0))
                break;
            if (c == QLatin1Char('('))
                ++depth;
            else if (c == QLatin1Char(')'))
                --depth;
        }
    }
    if (depth != 0 || i == start || i >= text.size())
        return false;
    const QString destination = text.mid(start, i - start);
    if (angled)
        ++i;
    const int beforeSpace = i;
    while (i < text.size() && text.at(i).isSpace())
        ++i;
    if (i < text.size() && text.at(i) != QLatin1Char(')')) {
        if (i == beforeSpace)
            return false;
        const QChar quote = text.at(i++);
        if (quote != QLatin1Char('"') && quote != QLatin1Char('\'') && quote != QLatin1Char('('))
            return false;
        const QChar closing = quote == QLatin1Char('(') ? QLatin1Char(')') : quote;
        while (i < text.size() && text.at(i) != closing) {
            if (text.at(i) == QLatin1Char('\\') && i + 1 < text.size()
                && punctuation(text.at(i + 1)))
                ++i;
            ++i;
        }
        if (i >= text.size())
            return false;
        ++i;
        while (i < text.size() && text.at(i).isSpace())
            ++i;
    }
    if (i != text.size() - 1 || text.at(i) != QLatin1Char(')'))
        return false;
    *path = unescape(destination);
    return true;
}

// Rendering a table must not implicitly load inline image resources.
class TableDocument : public QTextDocument {
protected:
    QVariant loadResource(int, const QUrl &) override { return {}; }
};

constexpr qint64 MaxPixels = Attachments::MaxImagePixels;
constexpr int MaxHeight = 1800;

} // namespace

namespace PreviewBlocks {

QList<Block> parse(const QString &source) {
    QList<Line> lines;
    for (int start = 0; start < source.size();) {
        int end = source.indexOf(QLatin1Char('\n'), start);
        if (end < 0)
            end = int(source.size());
        int contentEnd = end;
        if (contentEnd > start && source.at(contentEnd - 1) == QLatin1Char('\r'))
            --contentEnd;
        lines.append({start, contentEnd, source.mid(start, contentEnd - start)});
        start = end + 1;
    }
    QList<Block> result;
    auto append = [&](int first, int last, Kind kind, const QString &path = QString()) {
        const int start = lines.at(first).start;
        const int end = lines.at(last).end;
        result.append({start, end, kind, source.mid(start, end - start), path});
    };
    bool htmlComment = false;
    QString htmlTag;
    for (int i = 0; i < lines.size();) {
        const QString &text = lines.at(i).text;
        // Do not project Markdown-looking lines inside literal HTML/comments.
        static const QRegularExpression htmlStart(QStringLiteral("^ {0,3}<([A-Za-z][A-Za-z0-9-]*)(?:[ >\\t/])"));
        const auto tag = htmlStart.match(text);
        if (htmlComment) {
            htmlComment = !text.contains(QStringLiteral("-->"));
            ++i;
            continue;
        }
        if (!htmlTag.isEmpty()) {
            const bool literal = htmlTag == "pre" || htmlTag == "script" || htmlTag == "style" || htmlTag == "textarea";
            if ((!literal && text.trimmed().isEmpty())
                || text.contains(QStringLiteral("</") + htmlTag + QLatin1Char('>'), Qt::CaseInsensitive))
                htmlTag.clear();
            ++i;
            continue;
        }
        if (text.trimmed().startsWith(QStringLiteral("<!--"))) {
            htmlComment = !text.contains(QStringLiteral("-->"));
            ++i;
            continue;
        }
        if (tag.hasMatch()) {
            htmlTag = tag.captured(1).toLower();
            static const QStringList voidTags = {"area", "base", "br", "col", "embed", "hr", "img", "input", "link", "meta", "param", "source", "track", "wbr"};
            if (text.contains(QStringLiteral("</") + htmlTag + QLatin1Char('>'), Qt::CaseInsensitive)
                || text.trimmed().endsWith(QStringLiteral("/>")) || voidTags.contains(htmlTag))
                htmlTag.clear();
            ++i;
            continue;
        }
        const int fenceLength = MarkdownHighlighter::openingFenceLength(text);
        if (fenceLength) {
            const QString trimmed = text.trimmed();
            const QChar marker = trimmed.at(0);
            const bool mermaid = trimmed.mid(fenceLength).trimmed() == QStringLiteral("mermaid");
            int last = i + 1;
            while (last < lines.size()
                   && !MarkdownHighlighter::closesFence(lines.at(last).text, fenceLength, marker))
                ++last;
            if (last == lines.size())
                --last;
            if (mermaid)
                append(i, last, Kind::Mermaid);
            i = last + 1;
            continue;
        }
        // A blank/start boundary avoids turning Setext heading underlines into
        // rules. Other ambiguous container syntax stays editable Markdown.
        static const QRegularExpression horizontalRule(QStringLiteral("^ {0,3}(?:-[ \\t]*){3,}$"));
        if (horizontalRule.match(text).hasMatch()
            && (i == 0 || lines.at(i - 1).text.trimmed().isEmpty()
                || horizontalRule.match(lines.at(i - 1).text).hasMatch())) {
            append(i, i, Kind::HorizontalRule);
            ++i;
            continue;
        }
        QString path;
        if (standaloneImage(text, &path)) {
            append(i, i, Kind::Image, path);
            ++i;
            continue;
        }
        const QStringList header = cells(text);
        if (!header.isEmpty() && i + 1 < lines.size()) {
            const QStringList rule = cells(lines.at(i + 1).text);
            if (rule.size() == header.size() && delimiter(rule)) {
                int last = i + 1;
                while (last + 1 < lines.size()
                       && !MarkdownHighlighter::openingFenceLength(lines.at(last + 1).text)
                       && cells(lines.at(last + 1).text).size() == header.size())
                    ++last;
                append(i, last, Kind::Table);
                i = last + 1;
                continue;
            }
        }
        ++i;
    }
    return result;
}

QImage renderHorizontalRule(const QFont &font, const QColor &foreground, int maxWidth) {
    if (maxWidth <= 0)
        return {};
    QImage image(qMin(maxWidth, 8192), qMax(12, int(std::ceil(QFontMetricsF(font).height()))),
                 QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    QColor color = foreground;
    color.setAlphaF(color.alphaF() * 0.5);
    painter.setPen(color);
    painter.drawLine(0, image.height() / 2, image.width() - 1, image.height() / 2);
    return image;
}

QImage renderTable(const QString &markdown, const QFont &font,
                   const QColor &foreground, const QColor &background, int maxWidth) {
    if (maxWidth <= 0 || markdown.isEmpty())
        return {};
    TableDocument document;
    document.setDefaultFont(font);
    document.setDefaultStyleSheet(QStringLiteral("body, table, td, th, p { color: %1; }")
                                     .arg(foreground.name(QColor::HexRgb)));
    document.setMarkdown(markdown, QTextDocument::MarkdownDialectGitHub);
    QTextOption option = document.defaultTextOption();
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    document.setDefaultTextOption(option);
    const int width = qMin(maxWidth, 8192);
    document.setTextWidth(width);
    const QSizeF size = document.documentLayout()->documentSize();
    if (!std::isfinite(size.height()) || !std::isfinite(size.width())
        || size.height() <= 0 || size.width() <= 0)
        return {};
    const qreal scale = qMin(qreal(1), width / size.width());
    const qreal scaledHeight = std::ceil(size.height() * scale);
    if (scaledHeight > MaxPixels / width)
        return {};
    const int height = qMax(1, int(scaledHeight));
    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    if (image.isNull())
        return {};
    image.fill(background);
    QPainter painter(&image);
    painter.scale(scale, scale);
    QAbstractTextDocumentLayout::PaintContext context;
    context.palette.setColor(QPalette::Text, foreground);
    context.palette.setColor(QPalette::WindowText, foreground);
    context.palette.setColor(QPalette::Base, background);
    context.clip = QRectF(0, 0, width / scale, height / scale);
    document.documentLayout()->draw(&painter, context);
    return image;
}

QImage loadImage(const QUrl &url, int maxWidth) {
    if (maxWidth <= 0) return {};
    QImage image = Attachments::loadLocalImage(url);
    if (image.isNull()) return {};
    if (image.width() > maxWidth || image.height() > MaxHeight)
        image = image.scaled(QSize(maxWidth, MaxHeight), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return image;
}

} // namespace PreviewBlocks
