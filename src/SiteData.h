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
};

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
// Built-in default roster — example sites standing in for your fleet. The
// live roster is administrable at runtime (see SiteRegistry); it is seeded
// from this list and overridden by the saved JSON config. The endpoints are
// placeholders: the shipped FeedService is a stub and never contacts them.
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
