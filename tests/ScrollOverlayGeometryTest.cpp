#include "ScrollOverlayGeometry.h"
#include <QtTest>

using namespace ScrollOverlayGeometry;

class ScrollOverlayGeometryTest : public QObject
{
    Q_OBJECT
private slots:
    void externalAocWithSyntheticName()
    {
        const QList<Screen> outputs{{QStringLiteral("XWAYLAND0"), QRect(3840, 0, 2160, 1440), {}},
                                    {QStringLiteral("XWAYLAND1"), QRect(0, 0, 3840, 2160), {}}};
        QCOMPARE(screenIndex(outputs, {QStringLiteral("DP-1"), QRect(0, 0, 3840, 2160), QSize(3840, 2160)}), 1);
    }
    void fractionalScaleMapsOutward()
    {
        QCOMPARE(mapRect(QRect(101, 51, 801, 601), QSize(2560, 1440), QRect(-3840, 120, 3840, 2160)),
                 QRect(-3689, 196, 1202, 902));
    }
    void laptopScreenKeepsItsOffset()
    {
        QCOMPARE(mapRect(QRect(100, 200, 500, 400), QSize(2160, 1440), QRect(3840, 0, 2160, 1440)),
                 QRect(3940, 200, 500, 400));
    }
    void ambiguousOutputsAreRefused()
    {
        const QList<Screen> outputs{{QStringLiteral("XWAYLAND0"), QRect(0, 0, 3840, 2160), {}},
                                    {QStringLiteral("XWAYLAND1"), QRect(3840, 0, 3840, 2160), {}}};
        QCOMPARE(screenIndex(outputs, {QStringLiteral("DP-1"), QRect(0, 0, 2560, 1440), QSize(3840, 2160)}), -1);
    }
    void fullHeightRegionKeepsEveryRow()
    {
        QCOMPARE(mapRect(QRect(0, 0, 2560, 1440), QSize(2560, 1440), QRect(0, 0, 3840, 2160)),
                 QRect(0, 0, 3840, 2160));
    }
};
QTEST_GUILESS_MAIN(ScrollOverlayGeometryTest)
#include "ScrollOverlayGeometryTest.moc"
