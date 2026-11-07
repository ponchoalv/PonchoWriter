#include <QtTest>
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
#include <QInputMethodEvent>
#include <QTemporaryDir>
#include <QImage>
#include <QClipboard>
#include <QMimeData>
#include <QSignalSpy>
#include <QScopeGuard>
#include "../../src/mermaidrenderer.h"
#include "../../src/livepreview.h"

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
