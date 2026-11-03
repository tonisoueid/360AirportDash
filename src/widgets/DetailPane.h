#pragma once

#include "../Airport.h"
#include "../SiteData.h"

#include <QRect>
#include <QString>
#include <QWidget>

class FlightTable;
class QLineEdit;

// Detail pane for ONE selected airport: a custom-painted header (name, ICAO /
// IATA, local time, provider badge, board-window labels, refresh button) over
// an Arrivals and a Departures FlightTable that share one filter field.
//
// It is a pure view over stored data: setSite() copies what it shows out of a
// SiteData and starts nothing. The refresh button only emits
// refreshRequested(); whoever owns the FeedService decides whether a poll
// really starts, and the pane changes only when the next setSite() arrives
// from applySample/applyFailure. Bound by ICAO (siteCode), never by index.
// Repaints on setSite / minute tick() / hover; no timers inside.
class DetailPane : public QWidget
{
    Q_OBJECT
public:
    struct View {
        SiteData data;                 // stored state, shown as-is
        Airport airport;               // empty fields when metadata unknown
        bool authenticated = false;    // OpenSky credentials present
        bool refreshEnabled = true;    // false: offline or 429 pause
        QString refreshNote;           // why refresh is off (shown on hover)
    };

    explicit DetailPane(QWidget *parent = nullptr);

    // Bind to a site (or re-bind after a data update). Switching to a
    // different ICAO clears the shared filter.
    void setSite(const View &v);
    // Empty state: nothing selected / selected site removed.
    void clear();
    // Minute gate: repaint time-dependent text only.
    void tick();

    QString siteCode() const { return m_bound ? m_code : QString(); }
    bool bound() const { return m_bound; }
    FlightTable *arrivalsTable() const { return m_arr; }
    FlightTable *departuresTable() const { return m_dep; }
    QString filter() const;
    QRect refreshRect() const;
    static int headerHeight() { return 124; }

    QSize sizeHint() const override { return {640, 520}; }
    QSize minimumSizeHint() const override { return {360, 340}; }

signals:
    void refreshRequested();

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;
    bool event(QEvent *e) override;

private:
    QRect filterFrame() const;
    void layoutChildren();

    FlightTable *m_arr = nullptr;
    FlightTable *m_dep = nullptr;
    QLineEdit *m_filter = nullptr;

    bool m_bound = false;
    QString m_code;
    QString m_name;
    Airport m_airport;
    SiteData::State m_state = SiteData::State::Pending;
    bool m_auth = false;
    bool m_refreshEnabled = true;
    QString m_refreshNote;
    QString m_arrWindow, m_depWindow;   // boardWindowLabel(); empty = unknown
    bool m_refreshHover = false;
};
