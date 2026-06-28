#include "DataModel/QuickCommandManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

QuickCommandManager::QuickCommandManager(QObject *parent)
    : QObject(parent)
{
}

void QuickCommandManager::loadFromFile(const QString &filePath)
{
    QString path = filePath;
    if (path.isEmpty())
        path = defaultConfigPath();
    m_configPath = path;

    QFile file(path);
    if (!file.exists()) {
        QDir dir = QFileInfo(path).absoluteDir();
        if (!dir.exists())
            dir.mkpath(QStringLiteral("."));

        QJsonObject root;
        root["maxHistory"] = 20;
        root["maxDialog"] = 20;

        root["quickCommands"] = QJsonArray();

        QFile outFile(path);
        if (outFile.open(QIODevice::WriteOnly | QIODevice::Text))
            outFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit errorOccurred(QStringLiteral("Cannot open quick command config: %1").arg(path));
        return;
    }

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        emit errorOccurred(QStringLiteral("Quick command config parse error: %1").arg(error.errorString()));
        return;
    }

    QJsonObject root = doc.object();
    QJsonArray arr;

    if (doc.isArray()) {
        arr = doc.array();
    } else {
        arr = root.value(QStringLiteral("quickCommands")).toArray();
        m_maxHistory = root.value(QStringLiteral("maxHistory")).toInt(20);
        m_maxDialog = root.value(QStringLiteral("maxDialog")).toInt(20);
    }

    m_commands.clear();
    for (const QJsonValue &val : arr) {
        if (!val.isObject())
            continue;
        const QJsonObject obj = val.toObject();

        QVariantMap cmd;
        cmd["name"] = obj.value(QStringLiteral("name")).toString(QStringLiteral("Unnamed"));
        cmd["src"] = obj.value(QStringLiteral("src")).toString(QStringLiteral("0x0001"));
        cmd["dst"] = obj.value(QStringLiteral("dst")).toString(QStringLiteral("0x05"));
        cmd["seq"] = obj.value(QStringLiteral("seq")).toString(QStringLiteral("0001"));
        cmd["cmdset"] = obj.value(QStringLiteral("cmdset")).toString(QStringLiteral("0x00"));
        cmd["cmdid"] = obj.value(QStringLiteral("cmdid")).toString(QStringLiteral("0x00"));
        cmd["data"] = obj.value(QStringLiteral("data")).toString(QString());
        m_commands.append(cmd);
    }

    emit commandsChanged();
    emit configChanged();
}

bool QuickCommandManager::addCommand(const QString &name,
                                     const QString &src,
                                     const QString &dst,
                                     const QString &seq,
                                     const QString &cmdset,
                                     const QString &cmdid,
                                     const QString &data)
{
    const QString trimmedName = name.trimmed().isEmpty()
            ? QStringLiteral("快捷指令 %1").arg(m_commands.size() + 1)
            : name.trimmed();

    m_commands.prepend(makeCommand(trimmedName, src, dst, seq, cmdset, cmdid, data));
    if (!saveToFile()) {
        m_commands.removeFirst();
        return false;
    }

    emit commandsChanged();
    return true;
}

bool QuickCommandManager::updateCommand(int index,
                                        const QString &name,
                                        const QString &src,
                                        const QString &dst,
                                        const QString &seq,
                                        const QString &cmdset,
                                        const QString &cmdid,
                                        const QString &data)
{
    if (index < 0 || index >= m_commands.size())
        return false;

    const QVariant oldCommand = m_commands.at(index);
    m_commands[index] = makeCommand(name, src, dst, seq, cmdset, cmdid, data);
    if (!saveToFile()) {
        m_commands[index] = oldCommand;
        return false;
    }

    emit commandsChanged();
    return true;
}

bool QuickCommandManager::removeCommand(int index)
{
    if (index < 0 || index >= m_commands.size())
        return false;

    const QVariant removed = m_commands.takeAt(index);
    if (!saveToFile()) {
        m_commands.insert(index, removed);
        return false;
    }

    emit commandsChanged();
    return true;
}

void QuickCommandManager::clearCommands()
{
    if (m_commands.isEmpty())
        return;

    const QVariantList oldCommands = m_commands;
    m_commands.clear();
    if (!saveToFile()) {
        m_commands = oldCommands;
        return;
    }

    emit commandsChanged();
}

QString QuickCommandManager::defaultConfigPath() const
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString configPath = appDir + QStringLiteral("/config/quick_commands.json");

    if (appDir.endsWith(QStringLiteral(".app/Contents/MacOS"))) {
        const QString bundleResources = appDir + QStringLiteral("/../Resources/config/quick_commands.json");
        if (QFile::exists(bundleResources))
            return QDir(bundleResources).absolutePath();
    }

    return QDir(configPath).absolutePath();
}

bool QuickCommandManager::saveToFile(const QString &filePath)
{
    QString path = filePath;
    if (path.isEmpty())
        path = m_configPath.isEmpty() ? defaultConfigPath() : m_configPath;

    QDir dir = QFileInfo(path).absoluteDir();
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        emit errorOccurred(QStringLiteral("Cannot create quick command config dir: %1").arg(dir.absolutePath()));
        return false;
    }

    QJsonArray commands;
    for (const QVariant &item : m_commands) {
        const QVariantMap map = item.toMap();
        QJsonObject obj;
        obj["name"] = map.value(QStringLiteral("name")).toString();
        obj["src"] = map.value(QStringLiteral("src")).toString();
        obj["dst"] = map.value(QStringLiteral("dst")).toString();
        obj["seq"] = map.value(QStringLiteral("seq")).toString();
        obj["cmdset"] = map.value(QStringLiteral("cmdset")).toString();
        obj["cmdid"] = map.value(QStringLiteral("cmdid")).toString();
        obj["data"] = map.value(QStringLiteral("data")).toString();
        commands.append(obj);
    }

    QJsonObject root;
    root["maxHistory"] = m_maxHistory;
    root["maxDialog"] = m_maxDialog;
    root["quickCommands"] = commands;

    QFile outFile(path);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit errorOccurred(QStringLiteral("Cannot save quick command config: %1").arg(path));
        return false;
    }

    outFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

QVariantMap QuickCommandManager::makeCommand(const QString &name,
                                             const QString &src,
                                             const QString &dst,
                                             const QString &seq,
                                             const QString &cmdset,
                                             const QString &cmdid,
                                             const QString &data) const
{
    QVariantMap cmd;
    cmd["name"] = name.trimmed().isEmpty() ? QStringLiteral("未命名") : name.trimmed();
    cmd["src"] = src.trimmed().isEmpty() ? QStringLiteral("0x0001") : src.trimmed();
    cmd["dst"] = dst.trimmed().isEmpty() ? QStringLiteral("0x05") : dst.trimmed();
    cmd["seq"] = seq.trimmed().isEmpty() ? QStringLiteral("0001") : seq.trimmed();
    cmd["cmdset"] = cmdset.trimmed().isEmpty() ? QStringLiteral("0x00") : cmdset.trimmed();
    cmd["cmdid"] = cmdid.trimmed().isEmpty() ? QStringLiteral("0x00") : cmdid.trimmed();
    cmd["data"] = data.trimmed().toUpper();
    return cmd;
}
