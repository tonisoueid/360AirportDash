#pragma once

#include "Theme.h"
#include "widgets/IconGlyph.h"

#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QTransform>

// ----------------------------------------------------------------------------
// Application icon, painted (not a bitmap asset, matching the rest of the
// console): a dark rounded tile carrying the two things the template is —
//   a segmented cold→hot spectrum ring (the signature gauge ramp) · an
//   activity pulse in the status green (a live telemetry feed)
// All artwork geometry is verbatim Bootstrap Icons v1.11.3 path data
// (Glyph::Activity via GlyphPaint), in the app palette. Redraw freely — the
// icon renders itself at every needed size from this painter.
// ----------------------------------------------------------------------------
namespace AppIcon {

// Fill a path authored in the upstream 16×16 viewBox into a square of the
// given side, centered on the given point.
inline void fillGlyph(QPainter &p, const QPainterPath &path16,
                      const QPointF &center, double side, const QColor &color)
{
    QTransform t;
    t.translate(center.x() - side / 2.0, center.y() - side / 2.0);
    t.scale(side / 16.0, side / 16.0);
    p.fillPath(t.map(path16), color);
}

inline void paint(QPainter &p, int size)
{
    const double S = size;
    const QPointF c(S / 2.0, S / 2.0);

    // chassis: rounded dark tile, same family as the panel cards
    p.setPen(QPen(Theme::BorderHi, qMax(1.0, S / 128.0)));
    p.setBrush(Theme::PanelBg);
    p.drawRoundedRect(QRectF(0.5, 0.5, S - 1.0, S - 1.0), S * 0.225, S * 0.225);

    // segmented spectrum ring — cold→hot like the card gauges, with a gap at
    // the bottom where the pulse mark sits (coarser at tiny sizes so the
    // segments stay distinct)
    const int segs = S < 48 ? 16 : 24;
    const double spanEach = 300.0 / segs;
    const double r = S * 0.400;
    for (int i = 0; i < segs; ++i) {
        QPen pen(Theme::gaugeRamp(i, segs), qMax(1.2, S * 0.052));
        pen.setCapStyle(Qt::RoundCap);
        p.setPen(pen);
        p.drawArc(QRectF(c.x() - r, c.y() - r, 2 * r, 2 * r),
                  int((300.0 - i * spanEach - 2.5) * 16),
                  int(-(spanEach - 5.0) * 16));
    }

    // live telemetry pulse in the status green
    fillGlyph(p, GlyphPaint::glyphPath(GlyphPaint::Glyph::Activity),
              c, S * 0.52, Theme::GreenBright);
}

inline QPixmap pixmap(int size)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    paint(p, size);
    return pm;
}

inline QIcon icon()
{
    QIcon ic;
    for (int s : {16, 24, 32, 48, 64, 128, 256, 512})
        ic.addPixmap(pixmap(s));
    return ic;
}

} // namespace AppIcon
