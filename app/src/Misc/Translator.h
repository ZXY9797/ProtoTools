#pragma once

#include <QObject>
#include <QLocale>

namespace Misc {

/**
 * @brief 国际化支持 — 移植自 SerialStudio Translator
 *
 * 当前仅支持英文，后续可扩展 .qm 翻译文件加载。
 */
class Translator : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString welcomeConsoleText READ welcomeConsoleText NOTIFY languageChanged)

signals:
    void languageChanged();

private:
    explicit Translator();
    Translator(Translator&&) = delete;
    Translator(const Translator&) = delete;
    Translator& operator=(Translator&&) = delete;
    Translator& operator=(const Translator&) = delete;

public:
    static Translator &instance();

    enum Language { English, Chinese };
    Q_ENUM(Language);

    QString welcomeConsoleText() const;

private:
    Language m_language = English;
};

} // namespace Misc
