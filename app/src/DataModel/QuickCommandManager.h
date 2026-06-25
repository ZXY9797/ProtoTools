#ifndef QUICKCOMMANDMANAGER_H
#define QUICKCOMMANDMANAGER_H

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

/**
 * @brief 快捷指令管理器 - 管理快捷指令并持久化到配置文件
 *
 * 配置文件格式 (quick_commands.json):
 * {
 *   "maxHistory": 20,
 *   "maxDialog": 20,
 *   "quickCommands": [
 *     {
 *       "name": "查询版本号",
 *     "src": "0x0001",
 *     "dst": "0x0002",
 *     "seq": "0001",
 *     "cmdset": "0x01",
 *     "cmdid": "0x01",
 *     "data": "00"
 *   },
 *   ...
 * ]
 */
class QuickCommandManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList commands READ commands NOTIFY commandsChanged)
    Q_PROPERTY(int count READ count NOTIFY commandsChanged)
    Q_PROPERTY(int maxHistory READ maxHistory NOTIFY configChanged)
    Q_PROPERTY(int maxDialog READ maxDialog NOTIFY configChanged)

public:
    explicit QuickCommandManager(QObject *parent = nullptr);

    QVariantList commands() const { return m_commands; }
    int count() const { return m_commands.size(); }
    int maxHistory() const { return m_maxHistory; }
    int maxDialog() const { return m_maxDialog; }

    // 加载配置文件
    Q_INVOKABLE void loadFromFile(const QString &filePath);
    Q_INVOKABLE bool addCommand(const QString &name,
                                const QString &src,
                                const QString &dst,
                                const QString &seq,
                                const QString &cmdset,
                                const QString &cmdid,
                                const QString &data);
    Q_INVOKABLE bool updateCommand(int index,
                                   const QString &name,
                                   const QString &src,
                                   const QString &dst,
                                   const QString &seq,
                                   const QString &cmdset,
                                   const QString &cmdid,
                                   const QString &data);
    Q_INVOKABLE bool removeCommand(int index);
    Q_INVOKABLE void clearCommands();

    // 获取默认配置文件路径
    Q_INVOKABLE QString defaultConfigPath() const;

signals:
    void commandsChanged();
    void configChanged();
    void errorOccurred(const QString &message);

private:
    bool saveToFile(const QString &filePath = QString());
    QVariantMap makeCommand(const QString &name,
                            const QString &src,
                            const QString &dst,
                            const QString &seq,
                            const QString &cmdset,
                            const QString &cmdid,
                            const QString &data) const;

    QVariantList m_commands;
    int m_maxHistory = 20;
    int m_maxDialog = 20;
    QString m_configPath;
};

#endif // QUICKCOMMANDMANAGER_H
