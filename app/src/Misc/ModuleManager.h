#pragma once

#include <QObject>
#include <QQmlApplicationEngine>

namespace Misc {

/**
 * @brief 模块生命周期管理 — 移植自 SerialStudio ModuleManager
 *
 * 管理 QML 类型注册、引擎初始化、各模块上下文属性注入。
 */
class ModuleManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQmlApplicationEngine *engine READ engine CONSTANT)

public:
    explicit ModuleManager();

    QQmlApplicationEngine *engine() const;
    void registerQmlTypes();
    void initializeQmlInterface(QQmlApplicationEngine *engine);

private:
    QQmlApplicationEngine *m_engine = nullptr;
};

} // namespace Misc
