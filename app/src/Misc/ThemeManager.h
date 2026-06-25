#pragma once

#include <QColor>
#include <QObject>
#include <QPalette>
#include <QSettings>
#include <QVariantMap>

namespace Misc {

/**
 * @brief 统一主题管理 — 移植自 SerialStudio ThemeManager
 *
 * 提供 dark 主题颜色映射和 QPalette。QML 通过 colors 属性直接绑定颜色。
 * 当前仅内置 dark 主题，后续可扩展 JSON 主题文件加载。
 */
class ThemeManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap colors READ colors NOTIFY themeChanged)
    Q_PROPERTY(QPalette palette READ palette NOTIFY themeChanged)

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

private:
    void initDarkTheme();

    QPalette m_palette;
    QVariantMap m_colors;
};

} // namespace Misc
