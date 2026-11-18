#include "ScrollOverlayTheme.h"
#include <QtTest>
#include <qqml.h>

class ScrollOverlayThemeTest : public QObject
{
    Q_OBJECT
private slots:
    void kitThemeIsUsed()
    {
        // Model the public QML-module contract, including the namespace the
        // shared design system actually registers. A lookup in Unisic fails.
        qmlRegisterSingletonType<QObject>("Unisic.Kit", 1, 0, "Theme",
            [](QQmlEngine *, QJSEngine *) -> QObject * {
                auto *theme = new QObject;
                theme->setProperty("accent", QColor(QStringLiteral("#123456")));
                theme->setProperty("primary", QColor(QStringLiteral("#234567")));
                theme->setProperty("textPrimary", QColor(QStringLiteral("#345678")));
                return theme;
            });
        QQmlEngine engine;
        const auto colors = ScrollOverlayTheme::colors(&engine);
        QCOMPARE(colors.value(QStringLiteral("accent")).toString(), QStringLiteral("#123456"));
        QCOMPARE(colors.value(QStringLiteral("background")).toString(), QStringLiteral("#234567"));
        QCOMPARE(colors.value(QStringLiteral("foreground")).toString(), QStringLiteral("#345678"));
    }
    void controlsStillGetColorsWithoutAnEngine()
    {
        const auto colors = ScrollOverlayTheme::colors(nullptr);
        for (const auto &value : colors)
            QVERIFY(QColor(value.toString()).isValid());
        QCOMPARE(colors.size(), 3);
    }
};
QTEST_GUILESS_MAIN(ScrollOverlayThemeTest)
#include "ScrollOverlayThemeTest.moc"
