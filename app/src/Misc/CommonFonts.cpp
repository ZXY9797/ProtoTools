#include "CommonFonts.h"
#include <QApplication>
#include <QFontDatabase>

Misc::CommonFonts::CommonFonts()
    : m_widgetFontScale(1.0)
{
    // 系统 UI 字体
    m_uiFont = QApplication::font();

    // 粗体 UI 字体
    m_boldUiFont = m_uiFont;
    m_boldUiFont.setBold(true);

    // 等宽字体
    QString monoFamily;
#ifdef Q_OS_MAC
    monoFamily = QStringLiteral("Menlo");
#elif defined(Q_OS_WIN)
    monoFamily = QStringLiteral("Consolas");
#else
    monoFamily = QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
#endif

    m_monoFont = QFont(monoFamily);
    m_monoFont.setFixedPitch(true);
    m_monoFont.setStyleHint(QFont::Monospace);
    m_monoFont.setPointSizeF(m_uiFont.pointSizeF());

    // 读取用户设置
    m_widgetFontFamily = m_settings.value("CommonFonts/FontFamily", m_monoFont.family()).toString();
    m_widgetFontScale  = m_settings.value("CommonFonts/FontScale", kScaleNormal).toDouble();
    m_widgetFontScale  = qBound(0.5, m_widgetFontScale, 3.0);

    // 验证保存的字体是否存在
    if (!availableFonts().contains(m_widgetFontFamily))
        m_widgetFontFamily = m_monoFont.family();
}

Misc::CommonFonts &Misc::CommonFonts::instance()
{
    static CommonFonts singleton;
    return singleton;
}

const QFont &Misc::CommonFonts::uiFont() const
{
    return m_uiFont;
}

const QFont &Misc::CommonFonts::monoFont() const
{
    return m_monoFont;
}

const QFont &Misc::CommonFonts::boldUiFont() const
{
    return m_boldUiFont;
}

double Misc::CommonFonts::widgetFontScale() const
{
    return m_widgetFontScale;
}

QString Misc::CommonFonts::widgetFontFamily() const
{
    return m_widgetFontFamily;
}

QFont Misc::CommonFonts::customUiFont(double fraction, bool bold)
{
    QFont font = bold ? m_boldUiFont : m_uiFont;
    font.setPointSizeF(m_uiFont.pointSizeF() * qMax(0.1, fraction));
    return font;
}

QFont Misc::CommonFonts::customMonoFont(double fraction, bool bold)
{
    QFont font = m_monoFont;
    font.setPointSizeF(m_monoFont.pointSizeF() * qMax(0.1, fraction));
    if (bold)
        font.setWeight(QFont::Medium);
    return font;
}

QFont Misc::CommonFonts::widgetFont(double fraction, bool bold) const
{
    QFont font(m_widgetFontFamily);
    font.setPointSizeF(m_uiFont.pointSizeF() * qMax(0.1, fraction) * m_widgetFontScale);
    if (bold)
        font.setWeight(QFont::Medium);
    return font;
}

QStringList Misc::CommonFonts::availableFonts() const
{
    QStringList families;
    for (const QString &family : QFontDatabase::families()) {
        if (family.startsWith('.'))
            continue;
        if (QFontDatabase::isPrivateFamily(family))
            continue;
        families.append(family);
    }
    families.sort(Qt::CaseInsensitive);

    // 等宽字体置顶
    const QString mono = m_monoFont.family();
    families.removeAll(mono);
    families.prepend(mono);

    return families;
}

void Misc::CommonFonts::setWidgetFontScale(double scale)
{
    const double clamped = qBound(0.5, scale, 3.0);
    if (qFuzzyCompare(m_widgetFontScale, clamped))
        return;

    m_widgetFontScale = clamped;
    m_settings.setValue("CommonFonts/FontScale", m_widgetFontScale);
    emit fontsChanged();
}

void Misc::CommonFonts::setWidgetFontFamily(const QString &family)
{
    if (m_widgetFontFamily == family)
        return;

    m_widgetFontFamily = family;
    m_settings.setValue("CommonFonts/FontFamily", m_widgetFontFamily);
    emit fontsChanged();
}
