#include "DataModel/SettingsManager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDebug>

static SettingsManager *s_instance = nullptr;

SettingsManager *SettingsManager::instance()
{
    return s_instance;
}

SettingsManager::SettingsManager(QObject *parent)
    : QObject(parent)
    , m_settings(configFilePath(), QSettings::IniFormat)
{
    qDebug() << "Settings file:" << m_settings.fileName();
    s_instance = this;
    migrateLegacySettings();
}

SettingsManager::~SettingsManager()
{
    m_settings.sync();
}

QString SettingsManager::configFilePath() const
{
    // macOS: ~/Library/Application Support/ProtoDebug/settings.ini
    // 不再写入 app bundle 内部（会破坏 codesign）
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir configDir(dataDir);
    if (!configDir.exists()) {
        configDir.mkpath(".");
    }
    return configDir.absoluteFilePath("settings.ini");
}

void SettingsManager::migrateLegacySettings()
{
    if (m_settings.value(QStringLiteral("migration.legacyProtoDebugImported"), false).toBool())
        return;

    QDir dataRoot(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    dataRoot.cdUp();
    dataRoot.cdUp();
    const QString legacyPath = dataRoot.filePath(QStringLiteral("ProtoDebug/ProtoDebug/settings.ini"));
    if (!QFileInfo::exists(legacyPath))
        return;

    QSettings legacy(legacyPath, QSettings::IniFormat);
    const QStringList keys = legacy.allKeys();
    for (const QString &key : keys) {
        if (!m_settings.contains(key))
            m_settings.setValue(key, legacy.value(key));
    }

    m_settings.setValue(QStringLiteral("migration.legacyProtoDebugImported"), true);
    m_settings.sync();
}

void SettingsManager::setValues(const QVariantMap &values)
{
    m_values = values;
    emit valuesChanged();
}

void SettingsManager::loadAll()
{
    m_values.clear();
    const QStringList keys = m_settings.allKeys();
    for (const QString &key : keys) {
        m_values[key] = m_settings.value(key);
    }
    emit valuesChanged();
}

void SettingsManager::saveAll()
{
    for (auto it = m_values.constBegin(); it != m_values.constEnd(); ++it) {
        m_settings.setValue(it.key(), it.value());
    }
    m_settings.sync();
}

QVariant SettingsManager::loadValue(const QString &key, const QVariant &defaultValue)
{
    QVariant v = m_settings.value(key, defaultValue);
    // QSettings IniFormat stores booleans as strings "true"/"false".
    // Convert back to actual bool so QML receives proper boolean values.
    if (v.typeId() == QMetaType::QString) {
        const QString s = v.toString().toLower();
        if (s == QLatin1String("true"))  return QVariant(true);
        if (s == QLatin1String("false")) return QVariant(false);
    }
    return v;
}

void SettingsManager::saveValue(const QString &key, const QVariant &value)
{
    m_values[key] = value;
    m_settings.setValue(key, value);
    m_settings.sync();
}

void SettingsManager::seedDefault(const QString &key, const QVariant &defaultValue)
{
    if (!m_settings.contains(key)) {
        m_values[key] = defaultValue;
        m_settings.setValue(key, defaultValue);
    }
}
