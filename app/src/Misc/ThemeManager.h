#pragma once

#include <QColor>
#include <QObject>
#include <QPalette>
#include <QSettings>
#include <QVariantMap>

namespace Misc {

/**
 * @brief 统一主题管理 — 支持浅色/深色主题切换
 *
 * 提供 dark/light 主题颜色映射和 QPalette。
 * QML 通过 colors 属性直接绑定颜色，isDark 属性控制当前主题。
 */
class ThemeManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap colors READ colors NOTIFY themeChanged)
    Q_PROPERTY(QPalette palette READ palette NOTIFY themeChanged)
    Q_PROPERTY(bool isDark READ isDark NOTIFY themeChanged)

signals:
    void themeChanged();

private:
    explicit ThemeManager();
    ThemeManager(ThemeManager&&) = delete;
    ThemeManager(const ThemeManager&) = delete;
    ThemeManager& operator=(ThemeManager&&) = delete;
    ThemeManager& operator=(const ThemeManager&) = delete;

public:
    static ThemeManager &instance();

    const QVariantMap &colors() const;
    const QPalette &palette() const;
    QColor getColor(const QString &name) const;
    bool isDark() const;

    Q_INVOKABLE void toggleTheme();
    Q_INVOKABLE void setDarkMode(bool dark);

private:
    void initDarkTheme();
    void initLightTheme();
    void applyTheme();

    QPalette m_palette;
    QVariantMap m_colors;
    bool m_isDark = false;  // 默认浅色模式
};

} // namespace Misc
