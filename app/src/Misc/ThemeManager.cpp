#include "ThemeManager.h"

namespace Misc {

ThemeManager::ThemeManager()
{
    // 默认浅色模式
    QSettings settings("ProtoTools", "ProtoTools");
    m_isDark = settings.value("theme/darkMode", false).toBool();
    applyTheme();
}

ThemeManager &ThemeManager::instance()
{
    static ThemeManager singleton;
    return singleton;
}

void ThemeManager::applyTheme()
{
    m_colors.clear();
    if (m_isDark) {
        initDarkTheme();
    } else {
        initLightTheme();
    }

    // Build QPalette
    m_palette.setColor(QPalette::Mid,            getColor("mid"));
    m_palette.setColor(QPalette::Dark,           getColor("dark"));
    m_palette.setColor(QPalette::Text,           getColor("text"));
    m_palette.setColor(QPalette::Base,           getColor("base"));
    m_palette.setColor(QPalette::Link,           getColor("link"));
    m_palette.setColor(QPalette::Light,          getColor("light"));
    m_palette.setColor(QPalette::Window,         getColor("window"));
    m_palette.setColor(QPalette::Shadow,         getColor("shadow"));
    m_palette.setColor(QPalette::Accent,         getColor("accent"));
    m_palette.setColor(QPalette::Button,         getColor("button"));
    m_palette.setColor(QPalette::Midlight,       getColor("midlight"));
    m_palette.setColor(QPalette::Highlight,      getColor("highlight"));
    m_palette.setColor(QPalette::WindowText,     getColor("window_text"));
    m_palette.setColor(QPalette::BrightText,     getColor("bright_text"));
    m_palette.setColor(QPalette::ButtonText,     getColor("button_text"));
    m_palette.setColor(QPalette::ToolTipBase,    getColor("tooltip_base"));
    m_palette.setColor(QPalette::ToolTipText,    getColor("tooltip_text"));
    m_palette.setColor(QPalette::LinkVisited,    getColor("link_visited"));
    m_palette.setColor(QPalette::AlternateBase,  getColor("alternate_base"));
    m_palette.setColor(QPalette::PlaceholderText, getColor("placeholder_text"));
    m_palette.setColor(QPalette::HighlightedText, getColor("highlighted_text"));

    emit themeChanged();
}

void ThemeManager::initDarkTheme()
{
    m_colors.insert("page_bg",         QColor("#0C1017"));
    m_colors.insert("surface",         QColor("#131920"));
    m_colors.insert("surface_raised",  QColor("#1A222C"));
    m_colors.insert("surface_soft",    QColor("#0F141B"));
    m_colors.insert("outline",         QColor("#253040"));
    m_colors.insert("outline_strong",  QColor("#384858"));
    m_colors.insert("text_primary",    QColor("#E8EDF5"));
    m_colors.insert("text_secondary",  QColor("#9CAAB8"));
    m_colors.insert("text_muted",      QColor("#607080"));
    m_colors.insert("accent",          QColor("#3B8AFF"));
    m_colors.insert("success",         QColor("#34D399"));
    m_colors.insert("warning",         QColor("#FBBF24"));
    m_colors.insert("danger",          QColor("#F87171"));

    // Legacy colors for Console
    m_colors.insert("mid",             QColor("#323233"));
    m_colors.insert("dark",            QColor("#0C1017"));
    m_colors.insert("text",            QColor("#E8EDF5"));
    m_colors.insert("base",            QColor("#131920"));
    m_colors.insert("link",            QColor("#3B8AFF"));
    m_colors.insert("light",           QColor("#384858"));
    m_colors.insert("window",          QColor("#131920"));
    m_colors.insert("shadow",          QColor("#000000"));
    m_colors.insert("button",          QColor("#1A222C"));
    m_colors.insert("midlight",        QColor("#253040"));
    m_colors.insert("highlight",       QColor("#1A3050"));
    m_colors.insert("window_text",     QColor("#E8EDF5"));
    m_colors.insert("bright_text",     QColor("#FFFFFF"));
    m_colors.insert("button_text",     QColor("#E8EDF5"));
    m_colors.insert("tooltip_base",    QColor("#1A222C"));
    m_colors.insert("tooltip_text",    QColor("#E8EDF5"));
    m_colors.insert("link_visited",    QColor("#3B8AFF"));
    m_colors.insert("alternate_base",  QColor("#0F141B"));
    m_colors.insert("placeholder_text",QColor("#607080"));
    m_colors.insert("highlighted_text", QColor("#FFFFFF"));
    m_colors.insert("console_base",    QColor("#0C1017"));
    m_colors.insert("console_border",  QColor("#253040"));
    m_colors.insert("console_text",    QColor("#E8EDF5"));
    m_colors.insert("console_highlight", QColor("#1A3050"));
    m_colors.insert("alarm",           QColor("#F87171"));
    m_colors.insert("widget_base",     QColor("#0F141B"));
    m_colors.insert("widget_border",   QColor("#253040"));
}

void ThemeManager::initLightTheme()
{
    m_colors.insert("page_bg",         QColor("#F8F9FB"));
    m_colors.insert("surface",         QColor("#FFFFFF"));
    m_colors.insert("surface_raised",  QColor("#F0F2F5"));
    m_colors.insert("surface_soft",    QColor("#F4F6F8"));
    m_colors.insert("outline",         QColor("#D1D5DB"));
    m_colors.insert("outline_strong",  QColor("#9CA3AF"));
    m_colors.insert("text_primary",    QColor("#111827"));
    m_colors.insert("text_secondary",  QColor("#4B5563"));
    m_colors.insert("text_muted",      QColor("#9CA3AF"));
    m_colors.insert("accent",          QColor("#2563EB"));
    m_colors.insert("success",         QColor("#059669"));
    m_colors.insert("warning",         QColor("#D97706"));
    m_colors.insert("danger",          QColor("#DC2626"));

    // Legacy colors for Console
    m_colors.insert("mid",             QColor("#D1D5DB"));
    m_colors.insert("dark",            QColor("#111827"));
    m_colors.insert("text",            QColor("#111827"));
    m_colors.insert("base",            QColor("#FFFFFF"));
    m_colors.insert("link",            QColor("#2563EB"));
    m_colors.insert("light",           QColor("#E5E7EB"));
    m_colors.insert("window",          QColor("#F8F9FB"));
    m_colors.insert("shadow",          QColor("#000000"));
    m_colors.insert("accent",          QColor("#2563EB"));
    m_colors.insert("button",          QColor("#F0F2F5"));
    m_colors.insert("midlight",        QColor("#E5E7EB"));
    m_colors.insert("highlight",       QColor("#DBEAFE"));
    m_colors.insert("window_text",     QColor("#111827"));
    m_colors.insert("bright_text",     QColor("#FFFFFF"));
    m_colors.insert("button_text",     QColor("#111827"));
    m_colors.insert("tooltip_base",    QColor("#F0F2F5"));
    m_colors.insert("tooltip_text",    QColor("#111827"));
    m_colors.insert("link_visited",    QColor("#2563EB"));
    m_colors.insert("alternate_base",  QColor("#F4F6F8"));
    m_colors.insert("placeholder_text",QColor("#9CA3AF"));
    m_colors.insert("highlighted_text", QColor("#FFFFFF"));
    m_colors.insert("console_base",    QColor("#FFFFFF"));
    m_colors.insert("console_border",  QColor("#D1D5DB"));
    m_colors.insert("console_text",    QColor("#111827"));
    m_colors.insert("console_highlight", QColor("#DBEAFE"));
    m_colors.insert("alarm",           QColor("#DC2626"));
    m_colors.insert("widget_base",     QColor("#F4F6F8"));
    m_colors.insert("widget_border",   QColor("#D1D5DB"));
}

bool ThemeManager::isDark() const
{
    return m_isDark;
}

void ThemeManager::toggleTheme()
{
    setDarkMode(!m_isDark);
}

void ThemeManager::setDarkMode(bool dark)
{
    if (m_isDark == dark)
        return;

    m_isDark = dark;
    QSettings settings("ProtoTools", "ProtoTools");
    settings.setValue("theme/darkMode", m_isDark);
    applyTheme();
}

const QVariantMap &ThemeManager::colors() const
{
    return m_colors;
}

const QPalette &ThemeManager::palette() const
{
    return m_palette;
}

QColor ThemeManager::getColor(const QString &name) const
{
    if (m_colors.contains(name))
        return m_colors.value(name).value<QColor>();
    return QColor(255, 0, 255); // magenta = missing color
}

} // namespace Misc
