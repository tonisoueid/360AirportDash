#pragma once

#include "../Theme.h"

#include <QPainter>
#include <QPainterPath>

#include <cmath>
#include <cstdlib>

// ----------------------------------------------------------------------------
// Line-art glyphs — Bootstrap Icons v1.11.3, © 2019-2024 The Bootstrap
// Authors (MIT license, https://icons.getbootstrap.com). The SVG path data
// below is embedded verbatim from the upstream icon files, so the artwork is
// byte-identical to the published open-source set rather than a hand-drawn
// approximation. A small deterministic parser converts it to a QPainterPath,
// filled with a theme color.
//
// Rule (do not regress): never freehand icon geometry. When you change or add
// artwork, pull the pinned-version 'd' attribute from
// https://github.com/twbs/icons/tree/v1.11.3/icons and paste it verbatim.
// ----------------------------------------------------------------------------
namespace GlyphPaint {

enum class Glyph { None, Activity, Cpu, Wifi, Warn };

// pick the accent color per glyph when the caller doesn't override
inline QColor baseColor(Glyph g)
{
    switch (g) {
    case Glyph::Activity: return Theme::Green;   // live pulse
    case Glyph::Cpu:      return Theme::Blue;    // a node
    case Glyph::Wifi:     return Theme::Green;   // link up
    case Glyph::Warn:     return Theme::Amber;   // watch-rule warning
    default:              return Theme::TextFaint;
    }
}

// Verbatim 'd' attributes from bootstrap-icons@1.11.3 (16×16 viewBox):
// activity, cpu, wifi (its three subpaths concatenated — each starts with an
// absolute M so they parse as separate subpaths), exclamation-triangle-fill,
// graph-up-arrow.
inline const char *pathData(Glyph g)
{
    switch (g) {
    case Glyph::Activity:
        return R"BI(M6 2a.5.5 0 0 1 .47.33L10 12.036l1.53-4.208A.5.5 0 0 1 12 7.5h3.5a.5.5 0 0 1 0 1h-3.15l-1.88 5.17a.5.5 0 0 1-.94 0L6 3.964 4.47 8.171A.5.5 0 0 1 4 8.5H.5a.5.5 0 0 1 0-1h3.15l1.88-5.17A.5.5 0 0 1 6 2)BI";
    case Glyph::Cpu:
        return R"BI(M5 0a.5.5 0 0 1 .5.5V2h1V.5a.5.5 0 0 1 1 0V2h1V.5a.5.5 0 0 1 1 0V2h1V.5a.5.5 0 0 1 1 0V2A2.5 2.5 0 0 1 14 4.5h1.5a.5.5 0 0 1 0 1H14v1h1.5a.5.5 0 0 1 0 1H14v1h1.5a.5.5 0 0 1 0 1H14v1h1.5a.5.5 0 0 1 0 1H14a2.5 2.5 0 0 1-2.5 2.5v1.5a.5.5 0 0 1-1 0V14h-1v1.5a.5.5 0 0 1-1 0V14h-1v1.5a.5.5 0 0 1-1 0V14h-1v1.5a.5.5 0 0 1-1 0V14A2.5 2.5 0 0 1 2 11.5H.5a.5.5 0 0 1 0-1H2v-1H.5a.5.5 0 0 1 0-1H2v-1H.5a.5.5 0 0 1 0-1H2v-1H.5a.5.5 0 0 1 0-1H2A2.5 2.5 0 0 1 4.5 2V.5A.5.5 0 0 1 5 0m-.5 3A1.5 1.5 0 0 0 3 4.5v7A1.5 1.5 0 0 0 4.5 13h7a1.5 1.5 0 0 0 1.5-1.5v-7A1.5 1.5 0 0 0 11.5 3zM5 6.5A1.5 1.5 0 0 1 6.5 5h3A1.5 1.5 0 0 1 11 6.5v3A1.5 1.5 0 0 1 9.5 11h-3A1.5 1.5 0 0 1 5 9.5zM6.5 6a.5.5 0 0 0-.5.5v3a.5.5 0 0 0 .5.5h3a.5.5 0 0 0 .5-.5v-3a.5.5 0 0 0-.5-.5z)BI";
    case Glyph::Wifi:
        return R"BI(M15.384 6.115a.485.485 0 0 0-.047-.736A12.44 12.44 0 0 0 8 3C5.259 3 2.723 3.882.663 5.379a.485.485 0 0 0-.048.736.52.52 0 0 0 .668.05A11.45 11.45 0 0 1 8 4c2.507 0 4.827.802 6.716 2.164.205.148.49.13.668-.049 M13.229 8.271a.482.482 0 0 0-.063-.745A9.46 9.46 0 0 0 8 6c-1.905 0-3.68.56-5.166 1.526a.48.48 0 0 0-.063.745.525.525 0 0 0 .652.065A8.46 8.46 0 0 1 8 7a8.46 8.46 0 0 1 4.576 1.336c.206.132.48.108.653-.065m-2.183 2.183c.226-.226.185-.605-.1-.75A6.5 6.5 0 0 0 8 9c-1.06 0-2.062.254-2.946.704-.285.145-.326.524-.1.75l.015.015c.16.16.407.19.611.09A5.5 5.5 0 0 1 8 10c.868 0 1.69.201 2.42.56.203.1.45.07.61-.091zM9.06 12.44c.196-.196.198-.52-.04-.66A2 2 0 0 0 8 11.5a2 2 0 0 0-1.02.28c-.238.14-.236.464-.04.66l.706.706a.5.5 0 0 0 .707 0l.707-.707z)BI";
    case Glyph::Warn:
        return R"BI(M8.982 1.566a1.13 1.13 0 0 0-1.96 0L.165 13.233c-.457.778.091 1.767.98 1.767h13.713c.889 0 1.438-.99.98-1.767zM8 5c.535 0 .954.462.9.995l-.35 3.507a.552.552 0 0 1-1.1 0L7.1 5.995A.905.905 0 0 1 8 5m.002 6a1 1 0 1 1 0 2 1 1 0 0 1 0-2)BI";
    default: return nullptr;
    }
}

// -- SVG path 'd' → QPainterPath ---------------------------------------------
// Supports the command subset the embedded icons use — M m L l H h V v C c
// Q q A a Z z — including implicit parameter-list repetition per the SVG
// spec. Deterministic: same input always yields the same geometry.

namespace SvgPathDetail {

// SVG elliptical arc, endpoint parameterization (W3C F.6.5), approximated
// with ≤90° cubic segments whose control points ride the arc derivative.
inline void arcTo(QPainterPath &path, const QPointF &from, const QPointF &to,
                  double rx, double ry, double xRotDeg, bool largeArc, bool sweep)
{
    if (rx <= 0 || ry <= 0 || from == to) {
        path.lineTo(to);
        return;
    }
    const double phi = qDegreesToRadians(xRotDeg);
    const double cosP = std::cos(phi), sinP = std::sin(phi);
    const double hx = (from.x() - to.x()) / 2.0, hy = (from.y() - to.y()) / 2.0;
    const double x1p =  cosP * hx + sinP * hy;
    const double y1p = -sinP * hx + cosP * hy;

    const double lam = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry);
    if (lam > 1.0) {                       // endpoints too far: grow radii
        const double s = std::sqrt(lam);
        rx *= s;
        ry *= s;
    }

    const double num = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p;
    const double den = rx * rx * y1p * y1p + ry * ry * x1p * x1p;
    double co = std::sqrt(qMax(0.0, num) / (den > 0 ? den : 1.0));
    if (largeArc == sweep)
        co = -co;
    const double cxp =  co * rx * y1p / ry;
    const double cyp = -co * ry * x1p / rx;
    const double cx = cosP * cxp - sinP * cyp + (from.x() + to.x()) / 2.0;
    const double cy = sinP * cxp + cosP * cyp + (from.y() + to.y()) / 2.0;

    auto angleBetween = [](double ux, double uy, double vx, double vy) {
        const double dot = (ux * vx + uy * vy)
            / (std::sqrt(ux * ux + uy * uy) * std::sqrt(vx * vx + vy * vy) + 1e-12);
        const double a = std::acos(qBound(-1.0, dot, 1.0));
        return (ux * vy - uy * vx < 0) ? -a : a;
    };

    double t1 = angleBetween(1.0, 0.0, (x1p - cxp) / rx, (y1p - cyp) / ry);
    double dt = angleBetween((x1p - cxp) / rx, (y1p - cyp) / ry,
                             (-x1p - cxp) / rx, (-y1p - cyp) / ry);
    if (!sweep && dt > 0) dt -= 2 * M_PI;
    if (sweep && dt < 0) dt += 2 * M_PI;

    const int segs = qMax(1, int(std::ceil(std::abs(dt) / (M_PI / 2))));
    const double step = dt / segs;
    QPointF prev = from;
    for (int i = 0; i < segs; ++i) {
        const double ta = t1 + step * i;
        const double tb = ta + step;
        const QPointF end(cx + rx * cosP * std::cos(tb) - ry * sinP * std::sin(tb),
                          cy + rx * sinP * std::cos(tb) + ry * cosP * std::sin(tb));
        const QPointF d1(-rx * cosP * std::sin(ta) - ry * sinP * std::cos(ta),
                         -rx * sinP * std::sin(ta) + ry * cosP * std::cos(ta));
        const QPointF d2(-rx * cosP * std::sin(tb) - ry * sinP * std::cos(tb),
                         -rx * sinP * std::sin(tb) + ry * cosP * std::cos(tb));
        path.cubicTo(prev + d1 * (step / 3.0), end - d2 * (step / 3.0), end);
        prev = end;
    }
}

} // namespace SvgPathDetail

inline QPainterPath parse(const char *s)
{
    QPainterPath path;
    if (!s)
        return path;
    const char *c = s;
    auto skipSep = [&] {
        while (*c == ' ' || *c == ',' || *c == '\t' || *c == '\r' || *c == '\n')
            ++c;
    };
    auto num = [&](double &out) {
        skipSep();
        char *end = nullptr;
        out = std::strtod(c, &end);
        if (end == c)
            return false;
        c = end;
        return true;
    };
    auto flag = [&](bool &out) {
        skipSep();
        if (*c != '0' && *c != '1')
            return false;
        out = (*c == '1');
        ++c;
        return true;
    };

    char cmd = 0;
    double a1 = 0, a2 = 0, a3 = 0, a4 = 0, a5 = 0, a6 = 0;
    bool f1 = false, f2 = false;
    QPointF cur(0, 0), sub(0, 0);
    for (;;) {
        skipSep();
        if (*c == '\0')
            break;
        if ((*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z'))
            cmd = *c++;
        switch (cmd) {
        case 'M':
            if (!num(a1) || !num(a2)) return path;
            cur = sub = QPointF(a1, a2);
            path.moveTo(cur);
            cmd = 'L';               // implicit repeats after M are lineto
            break;
        case 'm':
            if (!num(a1) || !num(a2)) return path;
            cur += QPointF(a1, a2);
            sub = cur;
            path.moveTo(cur);
            cmd = 'l';
            break;
        case 'L':
            if (!num(a1) || !num(a2)) return path;
            cur = QPointF(a1, a2);
            path.lineTo(cur);
            break;
        case 'l':
            if (!num(a1) || !num(a2)) return path;
            cur += QPointF(a1, a2);
            path.lineTo(cur);
            break;
        case 'H':
            if (!num(a1)) return path;
            cur.setX(a1);
            path.lineTo(cur);
            break;
        case 'h':
            if (!num(a1)) return path;
            cur.rx() += a1;
            path.lineTo(cur);
            break;
        case 'V':
            if (!num(a1)) return path;
            cur.setY(a1);
            path.lineTo(cur);
            break;
        case 'v':
            if (!num(a1)) return path;
            cur.ry() += a1;
            path.lineTo(cur);
            break;
        case 'C':
            if (!num(a1) || !num(a2) || !num(a3) || !num(a4) || !num(a5) || !num(a6)) return path;
            path.cubicTo(QPointF(a1, a2), QPointF(a3, a4), QPointF(a5, a6));
            cur = QPointF(a5, a6);
            break;
        case 'c':
            if (!num(a1) || !num(a2) || !num(a3) || !num(a4) || !num(a5) || !num(a6)) return path;
            path.cubicTo(cur + QPointF(a1, a2), cur + QPointF(a3, a4), cur + QPointF(a5, a6));
            cur += QPointF(a5, a6);
            break;
        case 'Q':
            if (!num(a1) || !num(a2) || !num(a3) || !num(a4)) return path;
            path.quadTo(QPointF(a1, a2), QPointF(a3, a4));
            cur = QPointF(a3, a4);
            break;
        case 'q':
            if (!num(a1) || !num(a2) || !num(a3) || !num(a4)) return path;
            path.quadTo(cur + QPointF(a1, a2), cur + QPointF(a3, a4));
            cur += QPointF(a3, a4);
            break;
        case 'A':
            if (!num(a1) || !num(a2) || !num(a3) || !flag(f1) || !flag(f2) || !num(a4) || !num(a5)) return path;
            SvgPathDetail::arcTo(path, cur, QPointF(a4, a5), a1, a2, a3, f1, f2);
            cur = QPointF(a4, a5);
            break;
        case 'a':
            if (!num(a1) || !num(a2) || !num(a3) || !flag(f1) || !flag(f2) || !num(a4) || !num(a5)) return path;
            SvgPathDetail::arcTo(path, cur, cur + QPointF(a4, a5), a1, a2, a3, f1, f2);
            cur += QPointF(a4, a5);
            break;
        case 'Z':
        case 'z':
            path.closeSubpath();
            cur = sub;
            break;
        default:
            return path;             // unknown command: keep what parsed
        }
    }
    return path;
}

// Parsed glyph geometry, cached after first use (upstream 16×16 viewBox).
inline QPainterPath glyphPath(Glyph g)
{
    struct Cache {
        QVector<QPainterPath> paths;
        Cache() : paths(int(Glyph::Warn) + 1)
        {
            for (int i = 0; i < paths.size(); ++i)
                paths[i] = parse(pathData(Glyph(i)));
        }
    };
    static Cache cache;
    return cache.paths[int(g)];
}

// Draw the glyph centered in r as the largest square that fits, filled with
// the glyph's (or the caller's) accent color.
inline void draw(QPainter &p, Glyph g, const QRectF &r, const QColor &color = QColor())
{
    if (g == Glyph::None) {
        // explicit no-data placeholder, matching the app's gap convention
        QPen pen(Theme::TextFaint, 1.2, Qt::DashLine);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawRect(r.adjusted(2, 2, -2, -2));
        return;
    }
    const QPainterPath gp = glyphPath(g);
    const qreal side = qMin(r.width(), r.height());
    p.save();
    p.translate(r.center().x() - side / 2.0, r.center().y() - side / 2.0);
    p.scale(side / 16.0, side / 16.0);
    p.fillPath(gp, color.isValid() ? color : baseColor(g));
    p.restore();
}

} // namespace GlyphPaint
