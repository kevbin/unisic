#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QtTest>

class ScrollOverlayProcessTest : public QObject
{
    Q_OBJECT
    static bool waitForLine(QProcess &child, const QByteArray &wanted)
    {
        QElapsedTimer timer;
        timer.start();
        QByteArray received;
        while (timer.elapsed() < 5000) {
            received += child.readAllStandardOutput();
            if (received.split('\n').contains(wanted))
                return true;
            child.waitForReadyRead(100);
        }
        return false;
    }
    static QString probe()
    {
        return QCoreApplication::applicationDirPath() + QStringLiteral("/unisic_scroll_overlay_probe");
    }
private slots:
    void fragmentedStateAndParentExit()
    {
        QProcess child;
        child.start(probe(), {});
        QVERIFY(child.waitForStarted());
        QVERIFY2(waitForLine(child, "ready"), child.readAllStandardError().constData());
        QJsonObject state{
            {QStringLiteral("name"), QStringLiteral("DP-1")},
            {QStringLiteral("screen"), QJsonArray{0, 0, 3840, 2160}},
            {QStringLiteral("physical"), QJsonArray{3840, 2160}},
            {QStringLiteral("region"), QJsonArray{100, 100, 1000, 1200}},
            {QStringLiteral("coveredRows"), 0},
            {QStringLiteral("visible"), true},
            {QStringLiteral("frameCount"), 1},
            {QStringLiteral("label"), QStringLiteral("Scroll down to capture...")},
            {QStringLiteral("finish"), QStringLiteral("Finish")},
            {QStringLiteral("cancel"), QStringLiteral("Cancel")},
            {QStringLiteral("accent"), QStringLiteral("#c8acd6")},
            {QStringLiteral("background"), QStringLiteral("#17153b")},
            {QStringLiteral("foreground"), QStringLiteral("#ffffff")}};
        const QByteArray line = QJsonDocument(state).toJson(QJsonDocument::Compact) + '\n';
        child.write(line.left(37));
        QVERIFY(child.waitForBytesWritten());
        QVERIFY(!child.waitForFinished(100));
        child.write(line.mid(37));
        QVERIFY2(waitForLine(child, "state-applied"), child.readAllStandardError().constData());
        // Full-height finish hides all six windows before collecting bottom rows.
        state[QStringLiteral("region")] = QJsonArray{0, 0, 3840, 2160};
        state[QStringLiteral("coveredRows")] = 58;
        state[QStringLiteral("visible")] = false;
        child.write(QJsonDocument(state).toJson(QJsonDocument::Compact) + '\n');
        QVERIFY(waitForLine(child, "state-applied"));
        child.closeWriteChannel();
        QVERIFY(child.waitForFinished(5000));
        QCOMPARE(child.exitStatus(), QProcess::NormalExit);
        QCOMPARE(child.exitCode(), 0);
    }
    void invalidStateExitsInsteadOfLeavingAnOverlay()
    {
        QProcess child;
        child.start(probe(), {});
        QVERIFY(child.waitForStarted());
        QVERIFY(waitForLine(child, "ready"));
        child.write("{}\n");
        QVERIFY(child.waitForFinished(5000));
        QCOMPARE(child.exitCode(), 2);
    }
};
QTEST_GUILESS_MAIN(ScrollOverlayProcessTest)
#include "ScrollOverlayProcessTest.moc"
