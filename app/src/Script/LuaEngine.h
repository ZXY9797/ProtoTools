#ifndef LUAENGINE_H
#define LUAENGINE_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QVariantMap>
#include <QMutex>
#include <QTimer>
#include <QFileDialog>
#include <QDir>
#include <QVector>
#include <QVariantList>

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

/**
 * @brief Lua 脚本引擎
 *
 * 提供 C++ ↔ Lua 双向调用:
 * - C++ 调用 Lua: on_data_recv(), on_data_send()
 * - Lua 调用 C++: print(), send_data(), delay(), set_timer(), get_timestamp()
 */
class LuaEngine : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(QString scriptFile READ scriptFile WRITE setScriptFile NOTIFY scriptFileChanged)

public:
    explicit LuaEngine(QObject *parent = nullptr);
    ~LuaEngine() override;

    bool isRunning() const { return m_running; }
    QString scriptFile() const { return m_scriptFile; }
    void setScriptFile(const QString &path);

    // 加载并执行脚本
    Q_INVOKABLE bool loadScript(const QString &code);
    Q_INVOKABLE bool loadFile(const QString &filePath);
    Q_INVOKABLE bool saveFile(const QString &filePath, const QString &code);
    Q_INVOKABLE QString readScript() const { return m_lastScript; }

    // 调用 Lua 回调
    // on_frame_recv 返回 true 表示该帧需要显示，false 表示过滤掉
    QVariantMap processFrameRecv(const QVariantMap &frame);
    bool callOnFrameRecv(const QVariantMap &frame);
    void callOnFrameSend(const QVariantMap &frame);

    // Workspace 控件回调
    Q_INVOKABLE void addPlotData(const QString &channelName, double value);
    Q_INVOKABLE void callOnSlideChange(int index, double value);
    Q_INVOKABLE void callOnSwitchChange(int index, bool value);
    Q_INVOKABLE void callOnModeChange(int index, int mode);

    // 控制
    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();

    // 文件对话框
    Q_INVOKABLE QString openLuaFileDialog();
    Q_INVOKABLE QString saveLuaFileDialog(const QString &defaultName);
    Q_INVOKABLE QString readTextFile(const QString &filePath);
    Q_INVOKABLE bool writeTextFile(const QString &filePath, const QString &content);
    Q_INVOKABLE QString getLastDir();
    Q_INVOKABLE QString getAppConfigDir();

signals:
    void runningChanged();
    void scriptFileChanged();
    void consoleOutput(const QString &message);
    void sendDataRequested(const QByteArray &data);
    void errorOccurred(const QString &message);
    void plotDataReceived(const QString &channelName, double value);

private:
    // Lua 状态机
    lua_State *L = nullptr;
    bool m_running = false;
    QString m_scriptFile;
    QString m_lastScript;
    QMutex m_mutex;

    // 注册 C++ 函数到 Lua
    void registerApi();

    // Lua → C++ 回调函数（静态，通过 lua_upvalueindex 访问 this）
    static int lua_print(lua_State *L);
    static int lua_send(lua_State *L);
    static int lua_delay(lua_State *L);
    static int lua_set_timer(lua_State *L);
    static int lua_timestamp(lua_State *L);
    static int lua_hex_to_bytes(lua_State *L);
    static int lua_bytes_to_hex(lua_State *L);
    static int lua_read_data(lua_State *L);
    static int lua_log(lua_State *L);

    // Lua 定时器追踪
    struct LuaTimerInfo {
        QTimer *timer;
        int callbackRef;
    };
    QVector<LuaTimerInfo> m_luaTimers;
    void cleanupTimers();

    // 获取 LuaEngine 实例
    static LuaEngine *getEngine(lua_State *L);

    // 将 QVariantMap 推入 Lua table
    static void pushFrameToLua(lua_State *L, const QVariantMap &frame);
    static QVariant luaValueToVariant(lua_State *L, int index, int depth = 0);
    static QVariantMap luaTableToVariantMap(lua_State *L, int index, int depth = 0);
    static QVariantList luaTableToVariantList(lua_State *L, int index, int depth = 0);
    static bool luaTableIsArray(lua_State *L, int index);
    void applyFrameResult(QVariantMap &frame, const QVariant &result);
    void applyCurveValues(QVariantMap &frame, const QVariant &value);
    void sendReplyValue(const QVariant &value, bool hexOnly = false);
};

#endif // LUAENGINE_H
