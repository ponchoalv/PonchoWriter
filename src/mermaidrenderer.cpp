#include "mermaidrenderer.h"
#include "previewblocks.h"
#include "markdownhighlighter.h"

#include <QCryptographicHash>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#ifdef Q_OS_UNIX
#include <signal.h>
#include <unistd.h>
#endif

QStringList MermaidRenderer::defaultSearchDirectories() {
#ifdef Q_OS_MACOS
    return {QStringLiteral("/opt/homebrew/bin"), QStringLiteral("/usr/local/bin"), QStringLiteral("/opt/local/bin")};
#else
    return {};
#endif
}

MermaidRenderer::MermaidRenderer(QObject *parent)
    : MermaidRenderer(parent, QProcessEnvironment::systemEnvironment(), defaultSearchDirectories()) {}

MermaidRenderer::MermaidRenderer(QObject *parent, const QProcessEnvironment &environment,
                                 const QStringList &fallbackDirectories) : QObject(parent) {
    const auto inherited = environment.value(QStringLiteral("PATH")).split(QDir::listSeparator(), Qt::SkipEmptyParts);
    if (!inherited.isEmpty()) m_executable = QStandardPaths::findExecutable(QStringLiteral("mmdc"), inherited);
    if (m_executable.isEmpty() && !fallbackDirectories.isEmpty())
        m_executable = QStandardPaths::findExecutable(QStringLiteral("mmdc"), fallbackDirectories);
    auto child = environment;
    QStringList paths;
    if (!m_executable.isEmpty()) paths.append(QFileInfo(m_executable).absolutePath());
    paths.append(inherited);
    paths.append(fallbackDirectories);
    paths.removeDuplicates();
    child.insert(QStringLiteral("PATH"), paths.join(QDir::listSeparator()));
    m_process.setProcessEnvironment(child);
#ifdef Q_OS_UNIX
    // mmdc launches Chromium children: killing only Node leaks browsers.
    m_process.setChildProcessModifier([] { ::setsid(); });
#else
    // Keep source fallback until process-tree cancellation is implemented.
    m_executable.clear();
#endif
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(15000);
    // Do not buffer unbounded CLI diagnostics from an external process.
    m_process.setStandardOutputFile(QProcess::nullDevice());
    m_process.setStandardErrorFile(QProcess::nullDevice());
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        stopProcess();
        finish(false);
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this] { finish(false); });
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus status) {
        if (m_running)
            finish(code == 0 && status == QProcess::NormalExit);
        else
            startNext();
    });
}
MermaidRenderer::~MermaidRenderer() {
    disconnect(&m_process, nullptr, this, nullptr);
    stopProcess();
    m_process.waitForFinished(1000);
}
void MermaidRenderer::stopProcess() {
#ifdef Q_OS_UNIX
    const qint64 pid = m_process.processId();
    if (pid > 0)
        ::kill(-pid_t(pid), SIGKILL);
#endif
    if (m_process.state() != QProcess::NotRunning)
        m_process.kill();
}
void MermaidRenderer::setGeneration(const QString &generation) {
    if (generation == m_generationKey)
        return;
    m_generationKey = generation;
    ++m_generation;
    m_attempted.clear();
    m_queue.clear();
}
bool MermaidRenderer::supported(const QString &diagram) {
    if (diagram.isEmpty() || diagram.size() > 32000
        || diagram.contains(QLatin1Char('\\')) || diagram.contains(QLatin1Char('&'))
        || diagram.contains(QLatin1Char('@')))
        return false;
    // No styling/configuration or escaped/entity-encoded resource syntax.
    // This is a conservative syntax subset, not an OS browser sandbox.
    static const QRegularExpression unsafe(
        QStringLiteral("%%\\s*\\{|---|://|//|/\\*|\\*/|\\b(?:click|href|image|img|icon|style|classDef|linkStyle|cssClass|themeCSS|fontawesome)\\b|\\burl\\s*\\(|<\\s*[A-Za-z/!]|(?:file|data|javascript):"),
        QRegularExpression::CaseInsensitiveOption);
    if (unsafe.match(diagram).hasMatch())
        return false;
    static const QRegularExpression kind(
        QStringLiteral("^\\s*(?:graph\\s|flowchart\\s|sequenceDiagram\\b|classDiagram\\b|stateDiagram(?:-v2)?\\b|erDiagram\\b|gantt\\b|pie\\b|journey\\b)"));
    return kind.match(diagram).hasMatch();
}
QImage MermaidRenderer::request(const QString &markdown, bool dark, int width) {
    if (m_executable.isEmpty())
        return {};
    const int first = markdown.indexOf(QLatin1Char('\n'));
    const int last = markdown.lastIndexOf(QLatin1Char('\n'));
    if (first < 0 || last <= first)
        return {};
    const QString opening = markdown.left(first);
    const int fenceLength = MarkdownHighlighter::openingFenceLength(opening);
    if (!fenceLength || !MarkdownHighlighter::closesFence(markdown.mid(last + 1), fenceLength,
                                                         opening.trimmed().at(0)))
        return {};
    const QString diagram = markdown.mid(first + 1, last - first - 1);
    if (!supported(diagram))
        return {};
    const QString key = QString::fromLatin1(QCryptographicHash::hash(
        (diagram + QString::number(dark) + QString::number(width)).toUtf8(), QCryptographicHash::Sha256).toHex());
    if (m_cache.contains(key)) {
        m_attempted.insert(key);
        return m_cache.value(key);
    }
    // At most 16 distinct diagrams per source/geometry generation. Keeping
    // attempt records prevents eviction/failure from endlessly requeueing.
    if (m_attempted.contains(key) || m_attempted.size() >= 16 || m_queue.size() >= 8)
        return {};
    m_attempted.insert(key);
    m_queue.enqueue({key, diagram, dark, qBound(80, width, 2048), m_generation});
    startNext();
    return {};
}
void MermaidRenderer::startNext() {
    if (m_running || m_process.state() != QProcess::NotRunning || m_queue.isEmpty())
        return;
    m_current = m_queue.dequeue();
    m_directory = std::make_unique<QTemporaryDir>();
    m_running = true;
    if (!m_directory->isValid()) {
        finish(false);
        return;
    }
    QFile input(m_directory->filePath(QStringLiteral("input.mmd")));
    QFile config(m_directory->filePath(QStringLiteral("config.json")));
    if (!input.open(QIODevice::WriteOnly) || !config.open(QIODevice::WriteOnly)) {
        finish(false);
        return;
    }
    input.write(m_current.diagram.toUtf8()); input.close();
    config.write("{\"securityLevel\":\"strict\",\"htmlLabels\":false,\"maxTextSize\":32000,\"flowchart\":{\"htmlLabels\":false}}");
    config.close();
    m_process.setProgram(m_executable);
    m_process.setArguments({QStringLiteral("-i"), input.fileName(),
        QStringLiteral("-o"), m_directory->filePath(QStringLiteral("output.png")),
        QStringLiteral("-c"), config.fileName(),
        QStringLiteral("-t"), m_current.dark ? QStringLiteral("dark") : QStringLiteral("default"),
        // Do not pass the legacy -w/--width option: Mermaid CLI 12 removed
        // it. Qt bounds/scales the decoded result to m_current.width below.
        QStringLiteral("-b"), QStringLiteral("transparent")});
    m_process.start();
    m_timeout.start();
}
void MermaidRenderer::finish(bool success) {
    if (!m_running)
        return;
    m_running = false;
    m_timeout.stop();
    QImage image;
    if (success && m_directory)
        image = PreviewBlocks::loadImage(QUrl::fromLocalFile(m_directory->filePath(QStringLiteral("output.png"))), m_current.width);
    if (!image.isNull() && m_current.generation == m_generation) {
        constexpr qsizetype maxCacheBytes = 32 * 1024 * 1024;
        qsizetype bytes = image.sizeInBytes();
        for (const auto &cached : m_cache)
            bytes += cached.sizeInBytes();
        if (m_cache.size() >= 16 || bytes > maxCacheBytes)
            m_cache.clear();
        if (image.sizeInBytes() <= maxCacheBytes)
            m_cache.insert(m_current.key, image);
    }
    // Wait for the killed process to finish before cleaning its working files
    // or reusing QProcess. The finished signal resumes queued work.
    if (m_process.state() == QProcess::NotRunning)
        m_directory.reset();
    emit ready();
    QTimer::singleShot(0, this, &MermaidRenderer::startNext);
}
