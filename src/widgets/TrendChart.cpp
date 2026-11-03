#include "TrendChart.h"
#include "../Theme.h"

#include <QPainter>

#include <algorithm>
#include <cmath>

TrendChart::TrendChart(QWidget *parent)
    : QWidget(parent)
{
    setAutoFillBackground(false);
    setMinimumHeight(420);
}

void TrendChart::setData(const SiteData &d)
{
    m_data = d;
    m_hasData = d.valid() && d.recent.size() > 1;
    update();
}

double TrendChart::xForIndex(int i) const
{
    return m_plot.left() + (double(i - m_i0) / double(qMax(1, m_i1 - m_i0 - 1))) * m_plot.width();
}

double TrendChart::yForLoad(double v) const
{
    const double span = qMax(1.0, m_loadMax - m_loadMin);
    return m_plot.bottom() - ((v - m_loadMin) / span) * m_plot.height();
}

double TrendChart::yForLatency(double v) const
{
    const double span = qMax(1.0, m_latMax - m_latMin);
    return m_plot.bottom() - ((v - m_latMin) / span) * m_plot.height();
}

void TrendChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    if (!m_hasData) {
        p.setFont(Theme::sans(14, false, true));
        p.setPen(Theme::TextFaint);
        p.drawText(rect(), Qt::AlignCenter,
                   QStringLiteral("NO TRACE — SELECT A LIVE SITE"));
        return;
    }

    const int padL = 50, padR = 56, padT = 36, padB = 22;
    m_plot = QRectF(rect().left() + padL, rect().top() + padT,
                    rect().width() - padL - padR,
                    rect().height() - padT - padB);

    m_i0 = 0;
    m_i1 = m_data.recent.size();

    // independent scales per series, from the visible window, padded
    m_loadMin = 1e9; m_loadMax = -1e9;
    m_latMin = 1e9; m_latMax = -1e9;
    for (int i = m_i0; i < m_i1; ++i) {
        const Sample &s = m_data.recent[i];
        if (!std::isnan(s.loadPct)) {
            m_loadMin = std::min(m_loadMin, s.loadPct);
            m_loadMax = std::max(m_loadMax, s.loadPct);
        }
        if (!std::isnan(s.latencyMs)) {
            m_latMin = std::min(m_latMin, s.latencyMs);
            m_latMax = std::max(m_latMax, s.latencyMs);
        }
    }
    // the load example is a 0–100 percentage: keep the whole scale so the
    // gridlines carry meaningful reference values
    if (m_loadMin <= m_loadMax)
        m_loadMin = 0.0, m_loadMax = 100.0;
    if (m_latMin <= m_latMax) {
        m_latMin = std::floor(m_latMin);
        m_latMax = std::ceil(m_latMax) + 1.0;
    }
    if (m_loadMin > m_loadMax && m_latMin > m_latMax)
        return;   // nothing measurable in the window

    drawTrace(p);
}

void TrendChart::drawTrace(QPainter &p)
{
    const QVector<Sample> &h = m_data.recent;
    const bool hasLoad = m_loadMin <= m_loadMax;
    const bool hasLat = m_latMin <= m_latMax;

    // graticule: solid hairlines, both axes labeled at the same fractions so
    // the dual scales stay readable
    p.setFont(Theme::sans(12));
    for (int k = 0; k <= 5; ++k) {
        const double y = m_plot.bottom() - m_plot.height() * k / 5.0;
        p.setPen(QPen(Theme::Grid, 1));
        p.drawLine(QLineF(m_plot.left(), y, m_plot.right(), y));
        if (hasLoad) {
            const double v = m_loadMin + (m_loadMax - m_loadMin) * k / 5.0;
            p.setPen(Theme::Orange);
            p.drawText(QRectF(m_plot.left() - 46, y - 8, 42, 16),
                       Qt::AlignRight | Qt::AlignVCenter,
                       QStringLiteral("%1%").arg(qRound(v)));
        }
        if (hasLat) {
            const double v = m_latMin + (m_latMax - m_latMin) * k / 5.0;
            p.setPen(Theme::Blue);
            p.drawText(QRectF(m_plot.right() + 4, y - 8, 46, 16),
                       Qt::AlignLeft | Qt::AlignVCenter,
                       QStringLiteral("%1").arg(qRound(v)));
        }
    }
    for (int i = m_i0; i < m_i1; ++i) {
        if ((i - m_i0) % 6 != 0)
            continue;
        const double x = xForIndex(i);
        p.setPen(QPen(Theme::Grid, 1));
        p.drawLine(QLineF(x, m_plot.top(), x, m_plot.bottom()));
        p.setPen(Theme::TextMuted);
        p.drawText(QRectF(x - 26, m_plot.bottom() + 5, 52, 16),
                   Qt::AlignHCenter | Qt::AlignTop,
                   h[i].stampUtc.time().toString(QStringLiteral("hh:mm")));
    }

    // secondary series first (line under the primary's area fill), scaled to
    // its own axis
    if (hasLat) {
        QPainterPath path;
        bool started = false;
        for (int i = m_i0; i < m_i1; ++i) {
            const double v = h[i].latencyMs;
            if (std::isnan(v)) { started = false; continue; }
            const QPointF pt(xForIndex(i), yForLatency(v));
            if (!started) { path.moveTo(pt); started = true; }
            else path.lineTo(pt);
        }
        if (!path.isEmpty()) {
            p.setPen(QPen(Theme::Blue, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(Qt::NoBrush);
            p.drawPath(path);
        }
    }

    // primary series: soft area fill in the domain accent + brighter line
    if (hasLoad) {
        QPainterPath line;
        bool started = false;
        for (int i = m_i0; i < m_i1; ++i) {
            const double v = h[i].loadPct;
            if (std::isnan(v)) { started = false; continue; }
            const QPointF pt(xForIndex(i), yForLoad(v));
            if (!started) { line.moveTo(pt); started = true; }
            else line.lineTo(pt);
        }
        if (!line.isEmpty()) {
            QPainterPath area(line);
            area.lineTo(line.currentPosition().x(), m_plot.bottom());
            area.lineTo(xForIndex(m_i0), m_plot.bottom());
            area.closeSubpath();
            QLinearGradient grad(QPointF(0, m_plot.top()), QPointF(0, m_plot.bottom()));
            QColor top = Theme::Orange; top.setAlphaF(0.22);
            QColor bot = Theme::Orange; bot.setAlphaF(0.0);
            grad.setColorAt(0.0, top);
            grad.setColorAt(1.0, bot);
            p.setPen(Qt::NoPen);
            p.setBrush(grad);
            p.drawPath(area);

            p.setPen(QPen(Theme::OrangeBright, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(Qt::NoBrush);
            p.drawPath(line);

            // sample dots on the trace
            p.setPen(Qt::NoPen);
            p.setBrush(Theme::Text);
            for (int i = m_i0; i < m_i1; i += 6) {
                if (std::isnan(h[i].loadPct)) continue;
                p.drawEllipse(QPointF(xForIndex(i), yForLoad(h[i].loadPct)), 2.2, 2.2);
            }
        }
    }

    // LATEST cursor: dashed white line + chip-backed readout of the primary
    {
        const int last = m_i1 - 1;
        const double x = xForIndex(last);
        QColor c = Theme::Text; c.setAlphaF(0.45);
        p.setPen(QPen(c, 1, Qt::DashLine));
        p.drawLine(QLineF(x, m_plot.top() - 6, x, m_plot.bottom()));
        if (hasLoad && !std::isnan(h[last].loadPct)) {
            p.setPen(Qt::NoPen);
            p.setBrush(Theme::loadColor(h[last].loadPct));
            p.drawEllipse(QPointF(x, yForLoad(h[last].loadPct)), 3.2, 3.2);
        }
        p.setFont(Theme::sans(11, true));
        QString cap = QStringLiteral("LATEST ");
        QColor capC = Theme::TextMuted;
        if (hasLoad && !std::isnan(h[last].loadPct)) {
            cap += QStringLiteral("%1%").arg(h[last].loadPct, 0, 'f', 1);
            capC = Theme::loadColor(h[last].loadPct);
        } else {
            cap += QStringLiteral("---");
        }
        const int capW = QFontMetrics(p.font()).horizontalAdvance(cap);
        const qreal capX = qBound(m_plot.left(), x + 5, m_plot.right() - capW - 8);
        p.fillRect(QRectF(capX, m_plot.top() - 7, capW + 8, 22), Theme::BgDeep);
        p.setPen(Theme::TextMuted);
        p.drawText(QPointF(capX + 4, m_plot.top() + 10), QStringLiteral("LATEST"));
        p.setPen(capC);
        p.drawText(QPointF(capX + 4 + QFontMetrics(p.font()).horizontalAdvance(QStringLiteral("LATEST  ")),
                           m_plot.top() + 10),
                   cap.mid(7));
    }

    // min/max annotations on the primary series over the visible window
    if (hasLoad) {
        int iMin = -1, iMax = -1;
        for (int i = m_i0; i < m_i1; ++i) {
            const double v = h[i].loadPct;
            if (std::isnan(v)) continue;
            if (iMin < 0 || v < h[iMin].loadPct) iMin = i;
            if (iMax < 0 || v > h[iMax].loadPct) iMax = i;
        }
        p.setFont(Theme::sans(11, true));
        auto annotate = [&](int i, bool above, const QString &tag, const QColor &c) {
            const QPointF pt(xForIndex(i), yForLoad(h[i].loadPct));
            p.setPen(c);
            p.drawText(pt + QPointF(5, above ? -8 : 13),
                       QStringLiteral("%1 %2%").arg(tag).arg(h[i].loadPct, 0, 'f', 0));
        };
        if (iMax >= 0) annotate(iMax, true, QStringLiteral("MAX"), Theme::Orange);
        if (iMin >= 0 && iMin != iMax) annotate(iMin, false, QStringLiteral("MIN"), Theme::Blue);
    }

    // legend: swatches in the series' domain accents, self-describing units
    p.setFont(Theme::sans(11));
    const qreal lx = m_plot.left();
    p.setPen(QPen(Theme::OrangeBright, 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawLine(QPointF(lx + 1, 13), QPointF(lx + 23, 13));
    p.setPen(Theme::OrangeBright);
    p.drawText(QPointF(lx + 28, 17), QStringLiteral("LOAD %"));
    p.setPen(QPen(Theme::Blue, 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawLine(QPointF(lx + 101, 13), QPointF(lx + 123, 13));
    p.setPen(Theme::Blue);
    p.drawText(QPointF(lx + 128, 17), QStringLiteral("LATENCY MS"));
}
