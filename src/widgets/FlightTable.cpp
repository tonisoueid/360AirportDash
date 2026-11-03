#include "FlightTable.h"
#include "../Theme.h"

#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QScrollBar>
#include <QToolTip>
#include <QHelpEvent>
#include <algorithm>

namespace {

const QString kDash = QStringLiteral("—");
constexpr int kPadX = 12;

// Relative column widths (sum = 100).
constexpr int kColPct[FlightTable::ColCount] = {20, 15, 15, 16, 16, 18};

QFont titleFont()  { return Theme::sans(11, true, true); }
QFont headerFont() { return Theme::sans(10, true, true); }
QFont cellFont()   { return Theme::sans(12); }
QFont msgFont()    { return Theme::sans(14, true); }
QFont hintFont()   { return Theme::sans(11); }

} // namespace

FlightTable::FlightTable(QWidget *parent) : QWidget(parent)
{
    setAutoFillBackground(false);
    setMouseTracking(true);
    m_bar = new QScrollBar(Qt::Vertical, this);
    m_bar->hide();
    QPalette pal = m_bar->palette();   // keep the native bar on-theme (no stylesheet)
    pal.setColor(QPalette::Window, Theme::PanelBg);
    pal.setColor(QPalette::Button, Theme::BorderHi);
    pal.setColor(QPalette::Mid, Theme::Border);
    pal.setColor(QPalette::Dark, Theme::Border);
    pal.setColor(QPalette::Light, Theme::BorderHi);
    m_bar->setPalette(pal);
    connect(m_bar, &QScrollBar::valueChanged, this, [this] { update(); });
}

int FlightTable::rowHeight() const    { return QFontMetrics(cellFont()).height() + 10; }
int FlightTable::headerHeight() const { return QFontMetrics(headerFont()).height() + 12; }
int FlightTable::titleHeight() const  { return QFontMetrics(titleFont()).height() + 16; }

QRect FlightTable::bodyRect() const
{
    const int top = titleHeight() + headerHeight();
    const int w = width() - (m_bar->isVisible() ? m_bar->width() : 0);
    return QRect(0, top, w, qMax(0, height() - top));
}

int FlightTable::columnX(int c) const
{
    const int w = bodyRect().width();
    int x = 0;
    for (int i = 0; i < c; ++i) x += w * kColPct[i] / 100;
    return x;
}

int FlightTable::columnW(int c) const
{
    const int w = bodyRect().width();
    return c == ColCount - 1 ? w - columnX(c) : w * kColPct[c] / 100;
}

int FlightTable::columnAt(int x) const
{
    for (int c = ColCount - 1; c >= 0; --c)
        if (x >= columnX(c)) return c;
    return 0;
}

QString FlightTable::placeTitle() const
{
    return m_mode == Mode::Arrivals ? QStringLiteral("FROM") : QStringLiteral("TO");
}

void FlightTable::setBoard(const FlightBoard *board, Mode mode)
{
    m_mode = mode;
    m_rows.clear();
    m_known = board && board->known;
    m_window = board ? boardWindowLabel(*board) : QString();
    if (m_known) {
        m_rows.reserve(board->flights.size());
        for (const FlightEntry &e : board->flights) {
            Row r;
            const QString place = mode == Mode::Arrivals ? cellFrom(e) : cellTo(e);
            r.cell[ColFlight]    = cellCallsign(e);
            r.cell[ColPlace]     = place;
            r.cell[ColAircraft]  = cellIcao24(e);
            r.cell[ColFirstSeen] = cellFirstSeen(e);
            r.cell[ColLastSeen]  = cellLastSeen(e);
            r.cell[ColStatus]    = flightStatus(e);
            r.placeUnknown = (place == kDash);
            for (qint64 &k : r.key) k = -1;
            if (e.firstSeen.isValid()) r.key[ColFirstSeen] = e.firstSeen.toSecsSinceEpoch();
            if (e.lastSeen.isValid())  r.key[ColLastSeen]  = e.lastSeen.toSecsSinceEpoch();
            m_rows.push_back(r);
        }
    }
    m_bar->setValue(0);
    rebuildOrder();
}

void FlightTable::setGapReason(const QString &reason)
{
    m_gapReason = reason;
    update();
}

void FlightTable::setFilter(const QString &text)
{
    if (text == m_filter) return;
    m_filter = text;
    rebuildOrder();
}

void FlightTable::setSort(int column, bool ascending)
{
    if (column < 0 || column >= ColCount) return;
    m_sortCol = column;
    m_sortAsc = ascending;
    rebuildOrder();
}

void FlightTable::rebuildOrder()
{
    m_order.clear();
    const QString needle = m_filter.trimmed();
    for (int i = 0; i < m_rows.size(); ++i) {
        bool hit = needle.isEmpty();
        for (int c = 0; !hit && c < ColCount; ++c)
            hit = m_rows[i].cell[c].contains(needle, Qt::CaseInsensitive);
        if (hit) m_order.push_back(i);
    }
    const int col = m_sortCol;
    const bool asc = m_sortAsc;
    const bool timeCol = (col == ColFirstSeen || col == ColLastSeen);
    // Unknown cells always sink to the bottom regardless of direction.
    std::stable_sort(m_order.begin(), m_order.end(), [&](int a, int b) {
        const Row &ra = m_rows[a], &rb = m_rows[b];
        const bool ua = timeCol ? ra.key[col] < 0 : ra.cell[col] == kDash;
        const bool ub = timeCol ? rb.key[col] < 0 : rb.cell[col] == kDash;
        if (ua != ub) return ub;          // known before unknown
        if (ua) return false;
        const int cmp = timeCol ? (ra.key[col] < rb.key[col] ? -1 : ra.key[col] > rb.key[col])
                                : QString::compare(ra.cell[col], rb.cell[col], Qt::CaseInsensitive);
        return asc ? cmp < 0 : cmp > 0;
    });
    m_hoverRow = -1;
    updateScroll();
    update();
}

void FlightTable::updateScroll()
{
    const int bodyH = qMax(0, height() - titleHeight() - headerHeight());
    const int total = m_order.size() * rowHeight();
    const bool need = hasRows() && total > bodyH;
    m_bar->setVisible(need);
    if (need) {
        const int top = titleHeight() + headerHeight();
        m_bar->setGeometry(width() - m_bar->sizeHint().width(), top,
                           m_bar->sizeHint().width(), bodyH);
        m_bar->setRange(0, total - bodyH);
        m_bar->setPageStep(bodyH);
        m_bar->setSingleStep(rowHeight());
    } else {
        m_bar->setRange(0, 0);
    }
}

void FlightTable::resizeEvent(QResizeEvent *)
{
    updateScroll();
}

int FlightTable::rowAt(const QPoint &pos) const
{
    const QRect body = bodyRect();
    if (!body.contains(pos) || !hasRows()) return -1;
    const int idx = (pos.y() - body.top() + m_bar->value()) / rowHeight();
    return idx >= 0 && idx < m_order.size() ? idx : -1;
}

void FlightTable::mousePressEvent(QMouseEvent *e)
{
    const int hy = titleHeight();
    if (e->button() == Qt::LeftButton && e->pos().y() >= hy
        && e->pos().y() < hy + headerHeight() && e->pos().x() < bodyRect().width()) {
        const int c = columnAt(e->pos().x());
        if (c == m_sortCol) setSort(c, !m_sortAsc);
        else setSort(c, true);
        return;
    }
    QWidget::mousePressEvent(e);
}

void FlightTable::mouseMoveEvent(QMouseEvent *e)
{
    const int row = rowAt(e->pos());
    const int hy = titleHeight();
    int hdr = -1;
    if (e->pos().y() >= hy && e->pos().y() < hy + headerHeight()
        && e->pos().x() < bodyRect().width())
        hdr = columnAt(e->pos().x());
    if (row != m_hoverRow || hdr != m_hoverHeader) {
        m_hoverRow = row;
        m_hoverHeader = hdr;
        setCursor(hdr >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
}

void FlightTable::leaveEvent(QEvent *)
{
    if (m_hoverRow != -1 || m_hoverHeader != -1) {
        m_hoverRow = m_hoverHeader = -1;
        update();
    }
}

void FlightTable::wheelEvent(QWheelEvent *e)
{
    if (m_bar->isVisible()) {
        QCoreApplication::sendEvent(m_bar, e);
        e->accept();
    } else {
        QWidget::wheelEvent(e);
    }
}

bool FlightTable::event(QEvent *e)
{
    if (e->type() == QEvent::ToolTip) {
        auto *he = static_cast<QHelpEvent *>(e);
        const int row = rowAt(he->pos());
        if (row >= 0 && columnAt(he->pos().x()) == ColPlace
            && m_rows[m_order[row]].placeUnknown) {
            QToolTip::showText(he->globalPos(),
                QStringLiteral("%1 not reported: ADS-B only carries an estimated airport when "
                               "OpenSky could infer one from the track, so it is often unknown.")
                    .arg(m_mode == Mode::Arrivals ? QStringLiteral("Origin")
                                                  : QStringLiteral("Destination")), this);
        } else {
            QToolTip::hideText();
            e->ignore();
        }
        return true;
    }
    return QWidget::event(e);
}

void FlightTable::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.fillRect(rect(), Theme::PanelBg);

    const int tH = titleHeight(), hH = headerHeight(), rH = rowHeight();

    // --- title strip
    const QString what = m_mode == Mode::Arrivals ? QStringLiteral("ARRIVALS")
                                                  : QStringLiteral("DEPARTURES");
    {
        const QFontMetrics fm(titleFont());
        const int base = (tH + fm.ascent() - fm.descent()) / 2;
        Theme::text(p, kPadX, base, what, titleFont(), Theme::TextMuted);
        if (!m_window.isEmpty()) {
            const QFontMetrics wf(hintFont());
            const int w = wf.horizontalAdvance(m_window);
            Theme::text(p, width() - kPadX - w, (tH + wf.ascent() - wf.descent()) / 2,
                        m_window, hintFont(), Theme::TextSoft);
        }
        p.setPen(Theme::Border);
        p.drawLine(0, tH - 1, width(), tH - 1);
    }

    // --- header
    {
        const QFontMetrics fm(headerFont());
        const int base = tH + (hH + fm.ascent() - fm.descent()) / 2;
        const QString titles[ColCount] = {
            QStringLiteral("FLIGHT"), placeTitle(), QStringLiteral("AIRCRAFT"),
            QStringLiteral("FIRST SEEN"), QStringLiteral("LAST SEEN"), QStringLiteral("STATUS")};
        const int tableW = bodyRect().width();
        p.fillRect(QRect(0, tH, tableW, hH), Theme::PanelBgHi);
        for (int c = 0; c < ColCount; ++c) {
            const bool sorted = (c == m_sortCol) && hasRows();
            const QColor col = sorted || c == m_hoverHeader ? Theme::Text : Theme::TextMuted;
            const int x = columnX(c) + kPadX;
            Theme::text(p, x, base, titles[c], headerFont(), col);
            if (sorted) {
                const int ax = x + fm.horizontalAdvance(titles[c]) + 8;
                const int cy = tH + hH / 2;
                QPainterPath tri;
                if (m_sortAsc) { tri.moveTo(ax, cy + 3); tri.lineTo(ax + 8, cy + 3); tri.lineTo(ax + 4, cy - 3); }
                else           { tri.moveTo(ax, cy - 3); tri.lineTo(ax + 8, cy - 3); tri.lineTo(ax + 4, cy + 3); }
                tri.closeSubpath();
                p.setPen(Qt::NoPen);
                p.setBrush(Theme::Blue);
                p.drawPath(tri);
            }
        }
        p.setPen(Theme::Border);
        p.drawLine(0, tH + hH - 1, tableW, tH + hH - 1);
    }

    const QRect body = bodyRect();
    if (body.height() <= 0) return;

    // --- gap / empty states
    auto message = [&](const QString &head, const QString &hint, const QColor &c) {
        const QFontMetrics mf(msgFont()), hf(hintFont());
        const int total = mf.height() + (hint.isEmpty() ? 0 : hf.height() + 6);
        int y = body.top() + (body.height() - total) / 2 + mf.ascent();
        const int hw = mf.horizontalAdvance(head);
        Theme::text(p, body.left() + (body.width() - hw) / 2, y, head, msgFont(), c);
        if (!hint.isEmpty()) {
            y += mf.descent() + 6 + hf.ascent();
            const QString h = hf.elidedText(hint, Qt::ElideRight, body.width() - 2 * kPadX);
            Theme::text(p, body.left() + (body.width() - hf.horizontalAdvance(h)) / 2, y, h,
                        hintFont(), Theme::TextMuted);
        }
    };
    if (!m_known) {
        // Dashed frame: an explicit gap, visually distinct from "no flights".
        QPen dash(Theme::BorderHi, 1, Qt::DashLine);
        p.setPen(dash);
        p.setBrush(Qt::NoBrush);
        p.drawRect(body.adjusted(kPadX, kPadX, -kPadX, -kPadX));
        message(QStringLiteral("no %1 data").arg(what.toLower()),
                m_gapReason.isEmpty()
                    ? QStringLiteral("board not measured yet, or the last fetch failed")
                    : m_gapReason,
                Theme::Amber);
        return;
    }
    if (m_rows.isEmpty()) {
        message(QStringLiteral("no flights in window"),
                m_window.isEmpty() ? QString() : m_window, Theme::BlueBright);
        return;
    }
    if (m_order.isEmpty()) {
        message(QStringLiteral("no matches"),
                QStringLiteral("filter “%1” hides all %2 flights")
                    .arg(m_filter.trimmed()).arg(m_rows.size()),
                Theme::TextSoft);
        return;
    }

    // --- visible rows only
    p.setClipRect(body);
    const QFontMetrics fm(cellFont());
    const int off = m_bar->value();
    const int first = off / rH;
    const int last = qMin(m_order.size() - 1, (off + body.height()) / rH);
    for (int i = first; i <= last; ++i) {
        const int y = body.top() + i * rH - off;
        if (i == m_hoverRow)
            p.fillRect(QRect(0, y, body.width(), rH), Theme::PanelBgHi);
        const Row &r = m_rows[m_order[i]];
        const int base = y + (rH + fm.ascent() - fm.descent()) / 2;
        for (int c = 0; c < ColCount; ++c) {
            const QString &s = r.cell[c];
            const bool unk = (s == kDash);
            QColor col = unk ? Theme::TextFaint : (c == ColFlight ? Theme::Text : Theme::TextSoft);
            const QString t = fm.elidedText(s, Qt::ElideRight, columnW(c) - 2 * kPadX);
            Theme::text(p, columnX(c) + kPadX, base, t, cellFont(), col);
        }
        p.setPen(Theme::GridSoft);
        p.drawLine(0, y + rH - 1, body.width(), y + rH - 1);
    }
}
