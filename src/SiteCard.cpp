#include "SiteCard.h"
#include "Theme.h"
#include "widgets/IconGlyph.h"

#include <QDateTime>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <cmath>

SiteCard::SiteCard(int index, QWidget *parent)
    : QWidget(parent), m_index(index)
{
    setAutoFillBackground(false);
    setMouseTracking(false);
    setCursor(Qt::PointingHandCursor);
}

void SiteCard::setData(const SiteData &d)
{
    m_data = d;
    m_live = d.valid();
    update();
}

void SiteCard::setSelected(bool sel)
{
    m_selected = sel;
    update();
}

void SiteCard::mouseReleaseEvent(QMouseEvent *e)
{
    if (rect().contains(e->pos()))
        emit clicked(m_index);
    QWidget::mouseReleaseEvent(e);
}

void SiteCard::enterEvent(QEnterEvent *) { m_hover = true; update(); }
void SiteCard::leaveEvent(QEvent *) { m_hover = false; update(); }

void SiteCard::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // chassis — blue halo when selected, like the reference's focused card
    if (m_selected) {
        QColor halo = Theme::Blue; halo.setAlphaF(0.35);
        p.setPen(QPen(halo, 4));
        p.setBrush(Theme::CardBg);
        p.drawRoundedRect(rect().adjusted(2, 2, -3, -3), 10, 10);
        p.setPen(QPen(Theme::Blue, 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(rect().adjusted(2, 2, -3, -3), 10, 10);
    } else {
        Theme::panelBody(p, rect(), 10, Theme::CardBg,
                         m_hover ? Theme::BorderHi : Theme::Border);
    }

    const Site &st = m_data.site;

    // ---- row 0: site name (white, headline-sized) with the site code in
    // parentheses after it | link indicator
    const QFont nameFont = Theme::sans(16, true);
    p.setFont(nameFont);
    p.setPen(Theme::Text);
    p.drawText(QPoint(14, 25), st.name);
    const int nameW = QFontMetrics(nameFont).horizontalAdvance(st.name);
    p.setFont(Theme::sans(12, true));
    p.setPen(Theme::TextMuted);
    p.drawText(QPoint(14 + nameW + 7, 25),
               QStringLiteral("(%1)").arg(st.code));

    if (m_live) {
        GlyphPaint::draw(p, GlyphPaint::Glyph::Wifi, QRectF(width() - 96, 9, 18, 18));
        p.setFont(Theme::sans(12, true));
        p.setPen(Theme::GreenBright);
        p.drawText(QPoint(width() - 72, 25), QStringLiteral("ONLINE"));
    } else {
        p.setFont(Theme::sans(10, true, true));
        p.setPen(Theme::Red);
        p.drawText(QPoint(width() - 86, 25), QStringLiteral("OFFLINE"));
        p.setBrush(Theme::Red);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(width() - 92, 21), 3, 3);
    }

    if (!m_live) {
        p.setFont(Theme::sans(14, true, true));
        p.setPen(Theme::TextMuted);
        p.drawText(rect().adjusted(0, -20, 0, 0), Qt::AlignCenter,
                   m_data.state == SiteData::State::Pending
                       ? QStringLiteral("AWAITING FIRST SCAN")
                       : QStringLiteral("LINK ERROR"));
        if (!m_data.error.isEmpty()) {
            p.setFont(Theme::sans(11));
            p.setPen(Theme::Red);
            QString msg = m_data.error;
            if (msg.size() > 46) msg = msg.left(43) + "...";
            p.drawText(rect().adjusted(0, 12, 0, 0), Qt::AlignCenter, msg);
        }
        p.setPen(QPen(Theme::Grid, 1, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(14, height() - 44, width() - 28, 26), 5, 5);
        p.setFont(Theme::sans(11));
        p.setPen(Theme::TextFaint);
        p.drawText(QRectF(14, height() - 48, width() - 28, 30), Qt::AlignCenter,
                   QStringLiteral("NO DATA — GAP SHOWN, NOT FAKED"));
        return;
    }

    const Sample &c = m_data.current;

    // ---- primary-metric readout (example: load), sized no larger than the
    // site name and colored on the calm→hot domain ramp
    const QFont bigFont = Theme::sans(15, true);
    QString valStr = QStringLiteral("--");
    if (!std::isnan(c.loadPct))
        valStr = QStringLiteral("%1%").arg(c.loadPct, 0, 'f', 1);
    p.setFont(bigFont);
    p.setPen(std::isnan(c.loadPct) ? Theme::TextMuted : Theme::loadColor(c.loadPct));
    p.drawText(QPoint(14, 78), valStr);
    p.setFont(Theme::sans(11, true));
    p.setPen(Theme::TextMuted);
    p.drawText(QPoint(14, 94), QStringLiteral("LOAD"));

    // ---- trend chip of the same metric over the trailing samples, sharing
    // the readout's line, right-aligned to the card edge
    const QFont chipFont = Theme::sans(11, true);
    const double trend = m_data.loadTrend3();
    QString trendTxt = QStringLiteral("Δ3 ---");
    QColor trendC = Theme::TextMuted;
    if (!std::isnan(trend)) {
        if (trend > 2.0)       { trendTxt = QStringLiteral("Δ3 +%1 ↑").arg(trend, 0, 'f', 1); trendC = Theme::Amber; }
        else if (trend < -2.0) { trendTxt = QStringLiteral("Δ3 %1 ↓").arg(trend, 0, 'f', 1);  trendC = Theme::Blue; }
        else                     trendTxt = QStringLiteral("Δ3 %1 →").arg(trend, 0, 'f', 1);
    }
    const int trendW = QFontMetrics(chipFont).horizontalAdvance(trendTxt) + 20;
    Theme::chip(p, QRectF(width() - 40 - trendW, 60, trendW, 24), trendTxt, trendC, chipFont);
    GlyphPaint::draw(p, GlyphPaint::Glyph::Trend, QRectF(width() - 30, 56, 26, 26), trendC);

    // ---- segmented 0–100 gauge, right column below the trend row
    drawLoadGauge(p, QRectF(width() - 52, 104, 13, 92), c.loadPct);

    // ---- metrics: white labels, values in domain colors; 2-column block
    // centered horizontally, kept clear of the gauge
    const QFont labelFont = Theme::sans(11);
    const QFont vFont = Theme::sans(12, true);
    const QFontMetrics vFm(vFont);
    struct Row { const char *label; QString val; QColor c; };
    const QString lat = std::isnan(c.latencyMs) ? QStringLiteral("---")
        : QStringLiteral("%1 MS").arg(qRound(c.latencyMs));
    const QString sess = c.sessions < 0 ? QStringLiteral("---")
        : QStringLiteral("%1").arg(c.sessions);
    QString reg = st.region.isEmpty() ? QStringLiteral("---") : st.region;
    if (reg.size() > 12) reg = reg.left(11) + QLatin1String("…");
    const QVector<Row> rows = {
        {"LATENCY",  lat,  Theme::BlueBright},
        {"SESSIONS", sess, Theme::Green},
        {"LOAD Δ3",  std::isnan(trend) ? QStringLiteral("---")
                                       : QStringLiteral("%1 PTS").arg(trend, 0, 'f', 1),
                    std::isnan(trend) ? Theme::TextMuted
                                      : (trend > 2.0 ? Theme::Amber : Theme::TextSoft)},
        {"REGION",   reg,  Theme::TextSoft},
    };
    // measure the block from its widest values so centering is true, and
    // never slide it under the gauge column
    int colValW[2] = {0, 0};
    for (int i = 0; i < rows.size(); ++i)
        colValW[i / 2] = qMax(colValW[i / 2], vFm.horizontalAdvance(rows[i].val));
    const int labelW = 78;
    const int pitch = labelW + 54;
    const int blockW = pitch + labelW + colValW[1];
    const int x0 = qMax((width() - blockW) / 2, 14);
    for (int i = 0; i < rows.size(); ++i) {
        const int col = i / 2;
        const int row = i % 2;
        const int x = x0 + col * pitch;
        const int y = 124 + row * 20;
        p.setFont(labelFont);
        p.setPen(Theme::Text);
        p.drawText(QPoint(x, y), rows[i].label);
        p.setFont(vFont);
        p.setPen(rows[i].c);
        p.drawText(QPoint(x + labelW, y), rows[i].val);
    }

    // ---- sparkline of the secondary metric with gradient area fill
    const QRectF spark(14, height() - 80, width() - 28, 50);
    drawSparkline(p, spark);

    // ---- footer: state + data age; age is minute-grained and the card only
    // repaints on the minute tick (per-second was overload)
    p.setFont(Theme::sans(11));
    const int age = m_data.ageSecs();
    const QString ageStr = (age < 0) ? QStringLiteral("--")
        : (age < 60) ? QStringLiteral("<1 MIN")
                     : QStringLiteral("%1 MIN").arg(age / 60);
    QString footer;
    QColor footC = Theme::TextMuted;
    if (m_data.state == SiteData::State::Live) {
        footer = QStringLiteral("LIVE · LAST SAMPLE %1 AGO").arg(ageStr);
    } else {
        footer = QStringLiteral("STALE · LAST SAMPLE %1 AGO · RETAINED").arg(ageStr);
        footC = Theme::Amber;
    }
    p.setPen(footC);
    p.drawText(QPoint(14, height() - 11), footer);
}

// Vertical segmented stack: calm (bottom, blue) → hot (top, red); filled up
// to the current percentage with the ramp gradient, unfilled segments dark.
void SiteCard::drawLoadGauge(QPainter &p, const QRectF &r, double loadPct)
{
    const int n = 12;
    const double gap = 2.0;
    const double segH = (r.height() - gap * (n - 1)) / n;
    constexpr double lo = 0.0, hi = 100.0;

    int markerSeg = -1;
    if (!std::isnan(loadPct)) {
        const double frac = qBound(0.0, (loadPct - lo) / (hi - lo), 1.0);
        markerSeg = qMin(n - 1, int(frac * n));
    }

    for (int i = 0; i < n; ++i) {
        // segment 0 at the top = hottest
        const double y = r.top() + i * (segH + gap);
        const bool filled = markerSeg >= 0 && i >= (n - 1 - markerSeg);
        p.setPen(Qt::NoPen);
        if (filled) {
            QColor c = Theme::gaugeRamp(n - 1 - i, n);
            if (i == n - 1 - markerSeg)
                c = c.lighter(130);   // hot tip reads brighter
            p.setBrush(c);
        } else {
            p.setBrush(Theme::GridSoft);
        }
        p.drawRoundedRect(QRectF(r.left(), y, r.width(), segH), 1.5, 1.5);
    }

    // meaningful scale: ticks at the watch-rule thresholds, not bare extremes
    p.setFont(Theme::sans(11));
    const double tickPcts[3] = {hi, 75.0, 50.0};   // 75 = WARN rule, 50 = mid
    for (double t : tickPcts) {
        const double y = qBound(r.top(), r.bottom() - (t - lo) / (hi - lo) * r.height(), r.bottom());
        p.setPen(QPen(Theme::BorderHi, 1));
        p.drawLine(QLineF(r.right() + 1, y, r.right() + 5, y));
        p.setPen(Theme::TextMuted);
        p.drawText(QRectF(r.right() + 7, y - 9, 40, 18), Qt::AlignLeft | Qt::AlignVCenter,
                   QString::number(t, 'f', 0));
    }

    // current reading on the left of the stack, white with a leader tick
    if (!std::isnan(loadPct)) {
        const double frac = qBound(0.0, (loadPct - lo) / (hi - lo), 1.0);
        const double my = qBound(r.top(), r.bottom() - frac * r.height(), r.bottom());
        p.setPen(QPen(Theme::Text, 1));
        p.drawLine(QLineF(r.left() - 5, my, r.left() - 1, my));
        p.setFont(Theme::sans(11, true));
        p.setPen(Theme::Text);
        p.drawText(QRectF(r.left() - 46, my - 9, 40, 18), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(loadPct, 'f', 0));
    }
}

void SiteCard::drawSparkline(QPainter &p, const QRectF &r)
{
    // trailing window of the secondary metric (example: latency)
    QVector<double> vals;
    for (int i = m_data.recent.size() - 1; i >= 0 && vals.size() < 12; --i) {
        const double v = m_data.recent[i].latencyMs;
        if (!std::isnan(v))
            vals.push_back(v);          // newest first
    }
    double lo = 1e9, hi = -1e9;
    for (double v : vals) {
        lo = std::min(lo, v);
        hi = std::max(hi, v);
    }
    if (lo > hi) {
        p.setFont(Theme::sans(9));
        p.setPen(Theme::TextFaint);
        p.drawText(r, Qt::AlignCenter, QStringLiteral("---"));
        return;
    }
    const double span = qMax(1.0, hi - lo);
    std::reverse(vals.begin(), vals.end());   // oldest → newest, left → right

    // header band: white tag on the left, HI readout on the right, kept
    // clear of the trace so the label breathes above and below
    const QRectF band(r.left(), r.top(), r.width(), 18);
    p.fillRect(band, Theme::CardBg);
    p.setFont(Theme::sans(11, true));
    p.setPen(Theme::Text);
    p.drawText(band.adjusted(2, 0, -80, 0), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("LATENCY · LAST 12 SAMPLES"));

    // trace lives strictly below the band
    const QRectF plot(r.left(), r.top() + 20, r.width(), r.height() - 20);
    const int n = qMax(1, int(vals.size()) - 1);
    QVector<QPointF> pts;
    for (int i = 0; i < vals.size(); ++i) {
        const double x = plot.left() + 4 + (double(i) / n) * (plot.width() - 10);
        const double y = plot.bottom() - 4 - ((vals[i] - lo) / span) * (plot.height() - 12);
        pts.push_back(QPointF(x, y));
    }
    if (pts.size() < 2)
        return;

    // gradient area under the trace in the latency domain accent, fading out
    QPainterPath area;
    area.moveTo(pts.first().x(), plot.bottom());
    for (const QPointF &pt : pts) area.lineTo(pt);
    area.lineTo(pts.last().x(), plot.bottom());
    area.closeSubpath();
    QLinearGradient grad(QPointF(0, plot.top()), QPointF(0, plot.bottom()));
    QColor top = Theme::Blue; top.setAlphaF(0.38);
    QColor bot = Theme::Blue; bot.setAlphaF(0.02);
    grad.setColorAt(0.0, top);
    grad.setColorAt(1.0, bot);
    p.setPen(Qt::NoPen);
    p.setBrush(grad);
    p.drawPath(area);

    p.setPen(QPen(Theme::BlueBright, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    QPainterPath path;
    path.moveTo(pts[0]);
    for (int i = 1; i < pts.size(); ++i) path.lineTo(pts[i]);
    p.drawPath(path);

    // endpoint marker in the domain accent
    p.setBrush(Theme::BlueBright);
    p.setPen(Qt::NoPen);
    p.drawEllipse(pts.last(), 2.2, 2.2);

    // explicit hi/lo readouts with words, not bare numbers; LO sits directly
    // under HI, both right-aligned to the same edge
    p.setFont(Theme::sans(11, true));
    const QFontMetrics chipFm(p.font());
    const QString hiTxt = QStringLiteral("HI %1").arg(qRound(hi));
    const QString loTxt = QStringLiteral("LO %1").arg(qRound(lo));
    auto chipText = [&](const QRectF &box, const QString &txt) {
        p.fillRect(box, Theme::CardBg);
        p.setPen(Theme::TextSoft);
        p.drawText(box, Qt::AlignRight | Qt::AlignVCenter, txt);
    };
    chipText(QRectF(band.right() - chipFm.horizontalAdvance(hiTxt) - 10, band.top() + 1,
                    chipFm.horizontalAdvance(hiTxt) + 8, 16), hiTxt);
    chipText(QRectF(band.right() - chipFm.horizontalAdvance(loTxt) - 10, band.top() + 20,
                    chipFm.horizontalAdvance(loTxt) + 8, 16), loTxt);
}
