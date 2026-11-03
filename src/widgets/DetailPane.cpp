#include "DetailPane.h"
#include "FlightTable.h"
#include "../Theme.h"

#include <QDateTime>
#include <QEvent>
#include <QFontMetrics>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QTimeZone>
#include <QToolTip>

namespace {
constexpr int kPad = 14;
constexpr int kFilterH = 28;
constexpr int kBtnW = 96, kBtnH = 26;

QString orGap(const QString &s) { return s.isEmpty() ? QStringLiteral("---") : s; }
}

DetailPane::DetailPane(QWidget *parent) : QWidget(parent)
{
    setMouseTracking(true);

    // Minimal field: no stylesheet — palette + hand-painted frame (filterFrame).
    m_filter = new QLineEdit(this);
    m_filter->setFrame(false);
    m_filter->setClearButtonEnabled(false);
    m_filter->setPlaceholderText(QStringLiteral("Filter flights (callsign, airport, aircraft)"));
    m_filter->setFont(Theme::sans(12));
    QPalette pal = m_filter->palette();
    pal.setColor(QPalette::Base, Theme::CardBg);
    pal.setColor(QPalette::Text, Theme::Text);
    pal.setColor(QPalette::PlaceholderText, Theme::TextFaint);
    pal.setColor(QPalette::Highlight, Theme::Blue);
    pal.setColor(QPalette::HighlightedText, Theme::Text);
    m_filter->setPalette(pal);
    m_filter->setAutoFillBackground(false);

    m_arr = new FlightTable(this);
    m_dep = new FlightTable(this);
    // Two tables stacked in a short pane: allow them to shrink below the
    // standalone minimum; the gap/empty states still fit.
    m_arr->setMinimumHeight(96);
    m_dep->setMinimumHeight(96);

    connect(m_filter, &QLineEdit::textChanged, this, [this](const QString &t) {
        m_arr->setFilter(t);
        m_dep->setFilter(t);
    });
    clear();
}

QString DetailPane::filter() const { return m_filter->text(); }

QRect DetailPane::refreshRect() const
{
    return QRect(width() - kPad - kBtnW, 10, kBtnW, kBtnH);
}

QRect DetailPane::filterFrame() const
{
    return QRect(kPad, headerHeight() + 2, width() - 2 * kPad, kFilterH);
}

void DetailPane::layoutChildren()
{
    const QRect ff = filterFrame();
    m_filter->setGeometry(ff.adjusted(10, 3, -10, -3));
    const int top = ff.bottom() + 8;
    const int avail = qMax(0, height() - top - 6);
    const int gap = 8;
    const int h1 = (avail - gap) / 2;
    m_arr->setGeometry(kPad, top, width() - 2 * kPad, h1);
    m_dep->setGeometry(kPad, top + h1 + gap, width() - 2 * kPad, avail - gap - h1);
}

void DetailPane::resizeEvent(QResizeEvent *) { layoutChildren(); }

void DetailPane::setSite(const View &v)
{
    const SiteData &d = v.data;
    const bool rebind = !m_bound || m_code != d.site.code;
    m_bound = true;
    m_code = d.site.code;
    m_name = d.site.name;
    m_airport = v.airport;
    m_state = d.state;
    m_auth = v.authenticated;
    m_refreshEnabled = v.refreshEnabled;
    m_refreshNote = v.refreshNote;
    m_arrWindow = boardWindowLabel(d.current.arrivals);
    m_depWindow = boardWindowLabel(d.current.departures);

    if (rebind)
        m_filter->clear();   // a filter belongs to one airport
    m_filter->setEnabled(true);

    m_arr->setBoard(&d.current.arrivals, FlightTable::Mode::Arrivals);
    m_dep->setBoard(&d.current.departures, FlightTable::Mode::Departures);
    // Gap reasons: say what is actually true, never imply "no traffic".
    m_arr->setGapReason(!m_auth
        ? QStringLiteral("anonymous mode — arrivals need OpenSky credentials")
        : (d.state == SiteData::State::Error ? QStringLiteral("last fetch failed")
                                             : QString()));
    m_dep->setGapReason(d.state == SiteData::State::Error ? QStringLiteral("last fetch failed")
                                                          : QString());
    m_arr->show();
    m_dep->show();
    m_filter->show();
    update();
}

void DetailPane::clear()
{
    m_bound = false;
    m_code.clear();
    m_name.clear();
    m_airport = Airport{};
    m_arrWindow.clear();
    m_depWindow.clear();
    m_refreshHover = false;
    m_filter->clear();
    m_arr->setBoard(nullptr, FlightTable::Mode::Arrivals);
    m_dep->setBoard(nullptr, FlightTable::Mode::Departures);
    m_arr->hide();
    m_dep->hide();
    m_filter->hide();
    update();
}

void DetailPane::tick()
{
    if (!m_bound)
        return;
    m_arr->refreshTick();
    m_dep->refreshTick();
    update();
}

void DetailPane::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const int W = width();

    if (!m_bound) {
        const QFont f = Theme::sans(14, true);
        p.setFont(f);
        p.setPen(Theme::TextMuted);
        p.drawText(rect().adjusted(kPad, 0, -kPad, -20), Qt::AlignCenter,
                   QStringLiteral("No airport selected"));
        p.setFont(Theme::sans(12));
        p.setPen(Theme::TextFaint);
        p.drawText(rect().adjusted(kPad, 36, -kPad, 0), Qt::AlignCenter,
                   QStringLiteral("click a card, press 1–9 or use the arrow keys"));
        return;
    }

    // ---- line 1: airport name + refresh button
    const QRect btn = refreshRect();
    const QFont nameF = Theme::sans(16, true);
    p.setFont(nameF);
    p.setPen(Theme::Text);
    const QFontMetrics nfm(nameF);
    const QString name = m_airport.name.isEmpty() ? m_name : m_airport.name;
    const int nameW = W - 2 * kPad - kBtnW - 12;
    p.drawText(QPoint(kPad, 10 + nfm.ascent() + 2),
               nfm.elidedText(orGap(name), Qt::ElideRight, nameW));

    // refresh button: custom-painted pill; disabled = muted, no hover fill
    {
        const bool on = m_refreshEnabled;
        const QColor c = on ? Theme::BlueBright : Theme::TextFaint;
        QColor bg = c; bg.setAlphaF(on && m_refreshHover ? 0.26 : 0.10);
        QColor ln = c; ln.setAlphaF(0.55);
        p.setPen(QPen(ln, 1));
        p.setBrush(bg);
        p.drawRoundedRect(QRectF(btn).adjusted(0.5, 0.5, -0.5, -0.5), 9, 9);
        p.setFont(Theme::sans(11, true));
        p.setPen(c);
        p.drawText(btn, Qt::AlignCenter, QStringLiteral("REFRESH"));
    }

    // ---- line 2: codes · city, country
    const QFont mf = Theme::sans(12);
    const QFontMetrics mfm(mf);
    int y = 10 + nfm.height() + 6 + mfm.ascent();
    QString codes = QStringLiteral("ICAO %1").arg(m_code);
    codes += QStringLiteral("  ·  IATA %1").arg(orGap(m_airport.iata));
    QString place = m_airport.city;
    if (!m_airport.country.isEmpty())
        place += (place.isEmpty() ? QString() : QStringLiteral(", ")) + m_airport.country;
    codes += QStringLiteral("  ·  ") + orGap(place);
    p.setFont(mf);
    p.setPen(Theme::TextSoft);
    p.drawText(QPoint(kPad, y), mfm.elidedText(codes, Qt::ElideRight, W - 2 * kPad));

    // ---- line 3: provider chip + local time (only with a valid zone)
    const int chipH = mfm.height() + 4;
    const int chipY = y + mfm.descent() + 7;
    const QString prov = m_auth ? QStringLiteral("OPENSKY · AUTHENTICATED")
                                : QStringLiteral("OPENSKY · ANONYMOUS");
    const QFont cf = Theme::sans(11, true);
    const int chipW = QFontMetrics(cf).horizontalAdvance(prov) + 22;
    Theme::chip(p, QRectF(kPad, chipY, chipW, chipH), prov,
                m_auth ? Theme::Blue : Theme::TextMuted, cf);

    const QTimeZone tz(m_airport.timeZone.toUtf8());
    if (!m_airport.timeZone.isEmpty() && tz.isValid()) {
        const QDateTime now = QDateTime::currentDateTimeUtc();
        QString t = QStringLiteral("LOCAL %1").arg(
            now.toTimeZone(tz).time().toString(QStringLiteral("HH:mm")));
        const QString abbr = tz.abbreviation(now);
        if (!abbr.isEmpty())
            t += QLatin1Char(' ') + abbr;
        p.setFont(Theme::sans(13, true));
        p.setPen(Theme::Text);
        p.drawText(QPoint(kPad + chipW + 16, chipY + (chipH + QFontMetrics(Theme::sans(13, true)).ascent()) / 2 - 1), t);
    }

    // ---- line 4: stored board windows (from the feed's real queries)
    const QString windows = QStringLiteral("ARRIVALS %1  ·  DEPARTURES %2")
        .arg(m_arrWindow.isEmpty() ? QStringLiteral("window ---") : m_arrWindow,
             m_depWindow.isEmpty() ? QStringLiteral("window ---") : m_depWindow);
    p.setFont(Theme::sans(11));
    p.setPen(Theme::TextMuted);
    const QFontMetrics wfm(Theme::sans(11));
    p.drawText(QPoint(kPad, chipY + chipH + 6 + wfm.ascent()),
               wfm.elidedText(windows, Qt::ElideRight, W - 2 * kPad));

    // ---- filter frame behind the line edit
    const QRect ff = filterFrame();
    p.setPen(QPen(m_filter->hasFocus() ? Theme::Blue : Theme::BorderHi, 1));
    p.setBrush(Theme::CardBg);
    p.drawRoundedRect(QRectF(ff).adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);
}

void DetailPane::mousePressEvent(QMouseEvent *e)
{
    if (m_bound && m_refreshEnabled && e->button() == Qt::LeftButton
        && refreshRect().contains(e->pos())) {
        emit refreshRequested();
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void DetailPane::mouseMoveEvent(QMouseEvent *e)
{
    const bool h = m_bound && refreshRect().contains(e->pos());
    if (h != m_refreshHover) {
        m_refreshHover = h;
        setCursor(h && m_refreshEnabled ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update(refreshRect());
    }
    QWidget::mouseMoveEvent(e);
}

void DetailPane::leaveEvent(QEvent *e)
{
    if (m_refreshHover) {
        m_refreshHover = false;
        update(refreshRect());
    }
    QWidget::leaveEvent(e);
}

bool DetailPane::event(QEvent *e)
{
    if (e->type() == QEvent::ToolTip && m_bound) {
        auto *he = static_cast<QHelpEvent *>(e);
        if (refreshRect().contains(he->pos())) {
            QToolTip::showText(he->globalPos(),
                m_refreshEnabled ? QStringLiteral("Poll this airport now (data updates when the feed answers)")
                                 : m_refreshNote, this);
            return true;
        }
    }
    return QWidget::event(e);
}
