#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QObject>
#include <QSettings>
#include <QVariantMap>

/**
 * @brief 设置管理器 - 应用关闭时一次性保存参数
 *
 * 用法:
 *   QML: settingsManager.saveValue("key", value)
 *        settingsManager.loadValue("key", defaultValue)
 *   C++: app.aboutToQuit → settingsManager.saveAll()
 */
class SettingsManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap values READ values WRITE setValues NOTIFY valuesChanged)

public:
    explicit SettingsManager(QObject *parent = nullptr);
    ~SettingsManager() override;

    static SettingsManager *instance();

    QVariantMap values() const { return m_values; }
    void setValues(const QVariantMap &values);

    // 从 QSettings 加载所有值
    Q_INVOKABLE void loadAll();

    // 将当前 values 保存到 QSettings
    Q_INVOKABLE void saveAll();

    // 便捷方法
    Q_INVOKABLE QVariant loadValue(const QString &key, const QVariant &defaultValue = {});
    Q_INVOKABLE void saveValue(const QString &key, const QVariant &value);
    // 如果 key 不存在则写入默认值（用于初始化 seed）
    Q_INVOKABLE void seedDefault(const QString &key, const QVariant &defaultValue);

signals:
    void valuesChanged();

private:
    QString configFilePath() const;
    void migrateLegacySettings();

    QSettings m_settings;
    QVariantMap m_values;
};

#endif // SETTINGSMANAGER_H
