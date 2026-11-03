#include "EventLog.h"
#include "Theme.h"

#include <QDateTime>
#include <QScrollBar>

EventLog::EventLog(QWidget *parent)
    : QPlainTextEdit(parent)
{
    setReadOnly(true);
    setMaximumBlockCount(300);
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setFont(Theme::sans(12));
    setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background: %1; color: %2; border: none; selection-background-color: %3; }"
        "QScrollBar:vertical { background: %1; width: 8px; margin: 2px; }"
        "QScrollBar::handle:vertical { background: %4; border-radius: 4px; min-height: 20px; }"
        "QScrollBar:horizontal { background: %1; height: 8px; margin: 2px; }"
        "QScrollBar::handle:horizontal { background: %4; border-radius: 4px; min-width: 20px; }"
        "QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }"
        "QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }")
        .arg(Theme::BgDeep.name(), Theme::TextSoft.name(), Theme::BorderHi.name(), Theme::Border.name()));
}

QString EventLog::utcStamp()
{
    return QDateTime::currentDateTimeUtc().time().toString(QStringLiteral("hh:mm:ss")) + QLatin1Char('Z');
}

void EventLog::log(const QString &level, const QString &source, const QString &message,
                   const QColor &color)
{
    // Rich text collapses runs of spaces, so pad with NBSP for column alignment.
    // Level token gets its own severity color, like the reference's tags.
    const QChar nbsp(0x00A0);
    QColor levelC = Theme::TextMuted;
    if (level == QLatin1String("INFO")) levelC = Theme::Blue;
    else if (level == QLatin1String("WARN")) levelC = Theme::Amber;
    else if (level == QLatin1String("CRIT")) levelC = Theme::Red;
    else if (level == QLatin1String("FCST")) levelC = Theme::Magenta;

    const QString stamp = utcStamp();
    const QString src = (source + nbsp).leftJustified(14, nbsp);
    appendHtml(QStringLiteral("%1<span style='color:%2'>%3</span><span style='color:%4'>%5</span>%6")
                   .arg(stamp + nbsp, levelC.name(), level.leftJustified(4, nbsp) + nbsp,
                        color.name(), src, message.toHtmlEscaped()));
}
