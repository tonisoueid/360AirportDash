#pragma once

#include "Airport.h"
#include "SiteData.h"

#include <QWidget>

// One watched airport: ICAO (large) with a status badge, city, country text,
// the airport's local clock (from its IANA zone), arrivals / departures
// counts and an "updated N min ago" stamp. Everything unknown is a gap:
// missing metadata → "---", unmeasured board → "---", empty/invalid time
// zone → no clock, never fetched → no stamp. No bitmap assets, no flags.
//
// The clock and the stamp are computed at paint time and the card only
// repaints on data updates and MainWindow's minute gate (calm UI).
class SiteCard : public QWidget
{
    Q_OBJECT
public:
    explicit SiteCard(int index, QWidget *parent = nullptr);

    // `paused` = FeedService::rateLimitPaused(), fed to cardBadge().
    void setData(const SiteData &d, bool paused = false);
    // Metadata from AirportRegistry for this card's ICAO; nullptr = unknown.
    void setAirport(const Airport *a);
    void setSelected(bool sel);
    int index() const { return m_index; }

signals:
    void clicked(int index);

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;
    QSize minimumSizeHint() const override { return {384, 288}; }

private:
    int m_index;
    SiteData m_data;
    Airport m_airport;           // empty fields when metadata is unknown
    bool m_paused = false;
    bool m_selected = false;
    bool m_hover = false;
};
