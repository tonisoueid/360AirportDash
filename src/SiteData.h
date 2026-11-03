#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>
#include <cmath>

// ----------------------------------------------------------------------------
// Data model. "Measured, never invented": every field starts invalid
// (NaN for doubles, -1 for ints) and the UI renders an explicit gap ("---",
// dashed baseline) until a real sample arrives from the feed. Never fake a
// reading — this is the template's core invariant.
//
// The example payload is a node-health style sample (load / latency /
// sessions). Rename these fields to your domain; everything downstream
// (card rows, chart series, alert rules) follows the same NaN convention.
// See docs/recipes.md "Add a data domain".
// ----------------------------------------------------------------------------

// Default per-entry poll interval (matches FeedService::kRefreshSecs).
inline constexpr int kDefaultPollSecs = 900;

struct Site {
    QString name;      // display label, "FRANKFURT"
    QString code;      // ICAO airport code, exactly 4 letters A-Z, "EDDF"
    QString region;    // "EU"
    QString endpoint;  // feed locator — URL, host:port, device id… whatever
                       // your FeedService poll understands (may be empty)
    bool enabled = true;                   // watched / polled
    bool pinned = false;                   // kept at the top of the watchlist
    int  pollIntervalSecs = kDefaultPollSecs;   // per-entry poll interval
};

inline bool operator==(const Site &a, const Site &b)
{
    return a.name == b.name && a.code == b.code && a.region == b.region
        && a.endpoint == b.endpoint && a.enabled == b.enabled
        && a.pinned == b.pinned && a.pollIntervalSecs == b.pollIntervalSecs;
}

// One flight on an arrival/departure board (OpenSky). Text fields are empty
// when the feed reported null; firstSeen/lastSeen are invalid when absent.
struct FlightEntry {
    QString   icao24;          // transponder address, lowercase hex
    QString   callsign;        // trimmed; empty when unknown
    QString   estDepartureIcao;   // estimated departure airport; empty = unknown
    QString   estArrivalIcao;     // estimated arrival airport; empty = unknown
    QDateTime firstSeen;       // UTC; invalid when absent
    QDateTime lastSeen;        // UTC; invalid when absent
};

// A board is tri-state: !known = never measured / failed (render gaps, NOT
// zero flights); known && flights.isEmpty() = measured, nothing flew.
struct FlightBoard {
    bool known = false;
    QVector<FlightEntry> flights;
    // The begin/end actually queried for this board (UTC). Both invalid when
    // unknown; set only by the feed from a real request, never invented.
    QDateTime windowBeginUtc;
    QDateTime windowEndUtc;
};

inline bool boardWindowKnown(const FlightBoard &b)
{
    return b.known && b.windowBeginUtc.isValid() && b.windowEndUtc.isValid()
        && b.windowEndUtc > b.windowBeginUtc;
}

// "hour ending 09:15Z" / "24 h ending 09:15Z" from the stored window; empty
// when the board or its window is unknown.
inline QString boardWindowLabel(const FlightBoard &b)
{
    if (!boardWindowKnown(b))
        return QString();
    const qint64 secs = b.windowBeginUtc.secsTo(b.windowEndUtc);
    const QString end = b.windowEndUtc.toUTC().toString(QStringLiteral("HH:mm")) + QLatin1Char('Z');
    QString len;
    if (secs == 3600)
        return QStringLiteral("hour ending ") + end;
    if (secs % 3600 == 0)
        len = QStringLiteral("%1 h").arg(secs / 3600);
    else if (secs % 60 == 0)
        len = QStringLiteral("%1 min").arg(secs / 60);
    else
        len = QStringLiteral("%1 s").arg(secs);
    return len + QStringLiteral(" ending ") + end;
}

// Table-cell formatters: "\u2014" for any empty / invalid field.
inline QString cellText(const QString &s)
{
    const QString t = s.trimmed();
    return t.isEmpty() ? QStringLiteral("\u2014") : t;
}
inline QString cellTime(const QDateTime &t)
{
    return t.isValid() ? t.toUTC().toString(QStringLiteral("HH:mm")) + QLatin1Char('Z')
                       : QStringLiteral("\u2014");
}
inline QString cellCallsign(const FlightEntry &e) { return cellText(e.callsign); }
inline QString cellIcao24(const FlightEntry &e)   { return cellText(e.icao24); }
inline QString cellFrom(const FlightEntry &e)     { return cellText(e.estDepartureIcao); }
inline QString cellTo(const FlightEntry &e)       { return cellText(e.estArrivalIcao); }
inline QString cellFirstSeen(const FlightEntry &e){ return cellTime(e.firstSeen); }
inline QString cellLastSeen(const FlightEntry &e) { return cellTime(e.lastSeen); }

// Status derived ONLY from what the feed reported in firstSeen/lastSeen.
// Anything unreported or inconsistent -> "\u2014" (never a guess).
inline QString flightStatus(const FlightEntry &e)
{
    const bool f = e.firstSeen.isValid(), l = e.lastSeen.isValid();
    if (f && l) {
        if (e.lastSeen < e.firstSeen)
            return QStringLiteral("\u2014");
        return e.lastSeen == e.firstSeen ? QStringLiteral("Single sighting")
                                         : QStringLiteral("Tracked");
    }
    if (f) return QStringLiteral("First seen only");
    if (l) return QStringLiteral("Last seen only");
    return QStringLiteral("\u2014");
}

// One measurement of one site. Doubles start NaN, ints start -1; every
// consumer must check before use.
struct Sample {
    QDateTime stampUtc;               // when the feed says it was measured
    double loadPct    = std::nan(""); // primary metric, 0–100
    double latencyMs  = std::nan(""); // network / secondary metric
    int    sessions   = -1;           // tertiary count
    FlightBoard arrivals;             // default: unknown (known=false)
    FlightBoard departures;           // default: unknown (known=false)
};

struct SiteData {
    Site site;
    Sample current;                   // last applied sample (any state)
    QVector<Sample> recent;           // trailing history, newest last

    static constexpr int kHistoryCap = 48;   // samples retained per site

    enum class State { Pending, Live, Stale, Error };
    State state = State::Pending;
    QString error;
    QDateTime lastSyncUtc;            // when this site last answered
    // When a poll last returned at least one measured board. Invalid until
    // then; never touched by a failure (no fetch means no stamp).
    QDateTime boardsFetchedUtc;

    bool valid() const { return state == State::Live || state == State::Stale; }

    // Seconds since the site last answered the poll; -1 when never.
    int ageSecs() const
    { return lastSyncUtc.isValid() ? int(lastSyncUtc.secsTo(QDateTime::currentDateTimeUtc())) : -1; }

    // Primary-metric trend: newest minus oldest of the last 3 valid samples
    // (NaN when the history is too short) — the input for trend-style watch
    // rules. Signed: a falling metric yields a negative delta.
    double loadTrend3() const
    {
        QVector<double> v;
        v.reserve(3);
        for (int i = recent.size() - 1; i >= 0 && v.size() < 3; --i)
            if (!std::isnan(recent[i].loadPct))
                v.push_back(recent[i].loadPct);      // newest first
        if (v.size() < 3)
            return std::nan("");
        return v.front() - v.back();
    }
};

// ----------------------------------------------------------------------------
// Card badge state, derived (never stored) from what actually happened.
//   Stale / Error  — SiteData::State, taking precedence only when no pause.
//   Paused         — a 429 Retry-After pause is active and the site has no
//                    fresh (Live) data.
//   Live           — Live state, >= 1 known board holding flights.
//   Quiet          — Live state, >= 1 known board, all known boards empty.
//   Pending        — never polled (or Live with no known board): a gap,
//                    not a status claim. known=false boards are never zero.
// ----------------------------------------------------------------------------
enum class CardBadge { Live, Quiet, Stale, Error, Paused, Pending };

inline CardBadge cardBadge(const SiteData &d, bool paused)
{
    if (paused && d.state != SiteData::State::Live)
        return CardBadge::Paused;
    switch (d.state) {
    case SiteData::State::Stale: return CardBadge::Stale;
    case SiteData::State::Error: return CardBadge::Error;
    case SiteData::State::Pending: return CardBadge::Pending;
    case SiteData::State::Live: break;
    }
    const FlightBoard &a = d.current.arrivals;
    const FlightBoard &b = d.current.departures;
    if (!a.known && !b.known)
        return CardBadge::Pending;
    const bool anyFlights = (a.known && !a.flights.isEmpty())
                         || (b.known && !b.flights.isEmpty());
    return anyFlights ? CardBadge::Live : CardBadge::Quiet;
}

// ----------------------------------------------------------------------------
// Built-in default roster — example sites standing in for your fleet. The
// live roster is administrable at runtime (see SiteRegistry); it is seeded
// from this list and overridden by the saved JSON config. The endpoints are
// placeholders: the OpenSky feed queries by ICAO and never contacts them.
// ----------------------------------------------------------------------------
inline const QVector<Site> &siteList()
{
    static const QVector<Site> sites = {
        {"FRANKFURT", "EDDF", "EU", "https://example.com/api/v1/nodes/fra1"},
        {"LONDON HEATHROW", "EGLL", "EU", "https://example.com/api/v1/nodes/lhr1"},
        {"NEW YORK JFK", "KJFK", "US-EAST",   "https://example.com/api/v1/nodes/jfk1"},
        {"DALLAS FORT WORTH", "KDFW", "US-CENTRAL","https://example.com/api/v1/nodes/dfw1"},
        {"SAO PAULO GUARULHOS", "SBGR", "LATAM",     "https://example.com/api/v1/nodes/gru1"},
        {"CAPE TOWN", "FACT", "AFRICA",    "https://example.com/api/v1/nodes/cpt1"},
        {"SINGAPORE CHANGI", "WSSS", "APAC",      "https://example.com/api/v1/nodes/sin1"},
        {"SYDNEY", "YSSY", "APAC",      "https://example.com/api/v1/nodes/syd1"},
    };
    return sites;
}

// ----------------------------------------------------------------------------
// Watch rules — evaluated on every sample. NOC semantics: a rule that fires
// logs an event; a rule that stops firing logs a "nominal" clearing event
// (the diffing lives in MainWindow). Keys must be STABLE rule ids — the
// event log diffs raise / severity-change / clear on the key alone, so a
// message must never encode a changing value that should re-fire.
// ----------------------------------------------------------------------------

struct Alert {
    enum class Sev { Info, Warn, Crit };
    Sev sev = Sev::Info;
    QString key;      // stable rule id, used for raise/clear diffing
    QString message;  // human text, no site prefix
};

inline const char *sevLabel(Alert::Sev s)
{
    switch (s) { case Alert::Sev::Crit: return "CRIT"; case Alert::Sev::Warn: return "WARN";
                 default: return "INFO"; }
}

inline bool operator==(const Alert &a, const Alert &b) { return a.key == b.key; }

// Example rules over the example payload. Add yours the same way: guard
// every field for the missing-data convention, then add() with a stable key.
inline QVector<Alert> evaluateAlerts(const SiteData &d)
{
    QVector<Alert> out;
    const auto add = [&](Alert::Sev s, QString key, QString msg) { out.push_back({s, key, msg}); };
    const Sample &c = d.current;

    if (!std::isnan(c.loadPct)) {
        if (c.loadPct >= 90)      add(Alert::Sev::Crit, "LOAD", QStringLiteral("CRITICAL LOAD %1%").arg(c.loadPct, 0, 'f', 0));
        else if (c.loadPct >= 75) add(Alert::Sev::Warn, "LOAD", QStringLiteral("HIGH LOAD %1%").arg(c.loadPct, 0, 'f', 0));
    }
    if (!std::isnan(c.latencyMs)) {
        if (c.latencyMs >= 1000)      add(Alert::Sev::Crit, "LATENCY", QStringLiteral("LATENCY %1 MS").arg(qRound(c.latencyMs)));
        else if (c.latencyMs >= 400)  add(Alert::Sev::Warn, "LATENCY", QStringLiteral("LATENCY %1 MS").arg(qRound(c.latencyMs)));
    }
    // Trend rule over the trailing history — the pattern for "rate of
    // change" watches (mirrors the original template's pressure-trend rule).
    const double rise = d.loadTrend3();
    if (!std::isnan(rise) && rise >= 25.0)
        add(Alert::Sev::Warn, "LOADRISE",
            QStringLiteral("LOAD RISING +%1 PTS OVER LAST 3 SAMPLES").arg(rise, 0, 'f', 0));
    return out;
}
