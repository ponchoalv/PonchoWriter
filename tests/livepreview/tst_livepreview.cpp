#include <QtTest>
#include <QThread>
#include <atomic>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QQuickTextDocument>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>
#include <QTextFragment>
#include <QTextImageFormat>
#include <QTextLayout>
#include <QFontDatabase>
#include <QAbstractTextDocumentLayout>
#include <QTextTable>
#include <QInputMethodEvent>
#include <QTemporaryDir>
#include <QImage>
#include <QClipboard>
#include <QMimeData>
#include <QSignalSpy>
#include <QSignalBlocker>
#include <QScopeGuard>
#include "../../src/mermaidrenderer.h"
#include "../../src/livepreview.h"
#include "../../src/codehighlighter.h"
#include "../../src/markdownhighlighter.h"

class ImageResolver : public QObject {
    Q_OBJECT
public:
    QString root;
    Q_INVOKABLE QUrl resolveImageUrl(const QString &path) {
        return path.startsWith("https:") ? QUrl() : QUrl::fromLocalFile(root + "/" + path);
    }
};

class LivePreviewTest : public QObject {
    Q_OBJECT
private:
    QQmlEngine engine;
    std::unique_ptr<QObject> root;
    std::unique_ptr<LivePreview> controller;
    QObject *sourceItem = nullptr, *viewItem = nullptr;
    QTextDocument *source = nullptr, *view = nullptr;
    const QString markdown = QString::fromUtf8("😀 before\n\n| A | B |\n| --- | --- |\n| one | two |\n\nafter\n");
    void flush() { QCoreApplication::processEvents(); QCoreApplication::processEvents(); }
    void select(QObject *item, int anchor, int caret) {
        QVERIFY(QMetaObject::invokeMethod(item, "select", Q_ARG(int, anchor), Q_ARG(int, caret)));
        flush();
    }
    void type(const QString &text) {
        for (const QChar c : text) {
            QKeyEvent event(QEvent::KeyPress, c.unicode(), Qt::NoModifier, QString(c));
            QCoreApplication::sendEvent(viewItem, &event);
            flush();
        }
    }
private slots:
    void init() {
        QQmlComponent component(&engine);
        component.setData(
            "import QtQuick\n"
            "Item { width: 600; height: 800\n"
            "TextEdit { objectName: 'source'; visible: false; textFormat: TextEdit.PlainText }\n"
            "TextEdit { objectName: 'view'; width: 550; height: 800\n"
            "textFormat: TextEdit.RichText; wrapMode: TextEdit.Wrap\n"
            "font.pixelSize: 20; color: '#222222'\n"
            "property color previewBackground: 'white'\n"
            "property color previewAccent: 'blue'\n"
            "property bool previewDark: false\n"
            "} }", QUrl());
        root.reset(component.create());
        QVERIFY2(root, qPrintable(component.errorString()));
        sourceItem = root->findChild<QObject *>("source");
        viewItem = root->findChild<QObject *>("view");
        source = qvariant_cast<QQuickTextDocument *>(sourceItem->property("textDocument"))->textDocument();
        view = qvariant_cast<QQuickTextDocument *>(viewItem->property("textDocument"))->textDocument();
        source->setPlainText(markdown);
        source->clearUndoRedoStacks();
        controller = std::make_unique<LivePreview>();
        controller->attach(sourceItem, viewItem);
        view = qvariant_cast<QQuickTextDocument *>(viewItem->property("textDocument"))->textDocument();
        flush();
    }
    void cleanup() { controller.reset(); root.reset(); }
    void inactiveHorizontalRulesRevealSourceAndKeepUndo() {
        const QString text = "before\n\n---\n\nafter\n\n```\n---\n```\n";
        source->setPlainText(text); source->clearUndoRedoStacks();
        select(sourceItem, text.size(), text.size());
        QCOMPARE(view->toPlainText().count(QChar::ObjectReplacementCharacter), 1);
        const int object = view->toPlainText().indexOf(QChar::ObjectReplacementCharacter);
        QTextCursor cursor(view); cursor.setPosition(object); cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        QVERIFY(cursor.charFormat().isImageFormat());
        QCOMPARE(cursor.charFormat().toImageFormat().width(), viewItem->property("width").toReal() - 8);
        select(viewItem, object, object); // Same source activation used by pointer/keyboard navigation.
        QVERIFY(!view->toPlainText().contains(QChar::ObjectReplacementCharacter));
        QVERIFY(view->toPlainText().contains("---"));
        select(sourceItem, text.size(), text.size());
        QCOMPARE(view->toPlainText().count(QChar::ObjectReplacementCharacter), 1);
        const int rule = text.indexOf("---");
        select(sourceItem, rule - 1, rule + 4);
        QVERIFY(!view->toPlainText().contains(QChar::ObjectReplacementCharacter));
        controller->setReadMode(true); flush();
        controller->setReadMode(false); flush();
        QCOMPARE(source->toPlainText(), text);
        QVERIFY(!source->isUndoAvailable());
    }
    void manyCodePanelsBatchTheirSpacingUpdates() {
        QString text = "Before\n";
        for (int i = 0; i < 300; ++i) text += "```text\nvalue\n```\n";
        text += "After\n";
        source->setPlainText(text); source->clearUndoRedoStacks();
        select(sourceItem, text.size(), text.size()); controller->refresh(); flush();
        QCOMPARE(controller->codeBlockPanels().size(), 300);
        auto *layout = view->documentLayout();
        { // Invalidate presentation-only spacing, without measuring the fixture setup.
            QSignalBlocker layoutSignals(layout);
            QTextCursor batch(view); batch.beginEditBlock();
            for (auto block = view->begin(); block.isValid(); block = block.next()) {
                auto format = block.blockFormat(); format.setTopMargin(0); format.setBottomMargin(0);
                QTextCursor cursor(block); cursor.setBlockFormat(format);
            }
            batch.endEditBlock();
        }
        QSignalSpy updates(layout, &QAbstractTextDocumentLayout::update);
        layout->update(QRectF()); flush();
        QVERIFY(updates.count() > 0);
        QVERIFY2(updates.count() < 12, qPrintable(QString::number(updates.count())));
        QCOMPARE(controller->codeBlockPanels().size(), 300);
        const auto panels = controller->codeBlockPanels();
        updates.clear(); select(sourceItem, text.size() - 2, text.size() - 2); flush();
        QCOMPARE(controller->codeBlockPanels(), panels);
        QVERIFY2(updates.count() < 12, qPrintable(QString("caret refresh updates: %1").arg(updates.count())));
        QCOMPARE(source->toPlainText(), text);
        QVERIFY(!source->isUndoAvailable());
    }
    void paddedCodePanelsLeaveRoomForProseAndAdjacentBlocks() {
        const QString text = "Before\n```json\n{\"x\":true}\n```\n```text\nnext\n```\nAfter\n";
        source->setPlainText(text); source->clearUndoRedoStacks();
        select(sourceItem, text.size(), text.size());
        for (int pixels : {16, 28}) {
            QFont font = view->defaultFont(); font.setPixelSize(pixels);
            viewItem->setProperty("font", font);
            for (bool read : {false, true}) {
                controller->setReadMode(read); controller->refresh(); flush();
                const auto panels = controller->codeBlockPanels();
                QCOMPARE(panels.size(), 2);
                const auto a = panels[0].toMap(), b = panels[1].toMap();
                QVERIFY(a.value("y").toReal() + a.value("height").toReal() < b.value("y").toReal());
                QRectF before, after;
                QVERIFY(QMetaObject::invokeMethod(viewItem, "positionToRectangle", Q_RETURN_ARG(QRectF, before), Q_ARG(int, 0)));
                QVERIFY(QMetaObject::invokeMethod(viewItem, "positionToRectangle", Q_RETURN_ARG(QRectF, after), Q_ARG(int, view->find("After").position() - 1)));
                QVERIFY(before.bottom() < a.value("y").toReal());
                QVERIFY(b.value("y").toReal() + b.value("height").toReal() < after.top());
                const auto code = view->find("\"x\"").block();
                QRectF line;
                QVERIFY(QMetaObject::invokeMethod(viewItem, "positionToRectangle", Q_RETURN_ARG(QRectF, line), Q_ARG(int, code.position())));
                QVERIFY(line.top() - a.value("y").toReal() >= pixels / 2);
                QVERIFY(a.value("width").toReal() > viewItem->property("width").toReal() + 12);
                QCOMPARE(source->toPlainText(), text);
                QVERIFY(!source->isUndoAvailable());
            }
        }
        source->setPlainText("```text\nedge\n```");
        select(sourceItem, source->characterCount() - 1, source->characterCount() - 1);
        for (bool read : {false, true}) {
            controller->setReadMode(read); controller->refresh(); flush();
            const auto panel = controller->codeBlockPanels().first().toMap();
            QVERIFY(panel.value("y").toReal() >= 0);
            QVERIFY(panel.value("y").toReal() + panel.value("height").toReal()
                    <= viewItem->property("contentHeight").toReal());
        }
        source->setPlainText("Only prose"); flush();
        for (bool read : {false, true}) {
            controller->setReadMode(read); controller->refresh(); flush();
            QVERIFY(controller->codeBlockPanels().isEmpty());
            QCOMPARE(view->rootFrame()->frameFormat().topMargin(), view->documentMargin());
            QCOMPARE(view->rootFrame()->frameFormat().bottomMargin(), view->documentMargin());
        }
    }
    void codePanelsCoverNativeFontBoundsAcrossMarginsAndZoom() {
        const int id = QFontDatabase::addApplicationFont(QFINDTESTDATA("../../fonts/IBMPlexMono-Regular.ttf"));
        QVERIFY(id >= 0);
        const QString text = "Testing background\n\n```json\n{\"this\":\"that\", \"jgy\":true}\n```\n";
        source->setPlainText(text); source->clearUndoRedoStacks();
        select(sourceItem, text.size(), text.size());
        for (int pixels : {12, 20, 34}) {
            QFont font(QFontDatabase::applicationFontFamilies(id).first()); font.setPixelSize(pixels);
            viewItem->setProperty("font", font);
            for (bool read : {false, true}) {
                controller->setReadMode(read); controller->refresh(); flush();
                for (int margin : {0, 4, 9}) {
                    view->setDocumentMargin(margin); flush();
                    QCOMPARE(controller->codeBlockPanels().size(), 1);
                    const auto panel = controller->codeBlockPanels().first().toMap();
                    const auto block = view->find("\"this\"").block();
                    QVERIFY(block.isValid());
                    QRectF cursor;
                    QVERIFY(QMetaObject::invokeMethod(viewItem, "positionToRectangle", Q_RETURN_ARG(QRectF, cursor), Q_ARG(int, block.position())));
                    const qreal top = panel.value("y").toReal(), bottom = top + panel.value("height").toReal();
                    QVERIFY2(top <= cursor.top() && bottom >= cursor.bottom(),
                        qPrintable(QString("pixels=%1 read=%2 margin=%3 panel=%4..%5 cursor=%6..%7")
                            .arg(pixels).arg(read).arg(margin).arg(top).arg(bottom).arg(cursor.top()).arg(cursor.bottom())));
                    const auto *layout = block.layout();
                    const qreal origin = cursor.y() - layout->lineAt(0).y();
                    for (const auto &run : layout->glyphRuns()) {
                        const auto glyphs = run.glyphIndexes();
                        const auto positions = run.positions();
                        for (int i = 0; i < glyphs.size(); ++i) {
                            const auto ink = run.rawFont().boundingRect(glyphs.at(i)).translated(positions.at(i));
                            QVERIFY(top <= origin + ink.top());
                            QVERIFY(bottom >= origin + ink.bottom());
                        }
                    }
                }
            }
        }
        QCOMPARE(source->toPlainText(), text);
        QVERIFY(!source->isUndoAvailable());
    }
    void codePanelsCoverBlankAndWrappedLinesWithoutChangingSource() {
        const QString text = "before `inline`\n\n```cpp\nint value = 1;\n\n// " + QString(180, 'x')
            + "\n```\n```unknown-language\nplain\n```\n\nafter\n\n~~~\nunfinished\n";
        source->setPlainText(text);
        source->clearUndoRedoStacks();
        select(sourceItem, text.size(), text.size());
        for (bool read : {false, true, false}) {
            controller->setReadMode(read);
            controller->refresh(); flush();
            const auto panels = controller->codeBlockPanels();
            QCOMPARE(panels.size(), 3);
            for (const auto &panel : panels) {
                const auto r = panel.toMap();
                QVERIFY(r.value("width").toReal() >= viewItem->property("width").toReal());
                QVERIFY(r.value("height").toReal() > 0);
            }
            const auto first = panels.first().toMap();
            const auto blank = view->find("int value").block().next();
            QCOMPARE(blank.text(), QString());
            view->documentLayout()->blockBoundingRect(blank);
            const qreal blankY = blank.layout()->position().y();
            QVERIFY(first.value("y").toReal() <= blankY);
            QVERIFY(first.value("y").toReal() + first.value("height").toReal()
                    >= blankY + blank.layout()->lineAt(0).height());
            QVERIFY(first.value("height").toReal() > 4 * 20); // Includes blank + wrapped lines.
            viewItem->setProperty("width", 220); controller->refresh(); flush();
            QVERIFY(controller->codeBlockPanels().first().toMap().value("height").toReal()
                    > first.value("height").toReal());
            viewItem->setProperty("width", 550); controller->refresh(); flush();
            QCOMPARE(source->toPlainText(), text);
            QVERIFY(!source->isUndoAvailable());
        }
        source->setPlainText("ordinary prose"); controller->refresh(); flush();
        QVERIFY(controller->codeBlockPanels().isEmpty());
    }
    void codePanelsDoNotDependOnTransientSyntaxStates() {
        const QString text = "before\n\n```cpp\nint value = 1;\n\n// code\n```\n\nafter\n";
        source->setPlainText(text); source->clearUndoRedoStacks();
        select(sourceItem, text.size(), text.size()); controller->refresh(); flush();
        QCOMPARE(controller->codeBlockPanels().size(), 1);
        const auto before = controller->codeBlockPanels();
        // Syntax state is an asynchronous implementation detail, not whether
        // the unchanged visible Markdown still contains a fenced block.
        for (auto block = view->begin(); block.isValid(); block = block.next())
            block.setUserState(-1);
        view->documentLayout()->update(QRectF()); flush();
        QCOMPARE(controller->codeBlockPanels(), before);
        const QSizeF page = view->pageSize();
        view->setPageSize(QSizeF(0, 0));
        view->documentLayout()->update(QRectF()); flush();
        QCOMPARE(view->findBlockByNumber(2).layout()->lineCount(), 0);
        QCOMPARE(controller->codeBlockPanels(), before); // Never publish partial geometry.
        view->setPageSize(page);
        view->markContentsDirty(0, view->characterCount());
        view->documentLayout()->update(QRectF()); flush();
        QCOMPARE(controller->codeBlockPanels(), before);
        QCOMPARE(source->toPlainText(), text);
        QVERIFY(!source->isUndoAvailable());
        source->setPlainText("plain prose"); flush();
        QVERIFY(controller->codeBlockPanels().isEmpty()); // Genuine removal still clears it.
    }
    void codePanelsFollowWrappedPreeditWithoutRebuilding() {
        const QString text = "```text\nx\n```\n\n```text\nsecond\n```\n";
        source->setPlainText(text); source->clearUndoRedoStacks();
        select(sourceItem, text.indexOf("\nx\n") + 2, text.indexOf("\nx\n") + 2);
        controller->refresh(); flush();
        QCOMPARE(controller->codeBlockPanels().size(), 2);
        const auto before = controller->codeBlockPanels();
        QSignalSpy rebuilds(controller.get(), &LivePreview::projectionChanged);
        QInputMethodEvent preedit(QString(200, 'a'), {});
        QCoreApplication::sendEvent(viewItem, &preedit); flush();
        QTRY_VERIFY(controller->codeBlockPanels().first().toMap().value("height").toReal()
                    > before.first().toMap().value("height").toReal());
        QVERIFY(controller->codeBlockPanels().last().toMap().value("y").toReal()
                > before.last().toMap().value("y").toReal());
        QCOMPARE(rebuilds.count(), 0);
        QCOMPARE(source->toPlainText(), text);
        QInputMethodEvent cancel(QString(), {});
        QCoreApplication::sendEvent(viewItem, &cancel); flush();
        QTRY_COMPARE(controller->codeBlockPanels(), before);
        QCOMPARE(source->toPlainText(), text);
        QVERIFY(!source->isUndoAvailable());
    }
    void readPanelsKeepContainerAndAdjacentFenceBoundaries() {
        const QString text = "- item\n\n    ```cpp\n    int nested = 1;\n    ```\n\n```py\nfirst = 1\n```\n```py\nsecond = 2\n```\n";
        source->setPlainText(text); select(sourceItem, text.size(), text.size());
        controller->setReadMode(true); controller->refresh(); flush();
        QCOMPARE(controller->codeBlockPanels().size(), 3);
        QCOMPARE(source->toPlainText(), text);
    }
    void richImageRestylesCoalesceLayoutNotifications() {
        QTemporaryDir directory;
        QImage image(160, 100, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::darkCyan);
        QVERIFY(image.save(directory.filePath("image.png")));
        ImageResolver resolver;
        resolver.root = directory.path();
        controller->setResolver(&resolver);
        QString text = QStringLiteral("![Picture](image.png)\n\n");
        for (int i = 0; i < 1000; ++i)
            text += QStringLiteral("# Heading %1\n\n").arg(i);
        text += QStringLiteral("After.\n");
        source->setPlainText(text);
        select(sourceItem, text.size(), text.size());
        controller->refresh();
        flush();
        auto *highlighter = viewItem->findChild<MarkdownHighlighter *>();
        QVERIFY(highlighter);
        QSignalSpy updates(view->documentLayout(), &QAbstractTextDocumentLayout::update);
        QElapsedTimer elapsed;
        elapsed.start();
        highlighter->setTheme(true, "#111111", "#eeeeee", "#5584aa", "#252525");
        qInfo() << "1000-heading rich restyle:" << elapsed.elapsed() << "ms; layout updates:" << updates.count();
        // TextEdit must not synchronously relayout the document per heading.
        QVERIFY(updates.count() > 0);
        QVERIFY(updates.count() < 20);
        QCOMPARE(source->toPlainText(), text);
    }

    void imageLayoutsAreCompleteWhenProjectionIsPublished() {
        QTemporaryDir directory;
        QImage image(640, 400, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::darkCyan);
        QVERIFY(image.save(directory.filePath("image.png")));
        ImageResolver resolver;
        resolver.root = directory.path();
        controller->setResolver(&resolver);
        const QString text = QStringLiteral("# Heading\n\n![Picture](image.png)\n\n| A | B |\n| --- | --- |\n| one | two |\n\n```cpp\n/* open\nclose */\n```\n\nAfter.\n");
        source->setPlainText(text);
        select(sourceItem, text.size(), text.size());
        int checked = 0;
        const auto connection = connect(controller.get(), &LivePreview::projectionChanged, this, [&] {
            int imageCount = 0;
            for (QTextBlock block = view->begin(); block.isValid(); block = block.next()) {
                for (auto it = block.begin(); !it.atEnd(); ++it) {
                    const QTextFragment fragment = it.fragment();
                    if (!fragment.isValid() || !fragment.charFormat().isImageFormat()) continue;
                    QVERIFY(block.layout());
                    // Do not force layout here: it must already be ready when published.
                    QVERIFY(block.layout()->lineForTextPosition(fragment.position() - block.position()).isValid());
                    ++checked;
                    ++imageCount;
                }
            }
            QCOMPARE(imageCount, controller->readMode() ? 1 : 2);
        });
        const auto disconnect = qScopeGuard([connection] { QObject::disconnect(connection); });
        for (int i = 0; i < 12; ++i) {
            viewItem->setProperty("width", 320 + (i % 3) * 110);
            controller->setReadMode(i % 2 == 0);
            controller->refresh();
            flush();
            QCOMPARE(source->toPlainText(), text);
        }
        QVERIFY(checked >= 12);
    }

    void sceneGraphModeSwitchWithImages() {
        if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
            QSKIP("Run this rendered-window regression with QT_QPA_PLATFORM=cocoa (or the native desktop platform).");
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QImage image(160, 100, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::darkCyan);
        QVERIFY(image.save(directory.filePath("image.png")));
        ImageResolver resolver;
        resolver.root = directory.path();
        controller->setResolver(&resolver);
        const QString text = QStringLiteral("# Heading\n\n![First](image.png)\n\nBefore **bold**.\n\n| A | B |\n| --- | --- |\n| one | two |\n\n```cpp\n/* comment\n still comment */\nint value = 42;\n```\n\n![Last](image.png)\n\nAfter.\n");
        source->setPlainText(text);
        select(sourceItem, text.size(), text.size());
        controller->refresh();
        flush();
        QQuickWindow window;
        auto threaded = std::make_shared<std::atomic_bool>(false);
        connect(&window, &QQuickWindow::beforeRendering, &window, [threaded] {
            if (QThread::currentThread() != QCoreApplication::instance()->thread())
                threaded->store(true);
        }, Qt::DirectConnection);
        window.resize(600, 800);
        auto *item = qobject_cast<QQuickItem *>(root.get());
        item->setParentItem(window.contentItem());
        const auto detach = qScopeGuard([item] { item->setParentItem(nullptr); });
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QSignalSpy frames(&window, &QQuickWindow::frameSwapped);
        for (int i = 0; i < 40; ++i) {
            const bool read = i % 2 == 0;
            viewItem->setProperty("readOnly", read);
            controller->setReadMode(read);
            if (!read) {
                const int at = i % 4 ? text.indexOf("Before") : text.size();
                select(sourceItem, at, at);
            }
            window.requestUpdate();
            QTRY_VERIFY_WITH_TIMEOUT(frames.count() > 0, 5000);
            frames.clear();
            QVERIFY(!window.grabWindow().isNull());
            QCOMPARE(source->toPlainText(), text);
        }
        QVERIFY2(threaded->load(), "Use QSG_RENDER_LOOP=threaded and a native hardware rendering backend.");
    }

    void languageCodeTokensSurviveReadImportAndModeSwitches() {
        const QList<QPair<QString, QStringList>> fences = {
            {QStringLiteral("python"), {QStringLiteral("def greet(name):"), QStringLiteral("    return 'hello' # comment")}},
            {QStringLiteral("cpp"), {QStringLiteral("int answer = 42; // comment")}},
            {QStringLiteral("cpp"), {QStringLiteral("/*"), QStringLiteral("int value;"), QStringLiteral("*/")}},
            {QStringLiteral("js"), {QStringLiteral("const value = true;")}},
            {QStringLiteral("json"), {QStringLiteral("{\"key\": true, \"n\": 12}")}},
            {QStringLiteral("yaml"), {QStringLiteral("enabled: true # comment")}},
            {QStringLiteral("sh"), {QStringLiteral("echo \"hello\" # comment")}},
            {QStringLiteral("python"), {QStringLiteral("'''unfinished string")}},
            // Immediately adjacent, same language: MUST reset string state.
            {QStringLiteral("python"), {QStringLiteral("return 123")}}
        };
        // Indented literal fence must not claim the prefix of the real C++ comment.
        QString markdown = QStringLiteral("before\n\n    ```cpp\n    /*\n    ```\n\n");
        for (const auto &fence : fences)
            markdown += QStringLiteral("```%1\n%2\n```\n").arg(fence.first, fence.second.join(QLatin1Char('\n')));
        markdown += QStringLiteral("\n> ```js\n> let quoted = false;\n> ```\n\n- item\n\n  ```cpp\n  int nested = 17;\n  ```\n\n```unknown-omawrite-language\nplain _code_\n```\n\n```\nuntyped_var = 99\n```\n\n```cpp\n```\n\nafter\n\n```js\nconst unfinished = 19;\n");
        for (bool dark : {false, true}) {
            viewItem->setProperty("previewDark", dark);
            sourceItem->setProperty("text", markdown);
            source->clearUndoRedoStacks();
            controller->setReadMode(true);
            controller->refresh();
            flush();
            CodeHighlighter lexer(dark);
            QTextBlock nextBlock = view->begin();
            auto checkFence = [&](const QString &language, const QStringList &lines) {
                KSyntaxHighlighting::State state;
                const auto definition = CodeHighlighter::resolveLanguage(language);
                for (const QString &line : lines) {
                    QTextBlock block = nextBlock;
                    while (block.isValid() && block.text() != line) block = block.next();
                    QVERIFY2(block.isValid(), qPrintable(line));
                    nextBlock = block.next();
                    const auto expected = lexer.tokenize(line, definition, state);
                    state = expected.state;
                    QVERIFY(!expected.tokens.isEmpty());
                    for (const auto &token : expected.tokens) {
                        QTextCursor cursor(view);
                        cursor.setPosition(block.position() + token.offset);
                        cursor.setPosition(block.position() + token.offset + 1, QTextCursor::KeepAnchor);
                        if (token.format.hasProperty(QTextFormat::ForegroundBrush))
                            QCOMPARE(cursor.charFormat().foreground(), token.format.foreground());
                        else
                            QCOMPARE(cursor.charFormat().foreground().color(), viewItem->property("color").value<QColor>());
                    }
                }
            };
            for (const auto &fence : fences) checkFence(fence.first, fence.second);
            checkFence(QStringLiteral("js"), {QStringLiteral("let quoted = false;")});
            checkFence(QStringLiteral("cpp"), {QStringLiteral("int nested = 17;")});
            checkFence(QStringLiteral("js"), {QStringLiteral("const unfinished = 19;")});
            for (const auto &plain : {QStringLiteral("plain _code_"), QStringLiteral("untyped_var = 99")}) {
                const auto cursor = view->find(plain);
                QVERIFY(!cursor.isNull());
                QVERIFY(!cursor.charFormat().fontItalic());
                QCOMPARE(cursor.charFormat().foreground().color(), viewItem->property("color").value<QColor>());
            }
            QCOMPARE(source->toPlainText(), markdown);
            QVERIFY(!source->isUndoAvailable());
            controller->setReadMode(false);
            flush();
            QCOMPARE(source->toPlainText(), markdown);
            QVERIFY(!source->isUndoAvailable());
        }
    }

    void cumulativeTableAndImageBudgets() {
        viewItem->setProperty("width", 4096);
        QString tables = QStringLiteral("before\n\n");
        for (int n = 0; n < 8; ++n) {
            tables += QStringLiteral("| Name | Value |\n| --- | --- |\n");
            for (int row = 0; row < 32; ++row)
                tables += QStringLiteral("| table %1 row %2 | value |\n").arg(n).arg(row);
            tables += QLatin1Char('\n');
        }
        sourceItem->setProperty("text", tables);
        flush();
        auto resourceCount = [&] {
            qsizetype bytes = 0;
            int count = 0;
            for (QTextBlock block = view->begin(); block.isValid(); block = block.next()) {
                for (auto it = block.begin(); !it.atEnd(); ++it) {
                    if (!it.fragment().charFormat().isImageFormat()) continue;
                    const auto format = it.fragment().charFormat().toImageFormat();
                    bytes += view->resource(QTextDocument::ImageResource, QUrl(format.name()))
                                 .value<QImage>().sizeInBytes();
                    ++count;
                }
            }
            return qMakePair(count, bytes);
        };
        const auto tablesRendered = resourceCount();
        QVERIFY(tablesRendered.first > 0 && tablesRendered.first < 8);
        QVERIFY(tablesRendered.second <= 32 * 1024 * 1024);
        QCOMPARE(source->toPlainText(), tables);

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QImage fixture(512, 512, QImage::Format_RGB32);
        fixture.fill(Qt::blue);
        QVERIFY(fixture.save(dir.filePath("test.png")));
        ImageResolver resolver;
        resolver.root = dir.path();
        controller->setResolver(&resolver);
        QString images = QStringLiteral("before\n\n");
        for (int n = 0; n < 100; ++n)
            images += QStringLiteral("![Image %1](test.png)\n\n").arg(n);
        sourceItem->setProperty("text", images);
        flush();
        const auto editImages = resourceCount();
        QVERIFY(editImages.first > 0 && editImages.first <= 32);
        QVERIFY(editImages.second <= 32 * 1024 * 1024);
        controller->setReadMode(true);
        const auto readImages = resourceCount();
        QVERIFY(readImages.first > 0 && readImages.first <= 32);
        QVERIFY(readImages.second <= 32 * 1024 * 1024);
        QCOMPARE(source->toPlainText(), images);
        controller->setReadMode(false);
        sourceItem->setProperty("text", QStringLiteral("after"));
        flush();
        QVERIFY(view->resource(QTextDocument::ImageResource, QUrl("preview:read/31"))
                    .value<QImage>().isNull());
        QVERIFY(view->resource(QTextDocument::ImageResource, QUrl("preview:edit/31"))
                    .value<QImage>().isNull());
    }

    void readSpacingAndInlineStylesMatchEdit() {
        const QString text = QStringLiteral("before\n\n# Header\n\nA **bold** *italic* [link](https://example.com) `code`.\n\nAnother paragraph.\n\n| A | B |\n| --- | --- |\n| cell | y |\n\nAfter table.\n\n```text\nfirst\nsecond\n```\n\n- one\n- two\n");
        sourceItem->setProperty("text", text);
        sourceItem->setProperty("cursorPosition", 0);
        auto find = [&] (const QString &word) {
            for (QTextBlock block = view->begin(); block.isValid(); block = block.next())
                if (block.text().contains(word)) return block;
            return QTextBlock();
        };
        auto gap = [&] (const QString &left, const QString &right) {
            view->size();
            return view->documentLayout()->blockBoundingRect(find(right)).top()
                 - view->documentLayout()->blockBoundingRect(find(left)).bottom();
        };
        auto format = [&] (const QString &word, bool edit) {
            const QTextBlock block = find(word);
            const int start = block.text().indexOf(word);
            if (edit) {
                for (const auto &range : block.layout()->formats())
                    if (start >= range.start && start < range.start + range.length) return range.format;
            }
            QTextCursor cursor(block);
            cursor.setPosition(block.position() + start);
            cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
            return cursor.charFormat();
        };
        for (bool dark : {false, true}) {
            viewItem->setProperty("previewDark", dark);
            viewItem->setProperty("color", QColor(dark ? "#eeeeee" : "#222324"));
            viewItem->setProperty("previewAccent", QColor("#c98545"));
            for (int pixels : {16, 28}) {
                QFont font = qvariant_cast<QFont>(viewItem->property("font"));
                font.setPixelSize(pixels);
                viewItem->setProperty("font", font);
                controller->refresh();
                flush();
                const qreal headingGap = gap("Header", "bold");
                const qreal paragraphGap = gap("bold", "Another");
                const auto bold = format("bold", true);
                const auto italic = format("italic", true);
                const auto link = format("link", true);
                const auto code = format("code", true);
                controller->setReadMode(true);
                QVERIFY(qAbs(gap("Header", "bold") - headingGap) < 1);
                QVERIFY(qAbs(gap("bold", "Another") - paragraphGap) < 1);
                QCOMPARE(format("bold", false).fontWeight(), bold.fontWeight());
                QCOMPARE(format("italic", false).fontItalic(), italic.fontItalic());
                QCOMPARE(format("link", false).foreground(), link.foreground());
                QCOMPARE(format("link", false).fontUnderline(), link.fontUnderline());
                QCOMPARE(format("link", false).anchorHref(), QStringLiteral("https://example.com"));
                QCOMPARE(format("code", false).background(), code.background());
                QCOMPARE(format("code", false).foreground(), code.foreground());
                const QTextBlock first = find("first"), second = find("second");
                QCOMPARE(first.blockFormat().bottomMargin(), 0.0);
                QCOMPARE(second.blockFormat().topMargin(), 0.0);
                QCOMPARE(find("one").blockFormat().bottomMargin(), 0.0);
                QTextTable *table = QTextCursor(find("cell")).currentTable();
                QVERIFY(table);
                QCOMPARE(find("cell").blockFormat().bottomMargin(), 0.0);
                QVERIFY(table->format().bottomMargin() > 0);
                QCOMPARE(source->toPlainText(), text);
                controller->setReadMode(false);
            }
        }
    }

    void normalTextStaysRegularAcrossModeToggles() {
        const QString text = QStringLiteral("# Header\n\nParagraph with **bold** text.\n\nAnother paragraph.");
        sourceItem->setProperty("text", text);
        sourceItem->setProperty("cursorPosition", text.size());
        for (bool dark : {false, true}) {
            const QColor strong(dark ? "#eeeeee" : "#222324");
            const QColor body(dark ? "#b8b8b8" : "#575859");
            viewItem->setProperty("previewDark", dark);
            viewItem->setProperty("previewForeground", strong);
            viewItem->setProperty("color", body);
            controller->refresh();
            flush();
            for (bool read : {false, true, false, true, false}) {
                controller->setReadMode(read);
                for (auto block = view->begin(); block.isValid(); block = block.next()) {
                    QTextCursor cursor(block);
                    cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
                    const auto format = cursor.charFormat();
                    if (block.text().startsWith("Header") && read)
                        QCOMPARE(format.foreground().color(), strong);
                    if (!block.text().startsWith("Paragraph") && !block.text().startsWith("Another")) continue;
                    QCOMPARE(format.fontWeight(), int(QFont::Normal));
                    QVERIFY(!format.fontItalic());
                    QCOMPARE(block.blockFormat().headingLevel(), 0);
                    if (read) QCOMPARE(format.foreground().color(), body);
                    else QCOMPARE(format.foreground().style(), Qt::NoBrush); // uses the TextEdit body color
                }
                QCOMPARE(source->toPlainText(), text);
            }
        }
    }

    void nestedAndLooseListsKeepSourceSpacing() {
        for (const QString &items : {QStringLiteral("- parent\n  - child\n- sibling"),
                                    QStringLiteral("- parent\n\n- child\n\n- sibling")}) {
            sourceItem->setProperty("text", "before\n\n" + items + "\n\nafter");
            sourceItem->setProperty("cursorPosition", 0);
            flush();
            auto gap = [&] (const QString &left, const QString &right) {
                view->size();
                QRectF a, b;
                for (auto block = view->begin(); block.isValid(); block = block.next()) {
                    if (block.text().contains(left)) a = view->documentLayout()->blockBoundingRect(block);
                    if (block.text().contains(right)) b = view->documentLayout()->blockBoundingRect(block);
                }
                return b.top() - a.bottom();
            };
            const qreal firstGap = gap("parent", "child"), secondGap = gap("child", "sibling");
            controller->setReadMode(true);
            QVERIFY(qAbs(gap("parent", "child") - firstGap) < 1);
            QVERIFY(qAbs(gap("child", "sibling") - secondGap) < 1);
            controller->setReadMode(false);
        }
    }

    void headingSizesSurviveModeToggleAndZoom() {
        QString text = QStringLiteral("before\n\n");
        for (int level = 1; level <= 6; ++level)
            text += QString(level, QLatin1Char('#')) + QStringLiteral(" Title %1\n\n").arg(level);
        sourceItem->setProperty("text", text);
        sourceItem->setProperty("cursorPosition", 0);
        for (int pixels : {16, 28}) {
            QFont font = qvariant_cast<QFont>(viewItem->property("font"));
            font.setPixelSize(pixels);
            viewItem->setProperty("font", font);
            controller->refresh();
            flush();
            auto sizes = [&] {
                view->size();
                QList<qreal> result;
                for (QTextBlock block = view->begin(); block.isValid(); block = block.next()) {
                    const int start = block.text().indexOf(QStringLiteral("Title"));
                    if (start < 0) continue;
                    const auto runs = block.layout()->glyphRuns(start, 5);
                    result.append(runs.isEmpty() ? -1 : runs.first().rawFont().pixelSize());
                }
                return result;
            };
            const auto editSizes = sizes();
            QCOMPARE(editSizes.size(), 6);
            QVERIFY(editSizes.first() > pixels);
            QVERIFY(editSizes.last() > 0 && editSizes.last() < pixels);
            controller->setReadMode(true);
            QCOMPARE(sizes(), editSizes);
            controller->setReadMode(false);
            QCOMPARE(sizes(), editSizes);
            QCOMPARE(source->toPlainText(), text);
        }
    }

    void mappingUtf16() {
        const QList<LivePreview::Segment> segments{{0, 5, 0, 5, false}, {5, 25, 5, 6, true}, {25, 31, 6, 12, false}};
        QCOMPARE(LivePreview::toSource(segments, 5), 5);
        QCOMPARE(LivePreview::toSource(segments, 6), 25);
        QCOMPARE(LivePreview::toSource(segments, 10), 29);
        QCOMPARE(LivePreview::toView(segments, 18), 5);
        QCOMPARE(LivePreview::toView(segments, 25), 6);
        QCOMPARE(LivePreview::toView(segments, 29), 10);
        QCOMPARE(QString::fromUtf8("😀").size(), 2);
    }
    void nativeObjectAndSourcePreservation() {
        QVERIFY(view->toPlainText().contains(QChar::ObjectReplacementCharacter));
        QCOMPARE(source->toPlainText(), markdown);
        QVERIFY(!source->isUndoAvailable());
        for (int n = 0; n < 4; ++n) {
            controller->setReadMode(true);
            QVERIFY(!view->toPlainText().contains("| --- |"));
            controller->setReadMode(false);
            viewItem->setProperty("width", 400 + n * 20);
            controller->refresh();
            flush();
        }
        QCOMPARE(source->toPlainText(), markdown);
        QVERIFY(!source->isUndoAvailable());
    }
    void activeBlockAndBackwardsSelection() {
        const int tableStart = markdown.indexOf('|');
        select(sourceItem, tableStart + 4, tableStart + 4);
        QVERIFY(!view->toPlainText().contains(QChar::ObjectReplacementCharacter));
        QCOMPARE(view->toPlainText(), markdown);
        select(sourceItem, markdown.size() - 1, 2);
        QCOMPARE(viewItem->property("cursorPosition").toInt(), 2);
        QCOMPARE(viewItem->property("selectionEnd").toInt(), int(markdown.size()) - 1);
        QCOMPARE(source->toPlainText(), markdown);
    }
    void nativeTypingUndoRedo() {
        select(sourceItem, markdown.size() - 1, markdown.size() - 1);
        type("XYZ");
        QCOMPARE(source->toPlainText(), QString(markdown).insert(markdown.size() - 1, "XYZ"));
        QVERIFY(source->isUndoAvailable());
        QKeyEvent undo(QEvent::KeyPress, Qt::Key_Z, Qt::ControlModifier);
        QCoreApplication::sendEvent(viewItem, &undo); flush();
        QCOMPARE(source->toPlainText(), markdown);
        QKeyEvent redo(QEvent::KeyPress, Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
        QCoreApplication::sendEvent(viewItem, &redo); flush();
        QCOMPARE(source->toPlainText(), QString(markdown).insert(markdown.size() - 1, "XYZ"));
        controller->setReadMode(true);
        controller->setReadMode(false);
        source->undo(); flush();
        QCOMPARE(source->toPlainText(), markdown);
    }
    void boundaryDeleteIsOneSourceCharacter() {
        const int after = markdown.indexOf("\n\nafter");
        select(sourceItem, after + 1, after + 1);
        QKeyEvent key(QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier);
        QCoreApplication::sendEvent(viewItem, &key); flush();
        QCOMPARE(source->toPlainText(), QString(markdown).remove(after, 1));
        source->undo(); flush();
        QCOMPARE(source->toPlainText(), markdown);
    }
    void nativeSelectionReplacement() {
        const int tableStart = markdown.indexOf('|');
        select(viewItem, view->toPlainText().size() - 1, 2);
        QVERIFY(!view->toPlainText().contains(QChar::ObjectReplacementCharacter));
        type("X");
        QCOMPARE(source->toPlainText(), markdown.left(2) + "X\n");
        Q_UNUSED(tableStart);
        source->undo(); flush();
        QCOMPARE(source->toPlainText(), markdown);
    }
    void imePreservesSource() {
        select(sourceItem, markdown.size() - 1, markdown.size() - 1);
        QInputMethodEvent preedit(QString::fromUtf8("に"), {});
        QCoreApplication::sendEvent(viewItem, &preedit); flush();
        QCOMPARE(source->toPlainText(), markdown);
        QInputMethodEvent commit;
        commit.setCommitString(QString::fromUtf8("日本"));
        QCoreApplication::sendEvent(viewItem, &commit); flush();
        QCOMPARE(source->toPlainText(), QString(markdown).insert(markdown.size() - 1, QString::fromUtf8("日本")));
        source->undo(); flush();
        QCOMPARE(source->toPlainText(), markdown);
    }
    void localAndRemoteImages() {
        QTemporaryDir directory;
        QImage image(300, 200, QImage::Format_RGB32); image.fill(Qt::red);
        QVERIFY(image.save(directory.filePath("my image.png")));
        ImageResolver resolver; resolver.root = directory.path();
        controller->setResolver(&resolver);
        const QString text = "start\n\n![alt](<my image.png>)\n\n![remote](https://invalid.example/a.png)\n\nend";
        source->setPlainText(text); flush();
        controller->refresh(); flush();
        QCOMPARE(view->toPlainText().count(QChar::ObjectReplacementCharacter), 1);
        QCOMPARE(source->toPlainText(), text);
        controller->setReadMode(true);
        QVERIFY(!view->toHtml().contains("https://invalid.example/a.png"));
        controller->setReadMode(false);
        QCOMPARE(source->toPlainText(), text);
    }
    void repeatedTextUsesActualNativeEditRange() {
        source->setPlainText("aaaa"); flush();
        select(sourceItem, 0, 0);
        type("a");
        QCOMPARE(sourceItem->property("cursorPosition").toInt(), 1);
        QCOMPARE(source->toRawText(), QString("aaaaa"));
        source->setPlainText(QString(QChar::Nbsp) + QLatin1Char(' ')); flush();
        select(sourceItem, 0, 0);
        QKeyEvent key(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
        QCoreApplication::sendEvent(viewItem, &key); flush();
        QCOMPARE(source->toRawText(), QString(" "));
        source->undo(); flush();
        QCOMPARE(source->toRawText(), QString(QChar::Nbsp) + QLatin1Char(' '));
    }
    void implicitResourcesAreDeniedAndRichPasteCannotChangeSource() {
        QTemporaryDir directory;
        QImage image(40, 20, QImage::Format_ARGB32); image.fill(Qt::green);
        const QUrl url = QUrl::fromLocalFile(directory.filePath("private.png"));
        QVERIFY(image.save(url.toLocalFile()));
        QVERIFY(!view->resource(QTextDocument::ImageResource, url).isValid());
        auto *mime = new QMimeData;
        mime->setHtml("<p><img src='https://invalid.example/private.png'></p>");
        QGuiApplication::clipboard()->setMimeData(mime);
        QVERIFY(QMetaObject::invokeMethod(viewItem, "paste"));
        flush();
        QCOMPARE(source->toPlainText(), markdown);
        QVERIFY(!view->toHtml().contains("https://invalid.example/private.png"));
        QVERIFY(!view->resource(QTextDocument::ImageResource,
                               QUrl("https://invalid.example/private.png")).isValid());
    }
    void ambiguousObjectRemovalIsRejected() {
        int object = view->toPlainText().indexOf(QChar::ObjectReplacementCharacter);
        QVERIFY(object >= 0);
        QTextCursor cursor(view);
        cursor.setPosition(object);
        cursor.deleteChar(); flush();
        QCOMPARE(source->toPlainText(), markdown);
        QVERIFY(view->toPlainText().contains(QChar::ObjectReplacementCharacter));
    }
    void constrainedMermaidSyntax() {
        QVERIFY(MermaidRenderer::supported("flowchart LR\nA-->B"));
        QVERIFY(MermaidRenderer::supported("sequenceDiagram\nAlice->>Bob: Hello"));
        QVERIFY(!MermaidRenderer::supported("flowchart LR\nclick A 'https://example.com'"));
        QVERIFY(!MermaidRenderer::supported("flowchart LR\n%%{init:{}}%%"));
        QVERIFY(!MermaidRenderer::supported("flowchart LR\nA[<img src='x'>]"));
    }
    void mermaidGenerationIsBoundedAndCannotRerenderForever() {
#ifndef Q_OS_UNIX
        QSKIP("Fake mmdc uses a POSIX script");
#else
        const QByteArray previousPath = qgetenv("PATH");
        const auto restorePath = qScopeGuard([&] { qputenv("PATH", previousPath); });
        QTemporaryDir directory;
        QImage image(40, 20, QImage::Format_ARGB32); image.fill(Qt::red);
        const QString png = directory.filePath("fixture.png");
        QVERIFY(image.save(png));
        QFile script(directory.filePath("mmdc"));
        QVERIFY(script.open(QIODevice::WriteOnly));
        script.write(("#!/bin/sh\n/bin/cp '" + png + "' \"$4\"\n").toUtf8());
        script.close();
        QVERIFY(script.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        qputenv("PATH", directory.path().toUtf8());
        MermaidRenderer renderer;
        renderer.setGeneration("document");
        QSignalSpy ready(&renderer, &MermaidRenderer::ready);
        auto requestAll = [&] {
            for (int i = 0; i < 20; ++i)
                renderer.request(QString("```mermaid\nflowchart LR\nA%1-->B\n```").arg(i), false, 500);
        };
        connect(&renderer, &MermaidRenderer::ready, this, requestAll);
        requestAll();
        QTRY_COMPARE(ready.count(), 16);
        QTest::qWait(100);
        QCOMPARE(ready.count(), 16);
        renderer.setGeneration("next document");
        disconnect(&renderer, &MermaidRenderer::ready, this, nullptr);
        const QString newCode = "```mermaid\nflowchart LR\nNew-->B\n```";
        QVERIFY(renderer.request(newCode, false, 500).isNull());
        QTRY_COMPARE(ready.count(), 17);
        QCOMPARE(renderer.request(newCode, false, 500).size(), QSize(40, 20));
        QVERIFY(renderer.request("```mermaid\nflowchart LR\nA-->B", false, 500).isNull());
#endif
    }
    void readLinks() {
        source->setPlainText("[Example](https://example.com)\n\n**bold**"); flush();
        controller->setReadMode(true);
        QVERIFY(view->toHtml().contains("href=\"https://example.com\""));
        QVERIFY(!view->toPlainText().contains("**"));
        controller->setReadMode(false);
        QCOMPARE(source->toPlainText(), QString("[Example](https://example.com)\n\n**bold**"));
    }
};
QTEST_MAIN(LivePreviewTest)
#include "tst_livepreview.moc"
