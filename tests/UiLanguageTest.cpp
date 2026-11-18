#include "UiLanguage.h"
#include <QCoreApplication>
#include <QTranslator>
#include <QtTest>

class UiLanguageTest : public QObject
{
    Q_OBJECT
private slots:
    void resolve_data()
    {
        QTest::addColumn<QString>("preference");
        QTest::addColumn<QString>("locale");
        QTest::addColumn<QString>("expected");
        QTest::newRow("explicit Chinese") << "zh_CN" << "en_US" << "zh_CN";
        QTest::newRow("Chinese alias") << "zh" << "en_US" << "zh_CN";
        QTest::newRow("mainland system") << "system" << "zh_CN" << "zh_CN";
        QTest::newRow("Singapore system") << "system" << "zh_SG" << "zh_CN";
        QTest::newRow("Chinese system") << "system" << "zh_TW" << "zh_CN";
        QTest::newRow("explicit English") << "en" << "zh_CN" << "en";
        QTest::newRow("existing German") << "system" << "de_DE" << "de";
        QTest::newRow("existing Polish") << "pl" << "zh_CN" << "pl";
        QTest::newRow("unsupported system") << "system" << "ja_JP" << "en";
        QTest::newRow("C locale") << "system" << "C" << "en";
    }
    void resolve()
    {
        QFETCH(QString, preference);
        QFETCH(QString, locale);
        QFETCH(QString, expected);
        QCOMPARE(UiLanguage::resolve(preference, QLocale(locale)), expected);
    }
    void compiledChineseCatalog()
    {
        QTranslator translator;
        QVERIFY(translator.load(QStringLiteral(":/i18n/unisic_zh_CN.qm")));
        QCOMPARE(translator.translate("CapturePage", "Full screen"), QStringLiteral("全屏"));
        QCOMPARE(translator.translate("SettingsPage", "Language"), QStringLiteral("语言"));
        QCOMPARE(translator.translate("EditorWindow", "Undo"), QStringLiteral("撤销"));
        QCOMPARE(translator.translate("Main", "Settings"), QStringLiteral("设置"));
        // The XWayland scroll child receives these translated labels from
        // its parent, instead of maintaining a second language preference.
        QCOMPARE(translator.translate("ScrollCaptureOverlay", "Finish"), QStringLiteral("完成"));
        QCOMPARE(translator.translate("ScrollCaptureOverlay", "Cancel"), QStringLiteral("取消"));
        QVERIFY(QCoreApplication::installTranslator(&translator));
        QCOMPARE(QCoreApplication::translate("AppContext", "Added %n shortcut(s) to %1", nullptr, 3)
                     .arg(QStringLiteral("GNOME")), QStringLiteral("已向 GNOME 添加 3 个快捷键"));
        QCoreApplication::removeTranslator(&translator);
    }
};
QTEST_GUILESS_MAIN(UiLanguageTest)
#include "UiLanguageTest.moc"
