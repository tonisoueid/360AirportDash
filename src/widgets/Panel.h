#pragma once

#include <QColor>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>

// A rounded dashboard card with a white title, an optional right-aligned
// headline value (like the reference's "CPU Overview — 11.6%"), an optional
// corner control at the caption's right edge (the panel expand button), and
// a hairline separator above the content.
class Panel : public QWidget
{
    Q_OBJECT
public:
    explicit Panel(const QString &title, QWidget *parent = nullptr);

    void setTitle(const QString &title);
    void setValue(const QString &value, const QColor &color);
    // Host a small control at the caption's right edge, beside the headline
    // value. Single control only: a second call replaces the first.
    void setCornerWidget(QWidget *w);
    QVBoxLayout *contentLayout() { return m_content; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QString m_title, m_value;
    QColor m_valueColor;
    QVBoxLayout *m_content;
    QHBoxLayout *m_captionLay = nullptr;
    QWidget *m_corner = nullptr;
};
