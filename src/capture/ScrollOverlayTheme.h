#pragma once

#include <QColor>
#include <QJsonObject>
#include <QQmlEngine>

namespace ScrollOverlayTheme {
inline QJsonObject colors(QQmlEngine *engine)
{
    // Theme moved to the shared kit. The old Unisic URI returns no object;
    // that must never leave a capture running without Finish/Cancel controls.
    QObject *theme = engine ? engine->singletonInstance<QObject *>(
                                 QStringLiteral("Unisic.Kit"), QStringLiteral("Theme")) : nullptr;
    const auto color = [theme](const char *property, const QColor &fallback) {
        const QColor value = theme ? theme->property(property).value<QColor>() : QColor();
        return (value.isValid() ? value : fallback).name(QColor::HexRgb);
    };
    return {{QStringLiteral("accent"), color("accent", QColor(QStringLiteral("#C8ACD6")))},
            {QStringLiteral("background"), color("primary", QColor(QStringLiteral("#17153B")))},
            {QStringLiteral("foreground"), color("textPrimary", QColor(Qt::white))}};
}
}
