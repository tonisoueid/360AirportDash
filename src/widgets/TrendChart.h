#pragma once

#include "../SiteData.h"

#include <QWidget>

// Scope trace for the selected site: the trailing sample history (up to
// kHistoryCap) with two series on independent scales — the primary metric
// (example: load, orange, gradient area fill, left axis) and the secondary
// metric (example: latency, blue line, right axis). Subtle solid graticule,
// dashed LATEST cursor with a chip readout, MIN/MAX annotations on the
// primary series. Fully custom QPainter; missing samples render gaps, never
// zero.
class TrendChart : public QWidget
{
    Q_OBJECT
public:
    explicit TrendChart(QWidget *parent = nullptr);

    void setData(const SiteData &d);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    double xForIndex(int i) const;
    double yForLoad(double v) const;
    double yForLatency(double v) const;
    void drawTrace(QPainter &p);

    SiteData m_data;
    bool m_hasData = false;
    QRectF m_plot;
    int m_i0 = 0, m_i1 = 0;                 // visible sample window
    double m_loadMin = 0, m_loadMax = 1;    // primary scale
    double m_latMin = 0, m_latMax = 1;      // secondary scale
};
