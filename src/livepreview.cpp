#include "livepreview.h"
#include "spellingcontroller.h"
#include "markdownhighlighter.h"
#include "mermaidrenderer.h"
#include "readcodehighlighting.h"

#include <QClipboard>
#include <QAbstractTextDocumentLayout>
#include <QGuiApplication>
#include <QInputMethodEvent>
#include <QInputMethod>
#include <QKeyEvent>
#include <QQuickTextDocument>
#include <QScopedValueRollback>
#include <QScopeGuard>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextImageFormat>
#include <QTextLayout>
#include <QTextTable>
#include <QSet>
#include <QTimer>
#include <algorithm>

namespace {
constexpr qsizetype MaxProjectionBytes = 32 * 1024 * 1024;
constexpr int MaxProjectionObjects = 64;
QString documentText(const QTextDocument *document) {
    return document->toRawText().replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
}
class ResourceFreeDocument : public QTextDocument {
protected:
    QVariant loadResource(int, const QUrl &) override { return {}; }
};
QColor strongForeground(QObject *view) {
    const QColor foreground = view->property("previewForeground").value<QColor>();
    return foreground.isValid() ? foreground : view->property("color").value<QColor>();
}

QVector<bool> listBlankLines(const QString &source) {
    // Qt drops list looseness during Markdown import. Keep only this small
    // piece of source spacing metadata; never infer gaps from list identity
    // (nested tight items belong to different QTextLists).
    QVector<bool> gaps;
    const QRegularExpression item(QStringLiteral("^\\s*(?:[-+*]|[0-9]+[.)])\\s+"));
    const QRegularExpression fence(QStringLiteral("^\\s*(`{3,}|~{3,})(.*)$"));
    QChar fenceCharacter;
    int fenceLength = 0;
    bool blank = false;
    for (const QString &line : source.split(QLatin1Char('\n'))) {
        const auto match = fence.match(line);
        if (fenceLength) {
            if (match.hasMatch() && match.captured(1).front() == fenceCharacter
                && match.capturedLength(1) >= fenceLength && match.captured(2).trimmed().isEmpty())
                fenceLength = 0;
            continue;
        }
        if (match.hasMatch()) {
            fenceCharacter = match.captured(1).front();
            fenceLength = match.capturedLength(1);
        } else if (item.match(line).hasMatch()) {
            gaps.append(blank);
            blank = false;
        } else if (line.trimmed().isEmpty()) {
            blank = true;
        }
    }
    return gaps;
}

void styleReadDocument(QTextDocument &document, const QString &source,
                       const QColor &foreground, const QColor &body, const QColor &accent, bool dark) {
    // Measure an actual blank editor line: pointSize() is -1 for pixel fonts,
    // which makes Qt's imported Markdown paragraph margins collapse.
    QTextLayout blank(QStringLiteral(" "), document.defaultFont());
    blank.beginLayout();
    QTextLine line = blank.createLine();
    line.setLineWidth(100);
    const qreal gap = line.height();
    blank.endLayout();
    const QColor codeBackground(dark ? QStringLiteral("#252525") : QStringLiteral("#eeeeee"));
    const QColor quoteColor(dark ? QStringLiteral("#4f525a") : QStringLiteral("#aeb1b5"));
    QSet<QTextTable *> tables;
    QVector<bool> listGaps = listBlankLines(source);
    int listCount = 0;
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next())
        if (block.textList()) ++listCount;
    // HTML/quoted/ambiguous list syntax is left tight rather than assigning
    // unrelated source whitespace to the wrong imported list item.
    if (listCount != listGaps.size()) listGaps.clear();
    int listIndex = -1;
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        QTextCursor cursor(block);
        QTextTable *table = cursor.currentTable();
        if (table) tables.insert(table);
        QTextBlockFormat format = block.blockFormat();
        const bool code = format.hasProperty(QTextFormat::BlockCodeLanguage);
        const QTextBlock next = block.next();
        const bool nextCode = next.isValid()
            && next.blockFormat().hasProperty(QTextFormat::BlockCodeLanguage);
        if (block.textList()) ++listIndex;
        const bool tightList = block.textList() && next.isValid() && next.textList()
            && !listGaps.value(listIndex + 1, false);
        const bool nextTable = next.isValid() && QTextCursor(next).currentTable();
        format.setTopMargin(0);
        format.setBottomMargin(!next.isValid() || table || nextTable || tightList
                               || (code && nextCode) || block.text().isEmpty() ? 0 : gap);
        cursor.setBlockFormat(format);
        const bool quote = format.intProperty(QTextFormat::BlockQuoteLevel) > 0;
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            const QTextCharFormat existing = fragment.charFormat();
            QTextCharFormat patch;
            patch.setForeground(quote ? quoteColor : (format.headingLevel() > 0 ? foreground : body));
            if (quote) patch.setFontItalic(true);
            // Explicit Markdown code property, not the resolved font: the
            // whole editor may itself use a monospace writing font.
            if (code || (existing.hasProperty(QTextFormat::FontFixedPitch) && existing.fontFixedPitch())) {
                patch.setForeground(foreground);
                patch.setBackground(code && format.hasProperty(QTextFormat::BlockCodeFence)
                                    ? QBrush(Qt::NoBrush) : QBrush(codeBackground));
                patch.setFontFamilies(document.defaultFont().families());
            }
            if (existing.isAnchor() && !existing.anchorHref().isEmpty()) {
                patch.setForeground(accent);
                patch.setFontUnderline(true);
            }
            cursor.setPosition(fragment.position());
            cursor.setPosition(fragment.position() + fragment.length(), QTextCursor::KeepAnchor);
            cursor.mergeCharFormat(patch);
        }
    }
    // Frame margins belong outside tables, never inside every cell paragraph.
    for (QTextTable *table : tables) {
        QTextTableFormat format = table->format();
        const QTextBlock before = table->firstCursorPosition().block().previous();
        format.setTopMargin(before.isValid() && !before.text().isEmpty() ? gap : 0);
        format.setBottomMargin(gap);
        table->setFormat(format);
    }
}
}

LivePreview::LivePreview(QObject *parent) : QObject(parent), m_mermaid(new MermaidRenderer(this)) {
    connect(m_mermaid, &MermaidRenderer::ready, this, &LivePreview::refresh);
}

QObject *LivePreview::spelling() const { return m_spelling; }
void LivePreview::setSpelling(QObject *spelling) {
    auto *controller = qobject_cast<SpellingController *>(spelling);
    if (m_spelling == controller)
        return;
    if (m_spelling)
        disconnect(m_spelling, nullptr, this, nullptr);
    m_spelling = controller;
    if (m_spelling)
        connect(m_spelling, &SpellingController::rangesChanged, this, &LivePreview::refreshSpelling);
    refreshSpelling();
}
QVariantMap LivePreview::spellingWordAt(int position) {
    if (!m_spelling || m_readMode || m_composing)
        return {};
    for (const auto &segment : m_segments) {
        if (position < segment.viewStart || position >= segment.viewEnd)
            continue;
        if (segment.object)
            return {};
        const auto token = m_spelling->wordAt(segment.sourceStart + position - segment.viewStart);
        const int from = token.value(QStringLiteral("start"), -1).toInt();
        const int to = from + token.value(QStringLiteral("length")).toInt();
        return from >= segment.sourceStart && to <= segment.sourceEnd ? token : QVariantMap();
    }
    // Keyboard menu at the final insertion point is still useful.
    if (!m_segments.isEmpty() && !m_segments.last().object && position == m_segments.last().viewEnd)
        return m_spelling->wordAt(m_segments.last().sourceEnd);
    return {};
}

QVariantMap LivePreview::spellingWordAtPoint(qreal x, qreal y) {
    if (!m_view || !m_viewItem || m_readMode || m_composing)
        return {};
    const QSizeF size = m_view->documentLayout()->documentSize();
    const qreal left = m_viewItem->property("leftPadding").toReal();
    const qreal top = m_viewItem->property("topPadding").toReal();
    qreal dx = m_viewItem->property("width").toReal() - left
        - m_viewItem->property("rightPadding").toReal() - size.width();
    qreal dy = m_viewItem->property("height").toReal() - top
        - m_viewItem->property("bottomPadding").toReal() - size.height();
    const int horizontal = m_viewItem->property("effectiveHorizontalAlignment").toInt();
    const int vertical = m_viewItem->property("verticalAlignment").toInt();
    dx = horizontal == Qt::AlignHCenter ? dx / 2 : horizontal == Qt::AlignRight ? dx : 0;
    dy = vertical == Qt::AlignVCenter ? dy / 2 : vertical == Qt::AlignBottom ? dy : 0;
    const QPointF point(x - left - qMax(qreal(0), dx), y - top - dy);
    const int hit = m_view->documentLayout()->hitTest(point, Qt::ExactHit);
    if (hit < 0)
        return {};
    const QTextBlock block = m_view->findBlock(hit);
    const QTextLayout *layout = block.layout();
    if (!layout)
        return {};
    const QPointF local = point - layout->position();
    for (int i = 0; i < layout->lineCount(); ++i) {
        const QTextLine line = layout->lineAt(i);
        if (!line.naturalTextRect().contains(local))
            continue;
        const int position = block.position() + line.xToCursor(local.x(), QTextLine::CursorOnCharacter);
        const QVariantMap token = spellingWordAt(position);
        const int source = sourcePosition(position);
        const int start = token.value(QStringLiteral("start"), -1).toInt();
        return source >= start && source < start + token.value(QStringLiteral("length")).toInt()
            ? token : QVariantMap();
    }
    return {};
}

void LivePreview::refreshSpelling() {
    if (!m_highlighter || m_composing)
        return;
    QList<MarkdownHighlighter::Span> projected;
    if (m_spelling && !m_readMode) {
        int segmentIndex = 0;
        for (const auto &range : m_spelling->ranges()) {
            const int end = range.start + range.length;
            while (segmentIndex < m_segments.size() && m_segments.at(segmentIndex).sourceEnd <= range.start)
                ++segmentIndex;
            for (int i = segmentIndex; i < m_segments.size() && m_segments.at(i).sourceStart < end; ++i) {
                const auto &segment = m_segments.at(i);
                if (segment.object)
                    continue; // Never map an endpoint onto a rendered object's glyph.
                const int from = qMax(range.start, segment.sourceStart);
                const int to = qMin(end, segment.sourceEnd);
                if (from < to)
                    projected.append({segment.viewStart + from - segment.sourceStart, to - from});
            }
        }
    }
    // Deliberately no schedule(), refresh(), or rebuild(): a spelling reply
    // changes only the affected highlighter blocks, not the projection.
    m_highlighter->setSpellingRanges(projected);
}

int LivePreview::toSource(const QList<Segment> &segments, int p) {
    for (const auto &s : segments) {
        if (p < s.viewEnd)
            return s.object ? s.sourceStart : s.sourceStart + qMax(0, p - s.viewStart);
        if (p == s.viewEnd)
            return s.sourceEnd;
    }
    return segments.isEmpty() ? p : segments.last().sourceEnd;
}
int LivePreview::toView(const QList<Segment> &segments, int p) {
    for (const auto &s : segments) {
        if (p < s.sourceEnd)
            return s.object ? s.viewStart : s.viewStart + qMax(0, p - s.sourceStart);
        if (p == s.sourceEnd)
            return s.viewEnd;
    }
    return segments.isEmpty() ? p : segments.last().viewEnd;
}
int LivePreview::sourcePosition(int p) const { return toSource(m_segments, p); }
int LivePreview::viewPosition(int p) const { return toView(m_segments, p); }

void LivePreview::attach(QObject *sourceItem, QObject *viewItem) {
    if (m_sourceItem || !sourceItem || !viewItem)
        return;
    auto sourceQuick = qvariant_cast<QQuickTextDocument *>(sourceItem->property("textDocument"));
    auto viewQuick = qvariant_cast<QQuickTextDocument *>(viewItem->property("textDocument"));
    if (!sourceQuick || !viewQuick)
        return;
    m_sourceItem = sourceItem;
    m_viewItem = viewItem;
    m_source = sourceQuick->textDocument();
    // Qt 6.7+ permits supplying a native document to TextEdit. Deny implicit
    // resources at the document boundary, including rich paste/drop paths;
    // only explicitly registered, bounded local images can be displayed.
    auto *document = new ResourceFreeDocument;
    document->setParent(viewItem);
    viewQuick->setTextDocument(document);
    m_view = document;
    m_view->setUndoRedoEnabled(false);
    m_highlighter = new MarkdownHighlighter(m_view);
    viewItem->installEventFilter(this);
    connect(sourceItem, SIGNAL(cursorPositionChanged()), this, SLOT(sourceSelectionChanged()));
    connect(sourceItem, SIGNAL(selectionStartChanged()), this, SLOT(sourceSelectionChanged()));
    connect(sourceItem, SIGNAL(selectionEndChanged()), this, SLOT(sourceSelectionChanged()));
    connect(viewItem, SIGNAL(cursorPositionChanged()), this, SLOT(viewSelectionChanged()));
    connect(viewItem, SIGNAL(selectionStartChanged()), this, SLOT(viewSelectionChanged()));
    connect(viewItem, SIGNAL(selectionEndChanged()), this, SLOT(viewSelectionChanged()));
    connect(m_source, &QTextDocument::contentsChanged, this, &LivePreview::sourceTextChanged);
    connect(m_view, &QTextDocument::contentsChange, this, &LivePreview::viewContentsChanged);
    connect(m_view->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged,
            this, &LivePreview::scheduleCodeBlockPanels);
    connect(m_view->documentLayout(), &QAbstractTextDocumentLayout::update,
            this, &LivePreview::scheduleCodeBlockPanels);
    connect(viewItem, SIGNAL(widthChanged()), this, SLOT(sourceTextChanged()));
    sourceSelectionChanged();
    rebuild();
}
void LivePreview::select(QObject *item, int anchor, int caret) {
    QMetaObject::invokeMethod(item, "select", Q_ARG(int, anchor), Q_ARG(int, caret));
}
void LivePreview::sourceSelectionChanged() {
    if (m_guard || !m_sourceItem)
        return;
    m_caret = m_sourceItem->property("cursorPosition").toInt();
    const int start = m_sourceItem->property("selectionStart").toInt();
    const int end = m_sourceItem->property("selectionEnd").toInt();
    m_anchor = m_caret == start ? end : start;
    schedule();
}
void LivePreview::viewSelectionChanged() {
    if (m_guard || m_readMode || !m_viewItem)
        return;
    const int caret = m_viewItem->property("cursorPosition").toInt();
    const int start = m_viewItem->property("selectionStart").toInt();
    const int end = m_viewItem->property("selectionEnd").toInt();
    m_caret = sourcePosition(caret);
    m_anchor = sourcePosition(caret == start ? end : start);
    {
        QScopedValueRollback<bool> guard(m_guard, true);
        select(m_sourceItem, m_anchor, m_caret);
    }
    schedule();
}
void LivePreview::sourceTextChanged() {
    if (!m_guard)
        schedule();
}
void LivePreview::viewContentsChanged(int position, int removed, int addedCount) {
    if (m_guard || m_readMode || !m_source || !m_view)
        return;
    const QString next = documentText(m_view);
    if (next == m_viewText)
        return; // syntax highlighting/typography is not an edit
    // Use Qt's actual edit range, not a text diff: repeated characters and
    // NBSP must not move an edit to a different source position.
    const int prefix = qBound(0, position, int(m_viewText.size()));
    const int oldEnd = qBound(prefix, position + removed, int(m_viewText.size()));
    const int from = sourcePosition(prefix);
    const int to = sourcePosition(oldEnd);
    const QString added = next.mid(position, qMax(0, qMin(addedCount, int(next.size()) - position)));
    for (const auto &segment : m_segments) {
        if (segment.object && from < segment.sourceEnd && to > segment.sourceStart) {
            // A non-keyboard edit (e.g. an accessibility command or drop) may
            // bypass prepareEdit. Reject ambiguous object deletion, never turn
            // removal of one image glyph into removal of its Markdown block.
            m_projectionKey.clear();
            schedule();
            return;
        }
    }
    // Never accept a pasted/dragged rich-text object as source Markdown.
    if (added.contains(QChar::ObjectReplacementCharacter)) {
        m_projectionKey.clear();
        schedule();
        return;
    }
    {
        QScopedValueRollback<bool> guard(m_guard, true);
        QTextCursor cursor(m_source);
        cursor.setPosition(from);
        cursor.setPosition(to, QTextCursor::KeepAnchor);
        cursor.insertText(added);
        m_anchor = m_caret = from + added.size();
        select(m_sourceItem, m_anchor, m_caret);
        m_viewText = next;
    }
    // The edited range is identity text (object-boundary edits are revealed
    // before native input). Update the old map immediately, including during
    // composition, when rebuilding the presentation is forbidden.
    const int delta = added.size() - (to - from);
    for (auto &segment : m_segments) {
        if (segment.sourceStart > from) {
            segment.sourceStart += delta;
            segment.viewStart += delta;
        }
        if (segment.sourceEnd >= to) {
            segment.sourceEnd += delta;
            segment.viewEnd += delta;
        }
    }
    m_projectionKey.clear();
    schedule();
}
void LivePreview::schedule() {
    if (m_pending)
        return;
    m_pending = true;
    QTimer::singleShot(0, this, [this] {
        if (!m_pending)
            return;
        m_pending = false;
        if (!m_composing)
            rebuild();
    });
}
void LivePreview::refresh() {
    m_images.clear();
    m_projectionKey.clear();
    schedule();
}
void LivePreview::setSearchHighlight(const QString &query, int currentMatchStart) {
    m_searchQuery = query;
    m_currentMatch = currentMatchStart;
    if (m_highlighter && !m_readMode)
        m_highlighter->setSearch(query, currentMatchStart < 0 ? -1 : viewPosition(currentMatchStart));
}
void LivePreview::setReadMode(bool read) {
    if (read == m_readMode)
        return;
    // Finish native composition before replacing the presentation document.
    QGuiApplication::inputMethod()->commit();
    m_readMode = read;
    m_projectionKey.clear();
    rebuild();
    emit readModeChanged();
}
QUrl LivePreview::resolve(const QString &path) const {
    QUrl url;
    if (m_resolver)
        QMetaObject::invokeMethod(m_resolver, "resolveImageUrl", Qt::DirectConnection,
                                  Q_RETURN_ARG(QUrl, url), Q_ARG(QString, path));
    return url.isLocalFile() ? url : QUrl();
}
QImage LivePreview::imageFor(const PreviewBlocks::Block &block) {
    const int width = qMax(80, m_viewItem->property("width").toInt() - 8);
    const QFont font = qvariant_cast<QFont>(m_viewItem->property("font"));
    const QColor foreground = qvariant_cast<QColor>(m_viewItem->property("color"));
    const QColor background = m_viewItem->property("previewBackground").value<QColor>();
    const QUrl url = block.kind == PreviewBlocks::Kind::Image ? resolve(block.imagePath) : QUrl();
    const QString key = block.markdown + QLatin1Char('|') + QString::number(width)
        + font.toString() + foreground.name() + background.name() + url.toString();
    if (m_images.contains(key))
        return m_images.value(key);
    QImage image;
    if (block.kind == PreviewBlocks::Kind::HorizontalRule)
        image = PreviewBlocks::renderHorizontalRule(font, foreground, width);
    else if (block.kind == PreviewBlocks::Kind::Table)
        image = PreviewBlocks::renderTable(block.markdown, font, foreground, background, width);
    else if (block.kind == PreviewBlocks::Kind::Image)
        image = PreviewBlocks::loadImage(url, width);
    else if (block.kind == PreviewBlocks::Kind::Mermaid)
        image = m_mermaid->request(block.markdown, m_viewItem->property("previewDark").toBool(), width);
    // Missing/failed/unsupported Mermaid stays editable source.
    if (!image.isNull()) {
        qsizetype bytes = image.sizeInBytes();
        for (const auto &cached : m_images)
            bytes += cached.sizeInBytes();
        if (m_images.size() >= 32 || bytes > MaxProjectionBytes)
            m_images.clear();
        if (image.sizeInBytes() <= MaxProjectionBytes)
            m_images.insert(key, image);
    }
    return image;
}
void LivePreview::clearViewResources() {
    // Explicitly release resources no longer used by a smaller projection;
    // QTextDocument's named resource cache is independent of visible content.
    for (const auto &name : m_resourceNames)
        m_view->addResource(QTextDocument::ImageResource, name, QVariant());
    m_resourceNames.clear();
}
void LivePreview::buildReadDocument(const QString &source) {
    m_highlighter->setDocument(nullptr);
    // Import in a plain scratch document, not Qt Quick's resource-loading
    // document. Only sanitized preview: image names reach the visible item.
    ResourceFreeDocument rendered;
    rendered.setDefaultFont(m_view->defaultFont());
    QString markdown = source;
    QHash<QString, QImage> diagrams;
    const auto blocks = PreviewBlocks::parse(source);
    qsizetype diagramBytes = 0;
    for (auto it = blocks.crbegin(); it != blocks.crend(); ++it) {
        if (it->kind != PreviewBlocks::Kind::Mermaid
            || diagrams.size() >= MaxProjectionObjects || diagramBytes >= MaxProjectionBytes)
            continue;
        const QImage image = imageFor(*it);
        if (image.isNull())
            continue;
        if (image.sizeInBytes() > MaxProjectionBytes - diagramBytes) {
            diagramBytes = MaxProjectionBytes;
            continue;
        }
        diagramBytes += image.sizeInBytes();
        const QString name = QStringLiteral("preview:mermaid/%1").arg(it->start);
        diagrams.insert(name, image);
        markdown.replace(it->start, it->end - it->start, QStringLiteral("![Mermaid diagram](%1)").arg(name));
    }
    rendered.setMarkdown(markdown, QTextDocument::MarkdownDialectGitHub);
    // Resolve every image ourselves, including inline images. Qt Quick must
    // never receive an http(s) image URL from untrusted Markdown.
    QList<QPair<int, QTextImageFormat>> images;
    for (QTextBlock block = rendered.begin(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (fragment.charFormat().isImageFormat())
                images.append({fragment.position(), fragment.charFormat().toImageFormat()});
        }
    }
    int index = 0;
    QList<QPair<QUrl, QImage>> resources;
    qsizetype resourceBytes = 0;
    for (auto it = images.crbegin(); it != images.crend(); ++it) {
        const auto &entry = *it;
        QImage image;
        if (resources.size() < MaxProjectionObjects && resourceBytes < MaxProjectionBytes) {
            image = diagrams.contains(entry.second.name()) ? diagrams.value(entry.second.name())
                : PreviewBlocks::loadImage(resolve(entry.second.name()),
                                          qMax(80, m_viewItem->property("width").toInt() - 8));
            if (image.sizeInBytes() > MaxProjectionBytes - resourceBytes) {
                image = {};
                resourceBytes = MaxProjectionBytes;
            } else {
                resourceBytes += image.sizeInBytes();
            }
        }
        QTextCursor cursor(&rendered);
        cursor.setPosition(entry.first);
        cursor.setPosition(entry.first + 1, QTextCursor::KeepAnchor);
        if (image.isNull()) {
            cursor.insertText(QStringLiteral("[image unavailable]"));
        } else {
            QTextImageFormat format;
            format.setName(QStringLiteral("preview:read/%1").arg(index++));
            format.setWidth(image.width()); format.setHeight(image.height());
            resources.append({QUrl(format.name()), image});
            cursor.insertImage(format);
        }
    }
    styleReadDocument(rendered, markdown, strongForeground(m_viewItem),
                      m_viewItem->property("color").value<QColor>(),
                      m_viewItem->property("previewAccent").value<QColor>(),
                      m_viewItem->property("previewDark").toBool());
    clearViewResources();
    const auto codeGroups = highlightReadCode(rendered, markdown, m_viewItem->property("previewDark").toBool());
    m_view->setHtml(rendered.toHtml());
    // Carry explicit fence boundaries across HTML only when block identities
    // survive. Adjacent same-language fences must remain separate panels.
    m_readCodeBlocks = rendered.blockCount() == m_view->blockCount()
        && rendered.toPlainText() == m_view->toPlainText() ? codeGroups : QList<QPair<int, int>>{};
    // Qt's HTML round-trip can lose the smallest heading's relative size.
    // Keep every heading on the same Markdown scale used by Edit highlighting.
    for (QTextBlock block = m_view->begin(); block.isValid(); block = block.next()) {
        const int level = block.blockFormat().headingLevel();
        if (level < 1 || level > 6)
            continue;
        QTextCursor heading(block);
        heading.select(QTextCursor::BlockUnderCursor);
        QTextCharFormat size;
        size.setProperty(QTextFormat::FontSizeAdjustment, 4 - level);
        heading.mergeCharFormat(size);
    }
    for (const auto &resource : resources) {
        m_view->addResource(QTextDocument::ImageResource, resource.first, resource.second);
        m_resourceNames.append(resource.first);
    }
    m_view->markContentsDirty(0, m_view->characterCount());
}
void LivePreview::completeProjectionLayout() {
    if (!m_view)
        return;
    QAbstractTextDocumentLayout *layout = m_view->documentLayout();
    layout->documentSize();
    QList<int> invalidImages;
    for (QTextBlock block = m_view->begin(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid() || !fragment.charFormat().isImageFormat())
                continue;
            // documentSize() alone can leave later blocks lazily laid out.
            // Complete each object-containing block on the GUI thread before
            // Qt Quick's render thread calls QTextLine::ascent() for it.
            layout->blockBoundingRect(block);
            const QTextLayout *textLayout = block.layout();
            const QString text = fragment.text();
            for (int i = 0; i < text.size(); ++i) {
                if (text.at(i) != QChar::ObjectReplacementCharacter)
                    continue;
                const int position = fragment.position() + i;
                if (!textLayout || !textLayout->lineForTextPosition(position - block.position()).isValid())
                    invalidImages.append(position);
            }
        }
    }
    // Invalid layout must not reach QQuickTextNodeEngine. Keep a one-character
    // placeholder so the source/view mapping remains unchanged and recoverable.
    for (int position : invalidImages) {
        QTextCursor cursor(m_view);
        cursor.setPosition(position);
        cursor.setPosition(position + 1, QTextCursor::KeepAnchor);
        cursor.insertText(QString(QChar(0x25a1)), QTextCharFormat());
    }
    if (!invalidImages.isEmpty())
        layout->documentSize();
    m_viewText = documentText(m_view);
    updateCodeBlockPanels();
}
void LivePreview::scheduleCodeBlockPanels() {
    if (m_panelsPending) return;
    m_panelsPending = true;
    QTimer::singleShot(0, this, [this] {
        m_panelsPending = false;
        // Geometry is read-only and must follow wrapped IME preedit as well;
        // source/projection rebuilding remains deferred until composition ends.
        if (!m_guard) updateCodeBlockPanels();
    });
}
void LivePreview::updateCodeBlockPanels() {
    if (!m_view || !m_viewItem) return;
    QList<QPair<int, int>> groups = m_readCodeBlocks;
    if (!m_readMode) {
        groups.clear();
        // Highlighting can temporarily reset QTextBlock::userState while a
        // caret/selection change restyles the document. Decorations must follow
        // the visible text, not that deferred syntax-highlighter lifecycle.
        int first = -1, length = 0;
        QChar marker;
        for (auto block = m_view->begin(); block.isValid(); block = block.next()) {
            const QString text = block.text();
            if (first < 0) {
                length = MarkdownHighlighter::openingFenceLength(text);
                if (!length) continue;
                first = block.blockNumber();
                marker = text.trimmed().front();
            } else if (MarkdownHighlighter::closesFence(text, length, marker)) {
                groups.append({first, block.blockNumber()});
                first = -1;
            }
            if (first >= 0 && !block.next().isValid())
                groups.append({first, block.blockNumber()});
        }
    }
    auto *layout = m_view->documentLayout();
    layout->documentSize();
    const qreal left = m_viewItem->property("leftPadding").toReal() - m_view->documentMargin();
    // QTextLayout positions already include the document margin. Qt Quick's
    // vertical text origin adds padding/alignment, not a margin subtraction.
    qreal top = m_viewItem->property("topPadding").toReal();
    const qreal spare = m_viewItem->property("height").toReal()
        - m_viewItem->property("topPadding").toReal() - m_viewItem->property("bottomPadding").toReal()
        - m_viewItem->property("contentHeight").toReal();
    const int alignment = m_viewItem->property("verticalAlignment").toInt();
    if (alignment == Qt::AlignVCenter) top += spare / 2;
    else if (alignment == Qt::AlignBottom) top += spare;
    QVariantList panels;
    for (const auto &group : groups) {
        const auto first = m_view->findBlockByNumber(group.first);
        const auto last = m_view->findBlockByNumber(group.second);
        if (!first.isValid() || !last.isValid()) continue;
        const QRectF bounds = layout->blockBoundingRect(first);
        layout->blockBoundingRect(last);
        const auto *a = first.layout(), *b = last.layout();
        // Do not publish a partial/empty replacement model during relayout:
        // that destroys otherwise valid background delegates. A subsequent
        // layout notification will supply the completed geometry.
        if (!a || !b || !a->lineCount() || !b->lineCount()) return;
        const qreal y = top + a->position().y() + a->lineAt(0).y();
        const auto endLine = b->lineAt(b->lineCount() - 1);
        const qreal bottom = top + b->position().y() + endLine.y() + endLine.height();
        const qreal x = left + bounds.left() - 6;
        panels.append(QVariantMap{{"x", x}, {"y", y},
            {"width", qMax(qreal(0), m_viewItem->property("width").toReal()
                         - m_viewItem->property("rightPadding").toReal() + 6 - x)},
            {"height", bottom - y}});
    }
    if (panels != m_codeBlockPanels) {
        m_codeBlockPanels = panels;
        emit codeBlockPanelsChanged();
    }
}
void LivePreview::rebuild() {
    if (!m_source || !m_view || m_guard || m_composing)
        return;
    emit projectionAboutToChange();
    const auto notify = qScopeGuard([this] { emit projectionChanged(); });
    QScopedValueRollback<bool> guard(m_guard, true);
    const auto finishLayout = qScopeGuard([this] { completeProjectionLayout(); });
    m_pending = false;
    const QString source = documentText(m_source);
    m_anchor = qBound(0, m_anchor, int(source.size()));
    m_caret = qBound(0, m_caret, int(source.size()));
    const QFont font = qvariant_cast<QFont>(m_viewItem->property("font"));
    m_mermaid->setGeneration(source + QString::number(m_viewItem->property("width").toInt())
                             + m_viewItem->property("previewDark").toString());
    // QTextDocument does not equality-check this setter: even an unchanged
    // font invalidates the entire layout. A caret click must not restart it
    // while Qt Quick is updating the viewport from document-size callbacks.
    if (m_view->defaultFont() != font)
        m_view->setDefaultFont(font);
    m_segments.clear();
    if (m_readMode) {
        buildReadDocument(source);
        m_viewText = documentText(m_view);
        return;
    }
    const bool dark = m_viewItem->property("previewDark").toBool();
    struct Replacement { PreviewBlocks::Block block; QImage image; };
    QList<Replacement> replacements;
    const int low = qMin(m_anchor, m_caret), high = qMax(m_anchor, m_caret);
    qsizetype projectionBytes = 0;
    for (const auto &block : PreviewBlocks::parse(source)) {
        if (replacements.size() >= MaxProjectionObjects || projectionBytes >= MaxProjectionBytes)
            break;
        // Include both endpoints: typing/deleting against an object first
        // exposes Markdown rather than treating the whole block as one char.
        if (low <= block.end && high >= block.start)
            continue;
        const QImage image = imageFor(block);
        if (!image.isNull()) {
            if (image.sizeInBytes() > MaxProjectionBytes - projectionBytes)
                break;
            projectionBytes += image.sizeInBytes();
            replacements.append({block, image});
        }
    }
    // Caret-only changes that do not change object ranges need no relayout.
    QList<Segment> segments;
    int sourceAt = 0, viewAt = 0;
    for (const auto &replacement : replacements) {
        const auto &b = replacement.block;
        if (b.start > sourceAt) {
            segments.append({sourceAt, b.start, viewAt, viewAt + b.start - sourceAt, false});
            viewAt += b.start - sourceAt;
        }
        segments.append({b.start, b.end, viewAt, viewAt + 1, true});
        sourceAt = b.end; ++viewAt;
    }
    segments.append({sourceAt, int(source.size()), viewAt, viewAt + int(source.size()) - sourceAt, false});
    QString key = source + font.toString() + m_viewItem->property("color").toString()
        + m_viewItem->property("previewBackground").toString()
        + QString::number(m_viewItem->property("width").toInt());
    for (const auto &segment : segments)
        if (segment.object)
            key += QStringLiteral("/%1:%2").arg(segment.sourceStart).arg(segment.sourceEnd);
    m_segments = segments;
    const auto styleProjection = [this, dark](bool refreshFont) {
        if (m_highlighter->document() != m_view)
            m_highlighter->setDocument(m_view);
        m_highlighter->setTheme(dark, m_viewItem->property("previewBackground").value<QColor>().name(),
                                strongForeground(m_viewItem).name(),
                                m_viewItem->property("previewAccent").value<QColor>().name(),
                                dark ? QStringLiteral("#252525") : QStringLiteral("#eeeeee"));
        m_highlighter->setSearch(m_searchQuery, m_currentMatch < 0 ? -1 : viewPosition(m_currentMatch));
        m_highlighter->setCaret(viewPosition(m_caret));
        m_highlighter->setSelection(viewPosition(m_anchor), viewPosition(m_caret));
        if (refreshFont)
            m_highlighter->refreshFont();
        refreshSpelling();
    };
    if (key == m_projectionKey) {
        styleProjection(false);
        select(m_viewItem, viewPosition(m_anchor), viewPosition(m_caret));
        return;
    }
    m_projectionKey = key;
    // Never attach the Markdown highlighter to the old Read HTML, or let it
    // restyle a partially rebuilt document containing inline image objects.
    m_highlighter->setDocument(nullptr);
    // Native rich-text image objects, never hidden multiline source overlays.
    clearViewResources();
    m_view->clear();
    QTextCursor cursor(m_view);
    cursor.beginEditBlock();
    sourceAt = 0;
    int index = 0;
    for (const auto &replacement : replacements) {
        const auto &b = replacement.block;
        cursor.insertText(source.mid(sourceAt, b.start - sourceAt));
        QTextImageFormat format;
        format.setName(QStringLiteral("preview:edit/%1").arg(index++));
        format.setWidth(replacement.image.width());
        format.setHeight(replacement.image.height());
        const QUrl resourceName(format.name());
        m_view->addResource(QTextDocument::ImageResource, resourceName, replacement.image);
        m_resourceNames.append(resourceName);
        cursor.insertImage(format);
        sourceAt = b.end;
    }
    cursor.insertText(source.mid(sourceAt));
    cursor.endEditBlock();
    m_viewText = documentText(m_view);
    styleProjection(true);
    select(m_viewItem, viewPosition(m_anchor), viewPosition(m_caret));
}
void LivePreview::prepareEdit() {
    if (m_readMode || !m_source)
        return;
    // Flush before another native input event sees a stale mapping.
    if (m_pending)
        rebuild();
    const int low = qMin(m_anchor, m_caret), high = qMax(m_anchor, m_caret);
    bool boundary = false;
    for (const auto &segment : m_segments) {
        if (segment.object && low <= segment.sourceEnd + 1 && high >= segment.sourceStart - 1) {
            boundary = true;
            break;
        }
    }
    if (boundary) {
        // Temporarily project the full source. The next caret update restores
        // inactive previews. In particular Backspace cannot erase a table.
        QScopedValueRollback<bool> guard(m_guard, true);
        m_projectionKey.clear();
        clearViewResources();
        m_view->setPlainText(documentText(m_source));
        m_segments = {{0, int(m_source->toPlainText().size()), 0,
                      int(m_source->toPlainText().size()), false}};
        m_viewText = documentText(m_view);
        select(m_viewItem, m_anchor, m_caret);
    }
}
QString LivePreview::imagePathAt(int position) const {
    if (m_readMode || !m_source)
        return {};
    const int sourceAt = sourcePosition(position);
    for (const auto &block : PreviewBlocks::parse(documentText(m_source))) {
        if (block.kind == PreviewBlocks::Kind::Image
            && sourceAt >= block.start && sourceAt < block.end)
            return block.imagePath;
    }
    return {};
}
void LivePreview::copySource() {
    if (m_readMode) {
        QMetaObject::invokeMethod(m_viewItem, "copy");
        return;
    }
    QTextCursor cursor(m_source);
    cursor.setPosition(m_anchor);
    cursor.setPosition(m_caret, QTextCursor::KeepAnchor);
    QGuiApplication::clipboard()->setText(cursor.selectedText().replace(QChar::ParagraphSeparator, QLatin1Char('\n')));
}
bool LivePreview::eventFilter(QObject *object, QEvent *event) {
    if (object != m_viewItem || m_readMode)
        return QObject::eventFilter(object, event);
    if (event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        // Qt Quick can consume Undo's ShortcutOverride even with the view's
        // undo stack disabled. Always route native undo to the source item.
        if (key->matches(QKeySequence::Undo) || key->matches(QKeySequence::Redo)) {
            QMetaObject::invokeMethod(m_sourceItem,
                                     key->matches(QKeySequence::Undo) ? "undo" : "redo");
            return true;
        }
        if (key->matches(QKeySequence::Copy)) {
            copySource();
            return true;
        }
        prepareEdit();
    } else if (event->type() == QEvent::InputMethod) {
        auto *input = static_cast<QInputMethodEvent *>(event);
        if (!m_composing)
            prepareEdit();
        m_composing = !input->preeditString().isEmpty();
        if (m_spelling)
            m_spelling->setComposing(m_composing);
        if (!m_composing) {
            refreshSpelling();
            schedule();
        }
    }
    return QObject::eventFilter(object, event);
}
