#include "MainWindow.h"
#include "AdminDialog.h"
#include "AppIcon.h"
#include "SiteRegistry.h"
#include "Theme.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QGuiApplication>
#include <QLineEdit>
#include <QPalette>
#include <QPushButton>
#include <QTimer>
#include <QWidget>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setFont(Theme::sans(12));
    // Fusion honours QPalette on every platform, so the window shell and any
    // unstyled widget follow the dark theme instead of the system palette.
    QApplication::setStyle(QStringLiteral("Fusion"));
    QPalette pal;
    pal.setColor(QPalette::Window, Theme::Bg);
    pal.setColor(QPalette::WindowText, Theme::Text);
    pal.setColor(QPalette::Base, Theme::BgDeep);
    pal.setColor(QPalette::AlternateBase, Theme::PanelBg);
    pal.setColor(QPalette::Text, Theme::TextSoft);
    pal.setColor(QPalette::Button, Theme::PanelBgHi);
    pal.setColor(QPalette::ButtonText, Theme::Text);
    pal.setColor(QPalette::ToolTipBase, Theme::PanelBgHi);
    pal.setColor(QPalette::ToolTipText, Theme::Text);
    pal.setColor(QPalette::PlaceholderText, Theme::TextFaint);
    pal.setColor(QPalette::Highlight, Theme::BorderHi);
    pal.setColor(QPalette::HighlightedText, Theme::Text);
    pal.setColor(QPalette::Mid, Theme::Border);
    pal.setColor(QPalette::Dark, Theme::BgDeep);
    pal.setColor(QPalette::Light, Theme::BorderHi);
    pal.setColor(QPalette::Disabled, QPalette::Text, Theme::TextFaint);
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, Theme::TextFaint);
    pal.setColor(QPalette::Disabled, QPalette::WindowText, Theme::TextFaint);
    QApplication::setPalette(pal);
    QApplication::setApplicationName(QStringLiteral("DashboardTemplate"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QApplication::setWindowIcon(AppIcon::icon());
    // matches dashboardtemplate.desktop on Wayland docks (app id → launcher entry)
    QGuiApplication::setDesktopFileName(QStringLiteral("dashboardtemplate"));

    QCommandLineParser cli;
    cli.setApplicationDescription(QStringLiteral("DashboardTemplate — multi-site telemetry operations console"));
    cli.addHelpOption();
    cli.addVersionOption();
    QCommandLineOption grabOpt(QStringLiteral("grab"),
                               QStringLiteral("Render one frame to FILE after the first scan, then exit."),
                               QStringLiteral("FILE"));
    QCommandLineOption grabAdminOpt(QStringLiteral("grab-admin"),
                               QStringLiteral("Open the site-admin dialog over the console, drive an add + save "
                                              "through its widgets, render PREFIX-admin.png / -admin-added.png / "
                                              "-main.png, then exit."),
                               QStringLiteral("PREFIX"));
    QCommandLineOption offlineOpt(QStringLiteral("offline"),
                                  QStringLiteral("Start with feed polling disabled (link-down drill)."));
    QCommandLineOption iconOpt(QStringLiteral("icon"),
                               QStringLiteral("Render the application icon (256 px) to FILE, then exit."),
                               QStringLiteral("FILE"));
    cli.addOption(grabOpt);
    cli.addOption(grabAdminOpt);
    cli.addOption(offlineOpt);
    cli.addOption(iconOpt);
    cli.process(app);

    if (cli.isSet(iconOpt))
        return AppIcon::pixmap(256).save(cli.value(iconOpt)) ? 0 : 1;

    MainWindow win(cli.isSet(offlineOpt));
    win.show();

    const QString grabPath = cli.value(grabOpt);
    if (!grabPath.isEmpty()) {
        // Render pass: wait for the first full scan (or 30 s), let two blink
        // frames elapse, then dump the window to the requested file.
        QTimer::singleShot(30000, &win, [&win, grabPath] {
            const QPixmap px = win.grab();
            px.save(grabPath);
            QApplication::exit(px.isNull() ? 1 : 0);
        });
        QTimer *poll = new QTimer(&win);
        QObject::connect(poll, &QTimer::timeout, &win, [&win, poll, grabPath] {
            if (win.firstScanDone()) {
                QTimer::singleShot(1600, &win, [&win, grabPath] {
                    const QPixmap px = win.grab();
                    px.save(grabPath);
                    QApplication::exit(px.isNull() ? 1 : 0);
                });
                poll->stop();
            }
        });
        poll->start(200);
    }

    const QString adminPrefix = cli.value(grabAdminOpt);
    if (!adminPrefix.isEmpty()) {
        // Exercise the site-admin dialog end to end: render it as opened,
        // drive a real add + save through its widgets (same slots the
        // buttons use), then render the reconfigured console.
        auto *dlg = new AdminDialog(win.registry()->sites(), &win);
        dlg->show();
        int rc = 0;
        const auto grabTo = [&rc](QWidget *w, const QString &path) {
            const QPixmap px = w->grab();
            if (px.isNull() || !px.save(path))
                rc = 1;
        };
        QTimer::singleShot(1200, &win, [&win, dlg, adminPrefix, &grabTo, &rc] {
            grabTo(dlg, adminPrefix + QStringLiteral("-admin.png"));
            const auto set = [dlg](const char *name, const QString &v) {
                if (auto *e = dlg->findChild<QLineEdit *>(QLatin1String(name)))
                    e->setText(v);
            };
            set("nameEdit", QStringLiteral("EDGE-OSLO"));
            set("codeEdit", QStringLiteral("OSL1"));
            set("regionEdit", QStringLiteral("EU"));
            set("endpointEdit", QStringLiteral("https://example.com/api/v1/nodes/osl1"));
            if (auto *b = dlg->findChild<QPushButton *>(QStringLiteral("addButton")))
                b->click();
        });
        QTimer::singleShot(2400, &win, [&win, dlg, adminPrefix, &grabTo, &rc] {
            grabTo(dlg, adminPrefix + QStringLiteral("-admin-added.png"));
            if (auto *b = dlg->findChild<QPushButton *>(QStringLiteral("saveButton")))
                b->click();
            win.applyAdminRoster(dlg->sites());
        });
        QTimer::singleShot(3600, &win, [&win, adminPrefix, &grabTo, &rc] {
            grabTo(&win, adminPrefix + QStringLiteral("-main.png"));
            QApplication::exit(rc);
        });
    }

    return app.exec();
}
