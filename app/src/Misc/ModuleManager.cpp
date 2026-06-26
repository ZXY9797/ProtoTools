#include "ModuleManager.h"

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSettings>
#include <QStandardPaths>

#include <QFontDatabase>
#include <QDateTime>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QQueue>
#include <QQuickStyle>
#include <QScreen>
#include <QTimer>

#include <algorithm>
#include <memory>

#include "Misc/CommonFonts.h"
#include "Misc/ThemeManager.h"
#include "Misc/TimerEvents.h"
#include "Misc/Translator.h"

#include "IO/ConnectionManager.h"
#include "IO/Drivers/UART.h"
#include "IO/Drivers/UsbBulk.h"
#include "IO/Drivers/ZlgCanFd.h"
#include "IO/Drivers/BluetoothLE.h"

#include "Terminal/Terminal.h"
#include "Terminal/TerminalWidget.h"

#include "DataModel/ProtocolEngine.h"
#include "DataModel/FirmwareUpgradeManager.h"
#include "DataModel/DataMonitorModel.h"
#include "DataModel/QuickCommandManager.h"
#include "DataModel/SettingsManager.h"
#include "Script/LuaEngine.h"

#include "Script/LuaCodeEditor.h"

namespace Misc {

ModuleManager::ModuleManager() {}

QQmlApplicationEngine *ModuleManager::engine() const
{
    return m_engine;
}

void ModuleManager::registerQmlTypes()
{
    // QML types (参照 SerialStudio ModuleManager::registerQmlTypes)
    qmlRegisterType<ConnectionManager>("ProtoDebug", 1, 0, "ConnectionManager");
    qmlRegisterType<ProtocolEngine>("ProtoDebug", 1, 0, "ProtocolEngine");
    qmlRegisterType<FirmwareUpgradeManager>("ProtoDebug", 1, 0, "FirmwareUpgradeManager");
    qmlRegisterType<DataMonitorModel>("ProtoDebug", 1, 0, "DataMonitorModel");
    qmlRegisterType<TerminalWidget>("ProtoDebug", 1, 0, "TerminalWidget");
    qmlRegisterType<LuaCodeEditor>("ProtoDebug", 1, 0, "LuaCodeEditor");

    // Drivers (uncreatable — QML 只读属性)
    qmlRegisterUncreatableType<IO::Drivers::UART>("ProtoDebug", 1, 0, "UartDriver",
        "Access via linkManager.uart");
    qmlRegisterUncreatableType<IO::Drivers::UsbBulk>("ProtoDebug", 1, 0, "UsbBulkDriver",
        "Access via linkManager.usb");
    qmlRegisterUncreatableType<IO::Drivers::ZlgCanFd>("ProtoDebug", 1, 0, "CanDriver",
        "Access via linkManager.can");
    qmlRegisterUncreatableType<IO::Drivers::BluetoothLE>("ProtoDebug", 1, 0, "BLEDriver",
        "Access via linkManager.ble");
}

void ModuleManager::initializeQmlInterface(QQmlApplicationEngine *engine)
{
    m_engine = engine;
    auto *app = qobject_cast<QApplication *>(QCoreApplication::instance());
    if (!app) return;

    // ── SettingsManager 必须最先创建（ConnectionManager/Terminal 构造时依赖其 instance()）──
    auto *settingsManager   = new SettingsManager(app);
    if (!settingsManager->loadValue(QStringLiteral("migration.defaultWorkspaceUartMonitor"), false).toBool()) {
        settingsManager->saveValue(QStringLiteral("setup.linkType"), QStringLiteral("UART"));
        settingsManager->saveValue(QStringLiteral("layout.monitorTabIndex"), 0);
        settingsManager->saveValue(QStringLiteral("migration.defaultWorkspaceUartMonitor"), true);
    }
    settingsManager->saveValue("monitor.commandEchoEnabled", false);
    if (settingsManager->loadValue("protoeditor.seq", "0005").toString().trimmed().isEmpty())
        settingsManager->saveValue("protoeditor.seq", "0005");
    if (auto *screen = QGuiApplication::primaryScreen()) {
        const QRect available = screen->availableGeometry();
        const int minWidth = 900;
        const int minHeight = 640;
        const int defaultHeight = 700;
        const int width = std::clamp(settingsManager->loadValue("window.width", 1280).toInt(),
                                     minWidth, std::max(minWidth, available.width()));
        int savedHeight = settingsManager->loadValue("window.height", defaultHeight).toInt();
        if (!settingsManager->loadValue(QStringLiteral("migration.compactWindowHeight700"), false).toBool()) {
            if (savedHeight <= 900)
                savedHeight = defaultHeight;
            settingsManager->saveValue(QStringLiteral("migration.compactWindowHeight700"), true);
        }
        const int height = std::clamp(savedHeight,
                                      minHeight, std::max(minHeight, available.height()));
        const int maxX = available.x() + std::max(0, available.width() - width);
        const int maxY = available.y() + std::max(0, available.height() - height);
        const int x = std::clamp(settingsManager->loadValue("window.x", available.x() + 80).toInt(),
                                 available.x(), maxX);
        const int y = std::clamp(settingsManager->loadValue("window.y", available.y() + 80).toInt(),
                                 available.y(), maxY);
        settingsManager->saveValue("window.x", x);
        settingsManager->saveValue("window.y", y);
        settingsManager->saveValue("window.width", width);
        settingsManager->saveValue("window.height", height);
    }

    // ── 单例模块（参照 SerialStudio initializeQmlInterface）──
    auto *connectionManager = new ConnectionManager(app);
    auto *protocolEngine    = new ProtocolEngine(app);
    auto *firmwareUpgrade   = new FirmwareUpgradeManager(connectionManager, app);
    auto *monitorModel      = new DataMonitorModel(app);
    auto *quickCmdManager   = new QuickCommandManager(app);
    auto *luaEngine         = new LuaEngine(app);
    auto *terminalModel     = new Terminal(app);

    auto *timerEvents   = &Misc::TimerEvents::instance();
    auto *commonFonts   = &Misc::CommonFonts::instance();
    auto *themeManager  = &Misc::ThemeManager::instance();
    auto *translator    = &Misc::Translator::instance();

    // ── Context properties（参照 SS Cpp_XXX 命名）──
    auto *c = engine->rootContext();
    c->setContextProperty("Cpp_IO_Manager",       connectionManager);
    c->setContextProperty("Cpp_ProtocolEngine",   protocolEngine);
    c->setContextProperty("Cpp_FirmwareUpgrade",   firmwareUpgrade);
    c->setContextProperty("Cpp_DataMonitorModel", monitorModel);
    c->setContextProperty("Cpp_QuickCommandMgr",  quickCmdManager);
    c->setContextProperty("Cpp_SettingsManager",  settingsManager);
    c->setContextProperty("Cpp_LuaEngine",        luaEngine);
    c->setContextProperty("Cpp_Console_Handler",  terminalModel);
    c->setContextProperty("Cpp_ThemeManager",     themeManager);
    c->setContextProperty("Cpp_Misc_Translator",  translator);
    c->setContextProperty("Cpp_Misc_TimerEvents", timerEvents);
    c->setContextProperty("Cpp_Misc_CommonFonts", commonFonts);

    // Legacy aliases (existing QML uses these names)
    c->setContextProperty("linkManager",     connectionManager);
    c->setContextProperty("protocolEngine",  protocolEngine);
    c->setContextProperty("firmwareUpgrade", firmwareUpgrade);
    c->setContextProperty("monitorModel",    monitorModel);
    c->setContextProperty("quickCmdManager", quickCmdManager);
    c->setContextProperty("settingsManager", settingsManager);
    c->setContextProperty("luaEngine",       luaEngine);
    c->setContextProperty("terminal",        terminalModel);
    c->setContextProperty("themeManager",    themeManager);
    c->setContextProperty("commonFonts",     commonFonts);

    // Driver context properties
    c->setContextProperty("uartDriver",    connectionManager->uartDriver());
    c->setContextProperty("usbDriver",     connectionManager->usbDriver());
    c->setContextProperty("canDriver",     connectionManager->canDriver());
    c->setContextProperty("bleDriver",     connectionManager->bleDriver());

    // ── 信号连接 ──
    auto *sendEngine = new ProtocolEngine(app);

    // 数据流水线: frameReceived → Lua → monitor
    QObject::connect(connectionManager, &ConnectionManager::frameReceived,
                     luaEngine, [luaEngine, monitorModel, connectionManager](const QVariantMap &frame) {
        if (connectionManager->isTerminalMode()) {
            monitorModel->addFrame(frame);
            return;
        }
        QVariantMap processedFrame = luaEngine->processFrameRecv(frame);
        const bool showFrame = processedFrame.value(QStringLiteral("__show"), true).toBool();
        processedFrame.remove(QStringLiteral("__show"));
        if (showFrame)
            monitorModel->addFrame(processedFrame);
    });

    // 终端：原始数据
    QObject::connect(connectionManager, &ConnectionManager::rawDataReceived,
                     terminalModel, &Terminal::appendRxData);
    QObject::connect(connectionManager, &ConnectionManager::dataSent,
                     terminalModel, &Terminal::appendTxData);

    auto txEchoQueue = std::make_shared<QQueue<QByteArray>>();
    auto *txEchoTimer = new QTimer(app);
    txEchoTimer->setSingleShot(true);
    QObject::connect(txEchoTimer, &QTimer::timeout,
                     monitorModel, [monitorModel, sendEngine, txEchoQueue, txEchoTimer]() {
        if (!monitorModel->commandEchoEnabled())
        {
            txEchoQueue->clear();
            return;
        }

        QList<QVariantMap> frames;
        frames.reserve(qMin(txEchoQueue->size(), 256));

        QElapsedTimer budget;
        budget.start();

        int processed = 0;
        while (!txEchoQueue->isEmpty() && processed < 256 && budget.elapsed() < 6) {
            const QByteArray data = txEchoQueue->dequeue();
            ++processed;

            QVariantMap frame = sendEngine->parseFrame(data);
            if (frame.isEmpty()) {
                const QString hex = QString::fromLatin1(data.toHex(' ').toUpper());
                frame["timestamp"] = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
                frame["direction"] = QStringLiteral("TX");
                frame["len"] = QString::number(data.size());
                frame["type"] = QStringLiteral("RAW");
                frame["data"] = hex;
                frame["rawHex"] = hex;
            } else {
                frame["direction"] = QStringLiteral("TX");
            }

            frames.append(frame);
        }

        monitorModel->enqueueFrames(frames);
        if (!txEchoQueue->isEmpty())
            txEchoTimer->start(1);
    });
    QObject::connect(connectionManager, &ConnectionManager::dataSent,
                     monitorModel, [monitorModel, txEchoQueue, txEchoTimer](const QByteArray &data) {
        if (!monitorModel->commandEchoEnabled())
            return;

        txEchoQueue->enqueue(data);
        if (!txEchoTimer->isActive())
            txEchoTimer->start(16);
    });

    // 终端发送 + Lua 发送 → ConnectionManager
    QObject::connect(terminalModel, &Terminal::dataSendRequested,
                     connectionManager, &ConnectionManager::sendData);
    QObject::connect(luaEngine, &LuaEngine::sendDataRequested,
                     connectionManager, &ConnectionManager::sendData);

    // 数据发送时调用 Lua on_frame_send（终端模式跳过）
    QObject::connect(connectionManager, &ConnectionManager::dataSent,
                     luaEngine, [luaEngine, sendEngine, connectionManager](const QByteArray &data) {
        if (connectionManager->isTerminalMode()) return;
        QVariantMap frame = sendEngine->parseFrame(data);
        if (!frame.isEmpty()) luaEngine->callOnFrameSend(frame);
    });

    // ── 启动定时器 ──
    timerEvents->startTimers();

    // ── 首次运行：写入所有默认值 ──
    auto *sm = SettingsManager::instance();
    if (sm) {
        sm->seedDefault("setup.linkType", "UART");
        sm->seedDefault("terminal.displayMode", 0);
        sm->seedDefault("terminal.lineEnding", 1);
        sm->seedDefault("terminal.dataMode", 0);
        sm->seedDefault("terminal.echo", true);
        sm->seedDefault("terminal.showTimestamp", false);
        sm->seedDefault("terminal.hexMode", false);
        sm->seedDefault("terminal.autoscroll", true);
        sm->seedDefault("monitor.commandEchoEnabled", false);
        sm->seedDefault("status.speedUnitKb", false);
        sm->seedDefault("terminal.vt100", false);
        sm->seedDefault("terminal.ansi", false);
        sm->seedDefault("layout.monitorTabIndex", 0);
    }

    // ── 快捷指令加载 ──
    quickCmdManager->loadFromFile("");
}

} // namespace Misc
