#pragma once

#include "../SiteData.h"

#include <QString>
#include <QVector>
#include <QWidget>

class QScrollBar;

// Custom-painted, sortable, filterable, virtualized table for ONE flight
// board (arrivals or departures). Row model, sort and filter are rebuilt only
// when data / filter / sort changes; paintEvent just draws the visible slice.
// Repaints happen on setBoard / setFilter / sort / scroll / hover and the
// explicit minute-tick refreshTick() — there are no timers in here.
//
// States: !board.known paints a gap panel (never reads like "no traffic");
// known-but-empty paints "no flights in window"; a filter that hides every
// row paints "no matches". Unknown cells render an em dash.
class FlightTable : public QWidget
{
    Q_OBJECT
public:
    enum class Mode { Arrivals, Departures };
    enum Column { ColFlight, ColPlace, ColAircraft, ColFirstSeen, ColLastSeen, ColStatus, ColCount };

    explicit FlightTable(QWidget *parent = nullptr);

    // Copies what it needs; the pointer is not retained. nullptr = unknown.
    void setBoard(const FlightBoard *board, Mode mode);
    // Reason shown under the "no ... data" gap headline (e.g. anonymous mode).
    void setGapReason(const QString &reason);
    void setFilter(const QString &text);
    QString filter() const { return m_filter; }

    // Minute tick: repaint only (age-style text); no data change.
    void refreshTick() { update(); }

    int visibleRowCount() const { return m_order.size(); }
    int sortColumn() const { return m_sortCol; }
    bool sortAscending() const { return m_sortAsc; }
    void setSort(int column, bool ascending);

    QSize sizeHint() const override { return {640, 360}; }
    QSize minimumSizeHint() const override { return {320, 160}; }

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    bool event(QEvent *e) override;

private:
    struct Row {
        QString cell[ColCount];
        qint64  key[ColCount];     // sort keys for time columns; -1 = unknown
        bool    placeUnknown = true;
    };

    void rebuildOrder();
    void updateScroll();
    int  rowHeight() const;
    int  headerHeight() const;
    int  titleHeight() const;
    QRect bodyRect() const;
    int  columnX(int c) const;
    int  columnW(int c) const;
    int  columnAt(int x) const;
    int  rowAt(const QPoint &pos) const;
    QString placeTitle() const;
    bool hasRows() const { return m_known && !m_rows.isEmpty(); }

    QVector<Row> m_rows;          // full model (precomputed cells)
    QVector<int> m_order;         // filtered + sorted indexes into m_rows
    QScrollBar  *m_bar = nullptr;
    Mode    m_mode = Mode::Arrivals;
    bool    m_known = false;
    QString m_window;             // boardWindowLabel(), empty if unknown
    QString m_gapReason;
    QString m_filter;
    int     m_sortCol = ColFirstSeen;
    bool    m_sortAsc = false;
    int     m_hoverRow = -1;      // index into m_order
    int     m_hoverHeader = -1;
};
