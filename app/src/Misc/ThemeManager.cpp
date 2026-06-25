#include "ThemeManager.h"

namespace Misc {

ThemeManager::ThemeManager()
{
    initDarkTheme();
}

ThemeManager &ThemeManager::instance()
{
    static ThemeManager singleton;
    return singleton;
}

void ThemeManager::initDarkTheme()
{
    // Dark theme colors (aligned with SerialStudio "Fluent Dark" palette)
    m_colors.insert("mid",             QColor("#323233"));
    m_colors.insert("dark",            QColor("#1E1E1E"));
    m_colors.insert("text",            QColor("#CCCCCC"));
    m_colors.insert("base",            QColor("#1E1E1E"));
    m_colors.insert("link",            QColor("#007ACC"));
    m_colors.insert("light",           QColor("#505050"));
    m_colors.insert("window",          QColor("#2D2D2D"));
    m_colors.insert("shadow",          QColor("#000000"));
    m_colors.insert("accent",          QColor("#007ACC"));
    m_colors.insert("button",          QColor("#2D2D2D"));
    m_colors.insert("midlight",        QColor("#404040"));
    m_colors.insert("highlight",       QColor("#264F78"));
    m_colors.insert("window_text",     QColor("#CCCCCC"));
    m_colors.insert("bright_text",     QColor("#FFFFFF"));
    m_colors.insert("button_text",     QColor("#CCCCCC"));
    m_colors.insert("tooltip_base",    QColor("#2D2D2D"));
    m_colors.insert("tooltip_text",    QColor("#CCCCCC"));
    m_colors.insert("link_visited",    QColor("#007ACC"));
    m_colors.insert("alternate_base",  QColor("#252526"));
    m_colors.insert("placeholder_text",QColor("#555555"));
    m_colors.insert("highlighted_text", QColor("#FFFFFF"));
    m_colors.insert("console_base",    QColor("#1E1E1E"));
    m_colors.insert("console_border",  QColor("#3C3C3C"));
    m_colors.insert("console_text",    QColor("#CCCCCC"));
    m_colors.insert("console_highlight", QColor("#264F78"));
    m_colors.insert("alarm",           QColor("#F44747"));
    m_colors.insert("widget_base",     QColor("#252526"));
    m_colors.insert("widget_border",   QColor("#3C3C3C"));

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
