#pragma once

#include <QLocale>
#include <QStringList>

namespace UiLanguage {
inline const QStringList &supported()
{
    static const QStringList codes{QStringLiteral("en"), QStringLiteral("pl"),
                                   QStringLiteral("es"), QStringLiteral("it"),
                                   QStringLiteral("fr"), QStringLiteral("ru"),
                                   QStringLiteral("de"), QStringLiteral("zh_CN")};
    return codes;
}

// The main UI and the notification child must resolve the same catalog.
// Chinese catalogs carry a territory suffix; taking only the first two
// characters of the system locale would silently fall back to English.
inline QString resolve(const QString &preference, const QLocale &system = QLocale::system())
{
    if (supported().contains(preference))
        return preference;
    if (preference == QLatin1String("zh"))
        return QStringLiteral("zh_CN");
    const QString code = system.language() == QLocale::Chinese
                             ? QStringLiteral("zh_CN") : system.name().left(2);
    return supported().contains(code) ? code : QStringLiteral("en");
}
}
