#pragma once

#include <QObject>
#include <QPointer>
#include <QTextDocument>
#include <QVariant>
#include "previewblocks.h"
#include "listpreview.h"

class MarkdownHighlighter;
class MermaidRenderer;
class SpellingController;

// The backend owns source and its undo stack. This document is disposable UI.
class LivePreview : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool readMode READ readMode WRITE setReadMode NOTIFY readModeChanged)
    Q_PROPERTY(QObject *resolver READ resolver WRITE setResolver)
    Q_PROPERTY(QObject *spelling READ spelling WRITE setSpelling)
    Q_PROPERTY(QVariantList codeBlockPanels READ codeBlockPanels NOTIFY codeBlockPanelsChanged)
    Q_PROPERTY(QVariantList listMarkers READ listMarkers NOTIFY listMarkersChanged)
public:
    struct Segment {
        int sourceStart, sourceEnd, viewStart, viewEnd;
        bool object;
    };
    explicit LivePreview(QObject *parent = nullptr);
    bool readMode() const { return m_readMode; }
    void setReadMode(bool read);
    QObject *resolver() const { return m_resolver; }
    void setResolver(QObject *resolver) { m_resolver = resolver; }
    QObject *spelling() const;
    void setSpelling(QObject *spelling);
    Q_INVOKABLE void attach(QObject *sourceItem, QObject *viewItem);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void copySource();
    Q_INVOKABLE bool copyCodeBlock(int panelIndex);
    Q_INVOKABLE void prepareEdit();
    Q_INVOKABLE void setSearchHighlight(const QString &query, int currentMatchStart);
    Q_INVOKABLE QString imagePathAt(int viewPosition) const;
    Q_INVOKABLE QString imagePathAtPoint(qreal x, qreal y) const;
    Q_INVOKABLE QVariantMap spellingWordAt(int viewPosition);
    Q_INVOKABLE QVariantMap spellingWordAtPoint(qreal x, qreal y);
    Q_INVOKABLE int sourcePosition(int viewPosition) const;
    Q_INVOKABLE int viewPosition(int sourcePosition) const;
    // Public pure mapping helpers are also used by the isolated tests.
    static int toSource(const QList<Segment> &segments, int position);
    static int toView(const QList<Segment> &segments, int position);
    const QList<Segment> &segments() const { return m_segments; }
    QVariantList codeBlockPanels() const { return m_codeBlockPanels; }
    QVariantList listMarkers() const { return m_listMarkers; }
signals:
    void readModeChanged();
    void copyHtmlRequested();
    void codeBlockPanelsChanged();
    void listMarkersChanged();
    void projectionAboutToChange();
    void projectionChanged();
private slots:
    void sourceSelectionChanged();
    void viewSelectionChanged();
    void viewContentsChanged(int position, int removed, int added);
    void sourceTextChanged();
private:
    bool eventFilter(QObject *object, QEvent *event) override;
    void schedule();
    void rebuild();
    void refreshSpelling();
    void select(QObject *item, int anchor, int caret);
    QUrl resolve(const QString &path) const;
    QImage imageFor(const PreviewBlocks::Block &block);
    void buildReadDocument(const QString &source);
    void clearViewResources();
    void completeProjectionLayout();
    void updateCodeBlockPanels();
    void styleEditLists();
    void updateListMarkers();
    int characterAtPoint(qreal x, qreal y) const;
    QVariantList m_listMarkers;
    QList<ListPreview::Item> m_listItems;
    QString m_listText;
    QFont m_listFont;
    qreal m_listIndentWidth = 0;
    bool m_listStyleValid = false;
    void scheduleCodeBlockPanels();
    QVariantList m_codeBlockPanels;
    QList<QPair<int, int>> m_readCodeBlocks;
    QHash<QString, QString> m_readImagePaths; // Sanitized native resource -> original local destination.
    QStringList m_readCodeLanguages;
    bool m_panelsPending = false;
    QPointer<QObject> m_sourceItem, m_viewItem, m_resolver;
    QPointer<QTextDocument> m_source, m_view;
    MarkdownHighlighter *m_highlighter = nullptr;
    QPointer<SpellingController> m_spelling;
    MermaidRenderer *m_mermaid = nullptr;
    QList<Segment> m_segments;
    QList<QUrl> m_resourceNames;
    QString m_viewText;
    QString m_projectionSource, m_panelsSource, m_panelsView;
    QString m_projectionKey;
    QString m_searchQuery;
    int m_currentMatch = -1;
    int m_anchor = 0, m_caret = 0;
    bool m_guard = false, m_pending = false, m_readMode = false;
    bool m_composing = false;
    // Disposable, byte- and count-bounded raster cache.
    QHash<QString, QImage> m_images;
};
