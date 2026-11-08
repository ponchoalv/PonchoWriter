#include "cli.h"

#include <QTextStream>

QString Cli::usage() {
    return QStringLiteral(
        "Usage: omawrite [FILE ...]\n"
        "\n"
        "Opens Markdown files; already-open files are focused, others get new windows.\n"
        "Without files, starts an untitled window or activates the running app.\n"
        "\n"
        "Options:\n"
        "  -h, --help     Show this message and exit\n"
        "  -v, --version  Show the version and exit\n");
}

std::optional<int> Cli::handleArguments(const QStringList &arguments, const QString &version) {
    if (arguments.size() < 2)
        return std::nullopt;

    const QString &first = arguments.at(1);
    if (first == QLatin1String("-h") || first == QLatin1String("--help")) {
        QTextStream(stdout) << usage();
        return 0;
    }
    if (first == QLatin1String("-v") || first == QLatin1String("--version")) {
        QTextStream(stdout) << QStringLiteral("omawrite %1\n").arg(version);
        return 0;
    }

    return std::nullopt;
}

std::optional<int> Cli::handleArguments(int argc, char *argv[], const QString &version) {
    QStringList arguments;
    arguments.reserve(argc);
    for (int index = 0; index < argc; ++index)
        arguments.append(QString::fromLocal8Bit(argv[index]));

    return handleArguments(arguments, version);
}
