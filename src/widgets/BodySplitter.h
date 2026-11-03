#pragma once

#include "Theme.h"

#include <QEvent>
#include <QPainter>
#include <QPointF>
#include <QSplitter>
#include <QSplitterHandle>

// The draggable vertical seam between the site matrix and the analysis
// column. QSplitter supplies the drag mechanics (both sides clamp at their
// minimum widths and can never be dragged out of existence — showing and
// hiding panels is the caption expand controls' job); the handle paints the
// hairline that marks the divider and lights it up on hover.
class BodySplitterHandle : public QSplitterHandle
{
    Q_OBJECT
public:
    using QSplitterHandle::QSplitterHandle;

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        QPen pen(m_hover ? Theme::BorderHi : Theme::Border, m_hover ? 2 : 1);
        pen.setCapStyle(Qt::RoundCap);
        p.setPen(pen);
        const double x = width() / 2.0;
        p.drawLine(QPointF(x, 6), QPointF(x, height() - 6.0));
    }

    void enterEvent(QEnterEvent *) override { m_hover = true; update(); }
    void leaveEvent(QEvent *) override { m_hover = false; update(); }

private:
    bool m_hover = false;
};

class BodySplitter : public QSplitter
{
    Q_OBJECT
public:
    explicit BodySplitter(QWidget *parent = nullptr)
        : QSplitter(Qt::Horizontal, parent)
    {
        setObjectName(QStringLiteral("bodySplit"));
        setChildrenCollapsible(false);
        setHandleWidth(8);
    }

protected:
    QSplitterHandle *createHandle() override
    {
        auto *h = new BodySplitterHandle(orientation(), this);
        h->setCursor(Qt::SplitHCursor);
        return h;
    }
};
