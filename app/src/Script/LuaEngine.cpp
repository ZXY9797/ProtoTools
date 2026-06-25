#include "Script/LuaEngine.h"
#include <QDateTime>
#include <QTimer>
#include <QStandardPaths>
#include <QThread>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QSettings>
#include <QFileInfo>
#include <QRegularExpression>
#include <cmath>
#include <cstring>

namespace {

constexpr int MaxLuaCurveCount = 9;

QString compactHexText(QString text)
{
    text.remove(QRegularExpression(QStringLiteral("0[xX]")));
    text.remove(QRegularExpression(QStringLiteral("[^0-9A-Fa-f]")));
    return text;
}

bool looksLikeHexText(const QString &text)
{
    const QString compact = compactHexText(text);
    if (compact.size() < 2 || (compact.size() % 2) != 0)
        return false;

    return compact.size() == text.trimmed().size()
        || text.contains(' ')
        || text.contains(',')
        || text.contains(QStringLiteral("0x"), Qt::CaseInsensitive);
}

QByteArray bytesFromHexText(const QString &text)
{
    return QByteArray::fromHex(compactHexText(text).toLatin1());
}

bool variantListIsBytes(const QVariantList &list)
{
    if (list.isEmpty())
        return false;

    for (const QVariant &item : list) {
        bool ok = false;
        const int value = item.toInt(&ok);
        if (!ok || value < 0 || value > 0xff)
            return false;
    }
    return true;
}

QByteArray bytesFromVariant(const QVariant &value, bool hexOnly = false)
{
    if (!value.isValid() || value.isNull())
        return {};

    if (value.canConvert<QVariantMap>()) {
        const QVariantMap map = value.toMap();
        if (map.contains(QStringLiteral("hex")))
            return bytesFromHexText(map.value(QStringLiteral("hex")).toString());
        if (map.contains(QStringLiteral("data")))
            return bytesFromVariant(map.value(QStringLiteral("data")), true);
        if (map.contains(QStringLiteral("bytes")))
            return bytesFromVariant(map.value(QStringLiteral("bytes")), false);
    }

    if (value.canConvert<QVariantList>()) {
        const QVariantList list = value.toList();
        if (variantListIsBytes(list)) {
            QByteArray bytes;
            bytes.reserve(list.size());
            for (const QVariant &item : list)
                bytes.append(static_cast<char>(item.toInt() & 0xff));
            return bytes;
        }
    }

    const QString text = value.toString();
    if (hexOnly || looksLikeHexText(text))
        return bytesFromHexText(text);
    return text.toUtf8();
}

QByteArray bytesFromLuaValue(lua_State *L, int index, bool hexOnly = false)
{
    index = lua_absindex(L, index);

    if (lua_istable(L, index)) {
        lua_getfield(L, index, "data");
        if (!lua_isnil(L, -1)) {
            const QByteArray bytes = bytesFromLuaValue(L, -1, true);
            lua_pop(L, 1);
            return bytes;
        }
        lua_pop(L, 1);

        lua_getfield(L, index, "hex");
        if (!lua_isnil(L, -1)) {
            const QByteArray bytes = bytesFromLuaValue(L, -1, true);
            lua_pop(L, 1);
            return bytes;
        }
        lua_pop(L, 1);

        QByteArray bytes;
        const int len = static_cast<int>(lua_rawlen(L, index));
        bytes.reserve(len);
        for (int i = 1; i <= len; ++i) {
            lua_rawgeti(L, index, i);
            bytes.append(static_cast<char>(lua_tointeger(L, -1) & 0xff));
            lua_pop(L, 1);
        }
        return bytes;
    }

    if (lua_isstring(L, index)) {
        size_t len = 0;
        const char *data = lua_tolstring(L, index, &len);
        const QByteArray raw(data, static_cast<int>(len));
        const QString text = QString::fromUtf8(raw);
        if (hexOnly || looksLikeHexText(text))
            return bytesFromHexText(text);
        return raw;
    }

    return {};
}

quint64 readUIntLe(const QByteArray &bytes, int offset, int byteCount)
{
    quint64 value = 0;
    for (int i = 0; i < byteCount; ++i)
        value |= quint64(static_cast<quint8>(bytes.at(offset + i))) << (8 * i);
    return value;
}

qint64 toSignedValue(quint64 value, int bits)
{
    const quint64 signBit = quint64(1) << (bits - 1);
    if ((value & signBit) == 0)
        return static_cast<qint64>(value);

    if (bits >= 64)
        return static_cast<qint64>(value);

    const quint64 mask = bits >= 64 ? 0 : (quint64(1) << bits);
    return static_cast<qint64>(value - mask);
}

QString normalizedDataType(QString type)
{
    type = type.trimmed().toLower();
    type.remove(' ');
    type.remove('_');
    type.remove('-');
    if (type == QStringLiteral("float32"))
        return QStringLiteral("float");
    if (type == QStringLiteral("float64"))
        return QStringLiteral("double");
    return type;
}

bool readTypedValue(const QByteArray &bytes, int offset, const QString &typeText, double *out)
{
    const QString type = normalizedDataType(typeText);
    int size = 1;
    if (type == QStringLiteral("uint16") || type == QStringLiteral("int16"))
        size = 2;
    else if (type == QStringLiteral("uint32") || type == QStringLiteral("int32") || type == QStringLiteral("float"))
        size = 4;
    else if (type == QStringLiteral("uint64") || type == QStringLiteral("int64") || type == QStringLiteral("double"))
        size = 8;

    if (offset < 0 || offset + size > bytes.size())
        return false;

    if (type == QStringLiteral("uint8")) {
        *out = static_cast<quint8>(bytes.at(offset));
        return true;
    }
    if (type == QStringLiteral("int8")) {
        const int value = static_cast<quint8>(bytes.at(offset));
        *out = value >= 128 ? value - 256 : value;
        return true;
    }
    if (type.startsWith(QStringLiteral("uint"))) {
        *out = static_cast<double>(readUIntLe(bytes, offset, size));
        return true;
    }
    if (type.startsWith(QStringLiteral("int"))) {
        *out = static_cast<double>(toSignedValue(readUIntLe(bytes, offset, size), size * 8));
        return true;
    }
    if (type == QStringLiteral("float")) {
        const quint32 bits = static_cast<quint32>(readUIntLe(bytes, offset, 4));
        float value = 0.0f;
        std::memcpy(&value, &bits, sizeof(value));
        *out = value;
        return true;
    }
    if (type == QStringLiteral("double")) {
        const quint64 bits = readUIntLe(bytes, offset, 8);
        double value = 0.0;
        std::memcpy(&value, &bits, sizeof(value));
        *out = value;
        return true;
    }

    return false;
}

bool reservedFrameResultKey(const QString &key)
{
    const QString lower = key.trimmed().toLower();
    return lower == QStringLiteral("show")
        || lower == QStringLiteral("visible")
        || lower == QStringLiteral("filter")
        || lower == QStringLiteral("frame")
        || lower == QStringLiteral("curve")
        || lower == QStringLiteral("curves")
        || lower == QStringLiteral("plot")
        || lower == QStringLiteral("plots")
        || lower == QStringLiteral("reply")
        || lower == QStringLiteral("replies")
        || lower == QStringLiteral("response")
        || lower == QStringLiteral("responses")
        || lower == QStringLiteral("replyhex")
        || lower == QStringLiteral("responsehex");
}

} // namespace

LuaEngine::LuaEngine(QObject *parent)
    : QObject(parent)
{
    L = luaL_newstate();
    if (L) {
        luaL_openlibs(L);
        registerApi();
        // 存储 this 指针供静态回调使用
        lua_pushlightuserdata(L, this);
        lua_setglobal(L, "__engine");
    }
}

LuaEngine::~LuaEngine()
{
    stop();
    if (L) {
        lua_close(L);
        L = nullptr;
    }
}

void LuaEngine::setScriptFile(const QString &path)
{
    if (m_scriptFile != path) {
        m_scriptFile = path;
        emit scriptFileChanged();
    }
}

bool LuaEngine::loadScript(const QString &code)
{
    QMutexLocker lock(&m_mutex);
    if (!L) return false;

    // 清理上一次脚本留下的定时器
    cleanupTimers();

    m_lastScript = code;

    int result = luaL_dostring(L, code.toUtf8().constData());
    if (result != LUA_OK) {
        QString err = lua_tostring(L, -1);
        lua_pop(L, 1);
        emit errorOccurred(err);
        emit consoleOutput(QStringLiteral("[ERROR] %1").arg(err));
        return false;
    }

    emit consoleOutput("[OK] 脚本加载成功");
    return true;
}

bool LuaEngine::loadFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit errorOccurred(QStringLiteral("无法打开文件: %1").arg(filePath));
        return false;
    }

    QString code = QTextStream(&file).readAll();
    file.close();
    setScriptFile(filePath);
    return loadScript(code);
}

bool LuaEngine::saveFile(const QString &filePath, const QString &code)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        emit errorOccurred(QStringLiteral("无法保存文件: %1").arg(filePath));
        return false;
    }
    QTextStream out(&file);
    out << code;
    file.close();
    setScriptFile(filePath);
    m_lastScript = code;
    emit consoleOutput(QStringLiteral("[OK] 脚本已保存: %1").arg(filePath));
    return true;
}

void LuaEngine::start()
{
    if (!m_running) {
        m_running = true;
        emit runningChanged();
        emit consoleOutput("[OK] Lua 引擎启动");
    }
}

void LuaEngine::stop()
{
    if (m_running) {
        m_running = false;
        cleanupTimers();
        emit runningChanged();
        emit consoleOutput("[OK] Lua 引擎停止");
    }
}

void LuaEngine::cleanupTimers()
{
    for (auto &info : m_luaTimers) {
        if (info.timer) {
            info.timer->stop();
            delete info.timer;
        }
        // 取消 Lua registry 引用
        if (L && info.callbackRef != LUA_NOREF && info.callbackRef != LUA_REFNIL) {
            luaL_unref(L, LUA_REGISTRYINDEX, info.callbackRef);
        }
    }
    m_luaTimers.clear();
}

QVariantMap LuaEngine::processFrameRecv(const QVariantMap &frame)
{
    QVariantMap processed = frame;
    processed.insert(QStringLiteral("__show"), true);

    QVariant scriptResult;
    {
        QMutexLocker lock(&m_mutex);
        if (!L || !m_running)
            return processed;

        lua_getglobal(L, "on_frame_recv");
        if (!lua_isfunction(L, -1)) {
            lua_pop(L, 1);
            return processed;
        }

        pushFrameToLua(L, frame);
        if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
            QString err = lua_tostring(L, -1);
            lua_pop(L, 1);
            emit consoleOutput(QStringLiteral("[ERROR] on_frame_recv: %1").arg(err));
            return processed;
        }

        scriptResult = luaValueToVariant(L, -1);
        lua_pop(L, 1);
    }

    applyFrameResult(processed, scriptResult);
    return processed;
}

bool LuaEngine::callOnFrameRecv(const QVariantMap &frame)
{
    return processFrameRecv(frame).value(QStringLiteral("__show"), true).toBool();
}

void LuaEngine::callOnFrameSend(const QVariantMap &frame)
{
    QMutexLocker lock(&m_mutex);
    if (!L || !m_running) return;

    lua_getglobal(L, "on_frame_send");
    if (lua_isfunction(L, -1)) {
        pushFrameToLua(L, frame);
        if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
            QString err = lua_tostring(L, -1);
            lua_pop(L, 1);
            emit consoleOutput(QStringLiteral("[ERROR] on_frame_send: %1").arg(err));
        }
    } else {
        lua_pop(L, 1);
    }
}

// ========== 注册 C++ API 到 Lua ==========

void LuaEngine::registerApi()
{
    // 注册全局函数
    lua_pushcfunction(L, lua_print);
    lua_setglobal(L, "print");

    lua_pushcfunction(L, lua_log);
    lua_setglobal(L, "log");

    lua_pushcfunction(L, lua_send);
    lua_setglobal(L, "send");

    lua_pushcfunction(L, lua_delay);
    lua_setglobal(L, "delay");

    lua_pushcfunction(L, lua_set_timer);
    lua_setglobal(L, "set_timer");

    lua_pushcfunction(L, lua_timestamp);
    lua_setglobal(L, "get_timestamp");

    lua_pushcfunction(L, lua_hex_to_bytes);
    lua_setglobal(L, "hex_to_bytes");

    lua_pushcfunction(L, lua_bytes_to_hex);
    lua_setglobal(L, "bytes_to_hex");

    lua_pushcfunction(L, lua_read_data);
    lua_setglobal(L, "read_data");
}

// ========== Lua 回调实现 ==========

LuaEngine *LuaEngine::getEngine(lua_State *L)
{
    lua_getglobal(L, "__engine");
    auto *engine = static_cast<LuaEngine *>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return engine;
}

void LuaEngine::pushFrameToLua(lua_State *L, const QVariantMap &frame)
{
    lua_newtable(L);
    for (auto it = frame.constBegin(); it != frame.constEnd(); ++it) {
        lua_pushstring(L, it.key().toUtf8().constData());
        lua_pushstring(L, it.value().toString().toUtf8().constData());
        lua_settable(L, -3);
    }
}

QVariant LuaEngine::luaValueToVariant(lua_State *L, int index, int depth)
{
    if (depth > 5)
        return {};

    index = lua_absindex(L, index);
    switch (lua_type(L, index)) {
    case LUA_TNIL:
        return {};
    case LUA_TBOOLEAN:
        return lua_toboolean(L, index) != 0;
    case LUA_TNUMBER:
        if (lua_isinteger(L, index))
            return QVariant::fromValue<qlonglong>(lua_tointeger(L, index));
        return lua_tonumber(L, index);
    case LUA_TSTRING: {
        size_t len = 0;
        const char *text = lua_tolstring(L, index, &len);
        return QString::fromUtf8(text, static_cast<int>(len));
    }
    case LUA_TTABLE:
        if (luaTableIsArray(L, index))
            return luaTableToVariantList(L, index, depth + 1);
        return luaTableToVariantMap(L, index, depth + 1);
    default:
        return {};
    }
}

QVariantMap LuaEngine::luaTableToVariantMap(lua_State *L, int index, int depth)
{
    QVariantMap map;
    index = lua_absindex(L, index);

    lua_pushnil(L);
    while (lua_next(L, index) != 0) {
        QString key;
        if (lua_type(L, -2) == LUA_TSTRING)
            key = QString::fromUtf8(lua_tostring(L, -2));
        else if (lua_isinteger(L, -2))
            key = QString::number(lua_tointeger(L, -2));

        if (!key.isEmpty())
            map.insert(key, luaValueToVariant(L, -1, depth + 1));
        lua_pop(L, 1);
    }

    return map;
}

QVariantList LuaEngine::luaTableToVariantList(lua_State *L, int index, int depth)
{
    QVariantList list;
    index = lua_absindex(L, index);
    const int len = static_cast<int>(lua_rawlen(L, index));
    list.reserve(len);

    for (int i = 1; i <= len; ++i) {
        lua_rawgeti(L, index, i);
        list.append(luaValueToVariant(L, -1, depth + 1));
        lua_pop(L, 1);
    }

    return list;
}

bool LuaEngine::luaTableIsArray(lua_State *L, int index)
{
    index = lua_absindex(L, index);
    const int len = static_cast<int>(lua_rawlen(L, index));
    int count = 0;
    bool arrayLike = true;

    lua_pushnil(L);
    while (lua_next(L, index) != 0) {
        ++count;
        if (!lua_isinteger(L, -2)) {
            arrayLike = false;
        } else {
            const int key = static_cast<int>(lua_tointeger(L, -2));
            if (key < 1 || key > len)
                arrayLike = false;
        }
        lua_pop(L, 1);
    }

    return arrayLike && count == len;
}

void LuaEngine::applyFrameResult(QVariantMap &frame, const QVariant &result)
{
    if (!result.isValid() || result.isNull()) {
        frame.insert(QStringLiteral("__show"), false);
        return;
    }

    if (result.metaType().id() == QMetaType::Bool) {
        frame.insert(QStringLiteral("__show"), result.toBool());
        return;
    }

    if (result.canConvert<QVariantList>() && !result.canConvert<QVariantMap>()) {
        frame.insert(QStringLiteral("__show"), true);
        applyCurveValues(frame, result);
        return;
    }

    if (!result.canConvert<QVariantMap>()) {
        frame.insert(QStringLiteral("__show"), result.toBool());
        return;
    }

    const QVariantMap map = result.toMap();
    bool show = true;
    if (map.contains(QStringLiteral("show")))
        show = map.value(QStringLiteral("show")).toBool();
    if (map.contains(QStringLiteral("visible")))
        show = map.value(QStringLiteral("visible")).toBool();
    if (map.contains(QStringLiteral("filter")))
        show = !map.value(QStringLiteral("filter")).toBool();
    frame.insert(QStringLiteral("__show"), show);

    if (map.contains(QStringLiteral("frame")) && map.value(QStringLiteral("frame")).canConvert<QVariantMap>()) {
        const QVariantMap nested = map.value(QStringLiteral("frame")).toMap();
        for (auto it = nested.cbegin(); it != nested.cend(); ++it)
            frame.insert(it.key(), it.value());
    }

    for (auto it = map.cbegin(); it != map.cend(); ++it) {
        if (!reservedFrameResultKey(it.key()))
            frame.insert(it.key(), it.value());
    }

    if (map.contains(QStringLiteral("color")) && !frame.contains(QStringLiteral("rowColor")))
        frame.insert(QStringLiteral("rowColor"), map.value(QStringLiteral("color")));
    if (map.contains(QStringLiteral("background")) && !frame.contains(QStringLiteral("rowColor")))
        frame.insert(QStringLiteral("rowColor"), map.value(QStringLiteral("background")));
    if (map.contains(QStringLiteral("bgColor")) && !frame.contains(QStringLiteral("rowColor")))
        frame.insert(QStringLiteral("rowColor"), map.value(QStringLiteral("bgColor")));

    applyCurveValues(frame, map.value(QStringLiteral("curve")));
    applyCurveValues(frame, map.value(QStringLiteral("curves")));
    applyCurveValues(frame, map.value(QStringLiteral("plot")));
    applyCurveValues(frame, map.value(QStringLiteral("plots")));

    if (map.contains(QStringLiteral("replyHex")))
        sendReplyValue(map.value(QStringLiteral("replyHex")), true);
    if (map.contains(QStringLiteral("responseHex")))
        sendReplyValue(map.value(QStringLiteral("responseHex")), true);
    if (map.contains(QStringLiteral("reply")))
        sendReplyValue(map.value(QStringLiteral("reply")));
    if (map.contains(QStringLiteral("response")))
        sendReplyValue(map.value(QStringLiteral("response")));
    if (map.contains(QStringLiteral("replies")))
        sendReplyValue(map.value(QStringLiteral("replies")));
    if (map.contains(QStringLiteral("responses")))
        sendReplyValue(map.value(QStringLiteral("responses")));
}

void LuaEngine::applyCurveValues(QVariantMap &frame, const QVariant &value)
{
    if (!value.isValid() || value.isNull())
        return;

    auto assign = [&frame](int slot, const QVariant &item) {
        if (slot < 1 || slot > MaxLuaCurveCount)
            return;

        QVariant numeric = item;
        if (item.canConvert<QVariantMap>()) {
            const QVariantMap map = item.toMap();
            if (map.contains(QStringLiteral("index")))
                slot = qBound(1, map.value(QStringLiteral("index")).toInt(), MaxLuaCurveCount);
            if (map.contains(QStringLiteral("value")))
                numeric = map.value(QStringLiteral("value"));
        }

        bool ok = false;
        const double number = numeric.toDouble(&ok);
        if (ok && std::isfinite(number))
            frame.insert(QStringLiteral("luaCurve%1").arg(slot), number);
    };

    if (value.canConvert<QVariantList>()) {
        const QVariantList list = value.toList();
        for (int i = 0; i < list.size() && i < MaxLuaCurveCount; ++i)
            assign(i + 1, list.at(i));
        return;
    }

    assign(1, value);
}

void LuaEngine::sendReplyValue(const QVariant &value, bool hexOnly)
{
    if (!value.isValid() || value.isNull())
        return;

    if (value.canConvert<QVariantList>()) {
        const QVariantList list = value.toList();
        if (!variantListIsBytes(list)) {
            for (const QVariant &item : list)
                sendReplyValue(item, hexOnly);
            return;
        }
    }

    const QByteArray bytes = bytesFromVariant(value, hexOnly);
    if (!bytes.isEmpty())
        emit sendDataRequested(bytes);
}

int LuaEngine::lua_print(lua_State *L)
{
    auto *engine = getEngine(L);
    if (!engine) return 0;

    int n = lua_gettop(L);
    QString msg;
    for (int i = 1; i <= n; i++) {
        if (i > 1) msg += "\t";
        msg += lua_tostring(L, i);
    }
    emit engine->consoleOutput(msg);
    return 0;
}

int LuaEngine::lua_log(lua_State *L)
{
    return lua_print(L);  // log 和 print 功能相同
}

int LuaEngine::lua_send(lua_State *L)
{
    auto *engine = getEngine(L);
    if (!engine) return 0;

    if (lua_isstring(L, 1)) {
        size_t len;
        const char *data = lua_tolstring(L, 1, &len);
        emit engine->sendDataRequested(QByteArray(data, len));
    } else if (lua_istable(L, 1)) {
        // 从 table 构造字节数组
        QByteArray bytes;
        int len = lua_rawlen(L, 1);
        for (int i = 1; i <= len; i++) {
            lua_rawgeti(L, 1, i);
            bytes.append(static_cast<char>(lua_tointeger(L, -1)));
            lua_pop(L, 1);
        }
        emit engine->sendDataRequested(bytes);
    }
    return 0;
}

int LuaEngine::lua_delay(lua_State *L)
{
    int ms = luaL_checkinteger(L, 1);
    QThread::msleep(ms);
    return 0;
}

int LuaEngine::lua_set_timer(lua_State *L)
{
    auto *engine = getEngine(L);
    if (!engine) return 0;

    int intervalMs = luaL_checkinteger(L, 1);
    int callbackRef = luaL_ref(L, LUA_REGISTRYINDEX);

    QTimer *timer = new QTimer(engine);
    timer->setInterval(intervalMs);
    QObject::connect(timer, &QTimer::timeout, engine, [L = engine->L, callbackRef]() {
        lua_rawgeti(L, LUA_REGISTRYINDEX, callbackRef);
        if (lua_isfunction(L, -1)) {
            lua_pcall(L, 0, 0, 0);
        } else {
            lua_pop(L, 1);
        }
    });

    // 追踪定时器，停止/重载时统一清理
    engine->m_luaTimers.append({timer, callbackRef});
    timer->start();

    // 返回 timer 对象的 userdata
    lua_pushlightuserdata(L, timer);
    return 1;
}

int LuaEngine::lua_timestamp(lua_State *L)
{
    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    lua_pushstring(L, ts.toUtf8().constData());
    return 1;
}

int LuaEngine::lua_hex_to_bytes(lua_State *L)
{
    const char *hex = luaL_checkstring(L, 1);
    QByteArray cleaned(hex);
    cleaned = cleaned.replace(" ", "");
    QByteArray bytes = QByteArray::fromHex(cleaned);

    lua_newtable(L);
    for (int i = 0; i < bytes.size(); i++) {
        lua_pushinteger(L, static_cast<quint8>(bytes[i]));
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

int LuaEngine::lua_bytes_to_hex(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    QByteArray bytes;
    int len = lua_rawlen(L, 1);
    for (int i = 1; i <= len; i++) {
        lua_rawgeti(L, 1, i);
        bytes.append(static_cast<char>(lua_tointeger(L, -1)));
        lua_pop(L, 1);
    }
    lua_pushstring(L, bytes.toHex(' ').toUpper().constData());
    return 1;
}

int LuaEngine::lua_read_data(lua_State *L)
{
    const QByteArray bytes = bytesFromLuaValue(L, 1, true);
    const int offset = luaL_checkinteger(L, 2);
    const QString type = QString::fromUtf8(luaL_optstring(L, 3, "uint8"));

    double value = 0.0;
    if (!readTypedValue(bytes, offset, type, &value)) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushnumber(L, value);
    return 1;
}

void LuaEngine::addPlotData(const QString &channelName, double value)
{
    emit plotDataReceived(channelName, value);
}

void LuaEngine::callOnSlideChange(int index, double value)
{
    QMutexLocker lock(&m_mutex);
    if (!L || !m_running) return;

    lua_getglobal(L, "on_slide_change");
    if (lua_isfunction(L, -1)) {
        lua_pushinteger(L, index);
        lua_pushnumber(L, value);
        if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
            QString err = lua_tostring(L, -1);
            lua_pop(L, 1);
            emit consoleOutput(QStringLiteral("[ERROR] on_slide_change: %1").arg(err));
        }
    } else {
        lua_pop(L, 1);
    }
}

void LuaEngine::callOnSwitchChange(int index, bool value)
{
    QMutexLocker lock(&m_mutex);
    if (!L || !m_running) return;

    lua_getglobal(L, "on_switch_change");
    if (lua_isfunction(L, -1)) {
        lua_pushinteger(L, index);
        lua_pushboolean(L, value);
        if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
            QString err = lua_tostring(L, -1);
            lua_pop(L, 1);
            emit consoleOutput(QStringLiteral("[ERROR] on_switch_change: %1").arg(err));
        }
    } else {
        lua_pop(L, 1);
    }
}

void LuaEngine::callOnModeChange(int index, int mode)
{
    QMutexLocker lock(&m_mutex);
    if (!L || !m_running) return;

    lua_getglobal(L, "on_mode_change");
    if (lua_isfunction(L, -1)) {
        lua_pushinteger(L, index);
        lua_pushinteger(L, mode);
        if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
            QString err = lua_tostring(L, -1);
            lua_pop(L, 1);
            emit consoleOutput(QStringLiteral("[ERROR] on_mode_change: %1").arg(err));
        }
    } else {
        lua_pop(L, 1);
    }
}

// --- 文件对话框 ---

QString LuaEngine::openLuaFileDialog()
{
    QString startDir = getLastDir();
    QString path = QFileDialog::getOpenFileName(nullptr,
        tr("打开 Lua 脚本"), startDir, tr("Lua 脚本 (*.lua);;所有文件 (*)"));
    if (!path.isEmpty()) {
        // 保存目录
        QSettings settings("KPtools", "KPtools");
        settings.setValue("script/lastDir", QFileInfo(path).absolutePath());
    }
    return path;
}

QString LuaEngine::saveLuaFileDialog(const QString &defaultName)
{
    QString startDir = getLastDir();
    if (!defaultName.isEmpty()) {
        startDir = startDir + "/" + defaultName;
    }
    QString path = QFileDialog::getSaveFileName(nullptr,
        tr("保存 Lua 脚本"), startDir, tr("Lua 脚本 (*.lua);;所有文件 (*)"));
    if (!path.isEmpty()) {
        QSettings settings("KPtools", "KPtools");
        settings.setValue("script/lastDir", QFileInfo(path).absolutePath());
    }
    return path;
}

QString LuaEngine::readTextFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly | QFile::Text))
        return QString();
    return QString::fromUtf8(file.readAll());
}

bool LuaEngine::writeTextFile(const QString &filePath, const QString &content)
{
    QFile file(filePath);
    if (!file.open(QFile::WriteOnly | QFile::Text))
        return false;
    file.write(content.toUtf8());
    return true;
}

QString LuaEngine::getLastDir()
{
    QSettings settings("KPtools", "KPtools");
    QString dir = settings.value("script/lastDir").toString();
    if (dir.isEmpty() || !QDir(dir).exists())
        dir = QDir::homePath();
    return dir;
}

QString LuaEngine::getAppConfigDir()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    return dir;
}
