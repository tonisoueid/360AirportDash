#pragma once

#include "SiteData.h"

#include <QWidget>

// One watched site: site name with its code and a live/offline indicator,
// primary-metric readout (example: load) with a short-history trend chip, a
// segmented 0–100 gauge on the domain ramp, metric rows (latency / sessions
// / trend / region), a trailing sparkline of the secondary metric, and a
// data-age/status footer. Repaints on data updates and the minute tick.
// Pending sites render an explicit dashed placeholder — no invented values.
class SiteCard : public QWidget
{
    Q_OBJECT
public:
    explicit SiteCard(int index, QWidget *parent = nullptr);

    void setData(const SiteData &d);
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
    bool m_live = false;
    bool m_selected = false;
    bool m_hover = false;

    void drawSparkline(QPainter &p, const QRectF &r);
    void drawLoadGauge(QPainter &p, const QRectF &r, double loadPct);
};
