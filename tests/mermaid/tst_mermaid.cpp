#include "mermaidrenderer.h"
#include <QFile>
#include <QDir>
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
    void guiPathFindsCliAndItsInterpreter_data() {
        QTest::addColumn<bool>("separateNode");
        QTest::addColumn<bool>("symlinkCli");
        QTest::newRow("same-bin") << false << false;
        QTest::newRow("separate-bin") << true << false;
        QTest::newRow("npm-symlink") << false << true;
    }
    void guiPathFindsCliAndItsInterpreter() {
#ifndef Q_OS_UNIX
        QSKIP("Rendering remains disabled on Windows.");
#else
        QFETCH(bool, separateNode);
        QFETCH(bool, symlinkCli);
        QTemporaryDir cli, node, inherited;
        QVERIFY(cli.isValid() && node.isValid() && inherited.isValid());
        QImage fixture(40, 20, QImage::Format_ARGB32); fixture.fill(Qt::green);
        const QString png = cli.filePath("original.png"); QVERIFY(fixture.save(png));
        const auto executable = [](const QString &path, const QByteArray &bytes) {
            QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size()
                && file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        };
        const QString script = symlinkCli ? cli.filePath("lib/cli.mjs") : cli.filePath("mmdc");
        if (symlinkCli) QVERIFY(QDir().mkpath(cli.filePath("lib")));
        QVERIFY(executable(script, "#!/usr/bin/env node\nunused fake JavaScript\n"));
        if (symlinkCli) QVERIFY(QFile::link(script, cli.filePath("mmdc")));
        const QString nodeDir = separateNode ? node.path() : cli.path();
        // env passes the script name as $1, so the CLI output argument is $5.
        QVERIFY(executable(nodeDir + "/node", "#!/bin/sh\n/bin/cp \"$PONCHOWRITER_TEST_PNG\" \"$5\"\n"));
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("PATH", inherited.path()); environment.insert("PONCHOWRITER_TEST_PNG", png);
        const QByteArray parentPath = qgetenv("PATH");
        MermaidRenderer renderer(nullptr, environment, {cli.path(), nodeDir});
        QCOMPARE(renderer.m_executable, cli.filePath("mmdc"));
        const auto childPaths = renderer.m_process.processEnvironment().value("PATH").split(':');
        QStringList expectedPaths{cli.path(), inherited.path()};
        if (separateNode) expectedPaths.append(node.path());
        QCOMPARE(childPaths, expectedPaths); QCOMPARE(qgetenv("PATH"), parentPath);
        QSignalSpy ready(&renderer, &MermaidRenderer::ready);
        const auto code = fenced("flowchart LR\nA-->B");
        QVERIFY(renderer.request(code, false, 500).isNull()); QTRY_COMPARE(ready.count(), 1);
        QCOMPARE(renderer.request(code, false, 500).size(), QSize(40, 20));
        QCOMPARE(qgetenv("PATH"), parentPath);
        // PATH-installed CLI wins over fallback copies; an empty PATH does not
        // silently consult the host's real PATH during injected discovery.
        QVERIFY(executable(node.filePath("mmdc"), "#!/bin/sh\nexit 7\n"));
        environment.insert("PATH", cli.path());
        MermaidRenderer preferred(nullptr, environment, {node.path()});
        QCOMPARE(preferred.m_executable, cli.filePath("mmdc"));
        environment.remove("PATH");
        MermaidRenderer unavailable(nullptr, environment, {}); QVERIFY(unavailable.m_executable.isEmpty());
#endif
    }
    void missingNodeAndNonExecutableCliRemainSource() {
#ifndef Q_OS_UNIX
        QSKIP("Rendering remains disabled on Windows.");
#else
        QTemporaryDir directory; QVERIFY(directory.isValid());
        QFile cli(directory.filePath("mmdc")); QVERIFY(cli.open(QIODevice::WriteOnly));
        cli.write("#!/usr/bin/env node\nunused\n"); cli.close();
        QVERIFY(cli.setPermissions(QFile::ReadOwner | QFile::WriteOwner));
        auto environment = QProcessEnvironment::systemEnvironment(); environment.remove("PATH");
        MermaidRenderer notExecutable(nullptr, environment, {directory.path()}); QVERIFY(notExecutable.m_executable.isEmpty());
        QVERIFY(cli.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        MermaidRenderer missingNode(nullptr, environment, {directory.path()});
        QSignalSpy ready(&missingNode, &MermaidRenderer::ready);
        const auto code = fenced("flowchart LR\nA-->B");
        QVERIFY(missingNode.request(code, false, 500).isNull()); QTRY_COMPARE(ready.count(), 1);
        QVERIFY(missingNode.request(code, false, 500).isNull()); QCOMPARE(ready.count(), 1);
#endif
    }
    void platformFallbacksAreExplicit() {
#ifdef Q_OS_MACOS
        QCOMPARE(MermaidRenderer::defaultSearchDirectories(), QStringList({"/opt/homebrew/bin", "/usr/local/bin", "/opt/local/bin"}));
#else
        QVERIFY(MermaidRenderer::defaultSearchDirectories().isEmpty());
#endif
    }
    void unavailable() {
        qputenv("PATH", "/nonexistent/ponchowriter-mermaid-renderer");
        MermaidRenderer renderer(nullptr, QProcessEnvironment::systemEnvironment(), {});
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
        MermaidRenderer renderer(nullptr, QProcessEnvironment::systemEnvironment(), {});
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
        MermaidRenderer renderer(nullptr, QProcessEnvironment::systemEnvironment(), {});
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
