/**
 * ProtoTools - 协议调试工具
 * main.cpp - 应用入口
 *
 * 架构参照 SerialStudio: ModuleManager 管理 QML 注册和模块初始化，
 * main.cpp 只负责 OS 适配和 CLI 参数。
 */

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QIcon>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QTimer>

#ifdef Q_OS_MAC
#include "Platform/macos_helpers.h"
#endif

#include "Misc/ModuleManager.h"
#include "Misc/TimerEvents.h"
#include "Misc/ThemeManager.h"
#include "DataModel/SettingsManager.h"
#include "IO/ConnectionManager.h"
#include "IO/Drivers/UART.h"
#include "IO/Drivers/UsbBulk.h"
#include "DataModel/FirmwareUpgradeManager.h"

#include <algorithm>

int main(int argc, char *argv[])
{
    // 高 DPI 支持
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    // 抑制 Qt 底层无害警告
    qputenv("QT_LOGGING_RULES", "qt.qpa.socketnotifier.warning=false");

#ifdef Q_OS_MAC
    qputenv("OS_ACTIVITY_MODE", "disable");
#endif

    QApplication app(argc, argv);
    app.setApplicationName("ProtoTools");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("ProtoTools");
    app.setOrganizationDomain("prototools.app");
    app.setWindowIcon(QIcon(QStringLiteral(":/ProtoDebug/proto_512.png")));
    QQuickStyle::setStyle("Fusion");

    QStringList args = QCoreApplication::arguments();
    if (args.contains(QStringLiteral("--usb-open-test"))) {
        IO::Drivers::UsbBulk usb;
        usb.setVendorId(0x026A);
        usb.setProductId(0x0065);
        usb.setInterfaceNumber(0);
        usb.setBulkInEndpoint(0x81);
        usb.setBulkOutEndpoint(0x01);
        usb.setReadPacketSize(512);
        usb.setTimeoutMs(50);

        QObject::connect(&usb, &IO::HAL_Driver::errorOccurred, &app, [](const QString &message) {
            qInfo().noquote() << "usb-open-test: error:" << message;
        });
        QObject::connect(&usb, &IO::HAL_Driver::connectionChanged, &app, [&usb]() {
            qInfo() << "usb-open-test: connectionChanged open=" << usb.isOpen();
        });

        QTimer::singleShot(0, &app, [&usb]() {
            qInfo("usb-open-test: VID=0x026A PID=0x0065 IF=0 IN=0x81 OUT=0x01");
            const bool ok = usb.open();
            qInfo() << "usb-open-test: open=" << ok;
            if (ok) {
                usb.close();
                QCoreApplication::exit(0);
            } else {
                QCoreApplication::exit(2);
            }
        });
        return app.exec();
    }

    if (args.contains(QStringLiteral("--can-query-helper-test"))) {
        SettingsManager settingsManager(&app);
        ConnectionManager cm;
        FirmwareUpgradeManager fw(&cm);

        QObject::connect(&cm, &ConnectionManager::errorOccurred, &app, [](const QString &message) {
            qInfo().noquote() << "can-query-helper-test: link error:" << message;
        });
        QObject::connect(&fw, &FirmwareUpgradeManager::statusChanged, &app, [&fw]() {
            qInfo().noquote() << "can-query-helper-test: status:" << fw.status();
        });
        QObject::connect(&fw, &FirmwareUpgradeManager::deviceInfoChanged, &app, [&fw]() {
            qInfo().noquote() << "can-query-helper-test: device:" << fw.deviceSummary();
            QTimer::singleShot(100, qApp, []() { QCoreApplication::exit(0); });
        });
        QObject::connect(&fw, &FirmwareUpgradeManager::errorOccurred, &app, [](const QString &message) {
            qInfo().noquote() << "can-query-helper-test: firmware error:" << message;
            QTimer::singleShot(100, qApp, []() { QCoreApplication::exit(3); });
        });

        QTimer::singleShot(0, &app, [&cm, &fw]() {
            cm.setLinkType(QStringLiteral("CAN"));
            cm.openConnection();
            qInfo() << "can-query-helper-test: connected=" << cm.isConnected();
            fw.queryDevice();
        });
        QTimer::singleShot(20000, &app, []() {
            qInfo("can-query-helper-test: timeout");
            QCoreApplication::exit(4);
        });
        return app.exec();
    }

    if (args.contains(QStringLiteral("--usb-query-test"))) {
        SettingsManager settingsManager(&app);
        ConnectionManager cm;
        FirmwareUpgradeManager fw(&cm);

        QObject::connect(&cm, &ConnectionManager::errorOccurred, &app, [](const QString &message) {
            qInfo().noquote() << "usb-query-test: link error:" << message;
        });
        QObject::connect(&fw, &FirmwareUpgradeManager::statusChanged, &app, [&fw]() {
            qInfo().noquote() << "usb-query-test: status:" << fw.status();
        });
        QObject::connect(&fw, &FirmwareUpgradeManager::deviceInfoChanged, &app, [&fw]() {
            qInfo().noquote() << "usb-query-test: device:" << fw.deviceSummary();
            QTimer::singleShot(100, qApp, []() { QCoreApplication::exit(0); });
        });
        QObject::connect(&fw, &FirmwareUpgradeManager::errorOccurred, &app, [](const QString &message) {
            qInfo().noquote() << "usb-query-test: firmware error:" << message;
            QTimer::singleShot(100, qApp, []() { QCoreApplication::exit(3); });
        });

        QTimer::singleShot(0, &app, [&cm, &fw]() {
            const auto parseSettingInt = [](const QVariant &value, int fallback) {
                bool ok = false;
                const int parsed = value.toString().toInt(&ok, 0);
                if (ok)
                    return parsed;
                const int decimal = value.toInt(&ok);
                return ok ? decimal : fallback;
            };
            if (auto *settings = SettingsManager::instance()) {
                fw.setSrcAddr(parseSettingInt(settings->loadValue(QStringLiteral("upgrade.src"), QStringLiteral("0x10")), 0x10));
                fw.setDstAddr(parseSettingInt(settings->loadValue(QStringLiteral("upgrade.dst"), QStringLiteral("0x05")), 0x05));
                fw.setPacketSize(settings->loadValue(QStringLiteral("upgrade.packetSize"), fw.packetSize()).toInt());
            }
            cm.setLinkType(QStringLiteral("USB"));
            cm.openConnection();
            qInfo() << "usb-query-test: connected=" << cm.isConnected();
            fw.queryDevice();
        });
        QTimer::singleShot(20000, &app, []() {
            qInfo("usb-query-test: timeout");
            QCoreApplication::exit(4);
        });
        return app.exec();
    }

    if (args.contains(QStringLiteral("--usb-stress-test")) || args.contains(QStringLiteral("--stress-test"))) {
        SettingsManager settingsManager(&app);
        ConnectionManager cm;
        FirmwareUpgradeManager fw(&cm);

        const auto optionValue = [&args](const QString &name) -> QString {
            const QString prefix = name + QStringLiteral("=");
            for (int i = 0; i < args.size(); ++i) {
                if (args[i] == name && i + 1 < args.size())
                    return args[i + 1];
                if (args[i].startsWith(prefix))
                    return args[i].mid(prefix.size());
            }
            return {};
        };
        const auto parseInt = [](const QVariant &value, int fallback) {
            bool ok = false;
            const int parsed = value.toString().toInt(&ok, 0);
            if (ok)
                return parsed;
            const int decimal = value.toInt(&ok);
            return ok ? decimal : fallback;
        };
        const auto clampedCount = [](int value) {
            return std::max(1, std::min(9999, value));
        };

        QString firmwarePath = optionValue(QStringLiteral("--firmware"));
        if (firmwarePath.trimmed().isEmpty()) {
            firmwarePath = settingsManager.loadValue(QStringLiteral("upgrade.firmwarePath"), QString()).toString();
        }
        if (firmwarePath.trimmed().isEmpty() || !QFileInfo::exists(firmwarePath)) {
            qInfo().noquote() << "usb-stress-test: missing firmware, pass --firmware <path>";
            return 2;
        }

        int count = clampedCount(parseInt(settingsManager.loadValue(QStringLiteral("upgrade.stressCount"), 100), 100));
        const QString countArg = optionValue(QStringLiteral("--count"));
        if (!countArg.isEmpty())
            count = clampedCount(parseInt(countArg, count));

        QString linkType = optionValue(QStringLiteral("--link")).trimmed().toUpper();
        if (linkType.isEmpty()) {
            linkType = args.contains(QStringLiteral("--usb-stress-test"))
                ? QStringLiteral("USB")
                : settingsManager.loadValue(QStringLiteral("setup.linkType"), QStringLiteral("UART")).toString().trimmed().toUpper();
        }
        if (!cm.linkTypes().contains(linkType))
            linkType = QStringLiteral("UART");
        const QString logPrefix = QStringLiteral("%1-stress-test").arg(linkType.toLower());
        const QString logPathArg = optionValue(QStringLiteral("--log")).trimmed();
        const QString logPath = logPathArg.isEmpty()
            ? QDir::temp().absoluteFilePath(QStringLiteral("kptools_%1.log").arg(logPrefix))
            : logPathArg;
        auto *stressLogFile = new QFile(logPath, &app);
        if (stressLogFile->open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            QTextStream out(stressLogFile);
            out << QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
                << " " << logPrefix << ": log=" << QDir::toNativeSeparators(logPath) << '\n';
            out.flush();
        }
        const auto logLine = [stressLogFile](const QString &line) {
            qInfo().noquote() << line;
            if (!stressLogFile || !stressLogFile->isOpen())
                return;
            QTextStream out(stressLogFile);
            out << QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
                << " " << line << '\n';
            out.flush();
        };
        const auto hexLine = [](const QByteArray &data) {
            QStringList parts;
            parts.reserve(data.size());
            for (unsigned char byte : data)
                parts.append(QStringLiteral("0x") + QStringLiteral("%1").arg(byte, 2, 16, QChar('0')).toUpper());
            return parts.join(QLatin1Char(' '));
        };

        const QString portArg = optionValue(QStringLiteral("--port")).trimmed();
        const QString baudArg = optionValue(QStringLiteral("--baud")).trimmed();
        int baudRate = parseInt(settingsManager.loadValue(QStringLiteral("setup.uart.baudRate"), 115200), 115200);
        if (!baudArg.isEmpty())
            baudRate = parseInt(baudArg, baudRate);

        fw.setFirmwarePath(firmwarePath);
        fw.setSrcAddr(parseInt(settingsManager.loadValue(QStringLiteral("upgrade.src"), QStringLiteral("0x10")), 0x10));
        fw.setDstAddr(parseInt(settingsManager.loadValue(QStringLiteral("upgrade.dst"), QStringLiteral("0x05")), 0x05));
        const QString srcArg = optionValue(QStringLiteral("--src"));
        const QString dstArg = optionValue(QStringLiteral("--dst"));
        if (!srcArg.isEmpty())
            fw.setSrcAddr(parseInt(srcArg, fw.srcAddr()));
        if (!dstArg.isEmpty())
            fw.setDstAddr(parseInt(dstArg, fw.dstAddr()));
        fw.setPacketSize(settingsManager.loadValue(QStringLiteral("upgrade.packetSize"), fw.packetSize()).toInt());

        QObject::connect(&cm, &ConnectionManager::errorOccurred, &app, [logLine, logPrefix](const QString &message) {
            logLine(logPrefix + QStringLiteral(": link error: ") + message);
        });
        QObject::connect(&cm, &ConnectionManager::connectionChanged, &app, [&cm, logLine, logPrefix]() {
            logLine(logPrefix + QStringLiteral(": connection changed connected=%1").arg(cm.isConnected()));
        });
        QObject::connect(&cm, &ConnectionManager::dataSent, &app, [hexLine, logLine, logPrefix](const QByteArray &data) {
            logLine(logPrefix + QStringLiteral(": tx: ") + hexLine(data));
        });
        QObject::connect(&cm, &ConnectionManager::rawDataReceived, &app, [hexLine, logLine, logPrefix](const QByteArray &data) {
            logLine(logPrefix + QStringLiteral(": rx: ") + hexLine(data));
        });
        QObject::connect(&fw, &FirmwareUpgradeManager::statusChanged, &app, [&fw, logLine, logPrefix]() {
            logLine(logPrefix + QStringLiteral(": status: ") + fw.status());
        });
        QObject::connect(&fw, &FirmwareUpgradeManager::upgradeSuccessCountChanged, &app, [&fw, logLine, logPrefix]() {
            logLine(logPrefix + QStringLiteral(": success: %1").arg(fw.upgradeSuccessCount()));
        });
        QObject::connect(&fw, &FirmwareUpgradeManager::deviceInfoChanged, &app, [&fw, logLine, logPrefix]() {
            if (!fw.deviceSummary().isEmpty())
                logLine(logPrefix + QStringLiteral(": device: ") + fw.deviceSummary());
        });
        QObject::connect(&fw, &FirmwareUpgradeManager::errorOccurred, &app, [logLine, logPrefix](const QString &message) {
            logLine(logPrefix + QStringLiteral(": firmware error: ") + message);
            QTimer::singleShot(100, qApp, []() { QCoreApplication::exit(3); });
        });
        QObject::connect(&fw, &FirmwareUpgradeManager::runningChanged, &app, [&fw, count, logLine, logPrefix]() {
            if (!fw.running() && fw.upgradeSuccessCount() >= count) {
                logLine(logPrefix + QStringLiteral(": complete: %1/%2").arg(fw.upgradeSuccessCount()).arg(count));
                QTimer::singleShot(100, qApp, []() { QCoreApplication::exit(0); });
            }
        });

        QTimer::singleShot(0, &app, [&cm, &fw, count, linkType, portArg, baudRate, logLine, logPrefix]() {
            cm.setLinkType(linkType);
            if (linkType == QStringLiteral("UART")) {
                if (auto *uart = cm.uartDriver()) {
                    if (!portArg.isEmpty())
                        uart->setPortName(portArg);
                    uart->setBaudRate(baudRate);
                }
            }
            cm.openConnection();
            logLine(logPrefix + QStringLiteral(": link=%1 connected=%2 src=0x%3 dst=0x%4")
                    .arg(linkType)
                    .arg(cm.isConnected())
                    .arg(QStringLiteral("%1").arg(fw.srcAddr(), 4, 16, QChar('0')).toUpper())
                    .arg(QStringLiteral("%1").arg(fw.dstAddr(), 4, 16, QChar('0')).toUpper()));
            if (!cm.isConnected()) {
                QCoreApplication::exit(4);
                return;
            }
            fw.startStressUpgrade(count);
        });
        QTimer::singleShot(std::max(300000, count * 180000), &app, [logLine, logPrefix]() {
            logLine(logPrefix + QStringLiteral(": timeout"));
            QCoreApplication::exit(5);
        });
        return app.exec();
    }

    // ── 模块管理（参照 SerialStudio ModuleManager）──
    Misc::ModuleManager moduleManager;
    moduleManager.registerQmlTypes();

    QQmlApplicationEngine engine;
    moduleManager.initializeQmlInterface(&engine);

    // ── 加载主 QML ──
    const QUrl url(QStringLiteral("qrc:/ProtoDebug/qml/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    // ── macOS 暗色标题栏 ──
#ifdef Q_OS_MAC
    for (QObject *obj : engine.rootObjects()) {
        if (auto *win = qobject_cast<QQuickWindow *>(obj))
            setDarkTitleBar(reinterpret_cast<void *>(win->winId()));
    }
#endif

    // ── CLI 参数 ──
    bool optHexMode = args.contains(QStringLiteral("--hex-mode"));
    bool optTimestamp = args.contains(QStringLiteral("--timestamp"));
    engine.rootContext()->setContextProperty("optHexMode", optHexMode);
    engine.rootContext()->setContextProperty("optTimestamp", optTimestamp);

    const auto globalOptionValue = [&args](const QString &name) -> QString {
        const QString prefix = name + QStringLiteral("=");
        for (int i = 0; i < args.size(); ++i) {
            if (args[i] == name && i + 1 < args.size())
                return args[i + 1];
            if (args[i].startsWith(prefix))
                return args[i].mid(prefix.size());
        }
        return {};
    };
    const QString rawLogPath = globalOptionValue(QStringLiteral("--raw-log")).trimmed();
    if (!rawLogPath.isEmpty()) {
        auto *cm = engine.rootContext()->contextProperty("linkManager").value<ConnectionManager *>();
        auto *rawLogFile = new QFile(rawLogPath, &app);
        if (rawLogFile->open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            auto writeLogLine = [rawLogFile](const QString &line) {
                QTextStream out(rawLogFile);
                out << QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
                    << " " << line << '\n';
                out.flush();
            };
            const auto hexLine = [](const QByteArray &data) {
                QStringList parts;
                parts.reserve(data.size());
                for (unsigned char byte : data)
                    parts.append(QStringLiteral("0x") + QStringLiteral("%1").arg(byte, 2, 16, QChar('0')).toUpper());
                return parts.join(QLatin1Char(' '));
            };
            if (cm) {
                QObject::connect(cm, &ConnectionManager::rawDataReceived, rawLogFile,
                                 [writeLogLine, hexLine](const QByteArray &data) {
                    writeLogLine(QStringLiteral("RX_RAW len=%1 %2").arg(data.size()).arg(hexLine(data)));
                });
                QObject::connect(cm, &ConnectionManager::frameReceived, rawLogFile,
                                 [writeLogLine](const QVariantMap &frame) {
                    writeLogLine(QStringLiteral("FRAME cmd=%1 set=%2 id=%3 len=%4 data=%5")
                                 .arg(frame.value(QStringLiteral("cmd")).toString(),
                                      frame.value(QStringLiteral("cmdSet")).toString(),
                                      frame.value(QStringLiteral("cmdId")).toString(),
                                      frame.value(QStringLiteral("len")).toString(),
                                      frame.value(QStringLiteral("data")).toString()));
                });
                writeLogLine(QStringLiteral("raw log started"));
            }
        }
    }

    if (args.contains(QStringLiteral("--can-query-test"))) {
        auto *cm = engine.rootContext()->contextProperty("linkManager").value<ConnectionManager *>();
        auto *fw = engine.rootContext()->contextProperty("firmwareUpgrade").value<FirmwareUpgradeManager *>();
        if (!cm || !fw) {
            qInfo("can-query-test: missing managers");
            return -2;
        }

        QObject::connect(cm, &ConnectionManager::errorOccurred, &app, [](const QString &message) {
            qInfo().noquote() << "can-query-test: link error:" << message;
        });
        QObject::connect(fw, &FirmwareUpgradeManager::statusChanged, &app, [fw]() {
            qInfo().noquote() << "can-query-test: status:" << fw->status();
        });
        QObject::connect(fw, &FirmwareUpgradeManager::deviceInfoChanged, &app, [fw]() {
            qInfo().noquote() << "can-query-test: device:" << fw->deviceSummary();
            QTimer::singleShot(5000, qApp, []() {
                qInfo("can-query-test: success, exiting");
                QCoreApplication::exit(0);
            });
        });
        QObject::connect(fw, &FirmwareUpgradeManager::errorOccurred, &app, [](const QString &message) {
            qInfo().noquote() << "can-query-test: firmware error:" << message;
            QTimer::singleShot(100, qApp, []() { QCoreApplication::exit(3); });
        });

        QTimer::singleShot(1000, &app, [cm]() {
            qInfo("can-query-test: selecting CAN");
            cm->setLinkType(QStringLiteral("CAN"));
        });
        QTimer::singleShot(2500, &app, [cm, fw]() {
            qInfo() << "can-query-test: connected=" << cm->isConnected();
            fw->queryDevice();
        });
        QTimer::singleShot(20000, &app, []() {
            qInfo("can-query-test: timeout");
            QCoreApplication::exit(4);
        });
    }

    return app.exec();
}
