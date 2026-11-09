#include "mermaidrenderer.h"
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class MermaidTests : public QObject {
    Q_OBJECT
    QByteArray previousPath;
    static QString fenced(const QString &body) { return "```mermaid\n" + body + "\n```"; }
private slots:
    void init() { previousPath = qgetenv("PATH"); }
    void cleanup() {
        qputenv("PATH", previousPath);
        qunsetenv("PONCHOWRITER_TEST_PNG");
    }
    void supportedSyntax() {
        QVERIFY(MermaidRenderer::supported("flowchart LR\nA-->B"));
        QVERIFY(MermaidRenderer::supported("sequenceDiagram\nAlice->>Bob: Hello"));
        QVERIFY(!MermaidRenderer::supported("flowchart LR\nclick A \"https://example.com\""));
        QVERIFY(!MermaidRenderer::supported("%%{init: {}}%%\nflowchart LR\nA-->B"));
        QVERIFY(!MermaidRenderer::supported("flowchart LR\nA[<img src='file:///tmp/x'>]"));
        QVERIFY(!MermaidRenderer::supported(QStringLiteral("x").repeated(32001)));
        QVERIFY(!MermaidRenderer::supported("flowchart LR\nA-->B\nclassDef default fill:red"));
        QVERIFY(!MermaidRenderer::supported("flowchart LR\nA-->B\nstyle A fill:u\\75rl(x)"));
        QVERIFY(!MermaidRenderer::supported("flowchart LR\nA[&#x3c;img]"));
        QVERIFY(!MermaidRenderer::supported("flowchart LR\nA@{ img: 'something' }"));
    }
    void unavailable() {
        qputenv("PATH", "/nonexistent/ponchowriter-mermaid-renderer");
        MermaidRenderer renderer;
        QSignalSpy ready(&renderer, &MermaidRenderer::ready);
        QVERIFY(renderer.request(fenced("flowchart LR\nA-->B"), false, 500).isNull());
        QCOMPARE(ready.count(), 0);
    }
    void renderAndCache() {
#ifndef Q_OS_UNIX
        QSKIP("Fake renderer uses a POSIX shell.");
#else
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QImage fixture(40, 20, QImage::Format_ARGB32);
        fixture.fill(Qt::red);
        const QString png = dir.filePath("fixture.png");
        QVERIFY(fixture.save(png));
        qputenv("PONCHOWRITER_TEST_PNG", png.toUtf8());
        const QString script = dir.filePath("mmdc");
        QFile file(script);
        QVERIFY(file.open(QIODevice::WriteOnly));
        // CLI 12 rejects the old viewport-width switches. Fail rather than
        // let a permissive fake renderer conceal this compatibility bug.
        file.write("#!/bin/sh\nfor arg in \"$@\"; do\n"
                   "case \"$arg\" in -w|--width) echo 'unknown option' >&2; exit 1;; esac\n"
                   "done\ncp \"$PONCHOWRITER_TEST_PNG\" \"$4\"\n");
        file.close();
        QVERIFY(file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        qputenv("PATH", dir.path().toUtf8() + ":" + previousPath);
        MermaidRenderer renderer;
        QSignalSpy ready(&renderer, &MermaidRenderer::ready);
        const QString code = fenced("flowchart LR\nA[unicode 😀]-->B");
        QVERIFY(renderer.request(code, false, 500).isNull());
        QVERIFY(renderer.request(code, false, 500).isNull());
        QTRY_COMPARE(ready.count(), 1);
        QCOMPARE(renderer.request(code, false, 500).size(), QSize(40, 20));
        QCOMPARE(ready.count(), 1);
#endif
    }
    void invalidOutput() {
#ifndef Q_OS_UNIX
        QSKIP("Fake renderer uses a POSIX shell.");
#else
        QTemporaryDir dir;
        QFile script(dir.filePath("mmdc"));
        QVERIFY(script.open(QIODevice::WriteOnly));
        script.write("#!/bin/sh\nexit 1\n");
        script.close();
        QVERIFY(script.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        qputenv("PATH", dir.path().toUtf8() + ":" + previousPath);
        MermaidRenderer renderer;
        QSignalSpy ready(&renderer, &MermaidRenderer::ready);
        const QString code = fenced("flowchart LR\nA-->B");
        QVERIFY(renderer.request(code, false, 500).isNull());
        QTRY_COMPARE(ready.count(), 1);
        QVERIFY(renderer.request(code, false, 500).isNull());
        QCOMPARE(ready.count(), 1);
#endif
    }
};
QTEST_GUILESS_MAIN(MermaidTests)
#include "tst_mermaid.moc"
