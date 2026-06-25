#pragma once

#include <QFont>
#include <QObject>
#include <QSettings>

namespace Misc {

/**
 * @brief 统一字体管理（参考 Serial-Studio CommonFonts）
 *
 * 提供 UI 字体、等宽字体、控件字体的统一管理，支持全局缩放。
 */
class CommonFonts : public QObject
{
    Q_OBJECT
    Q_PROPERTY(const QFont& uiFont READ uiFont NOTIFY fontsChanged)
    Q_PROPERTY(const QFont& monoFont READ monoFont NOTIFY fontsChanged)
    Q_PROPERTY(const QFont& boldUiFont READ boldUiFont NOTIFY fontsChanged)
    Q_PROPERTY(double widgetFontScale READ widgetFontScale WRITE setWidgetFontScale NOTIFY fontsChanged)
    Q_PROPERTY(QString widgetFontFamily READ widgetFontFamily WRITE setWidgetFontFamily NOTIFY fontsChanged)

public:
    static constexpr double kScaleSmall      = 0.85;
    static constexpr double kScaleNormal     = 1.00;
    static constexpr double kScaleLarge      = 1.25;
    static constexpr double kScaleExtraLarge = 1.50;

signals:
    void fontsChanged();

private:
    explicit CommonFonts();
    CommonFonts(CommonFonts&&) = delete;
    CommonFonts(const CommonFonts&) = delete;
    CommonFonts& operator=(CommonFonts&&) = delete;
    CommonFonts& operator=(const CommonFonts&) = delete;

public:
    static CommonFonts& instance();

    const QFont& uiFont() const;
    const QFont& monoFont() const;
    const QFont& boldUiFont() const;
    double widgetFontScale() const;
    QString widgetFontFamily() const;

    Q_INVOKABLE QFont customUiFont(double fraction = 1, bool bold = false);
    Q_INVOKABLE QFont customMonoFont(double fraction = 1, bool bold = false);
    Q_INVOKABLE QFont widgetFont(double fraction = 1, bool bold = false) const;
    Q_INVOKABLE QStringList availableFonts() const;

public slots:
    void setWidgetFontScale(double scale);
    void setWidgetFontFamily(const QString &family);

private:
    QSettings m_settings;
    QFont m_uiFont;
    QFont m_monoFont;
    QFont m_boldUiFont;
    QString m_widgetFontFamily;
    double m_widgetFontScale;
};

} // namespace Misc
