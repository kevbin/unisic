#pragma once
#include <QList>
#include <QRect>
#include <QString>
#include <cmath>

namespace ScrollOverlayGeometry {
struct Screen {
    QString name;
    QRect logical;
    QSize physical;
};

// Mutter's XWayland outputs can have synthetic names and a different scale.
// Never silently pick the primary when several outputs could be the target.
inline int screenIndex(const QList<Screen> &outputs, const Screen &wanted)
{
    for (int i = 0; i < outputs.size(); ++i)
        if (outputs[i].name == wanted.name)
            return i;
    for (int i = 0; i < outputs.size(); ++i)
        if (outputs[i].logical == wanted.logical)
            return i;
    int match = -1;
    for (int i = 0; i < outputs.size(); ++i) {
        if (outputs[i].logical.size() != wanted.physical)
            continue;
        if (match >= 0)
            return -1;
        match = i;
    }
    return match >= 0 ? match : outputs.size() == 1 ? 0 : -1;
}

// A screen-local logical rectangle to absolute X pixels, rounded outwards.
inline QRect mapRect(const QRect &local, const QSize &logicalSize, const QRect &output)
{
    if (logicalSize.isEmpty() || output.isEmpty())
        return {};
    const double sx = double(output.width()) / logicalSize.width();
    const double sy = double(output.height()) / logicalSize.height();
    const int left = int(std::floor(local.x() * sx));
    const int top = int(std::floor(local.y() * sy));
    const int right = int(std::ceil((local.x() + local.width()) * sx));
    const int bottom = int(std::ceil((local.y() + local.height()) * sy));
    return QRect(output.x() + left, output.y() + top, right - left, bottom - top);
}
}
