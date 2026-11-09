#pragma once
#include "markdownhighlighter.h"
#include <QStringList>
#include <QList>

// Conservative, view-only list paragraphs. No source characters are replaced.
namespace ListPreview {
struct Item { int block, prefix, depth; QString marker; };
inline QList<Item> parse(const QString &text) {
    struct Level { int indent, content, number; bool ordered; QChar delimiter; };
    QList<Level> levels;
    QList<Item> result;
    const auto lines = text.split(QLatin1Char('\n'));
    static const QRegularExpression itemRe(QStringLiteral("^( *)([-+*]|[0-9]{1,9}[.)])( +)(.*)$"));
    static const QRegularExpression cellRe(QStringLiteral("^ *:?-{3,}:? *$"));
    static const QRegularExpression htmlRe(QStringLiteral("^</?(address|article|aside|base|basefont|blockquote|body|caption|center|col|colgroup|dd|details|dialog|dir|div|dl|dt|fieldset|figcaption|figure|footer|form|frame|frameset|h[1-6]|head|header|hr|html|iframe|legend|li|link|main|menu|menuitem|nav|noframes|ol|optgroup|option|p|param|pre|script|search|section|source|style|summary|table|tbody|td|th|thead|title|tr|track|ul|textarea)(?:\\s|>|/)"), QRegularExpression::CaseInsensitiveOption);
    int fence = 0, fenceIndent = 0;
    QChar fenceMarker;
    bool comment = false, table = false, blank = true;
    QString html;
    const auto delimiter = [&](const QString &line) {
        if (!line.contains(QLatin1Char('|'))) return false;
        auto cells = line.trimmed().split(QLatin1Char('|'));
        if (cells.first().isEmpty()) cells.removeFirst();
        if (!cells.isEmpty() && cells.last().isEmpty()) cells.removeLast();
        if (cells.isEmpty()) return false;
        for (const auto &cell : cells) if (!cellRe.match(cell).hasMatch()) return false;
        return true;
    };
    for (int n = 0; n < lines.size(); ++n) {
        const QString &line = lines[n];
        const QString trimmed = line.trimmed();
        int leading = 0;
        while (leading < line.size() && line[leading] == QLatin1Char(' ')) ++leading;
        if (fence) {
            if (MarkdownHighlighter::closesFence(line.mid(qMin(leading, fenceIndent)), fence, fenceMarker)) { fence = 0; blank = true; }
            continue;
        }
        if (comment) { if (line.contains(QStringLiteral("-->"))) comment = false; continue; }
        if (trimmed.startsWith(QStringLiteral("<!--"))) {
            comment = !trimmed.contains(QStringLiteral("-->")); levels.clear(); continue;
        }
        if (!html.isEmpty()) {
            const bool raw = html == "pre" || html == "script" || html == "style" || html == "textarea";
            if ((raw && trimmed.contains("</" + html, Qt::CaseInsensitive)) || (!raw && trimmed.isEmpty())) { html.clear(); blank = true; }
            continue;
        }
        const auto tag = htmlRe.match(trimmed);
        static const QRegularExpression completeTag(QStringLiteral("^</?[A-Za-z][A-Za-z0-9-]*(?:\\s+[^<>]*)?\\s*/?>\\s*$"));
        if (!tag.hasMatch() && blank && completeTag.match(trimmed).hasMatch()) { html = "block"; levels.clear(); continue; }
        if (tag.hasMatch()) {
            html = tag.captured(1).toLower();
            const bool raw = html == "pre" || html == "script" || html == "style" || html == "textarea";
            if (trimmed.startsWith("</")) html = "block";
            else if (raw && trimmed.contains("</" + html, Qt::CaseInsensitive)) { html.clear(); blank = true; }
            levels.clear(); continue;
        }
        if (table) { if (trimmed.isEmpty() || !line.contains('|')) table = false; else continue; }
        if (n + 1 < lines.size() && line.contains('|') && delimiter(lines[n + 1])) { table = true; levels.clear(); continue; }
        if (trimmed.isEmpty()) { blank = true; continue; }
        int enclosing = int(levels.size()) - 1;
        while (enclosing >= 0 && leading < levels[enclosing].content) --enclosing;
        const int container = enclosing >= 0 ? levels[enclosing].content : 0;
        const auto candidate = line.mid(qMin(leading, container));
        fence = MarkdownHighlighter::openingFenceLength(candidate);
        if (fence) {
            while (levels.size() > enclosing + 1) levels.removeLast(); // Keep the actual containing list, not a deeper sibling.
            fenceIndent = container; fenceMarker = candidate.trimmed().at(0); continue;
        }
        if (MarkdownHighlighter::isThematicBreak(line)) { levels.clear(); blank = true; continue; }
        if (line.contains('\t') || trimmed.startsWith('>')) { levels.clear(); blank = false; continue; }
        const auto match = itemRe.match(line);
        if (match.hasMatch()) {
            if (leading >= 4 && (levels.isEmpty() || leading >= levels.last().content + 4)) continue; // Indented code.
            while (levels.size() > 1 && leading < levels.last().indent) levels.removeLast();
            const bool ordered = match.captured(2).at(0).isDigit();
            const int number = ordered ? match.captured(2).chopped(1).toInt() : 0;
            const QChar delimiter = ordered ? match.captured(2).back() : QChar();
            if (levels.isEmpty() && !blank && ((ordered && number != 1) || match.captured(4).trimmed().isEmpty())) continue;
            if (levels.isEmpty() || leading >= levels.last().content)
                levels.append({levels.isEmpty() ? 0 : leading, int(match.capturedEnd(3)), number, ordered, delimiter});
            else if (levels.last().ordered != ordered || levels.last().delimiter != delimiter)
                levels.last() = {levels.last().indent, int(match.capturedEnd(3)), number, ordered, delimiter};
            else if (ordered) ++levels.last().number;
            levels.last().content = match.capturedEnd(3);
            const QString marker = ordered ? QString::number(levels.last().number) + delimiter
                : QString(QChar(levels.size() % 3 == 2 ? 0x25e6 : levels.size() % 3 == 0 ? 0x25aa : 0x2022));
            result.append({n, int(match.capturedEnd(3)), int(levels.size()), marker});
            blank = false;
        } else {
            if (blank) while (levels.size() > 1 && leading < levels.last().content) levels.removeLast();
            if (!levels.isEmpty() && !trimmed.startsWith('#')
                && leading < levels.last().content + 4 && (!blank || leading >= levels.last().content)) {
                result.append({n, leading, int(levels.size()), {}}); // Explicit/lazy continuation.
                blank = false;
            } else {
                levels.clear(); blank = trimmed.startsWith('#');
            }
        }
    }
    return result;
}
}
