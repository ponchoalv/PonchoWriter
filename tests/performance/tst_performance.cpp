// Opt-in native-desktop benchmark. No user documents or settings are opened.
#include <QtTest>
#include <QFontDatabase>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QScreen>
#include <QSettings>
#include <QSGRendererInterface>
#include <QStandardPaths>
#include <QSysInfo>
#include <QVariantAnimation>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <functional>
#include <mutex>
#include "backend.h"
#include "livepreview.h"
#include "spellingcontroller.h"
#include <ksyntaxhighlighting_version.h>
#include <sonnet_version.h>

static bool await(const std::function<bool()> &ready, int timeout = 15000) {
    QElapsedTimer clock; clock.start();
    while (!ready() && clock.elapsed() < timeout) QTest::qWait(1);
    return ready();
}

static QJsonObject distribution(QList<double> values) {
    if (values.isEmpty()) return {{"samples", 0}};
    std::sort(values.begin(), values.end());
    const auto percentile = [&](double p) {
        return values.at(qBound(0, int(std::ceil(p * values.size())) - 1, int(values.size()) - 1));
    };
    return {{"samples", values.size()}, {"median_ms", percentile(.5)},
            {"p95_ms", percentile(.95)}, {"max_ms", values.last()}};
}

// Direct render-thread callbacks: no GUI objects or documents are inspected here.
// A marker is consumed only at synchronization, then completed at frameSwapped.
// This avoids counting a frame already in flight before the GUI requested it.
class FrameRecorder {
public:
    explicit FrameRecorder(QQuickWindow *window) : window(window) {
        state->clock.start();
        const auto shared = state; // An already-running callback may outlive disconnect().
        sync = QObject::connect(window, &QQuickWindow::afterSynchronizing, window, [shared] {
            shared->synchronized.store(shared->requested.load());
            shared->threaded.store(QThread::currentThread() != qApp->thread());
        }, Qt::DirectConnection);
        swap = QObject::connect(window, &QQuickWindow::frameSwapped, window, [shared] {
            const qint64 now = shared->clock.nsecsElapsed();
            shared->completed.store(shared->synchronized.load());
            std::lock_guard<std::mutex> lock(shared->mutex);
            if (shared->recording) shared->timestamps.append(now);
        }, Qt::DirectConnection);
    }
    ~FrameRecorder() { QObject::disconnect(sync); QObject::disconnect(swap); }
    bool nextFrame() {
        const int marker = ++state->requested;
        window->update();
        return await([&] { return state->completed.load() >= marker; });
    }
    bool isThreaded() const { return state->threaded.load(); }
    void begin() {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->timestamps.clear(); state->phaseStart = state->clock.nsecsElapsed(); state->recording = true;
    }
    QJsonObject finish(double refreshRate) {
        QList<qint64> samples;
        qint64 start, end;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            end = state->clock.nsecsElapsed(); start = state->phaseStart;
            state->recording = false; samples = state->timestamps;
        }
        QList<double> intervals;
        int late = 0;
        const double budget = 1000. / refreshRate;
        qint64 previous = start;
        for (qsizetype i = 0; i < samples.size(); ++i) {
            const double ms = (samples[i] - previous) / 1e6;
            intervals.append(ms);
            if (ms > budget * 1.5) ++late;
            previous = samples[i];
        }
        const double tailMs = (end - previous) / 1e6;
        intervals.append(tailMs);
        if (tailMs > budget * 1.5) ++late;
        QJsonObject result = distribution(intervals);
        result["submitted_frames"] = samples.size();
        result["gaps_over_1_5_refresh_periods"] = late;
        const double seconds = samples.size() > 1 ? (samples.last() - samples.first()) / 1e9 : 0;
        result["inter_submission_fps"] = seconds > 0 ? (samples.size() - 1) / seconds : 0;
        result["phase_duration_ms"] = (end - start) / 1e6;
        result["phase_submission_fps"] = samples.size() * 1e9 / qMax(qint64(1), end - start);
        result["start_to_first_submission_ms"] = samples.isEmpty() ? (end - start) / 1e6 : (samples.first() - start) / 1e6;
        result["last_submission_to_phase_end_ms"] = tailMs;
        return result;
    }
private:
    struct State {
        QElapsedTimer clock;
        std::atomic<int> requested{0}, synchronized{0}, completed{0};
        std::atomic<bool> threaded{false};
        std::mutex mutex;
        bool recording = false;
        qint64 phaseStart = 0;
        QList<qint64> timestamps;
    };
    QQuickWindow *window;
    std::shared_ptr<State> state = std::make_shared<State>();
    QMetaObject::Connection sync, swap;
};

static QString graphicsApi(QSGRendererInterface::GraphicsApi api) {
    switch (api) {
    case QSGRendererInterface::Metal: return "Metal";
    case QSGRendererInterface::Vulkan: return "Vulkan";
    case QSGRendererInterface::OpenGL: return "OpenGL";
    case QSGRendererInterface::Direct3D11: return "Direct3D11";
    case QSGRendererInterface::Software: return "Software";
    default: return QString("API-%1").arg(int(api));
    }
}

class PerformanceTest : public QObject {
    Q_OBJECT
    QTemporaryDir settings;
    QJsonArray results;
    QJsonObject metadata;
    bool isolated = false;
private slots:
    void initTestCase() {
        QVERIFY(settings.isValid());
        QVERIFY2(!qEnvironmentVariableIsEmpty("OMAWRITE_PERF_OUTPUT"), "Run using bin/perf LABEL OUTPUT.json");
        QVERIFY2(QGuiApplication::platformName() != "offscreen" && QGuiApplication::platformName() != "minimal",
                 "FPS requires a native desktop platform, not offscreen/minimal");
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setApplicationName("omawrite-perf-" + QFileInfo(settings.path()).fileName());
        isolated = true;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
        QQuickStyle::setStyle("Material");
        qmlRegisterType<LivePreview>("Omawrite", 1, 0, "LivePreview");
        for (const char *style : {"Regular", "Italic", "Bold", "BoldItalic"}) {
            const QString path = QString("../../fonts/IBMPlexMono-%1.ttf").arg(style);
            const auto absolute = QTest::qFindTestData(path.toUtf8().constData(), __FILE__, __LINE__);
            QVERIFY(QFontDatabase::addApplicationFont(absolute) >= 0);
        }
        metadata = {{"schema", 1}, {"label", qEnvironmentVariable("OMAWRITE_PERF_LABEL")},
                    {"revision", qEnvironmentVariable("OMAWRITE_PERF_REVISION")},
                    {"application_sources_dirty", qEnvironmentVariable("OMAWRITE_PERF_DIRTY") == "true"},
                    {"benchmark_configuration_sha", qEnvironmentVariable("OMAWRITE_PERF_HARNESS_SHA")},
                    {"application_sources_sha", qEnvironmentVariable("OMAWRITE_PERF_SOURCES_SHA")},
                    {"syntax_highlighting_version", KSYNTAXHIGHLIGHTING_VERSION_STRING},
                    {"sonnet_version", SONNET_VERSION_STRING},
                    {"hardware_acceleration_verified", false},
                    {"hardware_verification", "API recorded; inspect QSG_INFO=1 output for adapter/driver; software drivers are not automatically detected"},
                    {"spelling_enabled", false},
                    {"background_work", "Word counting and recovery enabled; fixed warm-up is not a global quiescence guarantee"},
                    {"qt", qVersion()}, {"os", QSysInfo::prettyProductName()},
                    {"architecture", QSysInfo::currentCpuArchitecture()},
                    {"timestamp_utc", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
                    {"latency_metric", "GUI operation to synchronized follow-up frame submission; not scan-out or GPU execution time"}};
    }
    void baseline_data() {
        QTest::addColumn<QString>("fixture");
        for (const char *name : {"prose", "lists", "code", "images"}) QTest::newRow(name) << QString(name);
    }
    void baseline() {
        QFETCH(QString, fixture);
        QTemporaryDir assets; QVERIFY(assets.isValid());
        QImage image(1000, 600, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::darkCyan);
        const QString imagePath = assets.filePath("image.png"); QVERIFY(image.save(imagePath));
        QString text;
        if (fixture == "prose") {
            for (int i = 0; i < 400; ++i)
                text += QString("## Section %1\n\nA paragraph with **bold**, *emphasis* and a [link](https://example.com). More words for wrapping across the editor column.\n\n").arg(i);
        } else if (fixture == "lists") {
            for (int i = 0; i < 1000; ++i) text += QString("- Item %1 with **bold** and enough prose to wrap on narrow windows.\n").arg(i);
        } else if (fixture == "code") {
            text = "## Configuration\n\n```cpp\n";
            for (int i = 0; i < 1200; ++i) text += QString("int value_%1 = 42; // example comment\n").arg(i);
            text += "```\n\n";
        } else {
            for (int i = 0; i < 20; ++i) text += QString("## Image %1\n\n![Local image](%2)\n\n").arg(i).arg(imagePath);
        }
        text += "\nFinal paragraph for typing.\n";
        Backend backend(nullptr, false);
        backend.setEditorFont("IBM Plex Mono");
        backend.setDarkMode(true);
        qobject_cast<SpellingController *>(backend.spelling())->setEnabled(false);
        QQmlEngine engine;
        engine.rootContext()->setContextProperty("backend", &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(QFINDTESTDATA("../../src/Main.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> root(component.create());
        auto *window = qobject_cast<QQuickWindow *>(root.data()); QVERIFY(window);
        window->resize(1000, 760);
        auto *source = root->findChild<QObject *>("sourceEditor");
        auto *view = root->findChild<QQuickItem *>("previewEditor");
        auto *scroll = root->findChild<QQuickItem *>("editorScroll");
        auto *projection = root->findChild<LivePreview *>();
        QVERIFY(source && view && scroll && projection);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        FrameRecorder frames(window);
        QVERIFY(frames.nextFrame());
        const auto api = window->rendererInterface()->graphicsApi();
        QVERIFY2(api != QSGRendererInterface::Software && api != QSGRendererInterface::Unknown && api != QSGRendererInterface::Null,
                 "A non-Software graphics API is required (software drivers still need manual identification)");
        QVERIFY2(frames.isThreaded(), "A threaded render loop is required");
        metadata["graphics_api"] = graphicsApi(api);
        metadata["screen_refresh_hz"] = window->screen()->refreshRate();
        metadata["screen_name"] = window->screen()->name();
        metadata["device_pixel_ratio"] = window->devicePixelRatio();
        metadata["window_width"] = 1000; metadata["window_height"] = 760;
        metadata["font"] = backend.editorFont();
        QSignalSpy changed(projection, &LivePreview::projectionChanged);
        QElapsedTimer timer; timer.start();
        source->setProperty("text", text);
        source->setProperty("cursorPosition", text.size());
        QVERIFY(await([&] { return !changed.isEmpty(); }));
        QVERIFY(frames.nextFrame());
        const double loadMs = timer.nsecsElapsed() / 1e6;
        // Warm lazy layout and font resources before measured interactions.
        QTest::qWait(250);
        QVERIFY(QMetaObject::invokeMethod(view, "forceActiveFocus"));
        QList<double> typing, caret, modes, resize;
        for (int i = 0; i < 12; ++i) {
            const int before = changed.count(); timer.restart();
            QTest::keyClick(window, Qt::Key_X);
            QVERIFY(await([&] { return changed.count() > before; }));
            QVERIFY(frames.nextFrame());
            typing.append(timer.nsecsElapsed() / 1e6);
        }
        QCOMPARE(source->property("text").toString(), text + QString(12, 'x'));
        for (int i = 0; i < 24; ++i) {
            timer.restart();
            source->setProperty("cursorPosition", text.size() - 1 - i * 2);
            QCoreApplication::processEvents();
            QVERIFY(frames.nextFrame());
            caret.append(timer.nsecsElapsed() / 1e6);
        }
        for (int i = 0; i < 6; ++i) {
            const int before = changed.count(); timer.restart();
            root->setProperty("readMode", i % 2 == 0);
            QVERIFY(await([&] { return changed.count() > before; }));
            QVERIFY(frames.nextFrame());
            modes.append(timer.nsecsElapsed() / 1e6);
        }
        for (int i = 0; i < 6; ++i) {
            timer.restart(); window->resize(i % 2 ? 1000 : 820, 760);
            QCoreApplication::processEvents();
            QVERIFY(frames.nextFrame());
            resize.append(timer.nsecsElapsed() / 1e6);
        }
        QJsonArray scrolling;
        const double refreshRate = window->screen()->refreshRate();
        QVERIFY(refreshRate > 0);
        for (bool read : {false, true}) {
            root->setProperty("readMode", read);
            QCoreApplication::processEvents(); QVERIFY(frames.nextFrame());
            scroll->setProperty("contentY", 0);
            // Warm-up only: background word-count/recovery work remains enabled.
            QTest::qWait(750);
            QVERIFY(frames.nextFrame());
            const qreal extent = scroll->property("contentHeight").toReal() - scroll->height();
            QVERIFY(extent > 100);
            QVariantAnimation animation;
            animation.setStartValue(0.); animation.setEndValue(extent); animation.setDuration(1500);
            connect(&animation, &QVariantAnimation::valueChanged, scroll, [scroll](const QVariant &v) { scroll->setProperty("contentY", v); });
            frames.begin(); animation.start();
            QVERIFY(await([&] { return animation.state() == QAbstractAnimation::Stopped; }));
            QVERIFY(frames.nextFrame()); // Include final animation work and its synchronized submission.
            auto sample = frames.finish(refreshRate);
            QVERIFY2(sample["submitted_frames"].toInt() >= 10, "Not enough active frames for a meaningful scrolling sample");
            sample["mode"] = read ? "Read" : "Edit";
            sample["animation_ms"] = 1500;
            scrolling.append(sample);
        }
        QJsonObject result{{"fixture", fixture}, {"source_characters", text.size()},
                           {"initial_projection_to_frame_ms", loadMs},
                           {"typing_to_followup_frame", distribution(typing)},
                           {"caret_to_followup_frame", distribution(caret)},
                           {"mode_switch_to_followup_frame", distribution(modes)},
                           {"resize_to_followup_frame", distribution(resize)}, {"scrolling", scrolling}};
        results.append(result);
        qInfo().noquote() << QJsonDocument(result).toJson(QJsonDocument::Compact);
    }
    void cleanupTestCase() {
        metadata["results"] = results;
        metadata["complete"] = results.size() == 4;
        QFile output(qEnvironmentVariable("OMAWRITE_PERF_OUTPUT"));
        QVERIFY(output.open(QIODevice::WriteOnly | QIODevice::Truncate));
        const QByteArray json = QJsonDocument(metadata).toJson(QJsonDocument::Indented);
        QCOMPARE(output.write(json), qint64(json.size()));
        if (isolated) QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).removeRecursively();
    }
};
QTEST_MAIN(PerformanceTest)
#include "tst_performance.moc"
