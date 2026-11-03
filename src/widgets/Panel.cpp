#include "Panel.h"
#include "../Theme.h"

Panel::Panel(const QString &title, QWidget *parent)
    : QWidget(parent), m_title(title), m_valueColor(Theme::Text)
{
    setAttribute(Qt::WA_StyledBackground, false);
    setAutoFillBackground(false);

    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(1, 1, 1, 1);
    outer->setSpacing(0);

    QWidget *captionHost = new QWidget;
    captionHost->setFixedHeight(40);
    m_captionLay = new QHBoxLayout(captionHost);
    m_captionLay->setContentsMargins(0, 0, 8, 0);
    m_captionLay->setSpacing(8);
    m_captionLay->addStretch(1);   // corner control docks at the right edge
    outer->addWidget(captionHost);

    QWidget *body = new QWidget;
    m_content = new QVBoxLayout(body);
    m_content->setContentsMargins(10, 6, 10, 10);
    m_content->setSpacing(4);
    outer->addWidget(body, 1);
}

void Panel::setTitle(const QString &title) { m_title = title; update(); }

void Panel::setValue(const QString &value, const QColor &color)
{
    m_value = value;
    m_valueColor = color;
    update();
}

void Panel::setCornerWidget(QWidget *w)
{
    if (m_corner == w)
        return;
    if (m_corner)
        m_corner->deleteLater();
    m_corner = w;
    m_captionLay->addWidget(w);
    update();
}

void Panel::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    Theme::panelBody(p, rect(), 10, Theme::PanelBg, Theme::Border);

    // caption row
    const QFont titleFont = Theme::sans(15, true);
    p.setFont(titleFont);
    p.setPen(Theme::Text);
    p.drawText(QPoint(16, 27), m_title);

    if (!m_value.isEmpty()) {
        p.setFont(Theme::sans(16, true));
        p.setPen(m_valueColor);
        // keep the headline value clear of the corner control, when present
        const int reserve = m_corner ? m_corner->width() + 18 : 0;
        p.drawText(QRect(0, 10, qMax(0, width() - 16 - reserve), 24),
                   Qt::AlignRight | Qt::AlignVCenter, m_value);
    }

    // hairline separator under the caption
    p.setPen(QPen(Theme::Grid, 1));
    p.drawLine(12, 38, width() - 12, 38);
}
