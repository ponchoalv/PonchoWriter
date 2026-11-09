#pragma once
#include "markdownhighlighter.h"
#include <QStringList>
#include <QList>
#include <QTextDocument>
#include <QTextBlock>
#include <algorithm>

// Conservative, view-only list paragraphs. Checkpoints belong to this parser,
// not QTextBlockUserData, which is reserved for the embedded code lexer.
namespace ListPreview {
struct Item { int block, prefix, depth; QString marker; };
struct Level {
    int indent, content, number; bool ordered; QChar delimiter;
    bool operator==(const Level &b) const {
        return indent == b.indent && content == b.content && number == b.number
            && ordered == b.ordered && delimiter == b.delimiter;
    }
};
struct State {
    QList<Level> levels;
    int fence = 0, fenceIndent = 0;
    QChar fenceMarker;
    bool comment = false, table = false, blank = true;
    QString html;
    bool operator==(const State &b) const {
        return levels == b.levels && fence == b.fence && fenceIndent == b.fenceIndent
            && fenceMarker == b.fenceMarker && comment == b.comment && table == b.table
            && blank == b.blank && html == b.html;
    }
};
inline QList<Item> step(State &state, const QString &line, const QString &next, int n) {
    auto &levels = state.levels;
    int &fence = state.fence, &fenceIndent = state.fenceIndent;
    QChar &fenceMarker = state.fenceMarker;
    bool &comment = state.comment, &table = state.table, &blank = state.blank;
    QString &html = state.html;
    QList<Item> result;
    static const QRegularExpression itemRe(QStringLiteral("^( *)([-+*]|[0-9]{1,9}[.)])( +)(.*)$"));
    static const QRegularExpression cellRe(QStringLiteral("^ *:?-{3,}:? *$"));
    static const QRegularExpression htmlRe(QStringLiteral("^</?(address|article|aside|base|basefont|blockquote|body|caption|center|col|colgroup|dd|details|dialog|dir|div|dl|dt|fieldset|figcaption|figure|footer|form|frame|frameset|h[1-6]|head|header|hr|html|iframe|legend|li|link|main|menu|menuitem|nav|noframes|ol|optgroup|option|p|param|pre|script|search|section|source|style|summary|table|tbody|td|th|thead|title|tr|track|ul|textarea)(?:\\s|>|/)"), QRegularExpression::CaseInsensitiveOption);
    const auto delimiter = [&](const QString &text) {
        if (!text.contains(QLatin1Char('|'))) return false;
        auto cells = text.trimmed().split(QLatin1Char('|'));
        if (cells.first().isEmpty()) cells.removeFirst();
        if (!cells.isEmpty() && cells.last().isEmpty()) cells.removeLast();
        if (cells.isEmpty()) return false;
        for (const auto &cell : cells) if (!cellRe.match(cell).hasMatch()) return false;
        return true;
    };
    const QString trimmed = line.trimmed();
    int leading = 0;
    while (leading < line.size() && line[leading] == QLatin1Char(' ')) ++leading;
    if (fence) {
        if (MarkdownHighlighter::closesFence(line.mid(qMin(leading, fenceIndent)), fence, fenceMarker)) { fence = 0; blank = true; }
        return result;
    }
    if (comment) { if (line.contains(QStringLiteral("-->"))) comment = false; return result; }
    if (trimmed.startsWith(QStringLiteral("<!--"))) {
        comment = !trimmed.contains(QStringLiteral("-->")); levels.clear(); return result;
    }
    if (!html.isEmpty()) {
        const bool raw = html == "pre" || html == "script" || html == "style" || html == "textarea";
        if ((raw && trimmed.contains("</" + html, Qt::CaseInsensitive)) || (!raw && trimmed.isEmpty())) { html.clear(); blank = true; }
        return result;
    }
    const auto tag = htmlRe.match(trimmed);
    static const QRegularExpression completeTag(QStringLiteral("^</?[A-Za-z][A-Za-z0-9-]*(?:\\s+[^<>]*)?\\s*/?>\\s*$"));
    if (!tag.hasMatch() && blank && completeTag.match(trimmed).hasMatch()) { html = "block"; levels.clear(); return result; }
    if (tag.hasMatch()) {
        html = tag.captured(1).toLower();
        const bool raw = html == "pre" || html == "script" || html == "style" || html == "textarea";
        if (trimmed.startsWith("</")) html = "block";
        else if (raw && trimmed.contains("</" + html, Qt::CaseInsensitive)) { html.clear(); blank = true; }
        levels.clear(); return result;
    }
    if (table) { if (trimmed.isEmpty() || !line.contains('|')) table = false; else return result; }
    if (line.contains('|') && delimiter(next)) { table = true; levels.clear(); return result; }
    if (trimmed.isEmpty()) { blank = true; return result; }
    int enclosing = int(levels.size()) - 1;
    while (enclosing >= 0 && leading < levels[enclosing].content) --enclosing;
    const int container = enclosing >= 0 ? levels[enclosing].content : 0;
    const auto candidate = line.mid(qMin(leading, container));
    fence = MarkdownHighlighter::openingFenceLength(candidate);
    if (fence) {
        while (levels.size() > enclosing + 1) levels.removeLast();
        fenceIndent = container; fenceMarker = candidate.trimmed().at(0); return result;
    }
    if (MarkdownHighlighter::isThematicBreak(line)) { levels.clear(); blank = true; return result; }
    if (line.contains('\t') || trimmed.startsWith('>')) { levels.clear(); blank = false; return result; }
    const auto match = itemRe.match(line);
    if (match.hasMatch()) {
        if (leading >= 4 && (levels.isEmpty() || leading >= levels.last().content + 4)) return result;
        while (levels.size() > 1 && leading < levels.last().indent) levels.removeLast();
        const bool ordered = match.captured(2).at(0).isDigit();
        const int number = ordered ? match.captured(2).chopped(1).toInt() : 0;
        const QChar delimiter = ordered ? match.captured(2).back() : QChar();
        if (levels.isEmpty() && !blank && ((ordered && number != 1) || match.captured(4).trimmed().isEmpty())) return result;
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
            result.append({n, leading, int(levels.size()), {}});
            blank = false;
        } else { levels.clear(); blank = trimmed.startsWith('#'); }
    }
    return result;
}
inline QList<Item> parse(const QString &text) {
    State state;
    QList<Item> items;
    const auto lines = text.split(QLatin1Char('\n'));
    for (int n = 0; n < lines.size(); ++n) items.append(step(state, lines[n], lines.value(n + 1), n));
    return items;
}
class Parser {
    struct Row { QString text, next; State before, after; QList<Item> items; bool valid = false; };
    QList<Row> m_rows;
    int m_invalidLast = -1;
public:
    QList<Item> items;
    int changedFirst = 0, changedLast = -1, parsedLines = 0;
    void reset(const QTextDocument *document) {
        m_rows.clear(); items.clear(); m_rows.resize(document->blockCount()); m_invalidLast = document->blockCount() - 1; update(document, 0);
    }
    void splice(int first, int removedLines, int addedLines) {
        if (first < 0 || first + removedLines > m_rows.size()) { m_rows.clear(); m_invalidLast = -1; return; }
        m_invalidLast = qMax(first + addedLines - 1, m_invalidLast < first ? m_invalidLast : m_invalidLast + addedLines - removedLines);
        if (removedLines == addedLines) {
            for (int n = 0; n < addedLines; ++n) m_rows[first + n].valid = false;
        } else {
            m_rows.remove(first, removedLines);
            for (int n = 0; n < addedLines; ++n) m_rows.insert(first + n, Row());
            items.erase(std::remove_if(items.begin(), items.end(), [&](const Item &item) {
                return item.block >= first && item.block < first + removedLines;
            }), items.end());
            for (auto &item : items) if (item.block >= first + removedLines) item.block += addedLines - removedLines;
        }
    }
    void update(const QTextDocument *document, int dirtyFirst) {
        if (m_rows.size() != document->blockCount()) { reset(document); return; }
        changedFirst = qMax(0, dirtyFirst - 1); changedLast = changedFirst - 1; parsedLines = 0;
        const int dirtyLast = qMax(dirtyFirst, m_invalidLast);
        State state = changedFirst ? m_rows[changedFirst - 1].after : State();
        for (QTextBlock block = document->findBlockByNumber(changedFirst); block.isValid(); block = block.next()) {
            const int n = block.blockNumber();
            auto &row = m_rows[n];
            const QString text = block.text(), next = block.next().text();
            if (n > dirtyLast && row.valid && row.text == text && row.next == next && row.before == state) break;
            row.text = text; row.next = next; row.before = state;
            row.items = step(state, text, next, 0); row.after = state; row.valid = true;
            ++parsedLines; changedLast = n;
        }
        m_invalidLast = -1;
        auto a = std::lower_bound(items.cbegin(), items.cend(), changedFirst, [](const Item &item, int n) { return item.block < n; });
        auto b = std::upper_bound(items.cbegin(), items.cend(), changedLast, [](int n, const Item &item) { return n < item.block; });
        const int at = a - items.cbegin(), count = b - a;
        QList<Item> changed;
        for (int n = changedFirst; n <= changedLast; ++n)
            for (Item item : m_rows[n].items) { item.block = n; changed.append(item); }
        if (count == changed.size()) for (int n = 0; n < changed.size(); ++n) items[at + n] = changed[n];
        else { items.remove(at, count); for (int n = 0; n < changed.size(); ++n) items.insert(at + n, changed[n]); }
    }
};
}
