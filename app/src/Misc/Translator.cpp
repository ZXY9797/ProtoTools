#include "Translator.h"

namespace Misc {

Translator::Translator() {}

Translator &Translator::instance()
{
    static Translator singleton;
    return singleton;
}

QString Translator::welcomeConsoleText() const
{
    return QStringLiteral(
        "KPtools Terminal\n"
        "================\n"
        "Connect a device to begin debugging.\n"
    );
}

} // namespace Misc
