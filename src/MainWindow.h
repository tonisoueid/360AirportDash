#pragma once

#include "SiteData.h"
#include "FeedService.h"

#include <QHash>
#include <QMainWindow>
#include <QVector>

class EventLog;
class SiteCard;
class TrendChart;
class Panel;
class ExpandButton;
class QLabel;
class QPushButton;
class QGridLayout;
class QScrollArea;
class QSplitter;
class QWidget;
class SiteRegistry;

// ----------------------------------------------------------------------------
// Main operations console:
//   header   — masthead, UTC clock, LIVE badge, scan progress, link state,
//              SITE ADMIN button (roster editor, also Ctrl+E)
//   matrix   — site matrix (watched sites, clickable)
//   trace    — analysis trace for the selected site (scope)
//   log      — event log with raise/clear watch rules
//   footer   — fleet status: live count, sync time, next-scan countdown, uptime
// ----------------------------------------------------------------------------
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(bool offline, QWidget *parent = nullptr);

    bool offline() const { return m_offline; }

    bool firstScanDone() const { return m_firstScanDone; }
    SiteRegistry *registry() const { return m_registry; }

    // Commit an edited roster (from the admin dialog): validate, persist and
    // hot-swap the polling list + UI. Safe to call with an unchanged list.
    void applyAdminRoster(const QVector<Site> &list);

protected:
    void keyPressEvent(QKeyEvent *e) override;
    bool eventFilter(QObject *obj, QEvent *ev) override;

private slots:
    void onSecondTick();
    void onHalfSecond();
    void onSiteUpdated(int idx);
    void onScanProgress(int done, int total);
    void onLinkStatus(bool online);
    void onCardClicked(int idx);
    void openAdmin();
    void rebuildSiteGrid();

private:
    // Which body panel (if any) is blown up to fill the whole console body.
    // None = the normal three-panel layout.
    enum class SectionFocus { None, Grid, Trace, Log };

    QWidget *buildHeader();
    QWidget *buildStatusBar();
    void applyLinkChip(bool online);
    void refreshStatusText();
    void runAlertRules(int idx);
    void relayoutSiteCards();
    void reflowSiteGrid();
    int siteColumns() const;
    void setSectionFocus(SectionFocus f);

    FeedService *m_service = nullptr;
    SiteRegistry *m_registry = nullptr;
    QGridLayout *m_grid = nullptr;
    QWidget *m_gridHost = nullptr;
    QScrollArea *m_gridScroll = nullptr;
    QSplitter *m_body = nullptr;
    QWidget *m_rightCol = nullptr;
    ExpandButton *m_gridBtn = nullptr;
    ExpandButton *m_traceBtn = nullptr;
    ExpandButton *m_logBtn = nullptr;
    SectionFocus m_focus = SectionFocus::None;
    int m_gridCols = 2;
    QVector<SiteCard *> m_cards;
    QVector<QHash<QString, Alert::Sev>> m_activeAlerts;   // per site: rule key -> last severity
    QVector<bool> m_announcedOnline;
    EventLog *m_log = nullptr;
    TrendChart *m_scope = nullptr;
    Panel *m_scopePanel = nullptr;
    Panel *m_gridPanel = nullptr;
    Panel *m_logPanel = nullptr;
    int m_eventCount = 0;
    QLabel *m_clockLabel = nullptr;
    QLabel *m_scanLabel = nullptr;
    QLabel *m_linkLabel = nullptr;
    QLabel *m_liveLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_adminBtn = nullptr;
    int m_selected = -1;
    int m_scanDoneCount = 0;
    bool m_firstScanDone = false;
    bool m_offline = false;
    bool m_linkOnline = false;
    bool m_blink = true;
    qint64 m_lastCardMinute = -1;
    QDateTime m_bootUtc;
    QDateTime m_nextScanUtc;
};
