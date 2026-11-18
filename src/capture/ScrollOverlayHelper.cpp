#include "ScrollOverlayHelper.h"
#include "ScrollOverlayGeometry.h"

#include <QApplication>
#include <QColor>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QShortcut>
#include <QSocketNotifier>
#include <QWidget>
#include <array>
#include <cstdio>
#include <memory>
#include <unistd.h>

namespace {
const Qt::WindowFlags flags = Qt::Tool | Qt::FramelessWindowHint
                             | Qt::BypassWindowManagerHint | Qt::WindowStaysOnTopHint;

QRect rect(const QJsonValue &v)
{
    const auto a = v.toArray();
    return a.size() == 4 ? QRect(a[0].toInt(), a[1].toInt(), a[2].toInt(), a[3].toInt()) : QRect();
}

void command(const char *text)
{
    std::fputs(text, stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
}

// Four narrow windows, rather than a monitor-sized transparent surface. The
// latter also intercepts Mutter's drag-and-drop even with an empty input shape.
class Overlay
{
public:
    Overlay()
        : badge(nullptr, flags), preview(nullptr, flags | Qt::WindowTransparentForInput)
    {
        badge.setWindowTitle(QStringLiteral("Unisic"));
        preview.setWindowTitle(QStringLiteral("Unisic"));
        badge.setAttribute(Qt::WA_ShowWithoutActivating);
        auto *row = new QHBoxLayout(&badge);
        row->setContentsMargins(10, 4, 10, 4);
        row->addWidget(&label);
        row->addWidget(&finish);
        row->addWidget(&cancel);
        finish.setDefault(true);
        QObject::connect(&finish, &QPushButton::clicked, [] { command("finish"); });
        QObject::connect(&cancel, &QPushButton::clicked, [] { command("cancel"); });
        auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), &badge);
        QObject::connect(escape, &QShortcut::activated, [] { command("cancel"); });
        for (auto &bar : bars)
            bar = std::make_unique<QWidget>(nullptr, flags | Qt::WindowTransparentForInput
                                                       | Qt::WindowDoesNotAcceptFocus);
    }

    bool update(const QJsonObject &state)
    {
        using namespace ScrollOverlayGeometry;
        const QRect logical = rect(state.value(QStringLiteral("screen")));
        const QRect region = rect(state.value(QStringLiteral("region")));
        const auto physical = state.value(QStringLiteral("physical")).toArray();
        if (logical.isEmpty() || region.isEmpty() || physical.size() != 2)
            return false;
        const auto screens = QGuiApplication::screens();
        QList<Screen> outputs;
        for (QScreen *s : screens)
            outputs.append({s->name(), s->geometry(), {}});
        const int idx = screenIndex(outputs, {state.value(QStringLiteral("name")).toString(), logical,
                                              QSize(physical[0].toInt(), physical[1].toInt())});
        if (idx < 0)
            return false;
        const QRect screen = screens[idx]->geometry();
        const QRect capture = mapRect(region, logical.size(), screen);
        const double sy = double(screen.height()) / logical.height();
        const double sx = double(screen.width()) / logical.width();
        const auto px = [sy](int n) { return qMax(1, qRound(n * sy)); };
        const auto color = [&state](const char *key) {
            const QColor value(state.value(QLatin1String(key)).toString());
            return value.isValid() ? value.name(QColor::HexRgb) : QString();
        };
        const QString accent = color("accent"), bg = color("background"), fg = color("foreground");
        if (accent.isEmpty() || bg.isEmpty() || fg.isEmpty())
            return false;
        badge.setStyleSheet(QStringLiteral("QWidget { background: %1; color: %2; font-size: %3px; }"
                                          "QPushButton { border: 1px solid %4; border-radius: 4px; padding: 3px 8px; }"
                                          "QPushButton:focus { border: 2px solid %4; }")
                                .arg(bg, fg).arg(px(13)).arg(accent));
        label.setText(state.value(QStringLiteral("label")).toString());
        label.setAccessibleName(label.text());
        finish.setText(state.value(QStringLiteral("finish")).toString());
        cancel.setText(state.value(QStringLiteral("cancel")).toString());
        badge.adjustSize();
        const int bh = px(38);
        const int bw = qMin(badge.sizeHint().width(), screen.width());
        const int covered = state.value(QStringLiteral("coveredRows")).toInt();
        const int by = covered == 0 && region.y() >= 48
                           ? capture.top() - bh - px(10)
                           : screen.top() + qRound((region.y() + region.height() - covered + 10) * sy);
        badge.setGeometry(qBound(screen.left(), capture.left(), screen.right() - bw + 1), by, bw, bh);

        const int border = qMax(1, qRound(3 * qMax(sx, sy)));
        // Add one X pixel of slack: no rounding may put a frame pixel in the crop.
        const QRect hole = capture.adjusted(-1, -1, 1, 1);
        const std::array<QRect, 4> sides = {
            QRect(hole.left() - border, hole.top() - border, hole.width() + 2 * border, border),
            QRect(hole.left() - border, hole.bottom() + 1, hole.width() + 2 * border, border),
            QRect(hole.left() - border, hole.top(), border, hole.height()),
            QRect(hole.right() + 1, hole.top(), border, hole.height())};
        const bool visible = state.value(QStringLiteral("visible")).toBool();
        for (size_t i = 0; i < bars.size(); ++i) {
            bars[i]->setStyleSheet(QStringLiteral("background: %1;").arg(accent));
            bars[i]->setGeometry(sides[i]);
            bars[i]->setVisible(visible);
        }
        badge.setVisible(visible);

        if (state.contains(QStringLiteral("preview"))) {
            QPixmap image;
            image.loadFromData(QByteArray::fromBase64(state.value(QStringLiteral("preview")).toString().toLatin1()), "PNG");
            preview.setPixmap(image);
        }
        const int pw = qMax(1, qRound(120 * sx));
        const bool right = capture.right() + px(16) + pw < screen.right();
        const bool left = capture.left() - px(16) - pw >= screen.left();
        const bool showPreview = visible && state.value(QStringLiteral("frameCount")).toInt() > 1
                                 && (right || left) && !preview.pixmap().isNull();
        if (showPreview) {
            const QPixmap image = preview.pixmap().scaled(pw, px(300), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            preview.setFixedSize(image.size());
            preview.setScaledContents(true);
            preview.move(right ? capture.right() + px(12) : capture.left() - pw - px(12),
                         qBound(screen.top(), capture.top(), screen.bottom() - image.height() + 1));
        }
        preview.setVisible(showPreview);
        return true;
    }

private:
    QWidget badge;
    QLabel label;
    QPushButton finish, cancel;
    QLabel preview;
    std::array<std::unique_ptr<QWidget>, 4> bars;
};
}

int runScrollOverlayHelper(int argc, char *argv[], const char *platform)
{
    // Select xcb BEFORE QApplication. No capture API is ever used in this child.
    qputenv("QT_QPA_PLATFORM", platform);
    qputenv("QT_ENABLE_HIGHDPI_SCALING", "0");
    qunsetenv("QT_SCALE_FACTOR");
    qunsetenv("QT_SCREEN_SCALE_FACTORS");
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    Overlay overlay;
    QByteArray pending;
    QSocketNotifier stdinWatch(0, QSocketNotifier::Read);
    QObject::connect(&stdinWatch, &QSocketNotifier::activated, &app, [&] {
        char bytes[8192];
        const auto n = ::read(0, bytes, sizeof bytes);
        if (n <= 0) {
            app.quit();
            return;
        }
        pending.append(bytes, int(n));
        int newline;
        while ((newline = pending.indexOf('\n')) >= 0) {
            const QByteArray line = pending.left(newline);
            pending.remove(0, newline + 1);
            const auto doc = QJsonDocument::fromJson(line);
            if (!doc.isObject() || !overlay.update(doc.object())) {
                qWarning("Scroll overlay helper: invalid state or ambiguous monitor mapping");
                app.exit(2);
                return;
            }
            command("state-applied");
        }
        if (pending.size() > 1024 * 1024)
            app.exit(2);
    });
    command("ready");
    return app.exec();
}
