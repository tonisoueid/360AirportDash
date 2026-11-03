#include "MainWindow.h"
#include "AdminDialog.h"
#include "AirportRegistry.h"
#include "EventLog.h"
#include "SiteCard.h"
#include "SiteRegistry.h"
#include "Theme.h"
#include "widgets/Panel.h"
#include "widgets/TrendChart.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QTimer>
#include <QVBoxLayout>

#include "widgets/BodySplitter.h"
#include "widgets/ExpandButton.h"

// Outlined pill chip for QLabel, reference-tag style.
static QString chipStyle(const QColor &c, int px)
{
    QColor bg = c; bg.setAlphaF(0.10);
    QColor bd = c; bd.setAlphaF(0.55);
    return QStringLiteral(
        "color:%1; background:%2; border:1px solid %3; border-radius:9px;"
        "padding:2px 10px; font-size:%4px; font-weight:600;")
        .arg(c.name(), bg.name(QColor::HexArgb), bd.name(QColor::HexArgb))
        .arg(px);
}

// Chip-style header button that opens the site-admin dialog.
static QString adminBtnStyle()
{
    return QStringLiteral(
        "QPushButton { color:%1; background:rgba(88,166,255,0.10);"
        " border:1px solid rgba(88,166,255,0.55); border-radius:9px; padding:4px 14px; }"
        "QPushButton:hover { background:rgba(88,166,255,0.22); }"
        "QPushButton:pressed { background:rgba(88,166,255,0.32); }")
        .arg(Theme::BlueBright.name());
}

MainWindow::MainWindow(bool offline, QWidget *parent)
    : QMainWindow(parent), m_offline(offline)
{
    setWindowTitle(QStringLiteral("360AirportDash — Operations Console"));
    // two card columns + the analysis column + collapse bars
    setMinimumSize(1330, 800);
    resize(1680, 940);

    m_bootUtc = QDateTime::currentDateTimeUtc();

    m_registry = new SiteRegistry(this);
    const SiteRegistry::LoadResult rosterLoad = m_registry->load();
    m_service = new FeedService(m_registry->sites(), this);
    connect(m_service, &FeedService::siteUpdated, this, &MainWindow::onSiteUpdated);
    connect(m_service, &FeedService::scanProgress, this, &MainWindow::onScanProgress);
    connect(m_service, &FeedService::linkStatusChanged, this, &MainWindow::onLinkStatus);
    // Pause edge: repaint every card so a badge can pick it up (painting
    // itself lands with the card badge work).
    connect(m_service, &FeedService::rateLimitPausedChanged, this, [this](bool) {
        for (int i = 0; i < m_cards.size() && i < m_service->siteCount(); ++i)
            m_cards[i]->setData(m_service->data(i));
    });
    connect(m_service, &FeedService::sitesReconfigured, this, &MainWindow::rebuildSiteGrid);
    connect(m_registry, &SiteRegistry::sitesChanged, this, [this] {
        m_service->setSites(m_registry->sites());
    });

    // Airport directory: learn ICAOs only as the feed reports them, and
    // refill after a damaged airports.json once the feed says it finished.
    m_airports = new AirportRegistry(this);
    m_airports->load();
    connect(m_service, &FeedService::airportReported, this, [this](const QString &icao) {
        m_airports->addLearned(icao);   // no-op for known ICAOs
    });
    connect(m_service, &FeedService::airportFillCompleted, this, [this] {
        m_airports->clearRefill();
    });
    if (m_airports->needsRefill() && !m_offline)
        m_service->requestAirportFill();

    // ---------------------------------------------------------------- layout
    QWidget *central = new QWidget;
    central->setObjectName(QStringLiteral("centralW"));
    central->setStyleSheet(QStringLiteral("#centralW { background: %1; }").arg(Theme::Bg.name()));
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(8, 6, 8, 6);
    root->setSpacing(6);

    root->addWidget(buildHeader());

    // site matrix (scrollable) | draggable seam | right column: trace + log
    // The seam (BodySplitter) resizes the two sides by dragging; each panel
    // also carries a caption-corner expand control that blows it up to fill
    // the whole body, folding the other panels away (setSectionFocus).
    m_body = new BodySplitter;

    m_gridPanel = new Panel(QStringLiteral("Site Matrix"));
    m_gridPanel->setObjectName(QStringLiteral("gridPanel"));
    m_gridPanel->setValue(QStringLiteral("—"), Theme::TextMuted);
    m_gridPanel->setMinimumWidth(830);   // always room for two card columns
    QScrollArea *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setFocusPolicy(Qt::NoFocus);   // arrows must reach MainWindow::keyPressEvent, not scroll the area
    scroll->viewport()->setFocusPolicy(Qt::NoFocus);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QStringLiteral(
        "QScrollArea { background: transparent; }"
        "QScrollBar:vertical { background: transparent; width: 8px; margin: 2px; }"
        "QScrollBar::handle:vertical { background: %1; border-radius: 4px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
        .arg(Theme::Border.name()));
    m_gridHost = new QWidget;
    m_gridHost->setStyleSheet(QStringLiteral("background: transparent;"));
    m_grid = new QGridLayout(m_gridHost);
    m_grid->setContentsMargins(2, 2, 6, 2);
    m_grid->setSpacing(6);
    scroll->setWidget(m_gridHost);
    m_gridScroll = scroll;
    m_gridPanel->contentLayout()->addWidget(scroll);
    m_gridHost->installEventFilter(this);
    m_body->addWidget(m_gridPanel);
    m_body->setStretchFactor(0, 11);

    m_gridBtn = new ExpandButton;
    m_gridBtn->setObjectName(QStringLiteral("gridExpandBtn"));
    m_gridPanel->setCornerWidget(m_gridBtn);
    connect(m_gridBtn, &ExpandButton::toggled, this, [this](bool on) {
        setSectionFocus(on ? SectionFocus::Grid : SectionFocus::None);
    });

    m_rightCol = new QWidget;
    m_rightCol->setObjectName(QStringLiteral("rightCol"));
    m_rightCol->setMinimumWidth(430);   // keep the trace readable at any size
    auto *rightLay = new QVBoxLayout(m_rightCol);
    rightLay->setContentsMargins(0, 0, 0, 0);
    rightLay->setSpacing(6);

    m_scopePanel = new Panel(QStringLiteral("Analysis Trace"));
    m_scopePanel->setObjectName(QStringLiteral("scopePanel"));
    m_scope = new TrendChart;
    m_scopePanel->contentLayout()->addWidget(m_scope);
    rightLay->addWidget(m_scopePanel, 5);
    m_traceBtn = new ExpandButton;
    m_traceBtn->setObjectName(QStringLiteral("traceExpandBtn"));
    m_scopePanel->setCornerWidget(m_traceBtn);
    connect(m_traceBtn, &ExpandButton::toggled, this, [this](bool on) {
        setSectionFocus(on ? SectionFocus::Trace : SectionFocus::None);
    });

    m_logPanel = new Panel(QStringLiteral("Event Log — Watch Rules Active"));
    m_logPanel->setObjectName(QStringLiteral("logPanel"));
    m_logPanel->setValue(QStringLiteral("0 EVENTS"), Theme::TextMuted);
    m_log = new EventLog;
    m_logPanel->contentLayout()->addWidget(m_log);
    rightLay->addWidget(m_logPanel, 3);
    m_logBtn = new ExpandButton;
    m_logBtn->setObjectName(QStringLiteral("logExpandBtn"));
    m_logPanel->setCornerWidget(m_logBtn);
    connect(m_logBtn, &ExpandButton::toggled, this, [this](bool on) {
        setSectionFocus(on ? SectionFocus::Log : SectionFocus::None);
    });

    m_body->addWidget(m_rightCol);
    m_body->setStretchFactor(1, 9);
    m_body->setSizes({940, 700});   // opening split keeps the 11:9 balance

    root->addWidget(m_body, 1);

    // Populate the site matrix from the current roster (and again on every roster swap).
    rebuildSiteGrid();

    root->addWidget(buildStatusBar());
    setCentralWidget(central);

    // ---------------------------------------------------------------- timers
    QTimer *sec = new QTimer(this);
    connect(sec, &QTimer::timeout, this, &MainWindow::onSecondTick);
    sec->start(1000);
    QTimer *half = new QTimer(this);
    connect(half, &QTimer::timeout, this, &MainWindow::onHalfSecond);
    half->start(500);

    m_nextScanUtc = QDateTime::currentDateTimeUtc().addSecs(FeedService::kRefreshSecs);
    onSecondTick();
    if (m_offline)
        onLinkStatus(false);
    else
        applyLinkChip(true);

    m_log->log(QStringLiteral("INFO"), QStringLiteral("360AirportDash"),
               QStringLiteral("System online — %1 sites registered, watch rules armed")
                   .arg(m_service->siteCount()), Theme::Green);
    if (rosterLoad == SiteRegistry::LoadResult::File) {
        m_log->log(QStringLiteral("INFO"), QStringLiteral("ROSTER"),
                   QStringLiteral("Watched-site roster loaded from %1")
                       .arg(SiteRegistry::configPath()));
    } else if (rosterLoad == SiteRegistry::LoadResult::Corrupt) {
        m_log->log(QStringLiteral("WARN"), QStringLiteral("ROSTER"),
                   QStringLiteral("Roster config unreadable — built-in defaults in use"),
                   Theme::Amber);
    }
    if (m_offline) {
        m_log->log(QStringLiteral("WARN"), QStringLiteral("LINK"),
                   QStringLiteral("Offline mode requested — feed polling disabled"));
    } else {
        m_service->start();
    }
}

QWidget *MainWindow::buildHeader()
{
    QWidget *h = new QWidget;
    h->setObjectName(QStringLiteral("headerBar"));
    h->setFixedHeight(62);
    h->setStyleSheet(QStringLiteral(
        "#headerBar { background: %1; border: 1px solid %2; border-radius: 10px; }"
        "#headerBar QLabel { border: none; background: transparent; }")
            .arg(Theme::PanelBg.name(), Theme::Border.name()));
    auto *lay = new QHBoxLayout(h);
    lay->setContentsMargins(16, 6, 16, 6);
    lay->setSpacing(14);

    auto mkLabel = [](const QString &text, int px, bool bold, const QColor &c) {
        QLabel *l = new QLabel(text);
        l->setFont(Theme::sans(px, bold));
        QPalette pal = l->palette();
        pal.setColor(QPalette::WindowText, c);
        l->setPalette(pal);
        return l;
    };

    QLabel *mast = mkLabel(QStringLiteral("360AirportDash"), 20, true, Theme::Text);
    QLabel *sub = mkLabel(QStringLiteral("MULTI-SITE TELEMETRY CONSOLE"), 12, false, Theme::TextMuted);
    lay->addWidget(mast);
    lay->addWidget(sub);

    m_adminBtn = new QPushButton(QStringLiteral("SITE ADMIN"));
    m_adminBtn->setCursor(Qt::PointingHandCursor);
    m_adminBtn->setFont(Theme::sans(11, true));
    m_adminBtn->setStyleSheet(adminBtnStyle());
    connect(m_adminBtn, &QPushButton::clicked, this, &MainWindow::openAdmin);
    lay->addWidget(m_adminBtn);
    lay->addStretch(1);

    m_scanLabel = mkLabel(QStringLiteral("SCAN 0/0"), 13, true, Theme::TextMuted);
    lay->addWidget(m_scanLabel);

    m_linkLabel = new QLabel(QStringLiteral("LINK —"));
    m_linkLabel->setFont(Theme::sans(11, true));
    lay->addWidget(m_linkLabel);

    m_clockLabel = mkLabel(QStringLiteral("00:00:00Z"), 19, true, Theme::Text);
    lay->addWidget(m_clockLabel);

    m_liveLabel = new QLabel(QStringLiteral("● LIVE"));
    m_liveLabel->setFont(Theme::sans(11, true));
    lay->addWidget(m_liveLabel);
    return h;
}

QWidget *MainWindow::buildStatusBar()
{
    QWidget *s = new QWidget;
    s->setObjectName(QStringLiteral("statusBar"));
    s->setFixedHeight(34);
    s->setStyleSheet(QStringLiteral(
        "#statusBar { background: %1; border: 1px solid %2; border-radius: 10px; }"
        "#statusBar QLabel { border: none; background: transparent; }")
            .arg(Theme::PanelBg.name(), Theme::Border.name()));
    auto *lay = new QHBoxLayout(s);
    lay->setContentsMargins(14, 2, 14, 2);
    m_statusLabel = new QLabel;
    m_statusLabel->setFont(Theme::sans(12));
    QPalette pal = m_statusLabel->palette();
    pal.setColor(QPalette::WindowText, Theme::TextMuted);
    m_statusLabel->setPalette(pal);
    lay->addWidget(m_statusLabel);
    lay->addStretch(1);
    QLabel *hint = new QLabel(QStringLiteral("R rescan · 1–9 / ←→↑↓ select site · Ctrl+E site admin · Ctrl+Q quit"));
    hint->setFont(Theme::sans(11));
    pal = hint->palette();
    pal.setColor(QPalette::WindowText, Theme::TextFaint);
    hint->setPalette(pal);
    lay->addWidget(hint);
    return s;
}

void MainWindow::applyLinkChip(bool online)
{
    m_linkOnline = online;
    m_linkLabel->setText(online ? QStringLiteral("LINK UP") : QStringLiteral("LINK DOWN"));
    m_linkLabel->setStyleSheet(chipStyle(online ? Theme::Green : Theme::Red, 14));
    m_liveLabel->setText(online ? QStringLiteral("● LIVE") : QStringLiteral("● STANDBY"));
    m_liveLabel->setStyleSheet(chipStyle(online ? Theme::Green : Theme::Amber, 14));
}

void MainWindow::refreshStatusText()
{
    int liveCount = 0;
    for (const SiteData &d : m_service->all())
        if (d.state == SiteData::State::Live) ++liveCount;
    const int total = m_service->siteCount();
    const QString sync = m_service->lastFullSyncUtc().isValid()
        ? m_service->lastFullSyncUtc().time().toString(QStringLiteral("hh:mm:ss")) + QLatin1Char('Z')
        : QStringLiteral("--:--:--");
    const qint64 next = QDateTime::currentDateTimeUtc().secsTo(m_nextScanUtc);
    const QString nextStr = next > 0 ? QStringLiteral("%1:%2").arg(next / 60).arg(qint64(next % 60), 2, 10, QChar('0'))
                                     : QStringLiteral("now");
    const qint64 up = m_bootUtc.secsTo(QDateTime::currentDateTimeUtc());
    m_statusLabel->setText(QStringLiteral(
        "<span style='color:%1;'>Sites</span> <span style='color:%2;'>%3/%4</span>"
        "<span style='color:%1;'> live · Sync</span> <span style='color:%2;'>%5</span>"
        "<span style='color:%1;'> · Next scan</span> <span style='color:%2;'>%6</span>"
        "<span style='color:%1;'> · Uptime %7h %8m</span>")
        .arg(Theme::TextMuted.name(), Theme::TextSoft.name())
        .arg(liveCount).arg(total).arg(sync, nextStr)
        .arg(up / 3600).arg((up % 3600) / 60, 2, 10, QChar('0')));
}

void MainWindow::onSecondTick()
{
    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
    m_clockLabel->setText(nowUtc.time().toString(QStringLiteral("hh:mm:ss")) + QLatin1Char('Z'));

    // scheduled sweep: fire when the countdown expires and no scan is running
    if (!m_offline && m_firstScanDone && nowUtc >= m_nextScanUtc
        && !m_service->scanActive()) {
        m_log->log(QStringLiteral("INFO"), QStringLiteral("360AirportDash"),
                   QStringLiteral("Scheduled scan cycle — %1 sites")
                       .arg(m_service->siteCount()));
        m_service->rescan();
    }

    // refresh card readouts (data age) on the minute — the per-second
    // repaint churned the footer counter with no real value
    const qint64 minuteStamp = nowUtc.toSecsSinceEpoch() / 60;
    if (minuteStamp != m_lastCardMinute) {
        m_lastCardMinute = minuteStamp;
        for (SiteCard *c : m_cards)
            if (c->isVisible())
                c->update();
    }
    refreshStatusText();
}

void MainWindow::onHalfSecond()
{
    // header LIVE chip pulses between bright and restful green while the
    // link is up; a link-down STANDBY chip stays a steady amber
    if (!m_linkOnline)
        return;
    m_blink = !m_blink;
    m_liveLabel->setStyleSheet(chipStyle(m_blink ? Theme::GreenBright : Theme::Green, 14));
}

void MainWindow::onSiteUpdated(int idx)
{
    const SiteData &d = m_service->data(idx);
    m_cards[idx]->setData(d);
    if (d.state == SiteData::State::Live && idx < m_announcedOnline.size()
        && !m_announcedOnline[idx]) {
        m_announcedOnline[idx] = true;
        m_log->log(QStringLiteral("INFO"), d.site.code,
                   QStringLiteral("Site online — load %1% · latency %2 ms")
                       .arg(QString::number(d.current.loadPct, 'f', 1),
                            QString::number(d.current.latencyMs, 'f', 0)));
        m_eventCount = m_eventCount + 1;
    }
    runAlertRules(idx);
    if (idx == m_selected) {
        m_scope->setData(d);
        if (!std::isnan(d.current.loadPct))
            m_scopePanel->setValue(QStringLiteral("%1%").arg(d.current.loadPct, 0, 'f', 1),
                                   Theme::OrangeBright);
    } else if (m_selected < 0 && d.state == SiteData::State::Live) {
        onCardClicked(idx);   // first live site becomes the scope default
    }

    int live = 0;
    for (const SiteData &sd : m_service->all())
        if (sd.state == SiteData::State::Live) ++live;
    m_gridPanel->setValue(QStringLiteral("%1/%2 LIVE").arg(live).arg(m_service->siteCount()),
                          live == m_service->siteCount() ? Theme::GreenBright : Theme::TextMuted);
}

void MainWindow::onScanProgress(int done, int total)
{
    m_scanLabel->setText(QStringLiteral("SCAN %1/%2").arg(done).arg(total));
    if (done >= total && total > 0) {
        m_firstScanDone = true;
        m_nextScanUtc = QDateTime::currentDateTimeUtc().addSecs(FeedService::kRefreshSecs);
        int live = 0;
        for (const SiteData &d : m_service->all())
            if (d.state == SiteData::State::Live) ++live;
        m_log->log(live == total ? QStringLiteral("INFO") : QStringLiteral("WARN"),
                   QStringLiteral("360AirportDash"),
                   QStringLiteral("Scan complete — %1/%2 sites reporting").arg(live).arg(total),
                   live == total ? Theme::Green : Theme::Amber);
        if (live == 0)
            m_log->log(QStringLiteral("INFO"), QStringLiteral("FEED"),
                       QStringLiteral("No samples yet — OpenSky unreachable, rate limited "
                                      "or rejected the request"),
                       Theme::TextSoft);
        m_eventCount = m_eventCount + 1;
    }
    m_scanDoneCount = done;
}

void MainWindow::onLinkStatus(bool online)
{
    applyLinkChip(online);
    m_log->log(online ? QStringLiteral("INFO") : QStringLiteral("CRIT"),
               QStringLiteral("LINK"),
               online ? QStringLiteral("Feed link restored")
                      : QStringLiteral("Feed link down — retaining last samples"));
    m_eventCount = m_eventCount + 1;
}

void MainWindow::onCardClicked(int idx)
{
    if (m_selected >= 0 && m_selected < m_cards.size())
        m_cards[m_selected]->setSelected(false);
    m_selected = idx;
    m_cards[idx]->setSelected(true);
    const SiteData &d = m_service->data(idx);
    m_scopePanel->setTitle(QStringLiteral("Analysis Trace — %1 · %2")
                               .arg(d.site.name, d.site.code));
    if (d.valid() && !std::isnan(d.current.loadPct)) {
        m_scopePanel->setValue(QStringLiteral("%1%").arg(d.current.loadPct, 0, 'f', 1),
                               Theme::OrangeBright);
    } else {
        m_scopePanel->setValue(QStringLiteral("—"), Theme::TextMuted);
    }
    m_scope->setData(d);
    m_log->log(QStringLiteral("INFO"), d.site.code,
               QStringLiteral("Site selected for analysis"));
    m_eventCount = m_eventCount + 1;
}

// Column count for the matrix: as many 384px-min card columns as the scroll
// viewport admits, capped by the roster size. Measured on the viewport, not
// the grid widget: a widgetResizable scroll area never shrinks its widget
// below the current layout minimum, so the widget's own width can't see a
// shrink. A small scrollbar allowance keeps the column count stable when the
// vertical scrollbar appears/disappears.
int MainWindow::siteColumns() const
{
    const QMargins cm = m_grid->contentsMargins();
    const int avail = m_gridScroll->viewport()->width() - 12 - cm.left() - cm.right();
    const int gap = m_grid->horizontalSpacing();
    const int byWidth = (avail + gap) / (384 + gap);   // N*384 + (N-1)*gap ≤ avail
    return qBound(1, byWidth, qMax(1, m_service->siteCount()));
}

// Re-run the grid layout when the usable width crosses a column threshold.
// Pure geometry: cards are recreated with their last-good samples and the
// selection/scope state carries over.
void MainWindow::reflowSiteGrid()
{
    if (!m_gridPanel->isVisible())
        return;
    const int cols = siteColumns();
    if (cols == m_gridCols)
        return;
    m_gridCols = cols;
    relayoutSiteCards();
}

// Blow one body panel up to fill the whole console body (None restores the
// three-panel layout). The other panels — and the splitter seam, when a whole
// side folds away — disappear; they come back unchanged on restore. The grid
// re-flows its columns through the same viewport-width path as a drag.
void MainWindow::setSectionFocus(SectionFocus f)
{
    if (m_focus == f)
        return;
    m_focus = f;

    const bool grid = f == SectionFocus::Grid;
    const bool trace = f == SectionFocus::Trace;
    const bool log = f == SectionFocus::Log;
    m_gridPanel->setVisible(!trace && !log);
    m_rightCol->setVisible(!grid);
    m_scopePanel->setVisible(!log);
    m_logPanel->setVisible(!trace);

    m_gridBtn->setExpanded(grid);
    m_traceBtn->setExpanded(trace);
    m_logBtn->setExpanded(log);

    m_log->log(QStringLiteral("INFO"), QStringLiteral("360AirportDash"),
               f == SectionFocus::None
                   ? QStringLiteral("Console view restored")
                   : QStringLiteral("%1 expanded to full view")
                         .arg(grid ? QStringLiteral("Site matrix")
                                   : trace ? QStringLiteral("Analysis trace")
                                           : QStringLiteral("Event log")));
    m_eventCount = m_eventCount + 1;
    m_logPanel->setValue(QStringLiteral("%1 EVENTS").arg(m_eventCount), Theme::TextMuted);
}

// Rebuild the site matrix from the service's current roster. Runs at startup
// and after every admin-roster swap (service->sitesReconfigured): drops the
// old cards, re-adds one card per site, carries over last-good samples, and
// re-arms the watch rules for the new indexing.
void MainWindow::rebuildSiteGrid()
{
    m_gridCols = siteColumns();
    m_selected = -1;
    relayoutSiteCards();

    m_activeAlerts = QVector<QHash<QString, Alert::Sev>>(m_service->siteCount());     // watch rules re-armed
    m_announcedOnline.resize(m_service->siteCount());
    for (int i = 0; i < m_service->siteCount(); ++i)
        m_announcedOnline[i] = m_service->data(i).valid();   // don't re-announce carried-over sites

    m_scope->setData(SiteData{});
    m_scopePanel->setTitle(QStringLiteral("Analysis Trace"));
    m_scopePanel->setValue(QStringLiteral("—"), Theme::TextMuted);

    const int n = m_service->siteCount();
    int live = 0;
    for (const SiteData &d : m_service->all())
        if (d.state == SiteData::State::Live) ++live;
    m_gridPanel->setTitle(QStringLiteral("Site Matrix — %1 Site%2")
                              .arg(n).arg(n == 1 ? QString() : QStringLiteral("s")));
    m_gridPanel->setValue(QStringLiteral("%1/%2 LIVE").arg(live).arg(n),
                          live == n ? Theme::GreenBright : Theme::TextMuted);
    m_scanLabel->setText(QStringLiteral("SCAN 0/%1").arg(n));
}

// (Re)create the site cards in a grid of m_gridCols columns. Safe to run
// on reflow: keeps each site's last-good sample and the current selection.
void MainWindow::relayoutSiteCards()
{
    for (SiteCard *c : m_cards)
        c->deleteLater();
    m_cards.clear();
    while (QLayoutItem *it = m_grid->takeAt(0))
        delete it;

    const int n = m_service->siteCount();
    for (int i = 0; i < n; ++i) {
        SiteCard *card = new SiteCard(i);
        m_cards.push_back(card);
        connect(card, &SiteCard::clicked, this, &MainWindow::onCardClicked);
        m_grid->addWidget(card, i / m_gridCols, i % m_gridCols);
        card->setData(m_service->data(i));
    }
    // setRowStretch(r, …) grows rowCount() once r reaches it, so the count
    // must be read once up front or the reset loop never terminates.
    const int rows = m_grid->rowCount();
    for (int r = 0; r < rows; ++r)
        m_grid->setRowStretch(r, 0);
    m_grid->setRowStretch(rows, 1);   // trailing stretch absorbs leftover height

    if (m_selected >= 0 && m_selected < m_cards.size())
        m_cards[m_selected]->setSelected(true);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj == m_gridHost && ev->type() == QEvent::Resize)
        reflowSiteGrid();
    return QMainWindow::eventFilter(obj, ev);
}

void MainWindow::openAdmin()
{
    AdminDialog dlg(m_registry->sites(), this);
    if (dlg.exec() == QDialog::Accepted)
        applyAdminRoster(dlg.sites());
}

void MainWindow::applyAdminRoster(const QVector<Site> &list)
{
    const QString err = SiteRegistry::validateList(list);
    if (!err.isEmpty()) {
        m_log->log(QStringLiteral("WARN"), QStringLiteral("ROSTER"),
                   QStringLiteral("Roster rejected — %1").arg(err), Theme::Amber);
        m_eventCount = m_eventCount + 1;
        return;
    }
    if (list == m_registry->sites())
        return;   // opened and saved without edits

    QSet<QString> before, after;
    for (const Site &s : m_registry->sites())
        before.insert(s.code);
    for (const Site &s : list)
        after.insert(s.code);
    int added = 0, removed = 0;
    for (const QString &c : after)
        if (!before.contains(c)) ++added;
    for (const QString &c : before)
        if (!after.contains(c)) ++removed;

    // On success the registry persists the roster and emits sitesChanged,
    // which hot-swaps the service roster; the grid then rebuilds via
    // sitesReconfigured.
    if (!m_registry->setSites(list)) {
        m_log->log(QStringLiteral("CRIT"), QStringLiteral("ROSTER"),
                   QStringLiteral("Roster save failed — %1").arg(SiteRegistry::configPath()),
                   Theme::Red);
        m_eventCount = m_eventCount + 1;
        return;
    }
    m_log->log(QStringLiteral("INFO"), QStringLiteral("ROSTER"),
               QStringLiteral("Site roster updated — %1 sites (%2 added, %3 removed) — saved to %4")
                   .arg(list.size()).arg(added).arg(removed).arg(SiteRegistry::configPath()),
               Theme::Green);
    m_eventCount = m_eventCount + 1;
}

void MainWindow::runAlertRules(int idx)
{
    const SiteData &d = m_service->data(idx);
    if (!d.valid())
        return;

    const QVector<Alert> current = evaluateAlerts(d);
    QHash<QString, Alert::Sev> currentSev;
    for (const Alert &a : current)
        currentSev.insert(a.key, a.sev);

    const auto sevColor = [](Alert::Sev s) {
        return (s == Alert::Sev::Crit) ? Theme::Red
               : (s == Alert::Sev::Warn) ? Theme::Amber
                                         : Theme::Blue;
    };
    const auto sevName = [](Alert::Sev s) {
        return (s == Alert::Sev::Crit) ? QStringLiteral("CRITICAL")
               : (s == Alert::Sev::Warn) ? QStringLiteral("WARNING")
                                         : QStringLiteral("INFO");
    };

    const QHash<QString, Alert::Sev> previous = m_activeAlerts[idx];
    for (const Alert &a : current) {
        const auto it = previous.constFind(a.key);
        if (it == previous.constEnd()) {
            m_log->log(QString::fromLatin1(sevLabel(a.sev)), d.site.code, a.message.toLower(),
                       sevColor(a.sev));
            m_eventCount = m_eventCount + 1;
        } else if (it.value() != a.sev) {
            // Same rule still active, severity moved: log escalation / de-escalation.
            const bool raised = a.sev > it.value();
            m_log->log(QString::fromLatin1(sevLabel(a.sev)), d.site.code,
                       QStringLiteral("%1 alert %2 to %3: %4")
                           .arg(a.key.toLower(), raised ? QStringLiteral("raised")
                                                        : QStringLiteral("lowered"),
                                sevName(a.sev), a.message.toLower()),
                       sevColor(a.sev));
            m_eventCount = m_eventCount + 1;
        }
    }
    for (auto it = previous.constBegin(); it != previous.constEnd(); ++it) {
        if (!currentSev.contains(it.key())) {
            m_log->log(QStringLiteral("INFO"), d.site.code,
                       QStringLiteral("%1 nominal").arg(it.key().toLower()), Theme::Green);
            m_eventCount = m_eventCount + 1;
        }
    }
    m_activeAlerts[idx] = currentSev;
    m_logPanel->setValue(QStringLiteral("%1 EVENTS").arg(m_eventCount), Theme::TextMuted);
}

void MainWindow::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_R && !m_offline) {
        m_log->log(QStringLiteral("INFO"), QStringLiteral("360AirportDash"),
                   QStringLiteral("Manual rescan requested by operator"));
        m_eventCount = m_eventCount + 1;
        m_service->rescan();
    } else if (e->key() == Qt::Key_E && e->modifiers() & Qt::ControlModifier) {
        openAdmin();
    } else if (e->key() >= Qt::Key_1 && e->key() <= Qt::Key_9
               && (e->key() - Qt::Key_0) <= m_cards.size()) {
        onCardClicked(e->key() - Qt::Key_0 - 1);
    } else if (!(e->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))
               && (e->key() == Qt::Key_Left || e->key() == Qt::Key_Right
                   || e->key() == Qt::Key_Up || e->key() == Qt::Key_Down)) {
        // Arrow navigation: Left/Right = previous/next site, Up/Down = one
        // grid row back/forward. Clamps at the ends (no wrap). Index and roster
        // size are read now, never cached across sitesReconfigured.
        const int n = m_cards.size();
        if (n > 0) {
            int step = 1;
            if (e->key() == Qt::Key_Up || e->key() == Qt::Key_Down)
                step = qMax(1, m_gridCols);
            if (e->key() == Qt::Key_Left || e->key() == Qt::Key_Up)
                step = -step;
            const int target = m_selected < 0 ? (step > 0 ? 0 : n - 1)
                                              : qBound(0, m_selected + step, n - 1);
            if (target != m_selected) {
                onCardClicked(target);
                m_gridScroll->ensureWidgetVisible(m_cards[target]);
            }
        }
        e->accept();
        return;
    } else if (e->key() == Qt::Key_Q && e->modifiers() & Qt::ControlModifier) {
        close();
    }
    QMainWindow::keyPressEvent(e);
}
