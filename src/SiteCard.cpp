#include "SiteCard.h"
#include "Theme.h"

#include <QDateTime>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QTimeZone>

namespace {

constexpr int kPad = 16;

struct BadgeLook { QString text; QColor color; };

// Fixed severity colors: clear/ok green, Info blue, Warn amber, Crit red.
// Pending is a gap (empty text), not a status claim.
BadgeLook badgeLook(CardBadge b)
{
    switch (b) {
    case CardBadge::Live:    return {QStringLiteral("LIVE"),   Theme::GreenBright};
    case CardBadge::Quiet:   return {QStringLiteral("QUIET"),  Theme::BlueBright};
    case CardBadge::Stale:   return {QStringLiteral("STALE"),  Theme::Amber};
    case CardBadge::Error:   return {QStringLiteral("ERROR"),  Theme::RedBright};
    case CardBadge::Paused:  return {QStringLiteral("PAUSED"), Theme::Amber};
    case CardBadge::Pending: break;
    }
    return {QString(), Theme::TextMuted};
}

QString countText(const FlightBoard &b)
{
    return b.known ? QString::number(b.flights.size()) : QStringLiteral("---");
}

} // namespace

SiteCard::SiteCard(int index, QWidget *parent)
    : QWidget(parent), m_index(index)
{
    setAutoFillBackground(false);
    setMouseTracking(false);
    setCursor(Qt::PointingHandCursor);
}

void SiteCard::setData(const SiteData &d, bool paused)
{
    m_data = d;
    m_paused = paused;
    update();
}

void SiteCard::setAirport(const Airport *a)
{
    m_airport = a ? *a : Airport();
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

    // chassis — blue halo when selected
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

    const int W = width();
    const int H = height();

    // ---- header: ICAO (large) | status badge
    const QFont icaoFont = Theme::sans(26, true);
    p.setFont(icaoFont);
    p.setPen(Theme::Text);
    p.drawText(QPoint(kPad, 42), m_data.site.code);

    const BadgeLook badge = badgeLook(cardBadge(m_data, m_paused));
    const QFont badgeFont = Theme::sans(11, true, true);
    if (badge.text.isEmpty()) {
        const QRectF r(W - kPad - 64, 16, 64, 24);
        p.setPen(QPen(Theme::Grid, 1, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(r, 12, 12);
        p.setFont(badgeFont);
        p.setPen(Theme::TextMuted);
        p.drawText(r, Qt::AlignCenter, QStringLiteral("---"));
    } else {
        const int bw = QFontMetrics(badgeFont).horizontalAdvance(badge.text) + 26;
        Theme::chip(p, QRectF(W - kPad - bw, 16, bw, 24), badge.text,
                    badge.color, badgeFont);
    }

    // ---- city + country (text only; gaps when metadata is missing)
    const int textW = W - 2 * kPad;
    p.setFont(Theme::sans(15, true));
    const bool haveCity = !m_airport.city.isEmpty();
    p.setPen(haveCity ? Theme::Text : Theme::TextMuted);
    p.drawText(QPoint(kPad, 70),
               QFontMetrics(p.font()).elidedText(
                   haveCity ? m_airport.city : QStringLiteral("---"),
                   Qt::ElideRight, textW));
    p.setFont(Theme::sans(12));
    const bool haveCountry = !m_airport.country.isEmpty();
    p.setPen(haveCountry ? Theme::TextSoft : Theme::TextMuted);
    p.drawText(QPoint(kPad, 90),
               QFontMetrics(p.font()).elidedText(
                   haveCountry ? m_airport.country.toUpper() : QStringLiteral("---"),
                   Qt::ElideRight, textW));

    p.setPen(QPen(Theme::Grid, 1));
    p.drawLine(kPad, 104, W - kPad, 104);

    // ---- local clock: only with a valid IANA zone; otherwise nothing
    const QTimeZone tz(m_airport.timeZone.toUtf8());
    if (!m_airport.timeZone.isEmpty() && tz.isValid()) {
        const QDateTime now = QDateTime::currentDateTimeUtc();
        const QDateTime local = now.toTimeZone(tz);
        p.setFont(Theme::sans(11));
        p.setPen(Theme::Text);
        p.drawText(QPoint(kPad, 126), QStringLiteral("LOCAL TIME"));
        const QFont clockFont = Theme::sans(26, true);
        const QString hm = local.time().toString(QStringLiteral("HH:mm"));
        p.setFont(clockFont);
        p.setPen(Theme::Text);
        p.drawText(QPoint(kPad, 158), hm);
        const QString abbr = tz.abbreviation(now);
        if (!abbr.isEmpty()) {
            p.setFont(Theme::sans(12, true));
            p.setPen(Theme::TextMuted);
            p.drawText(QPoint(kPad + QFontMetrics(clockFont).horizontalAdvance(hm) + 10, 158),
                       abbr);
        }
    }

    // ---- arrivals | departures counts; unmeasured board = "---", never 0
    struct Col { const char *label; QString val; };
    const Col cols[2] = {
        {"ARRIVALS",   countText(m_data.current.arrivals)},
        {"DEPARTURES", countText(m_data.current.departures)},
    };
    const int colX[2] = {kPad, W / 2};
    for (int i = 0; i < 2; ++i) {
        p.setFont(Theme::sans(11, true));
        p.setPen(Theme::Text);
        p.drawText(QPoint(colX[i], 196), QLatin1String(cols[i].label));
        p.setFont(Theme::sans(26, true));
        p.setPen(cols[i].val == QLatin1String("---") ? Theme::TextMuted
                                                      : Theme::BlueBright);
        p.drawText(QPoint(colX[i], 230), cols[i].val);
    }

    // ---- updated-ago stamp: only after a real fetch; minute-grained
    if (m_data.boardsFetchedUtc.isValid()) {
        const qint64 secs = qMax<qint64>(
            0, m_data.boardsFetchedUtc.secsTo(QDateTime::currentDateTimeUtc()));
        QString ago;
        if (secs < 60)               ago = QStringLiteral("<1 MIN");
        else if (secs < 120 * 60)    ago = QStringLiteral("%1 MIN").arg(secs / 60);
        else if (secs < 48 * 3600)   ago = QStringLiteral("%1 H").arg(secs / 3600);
        else                         ago = QStringLiteral("%1 D").arg(secs / 86400);
        p.setFont(Theme::sans(11));
        p.setPen(Theme::TextMuted);
        p.drawText(QPoint(kPad, H - 14), QStringLiteral("UPDATED %1 AGO").arg(ago));
    }
}
