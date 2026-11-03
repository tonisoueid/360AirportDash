#pragma once

#include <QColor>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QRect>

// ----------------------------------------------------------------------------
// Design system: deep charcoal-blue chassis, white primary text over muted
// gray labels, subtle solid grids, and exactly ONE saturated accent per data
// domain. The shipped example maps:
//   load/primary orange · latency/network blue · status/ok green
//   warnings amber · critical red · derived early-warning rules magenta
// Purple and cyan are spare accents for additional domains you add.
// See docs/design-system.md before changing anything here.
// ----------------------------------------------------------------------------
namespace Theme {

// chassis
inline const QColor Bg        {0x0b, 0x0e, 0x13};
inline const QColor BgDeep    {0x08, 0x0a, 0x0f};
inline const QColor PanelBg   {0x11, 0x16, 0x1d};
inline const QColor PanelBgHi {0x17, 0x1d, 0x26};
inline const QColor CardBg    {0x12, 0x18, 0x20};
inline const QColor Border    {0x25, 0x2c, 0x36};
inline const QColor BorderHi  {0x3c, 0x46, 0x54};
inline const QColor Grid      {0x26, 0x30, 0x3c};
inline const QColor GridSoft  {0x21, 0x2a, 0x36};

// text — four contrast tiers, all deliberately bright on the chassis
inline const QColor Text      {0xec, 0xf2, 0xf8};   // primary white
inline const QColor TextSoft  {0xd0, 0xda, 0xe4};   // body gray
inline const QColor TextMuted {0xa9, 0xb5, 0xc2};   // secondary labels
inline const QColor TextFaint {0x8a, 0x96, 0xa3};   // faint labels, axes

// domain accents
inline const QColor Blue        {0x58, 0xa6, 0xff}; // latency, network
inline const QColor BlueBright  {0x79, 0xc0, 0xff};
inline const QColor Green       {0x3f, 0xb9, 0x50}; // status, ok state
inline const QColor GreenBright {0x56, 0xd3, 0x64};
inline const QColor Amber       {0xe3, 0xb3, 0x41}; // warnings
inline const QColor Orange      {0xf0, 0x88, 0x3e}; // primary metric (load)
inline const QColor OrangeBright{0xff, 0xa6, 0x57};
inline const QColor Red         {0xf8, 0x51, 0x49}; // critical
inline const QColor RedBright   {0xff, 0x7b, 0x72};
inline const QColor Cyan        {0x56, 0xd4, 0xdd}; // spare domain accent
inline const QColor Purple      {0xb4, 0x8c, 0xea}; // spare domain accent
inline const QColor PurpleBright{0xd2, 0xa8, 0xff};
inline const QColor Magenta     {0xf0, 0x6e, 0xc0}; // derived early warnings (FCST)

inline QFont sans(int px, bool bold = false, bool spaced = false)
{
    QFont f(QGuiApplication::font().family());
    if (f.family().isEmpty())
        f = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    // global legibility floor: never render below 10 px, everything +20%
    f.setPixelSize(qMax(10, int(px * 1.2 + 0.5)));
    f.setBold(bold);
    if (spaced)
        f.setLetterSpacing(QFont::AbsoluteSpacing, qMax(1, f.pixelSize() / 8));
    return f;
}

// legacy alias for numeric readouts that still want tabular feel
inline QFont mono(int px, bool bold = false, bool spaced = false)
{ return sans(px, bold, spaced); }

inline void text(QPainter &p, int x, int y, const QString &s, const QFont &f,
                 const QColor &c, double glow = 0.0)
{
    Q_UNUSED(glow);
    p.setFont(f);
    p.setPen(c);
    p.drawText(QPoint(x, y), s);
}

// Card chassis: rounded fill + hairline border. Glow when highlighted.
inline void panelBody(QPainter &p, const QRect &r, qreal radius = 10,
                      const QColor &fill = PanelBg, const QColor &border = Border,
                      qreal borderWidth = 1)
{
    p.setPen(Qt::NoPen);
    p.setBrush(fill);
    p.drawRoundedRect(r.adjusted(1, 1, -1, -1), radius, radius);
    if (borderWidth > 0) {
        p.setBrush(Qt::NoBrush);
        QPen pen(border, borderWidth);
        p.setPen(pen);
        p.drawRoundedRect(r.adjusted(1, 1, -2, -2), radius, radius);
    }
}

// Outlined pill chip with a faint tinted fill — the reference tag style.
inline void chip(QPainter &p, const QRectF &r, const QString &s, const QColor &c,
                 const QFont &f)
{
    p.setFont(f);
    QColor bg = c; bg.setAlphaF(0.13);
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(r, r.height() / 2, r.height() / 2);
    QColor ln = c; ln.setAlphaF(0.55);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(ln, 1));
    p.drawRoundedRect(r.adjusted(0, 0, -1, -1), r.height() / 2, r.height() / 2);
    p.setPen(c);
    p.drawText(r, Qt::AlignCenter, s);
}

// EXAMPLE domain ramp: color for the 0–100 load percentage (low = calm,
// high = hot). When you add your own metric, write a sibling function and
// keep the domain's single accent consistent everywhere it appears.
inline QColor loadColor(double pct)
{
    if (pct < 0)   return TextMuted;   // missing-data convention
    if (pct <= 40) return Green;
    if (pct <= 60) return Amber;
    if (pct <= 80) return Orange;
    return Red;
}

// Segment i of n (0 = coldest) on a segmented gauge: smooth interpolation
// across the ramp stops so the column reads as a gradient.
inline QColor gaugeRamp(int i, int n)
{
    static const QColor stops[] = {Blue, Cyan, Green, Amber, Orange, Red};
    const int ns = 6;
    const double x = (n <= 1) ? 1.0 : double(i) / double(n - 1) * (ns - 1);
    const int k = qMin(ns - 2, int(x));
    const double f = x - k;
    const QColor &a = stops[k], &b = stops[k + 1];
    return QColor(a.red() + (b.red() - a.red()) * f,
                  a.green() + (b.green() - a.green()) * f,
                  a.blue() + (b.blue() - a.blue()) * f);
}

} // namespace Theme
