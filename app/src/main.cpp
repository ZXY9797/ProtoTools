/**
 * KPtools - 协议调试工具
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
#include <QFileInfo>
#include <QTimer>

#ifdef Q_OS_MAC
#include "Platform/macos_helpers.h"
#endif

#include "Misc/ModuleManager.h"
#include "Misc/TimerEvents.h"
#include "Misc/ThemeManager.h"
#include "DataModel/SettingsManager.h"
#include "IO/ConnectionManager.h"
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
    app.setApplicationName("KPtools");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("KPtools");
    app.setOrganizationDomain("kptools.app");
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
                fw.setSrcAddr(parseSettingInt(settings->loadValue(QStringLiteral("upgrade.src"), QStringLiteral("0x0101")), 0x0101));
                fw.setDstAddr(parseSettingInt(settings->loadValue(QStringLiteral("upgrade.dst"), QStringLiteral("0x0500")), 0x0500));
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

    if (args.contains(QStringLiteral("--usb-stress-test"))) {
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

        fw.setFirmwarePath(firmwarePath);
        fw.setSrcAddr(parseInt(settingsManager.loadValue(QStringLiteral("upgrade.src"), QStringLiteral("0x0101")), 0x0101));
        fw.setDstAddr(parseInt(settingsManager.loadValue(QStringLiteral("upgrade.dst"), QStringLiteral("0x0500")), 0x0500));
        const QString srcArg = optionValue(QStringLiteral("--src"));
        const QString dstArg = optionValue(QStringLiteral("--dst"));
        if (!srcArg.isEmpty())
            fw.setSrcAddr(parseInt(srcArg, fw.srcAddr()));
        if (!dstArg.isEmpty())
            fw.setDstAddr(parseInt(dstArg, fw.dstAddr()));
        fw.setPacketSize(settingsManager.loadValue(QStringLiteral("upgrade.packetSize"), fw.packetSize()).toInt());

        QObject::connect(&cm, &ConnectionManager::errorOccurred, &app, [](const QString &message) {
            qInfo().noquote() << "usb-stress-test: link error:" << message;
        });
        QObject::connect(&fw, &FirmwareUpgradeManager::statusChanged, &app, [&fw]() {
            qInfo().noquote() << "usb-stress-test: status:" << fw.status();
        });
        QObject::connect(&fw, &FirmwareUpgradeManager::upgradeSuccessCountChanged, &app, [&fw]() {
            qInfo().noquote() << "usb-stress-test: success:" << fw.upgradeSuccessCount();
        });
        QObject::connect(&fw, &FirmwareUpgradeManager::deviceInfoChanged, &app, [&fw]() {
            if (!fw.deviceSummary().isEmpty())
                qInfo().noquote() << "usb-stress-test: device:" << fw.deviceSummary();
        });
        QObject::connect(&fw, &FirmwareUpgradeManager::errorOccurred, &app, [](const QString &message) {
            qInfo().noquote() << "usb-stress-test: firmware error:" << message;
            QTimer::singleShot(100, qApp, []() { QCoreApplication::exit(3); });
        });
        QObject::connect(&fw, &FirmwareUpgradeManager::runningChanged, &app, [&fw, count]() {
            if (!fw.running() && fw.upgradeSuccessCount() >= count) {
                qInfo().noquote() << "usb-stress-test: complete:" << fw.upgradeSuccessCount() << "/" << count;
                QTimer::singleShot(100, qApp, []() { QCoreApplication::exit(0); });
            }
        });

        QTimer::singleShot(0, &app, [&cm, &fw, count]() {
            cm.setLinkType(QStringLiteral("USB"));
            cm.openConnection();
            qInfo() << "usb-stress-test: connected=" << cm.isConnected();
            if (!cm.isConnected()) {
                QCoreApplication::exit(4);
                return;
            }
            fw.startStressUpgrade(count);
        });
        QTimer::singleShot(std::max(300000, count * 180000), &app, []() {
            qInfo("usb-stress-test: timeout");
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
