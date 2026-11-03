#pragma once

#include "Theme.h"

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPointF>
#include <QWidget>

// Small caption-corner control that blows its panel up to fill the whole
// console body — and back. Icon: a rounded rectangle tile with a plus sign
// while the panel is at its normal size (click = expand) and a minus sign
// while the panel is expanded (click = restore). All state changes — clicks
// included — go through setExpanded() so the glyph and tool tip can never
// drift apart.
class ExpandButton : public QWidget
{
    Q_OBJECT
public:
    explicit ExpandButton(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setAutoFillBackground(false);
        setCursor(Qt::PointingHandCursor);
        setFixedSize(26, 26);
        setToolTip(QStringLiteral("Expand to full view"));
    }

    bool expanded() const { return m_expanded; }

    // Reflect the panel's state (also driven from outside, e.g. when the
    // operator restores the layout from a different panel's control).
    void setExpanded(bool on)
    {
        if (m_expanded == on)
            return;
        m_expanded = on;
        setToolTip(on ? QStringLiteral("Restore panel size")
                      : QStringLiteral("Expand to full view"));
        update();
    }

signals:
    void toggled(bool expanded);

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        // chassis: rounded hairline tile in the panel-border family
        p.setPen(QPen(m_hover ? Theme::BorderHi : Theme::Border, 1));
        p.setBrush(Theme::PanelBg);
        p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 5, 5);

        // glyph: + while normal, − while expanded
        const double cx = width() / 2.0, cy = height() / 2.0;
        QPen pen(m_hover ? Theme::BlueBright : Theme::TextMuted, 1.8);
        pen.setCapStyle(Qt::RoundCap);
        p.setPen(pen);
        const double arm = 4.2;
        p.drawLine(QPointF(cx - arm, cy), QPointF(cx + arm, cy));
        if (!m_expanded)
            p.drawLine(QPointF(cx, cy - arm), QPointF(cx, cy + arm));
    }

    void mouseReleaseEvent(QMouseEvent *e) override
    {
        if (rect().contains(e->pos())) {
            setExpanded(!m_expanded);
            emit toggled(m_expanded);
        }
        QWidget::mouseReleaseEvent(e);
    }

    void enterEvent(QEnterEvent *) override { m_hover = true; update(); }
    void leaveEvent(QEvent *) override { m_hover = false; update(); }

private:
    bool m_expanded = false;
    bool m_hover = false;
};
