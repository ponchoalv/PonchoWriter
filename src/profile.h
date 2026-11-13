#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QVariantMap>

// Legacy identifiers are compatibility interfaces, never product branding.
// Reuse the recovery store and IPC locks: never copy live drafts or allow two
// independently branded processes to autosave the same document.
namespace Profile {
inline QString previousData, previousRuntime, activeData;
inline QVariantMap previousSettings;
inline bool configured = false;
inline QString ipcDirectoryName() { return QStringLiteral("omawrite-instance"); }
inline QString pipePrefix() { return QStringLiteral("omawrite-"); }
inline void configureIdentity() {
    QCoreApplication::setApplicationName(QStringLiteral("omawrite"));
    QCoreApplication::setOrganizationName(QStringLiteral("Omacom"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("omacom.io"));
    previousRuntime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    QCoreApplication::setApplicationName(QStringLiteral("ponchowriter"));
    QCoreApplication::setOrganizationName(QStringLiteral("ponchoalv"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("ponchoalv.github.io"));
    previousSettings.clear();
    {
        QSettings legacy;
        for (const auto &key : legacy.allKeys()) previousSettings.insert(key, legacy.value(key));
        previousData = legacy.value(QStringLiteral("profileStorageRoot"),
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).toString();
        if (previousData.isEmpty())
            previousData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
    QCoreApplication::setApplicationName(QStringLiteral("omawrite"));
    QCoreApplication::setOrganizationName(QStringLiteral("Omacom"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("omacom.io"));
    configured = true;
}
// Idempotent and non-destructive: destination preferences win. Only settings
// are imported. Recovery JSON, images and slot locks stay together in place.
inline bool adopt(QSettings &settings, const QVariantMap &legacy,
                  const QString &oldData, const QString &freshData) {
    if (!settings.contains(QStringLiteral("profileMigrationVersion"))) {
        for (auto it = legacy.cbegin(); it != legacy.cend(); ++it)
            if (!settings.contains(it.key())) settings.setValue(it.key(), it.value());
        if (settings.value(QStringLiteral("profileStorageRoot")).toString().isEmpty())
            settings.setValue(QStringLiteral("profileStorageRoot"), !oldData.isEmpty() && QDir(oldData).exists() ? oldData : freshData);
        settings.setValue(QStringLiteral("profileMigrationVersion"), 1);
        settings.sync();
    }
    return settings.status() == QSettings::NoError;
}
// A one-time return from the fork identity must retain the *active* fork
// profile, even when older Omawrite settings already contain a migration flag.
inline bool adoptWrite(QSettings &settings, const QVariantMap &legacy,
                       const QString &oldData, const QString &freshData) {
    if (!adopt(settings, legacy, oldData, freshData)) return false;
    if (!settings.contains(QStringLiteral("writeIdentityMigrationVersion"))) {
        if (!legacy.isEmpty()) {
            for (auto it = legacy.cbegin(); it != legacy.cend(); ++it)
                settings.setValue(it.key(), it.value());
            if (!oldData.isEmpty()) settings.setValue(QStringLiteral("profileStorageRoot"), oldData);
        }
        settings.setValue(QStringLiteral("writeIdentityMigrationVersion"), 1);
        settings.sync();
    }
    return settings.status() == QSettings::NoError;
}
inline bool activate() {
    QSettings settings;
    if (!adoptWrite(settings, previousSettings, previousData,
                    QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))) return false;
    activeData = settings.value(QStringLiteral("profileStorageRoot")).toString();
    previousSettings.clear();
    return !activeData.isEmpty();
}
inline QString dataLocation() {
    return activeData.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) : activeData;
}
inline QString runtimeLocation() {
    return configured ? previousRuntime : QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
}
}
