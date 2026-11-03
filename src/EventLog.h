#pragma once

#include <QPlainTextEdit>
#include <QString>

// Timestamped operations log, NOC style: "HH:MM:SSZ LEVEL SOURCE MESSAGE".
// Newest entries on top, capped at 300 lines.
class EventLog : public QPlainTextEdit
{
    Q_OBJECT
public:
    explicit EventLog(QWidget *parent = nullptr);

    void log(const QString &level, const QString &source, const QString &message,
             const QColor &color = QColor(0xd0, 0xda, 0xe4));

    static QString utcStamp();
};
